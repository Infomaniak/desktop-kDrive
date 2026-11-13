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

#include "selfsignedcert.h"

#include "comm.h"
#include "libcommonserver/utility/crypto.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/keychainmanager/keychainmanager.h"

#include <Poco/Crypto/X509Certificate.h>

#include <log4cplus/loggingmacros.h>

namespace KDC {

namespace {

constexpr int64_t certValiditySeconds = 60LL * 60LL * 24LL * 365LL;

bool addExtension(X509 *const x509, const int32_t nid, const char *const value) {
    X509V3_CTX ctx;
    X509V3_set_ctx_nodb(&ctx);
    X509V3_set_ctx(&ctx, x509, x509, nullptr, nullptr, 0); // issuer == subject (self-signed)

    X509_EXTENSION *const ext = X509V3_EXT_conf_nid(nullptr, &ctx, nid, value);
    if (!ext) {
        LOG_ERROR(Log::instance()->getLogger(), "X509V3_EXT_conf_nid failed for nid " << nid << ": " << sslError());
        return false;
    }
    const int32_t rc = X509_add_ext(x509, ext, -1);
    X509_EXTENSION_free(ext);
    if (rc != 1) {
        LOG_ERROR(Log::instance()->getLogger(), "X509_add_ext failed for nid " << nid << ": " << sslError());
        return false;
    }
    return true;
}

bool setRandomSerialNumber(X509 *const x509) {
    // RFC 5280 §4.1.2.2: the serial number must be a positive integer (≤ 20 bytes).
    // Generate a random 159-bit BIGNUM: strictly positive, always minimally DER-encoded,
    // and safely inside the 20-byte ceiling with the sign bit clear. This mirrors
    // OpenSSL's own rand_serial() and avoids the weak cstdlib rand() and manual masking.
    UniqueBignum bn(BN_new());
    if (!bn) {
        LOG_ERROR(Log::instance()->getLogger(), "BN_new failed for serial: " << sslError());
        return false;
    }
    if (constexpr int serialBits = 159; BN_rand(bn.get(), serialBits, BN_RAND_TOP_ONE, BN_RAND_BOTTOM_ANY) != 1) {
        LOG_ERROR(Log::instance()->getLogger(), "BN_rand failed for serial: " << sslError());
        return false;
    }
    ASN1_INTEGER *const serial = X509_get_serialNumber(x509);
    if (BN_to_ASN1_INTEGER(bn.get(), serial) == nullptr) {
        LOG_ERROR(Log::instance()->getLogger(), "BN_to_ASN1_INTEGER failed for serial: " << sslError());
        return false;
    }
    return true;
}

bool fillCertificateFields(X509 *const x509, bool isClientCert) {
    if (X509_set_version(x509, 2) != 1) {
        LOG_ERROR(Log::instance()->getLogger(), "X509_set_version failed: " << sslError());
        return false;
    }
    if (!setRandomSerialNumber(x509)) return false;

    if (X509_gmtime_adj(X509_getm_notBefore(x509), 0) == nullptr) {
        LOG_ERROR(Log::instance()->getLogger(), "X509_gmtime_adj (notBefore) failed: " << sslError());
        return false;
    }
    if (X509_gmtime_adj(X509_getm_notAfter(x509), certValiditySeconds) == nullptr) {
        LOG_ERROR(Log::instance()->getLogger(), "X509_gmtime_adj (notAfter) failed: " << sslError());
        return false;
    }

    const char *const subject = isClientCert ? clientCertSubject : serverCertSubject;
    X509_NAME *const name = X509_get_subject_name(x509);
    if (X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char *>(subject), -1, -1, 0) != 1) {
        LOG_ERROR(Log::instance()->getLogger(), "X509_NAME_add_entry_by_txt failed: " << sslError());
        return false;
    }
    if (X509_set_issuer_name(x509, name) != 1) {
        LOG_ERROR(Log::instance()->getLogger(), "X509_set_issuer_name failed: " << sslError());
        return false;
    }

    // v3 extensions. Must be added after the subject/issuer name is set (the SAN and
    // basicConstraints config draw on the X509V3 context) and before X509_sign, since
    // signing freezes the certificate and anything added afterwards is not covered by
    // the signature.
    if (!addExtension(x509, NID_basic_constraints, "critical,CA:FALSE")) return false;
    if (!addExtension(x509, NID_key_usage, "critical,digitalSignature,keyEncipherment")) return false;
    if (!addExtension(x509, NID_ext_key_usage, isClientCert ? "clientAuth" : "serverAuth")) return false;
    if (!isClientCert) {
        const std::string san = std::string("IP:127.0.0.1,IP:0:0:0:0:0:0:0:1,DNS:") + localHostName;
        if (!addExtension(x509, NID_subject_alt_name, san.c_str())) return false;
    }

    return true;
}

} // namespace

bool SelfSignedCert::generateAndPublishServerCert(Pem &pem) {
    const auto keychain = KeyChainManager::instance();
    if (!keychain) {
        LOG_ERROR(Log::instance()->getLogger(), "Keychain unavailable");
        return false;
    }
    if (!generate(pem, false)) return false;

    if (!keychain->writeData(std::string(certKeychainKey), pem.cert)) {
        LOG_ERROR(Log::instance()->getLogger(), "Failed to store the TLS certificate in the keychain");
        return false;
    }

    return true;
}

bool SelfSignedCert::generateAndPublishClientCert(Pem &pem) {
    const auto keychain = KeyChainManager::instance();
    if (!keychain) {
        LOG_ERROR(Log::instance()->getLogger(), "Keychain unavailable");
        return false;
    }
    if (!generate(pem, true)) return false;

    if (!keychain->writeData(std::string(clientCertKeychainKey), pem.cert)) {
        LOG_ERROR(Log::instance()->getLogger(), "Failed to store the TLS client certificate in the keychain");
        return false;
    }
    if (!keychain->writeData(std::string(clientKeyKeychainKey), pem.key)) {
        LOG_ERROR(Log::instance()->getLogger(), "Failed to store the TLS client private key in the keychain");
        return false;
    }

    return true;
}

bool SelfSignedCert::generate(Pem &pem, bool isClientCert) {
    LOG_INFO(Log::instance()->getLogger(),
             "Generating self-signed " << (isClientCert ? "client" : "server") << " certificate/key pair for local TLS IPC");
    try {
        // Generation of an asymmetric key pair.
        UniqueEvpPkey pkey(EVP_PKEY_Q_keygen(nullptr, nullptr, "RSA", 2048));
        if (!pkey) {
            LOG_ERROR(Log::instance()->getLogger(), "EVP_PKEY_Q_keygen failed: " << sslError());
            return false;
        }

        // Generation of a X.509 certificate
        UniqueX509 x509(X509_new());
        if (!x509) {
            LOG_ERROR(Log::instance()->getLogger(), "X509_new failed: " << sslError());
            return false;
        }

        if (!fillCertificateFields(x509.get(), isClientCert)) return false;

        if (X509_set_pubkey(x509.get(), pkey.get()) != 1) {
            LOG_ERROR(Log::instance()->getLogger(), "X509_set_pubkey failed: " << sslError());
            return false;
        }
        if (X509_sign(x509.get(), pkey.get(), EVP_sha256()) == 0) {
            LOG_ERROR(Log::instance()->getLogger(), "X509_sign failed: " << sslError());
            return false;
        }

        // Save the certificate to pem.cert
        Poco::Crypto::X509Certificate cert(x509.release()); // takes ownership

        std::ostringstream certOut;
        cert.save(certOut);
        pem.cert = certOut.str();

        // Export the pkey to a PEM BIO buffer
        UniqueBio bio(BIO_new(BIO_s_mem()));
        if (!bio || PEM_write_bio_PrivateKey(bio.get(), pkey.get(), nullptr, nullptr, 0, nullptr, nullptr) != 1) {
            LOG_ERROR(Log::instance()->getLogger(), "Private key export failed: " << sslError());
            return false;
        }

        // Save the BIO buffer to pem.key
        char *data = nullptr;
        const auto len = BIO_get_mem_data(bio.get(), &data);
        if (len <= 0 || !data) {
            LOG_ERROR(Log::instance()->getLogger(), "Private key export returned no data: " << sslError());
            return false;
        }
        (void) pem.key.assign(data, static_cast<size_t>(len));

        // Clear memory
        OPENSSL_cleanse(data, static_cast<size_t>(len));

        if (pem.cert.empty() || pem.key.empty()) {
            LOG_ERROR(Log::instance()->getLogger(), "Empty PEM output after certificate generation");
            return false;
        }
    } catch (const Poco::Exception &e) {
        LOG_ERROR(Log::instance()->getLogger(), "Certificate generation failed: " << e.displayText());
        return false;
    }
    return true;
}

} // namespace KDC
