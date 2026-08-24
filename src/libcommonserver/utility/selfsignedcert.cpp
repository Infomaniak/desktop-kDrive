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
#include "libcommonserver/log/log.h"
#include "libcommonserver/keychainmanager/keychainmanager.h"

#include <Poco/Crypto/RSAKey.h>
#include <Poco/Exception.h>
#include <Poco/Crypto/X509Certificate.h>

#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/bn.h>

#include <log4cplus/loggingmacros.h>

namespace KDC {

namespace {

struct X509Deleter {
        void operator()(X509 *const x) const { X509_free(x); }
};
using UniqueX509 = std::unique_ptr<X509, X509Deleter>;

struct EvpPkeyDeleter {
        void operator()(EVP_PKEY *const p) const { EVP_PKEY_free(p); }
};
using UniqueEvpPkey = std::unique_ptr<EVP_PKEY, EvpPkeyDeleter>;

struct BignumDeleter {
        void operator()(BIGNUM *const b) const { BN_free(b); }
};
using UniqueBignum = std::unique_ptr<BIGNUM, BignumDeleter>;

constexpr int64_t certValiditySeconds = 60LL * 60LL * 24LL * 365LL;

const char *sslError() {
    const unsigned long err = ERR_get_error();
    return err ? ERR_error_string(err, nullptr) : "no OpenSSL error";
}

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
        Poco::Crypto::RSAKey key(Poco::Crypto::RSAKey::KL_2048, Poco::Crypto::RSAKey::EXP_LARGE);

        UniqueEvpPkey pkey(EVP_PKEY_new());
        if (!pkey || EVP_PKEY_set1_RSA(pkey.get(), key.impl()->getRSA()) != 1) {
            LOG_ERROR(Log::instance()->getLogger(), "EVP_PKEY setup failed: " << sslError());
            return false;
        }

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

        Poco::Crypto::X509Certificate cert(x509.release()); // takes ownership

        std::ostringstream certOut;
        cert.save(certOut);

        std::ostringstream keyOut;
        key.save(nullptr, &keyOut, ""); // public stream null, private to keyOut, no passphrase

        pem.cert = certOut.str();
        pem.key = keyOut.str();

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
