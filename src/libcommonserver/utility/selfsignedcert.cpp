#include "selfsignedcert.h"

#include "libcommonserver/log/log.h"
#include "libcommonserver/keychainmanager/keychainmanager.h"

#include <Poco/Crypto/RSAKey.h>
#include <Poco/Exception.h>
#include <Poco/Crypto/X509Certificate.h>
#include <openssl/x509.h>
#include <openssl/evp.h>

#include <log4cplus/loggingmacros.h>

namespace KDC {

namespace {

struct X509Deleter {
        void operator()(X509 *x) const { X509_free(x); }
};
using UniqueX509 = std::unique_ptr<X509, X509Deleter>;

struct EvpPkeyDeleter {
        void operator()(EVP_PKEY *p) const { EVP_PKEY_free(p); }
};
using UniqueEvpPkey = std::unique_ptr<EVP_PKEY, EvpPkeyDeleter>;


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
        if (!pkey || !EVP_PKEY_set1_RSA(pkey.get(), const_cast<RSA *>(key.impl()->getRSA()))) {
            LOG_ERROR(Log::instance()->getLogger(), "EVP_PKEY setup failed");
            return false;
        }

        UniqueX509 x509(X509_new());
        if (!x509) {
            LOG_ERROR(Log::instance()->getLogger(), "X509_new failed");
            return false;
        }

        X509_set_version(x509.get(), 2);
        ASN1_INTEGER_set(X509_get_serialNumber(x509.get()), 1);
        X509_gmtime_adj(X509_getm_notBefore(x509.get()), 0);
        X509_gmtime_adj(X509_getm_notAfter(x509.get()), 60L * 60L * 24L * 365);

        X509_NAME *name = X509_get_subject_name(x509.get());
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char *>("kDrive-localhost"), -1, -1,
                                   0);
        X509_set_issuer_name(x509.get(), name);

        if (!X509_set_pubkey(x509.get(), pkey.get()) || !X509_sign(x509.get(), pkey.get(), EVP_sha256())) {
            LOG_ERROR(Log::instance()->getLogger(), "Certificate signing failed");
            return false;
        }

        Poco::Crypto::X509Certificate cert(x509.release()); // takes ownership
        std::ostringstream certOut;
        cert.save(certOut);

        std::ostringstream keyOut;
        key.save(nullptr, &keyOut, ""); // public stream null, private to keyOut, no passphrase

        pem.cert = certOut.str();
        pem.key = keyOut.str();
        if (pem.cert.empty() || pem.key.empty()) return false;
    } catch (const Poco::Exception &e) {
        LOG_ERROR(Log::instance()->getLogger(), "Certificate generation failed: " << e.displayText());
        return false;
    }

    return true;
}

} // namespace KDC
