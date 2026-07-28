#include "selfsignedcert.h"

#include "comm.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/keychainmanager/keychainmanager.h"

#include <Poco/Crypto/RSAKey.h>
#include <Poco/Exception.h>
#include <Poco/Crypto/X509Certificate.h>

#include <openssl/x509.h>
#include <openssl/evp.h>
#include <openssl/err.h>

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

constexpr int64_t certValiditySeconds = 60LL * 60LL * 24LL * 365LL;

const char *sslError() {
    const unsigned long err = ERR_get_error();
    return err ? ERR_error_string(err, nullptr) : "no OpenSSL error";
}

bool fillCertificateFields(X509 *const x509) {
    if (X509_set_version(x509, 2) != 1) {
        LOG_ERROR(Log::instance()->getLogger(), "X509_set_version failed: " << sslError());
        return false;
    }
    if (ASN1_INTEGER_set(X509_get_serialNumber(x509), 1) != 1) {
        LOG_ERROR(Log::instance()->getLogger(), "ASN1_INTEGER_set failed: " << sslError());
        return false;
    }
    if (X509_gmtime_adj(X509_getm_notBefore(x509), 0) == nullptr) {
        LOG_ERROR(Log::instance()->getLogger(), "X509_gmtime_adj (notBefore) failed: " << sslError());
        return false;
    }
    if (X509_gmtime_adj(X509_getm_notAfter(x509), certValiditySeconds) == nullptr) {
        LOG_ERROR(Log::instance()->getLogger(), "X509_gmtime_adj (notAfter) failed: " << sslError());
        return false;
    }

    X509_NAME *const name = X509_get_subject_name(x509);
    if (X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const uint8_t *>("kDrive-localhost"), -1, -1, 0) !=
        1) {
        LOG_ERROR(Log::instance()->getLogger(), "X509_NAME_add_entry_by_txt failed: " << sslError());
        return false;
    }
    if (X509_set_issuer_name(x509, name) != 1) {
        LOG_ERROR(Log::instance()->getLogger(), "X509_set_issuer_name failed: " << sslError());
        return false;
    }
    return true;
}

} // namespace

bool SelfSignedCert::loadOrGenerate(Pem &pem) {
    const auto keychain = KeyChainManager::instance();
    if (!keychain) {
        LOG_ERROR(Log::instance()->getLogger(), "Keychain unavailable");
        return false;
    }

    bool certFound = false;
    bool keyFound = false;
    const std::string certKey(certKeychainKey);
    const std::string privKey(keyKeychainKey);

    if (keychain->readDataFromKeystore(certKey, pem.cert, certFound) &&
        keychain->readDataFromKeystore(privKey, pem.key, keyFound) && certFound && keyFound && !pem.cert.empty() &&
        !pem.key.empty()) {
        return true;
    }

    if (!generate(pem)) return false;

    if (!keychain->writeToken(certKey, pem.cert) || !keychain->writeToken(privKey, pem.key)) {
        LOG_ERROR(Log::instance()->getLogger(), "Failed to store TLS material in the keychain");
        return false;
    }
    return true;
}

bool SelfSignedCert::generate(Pem &pem) {
    LOG_INFO(Log::instance()->getLogger(), "Generating self-signed certificate/key pair for local TLS IPC");
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

        if (!fillCertificateFields(x509.get())) return false;

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
