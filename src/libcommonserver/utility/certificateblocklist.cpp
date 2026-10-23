/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "certificateblocklist.h"

#include "libcommonserver/log/log.h"

#include "libcommon/log/sentry/handler.h"

#include <log4cplus/loggingmacros.h>

#include <openssl/evp.h>
#include <openssl/x509.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>

namespace KDC {

namespace {

// SHA-256 of the SubjectPublicKeyInfo of blocked certificates, lowercase hex.
// Hashing the key rather than the certificate also covers cross-signed copies of the same intermediate.
constexpr std::array<std::string_view, 8> blockedSpkiSha256s = {
        // CVE-2026-111943: eMudhra intermediates with trivially recoverable keys.
        // Sources: Mozilla bug 2083449, OkHttp commit fb34d4da74.
        "43bffa19d6ef651451f6841e019931e96f6f2eaeff28ab4b6f1ce4576de2717b", // EM OV TLS CA - G3B
        "49dd353dc2b919a3770645117d0e60c16115e7d1bc259eefe734b00ea434fe5f", // EM DV TLS CA - G3C
        "5c0660a2b164910d8ff589c18e14552e9e0ec237dcefe2cb736d41a4811f81a0", // EM EV TLS CA - G3C
        "63ecefee431d49523bc1136c4e72d0206a477086caf606cd5cf025686c109cf8", // EM OV TLS CA - G3C
        "92c063d67e8f14db71d288e11266ffdaf69cbf115f7bab14f3bc2f68d25e0e30", // EM OV CS CA - G3C (code signing)
        "e0254c7fcdc265e2f0f8a428dabb75ab4ae039b4872d62f5ee57f0877d5a8a41", // EM EV TLS CA - G3B
        "ebed6475c2c2d80fcb199f3fe8dd6e6b3ce60b77eb15d4e42c3dec0c6f5b5105", // EM DV TLS CA - G3B
        "ec4ffa1ad0ac88081314b7aaffef6a91f97cd57291bb5ba686c5415f5eb4f6b6", // EM OV CS CA - G3B (code signing)
};

// Returns the lowercase hex SHA-256 of the certificate's DER-encoded SubjectPublicKeyInfo, or an empty string on error.
std::string spkiSha256Hex(const X509 *const cert) {
    unsigned char *der = nullptr;
    const int32_t derLen = i2d_X509_PUBKEY(X509_get_X509_PUBKEY(cert), &der);
    if (derLen <= 0 || !der) return {};

    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    uint32_t digestLen = 0;
    const int32_t rc = EVP_Digest(der, static_cast<size_t>(derLen), digest.data(), &digestLen, EVP_sha256(), nullptr);
    OPENSSL_free(der);
    if (rc != 1) return {};

    static constexpr char hexDigits[] = "0123456789abcdef";
    std::string hex;
    hex.reserve(digestLen * 2);
    for (uint32_t i = 0; i < digestLen; ++i) {
        hex.push_back(hexDigits[digest[i] >> 4]);
        hex.push_back(hexDigits[digest[i] & 0x0F]);
    }
    return hex;
}

std::string nameToString(const X509_NAME *const name) {
    if (!name) return {};
    std::array<char, 512> buffer{};
    if (!X509_NAME_oneline(name, buffer.data(), static_cast<int32_t>(buffer.size()))) return {};
    return std::string(buffer.data());
}

// Returns the SNI host of the connection being verified, empty when there is none (e.g. in tests).
std::string serverName(const X509_STORE_CTX *const storeCtx) {
    const auto *const ssl = static_cast<const SSL *>(X509_STORE_CTX_get_ex_data(storeCtx, SSL_get_ex_data_X509_STORE_CTX_idx()));
    if (!ssl) return {};
    const char *const host = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
    return host ? std::string(host) : std::string();
}

void reportRejection(const X509_STORE_CTX *const storeCtx, const X509 *const cert, const int32_t depth) {
    const std::string message = "Rejected TLS chain with blocked certificate (host=" + serverName(storeCtx) +
                                ", depth=" + std::to_string(depth) + ", subject=" + nameToString(X509_get_subject_name(cert)) +
                                ", issuer=" + nameToString(X509_get_issuer_name(cert)) + ")";
    LOG_WARN(Log::instance()->getLogger(), message);

    // Sentry is notified once per process: a rejection likely means an active interception.
    static std::atomic_flag sentryNotified;
    if (!sentryNotified.test_and_set()) {
        sentry::Handler::captureMessage(sentry::Level::Error, "CertificateBlocklist::verifyChain", message);
    }
}

} // namespace

void CertificateBlocklist::install(SSL_CTX *ctx) {
    if (!ctx) {
        LOG_WARN(Log::instance()->getLogger(), "SSL_CTX is null, certificate blocklist not installed");
        return;
    }
    SSL_CTX_set_cert_verify_callback(ctx, &CertificateBlocklist::verifyChain, nullptr);
}

int CertificateBlocklist::verifyChain(X509_STORE_CTX *storeCtx, void * /*arg*/) {
    // Runs the regular verification, which also calls Poco's per-certificate callback.
    if (X509_verify_cert(storeCtx) != 1) return 0;

    // Check the validated chain, not the one sent by the peer: a sub-CA signed with a leaked key
    // still has the compromised intermediate in its validated chain.
    STACK_OF(X509) *const chain = X509_STORE_CTX_get0_chain(storeCtx);
    const int32_t count = chain ? sk_X509_num(chain) : 0;
    for (int32_t depth = 0; depth < count; ++depth) {
        auto *const cert = sk_X509_value(chain, depth);
        if (!isBlocked(cert)) continue;

        reportRejection(storeCtx, cert, depth);
        X509_STORE_CTX_set_current_cert(storeCtx, cert);
        X509_STORE_CTX_set_error_depth(storeCtx, depth);
        X509_STORE_CTX_set_error(storeCtx, X509_V_ERR_CERT_REJECTED);
        return 0;
    }
    return 1;
}

bool CertificateBlocklist::isBlocked(const X509 *cert) {
    if (!cert) return false;
    const std::string hash = spkiSha256Hex(cert);
    if (hash.empty()) return false;
    return std::ranges::find(blockedSpkiSha256s, hash) != blockedSpkiSha256s.end();
}

} // namespace KDC
