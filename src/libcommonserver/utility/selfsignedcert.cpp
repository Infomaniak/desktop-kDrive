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

#include "libcommon/utility/utility.h"
#include "libcommonserver/log/log.h"

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/rsa.h>
#include <openssl/err.h>

#include <log4cplus/loggingmacros.h>

namespace KDC {

namespace {

struct EvpPkeyDeleter {
        void operator()(EVP_PKEY *p) const { EVP_PKEY_free(p); }
};
using UniqueEvpPkey = std::unique_ptr<EVP_PKEY, EvpPkeyDeleter>;

struct X509Deleter {
        void operator()(X509 *x) const { X509_free(x); }
};
using UniqueX509 = std::unique_ptr<X509, X509Deleter>;

void restrictPermissions(const SyncPath &path) {
    std::error_code ec;
    std::filesystem::permissions(path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                                 std::filesystem::perm_options::replace, ec);
    if (ec) {
        LOG_WARN(Log::instance()->getLogger(), "Failed to restrict permissions for " << path.string() << ": " << ec.message());
    }
}

} // namespace

SyncPath SelfSignedCert::certPath() {
    return CommonUtility::getAppSupportDir() / certFileName;
}

SyncPath SelfSignedCert::keyPath() {
    return CommonUtility::getAppSupportDir() / keyFileName;
}

bool SelfSignedCert::generateIfNeeded() {
    const auto cPath = certPath();
    const auto kPath = keyPath();

    if (std::filesystem::exists(cPath) && std::filesystem::exists(kPath)) {
        return true;
    }

    return generate(cPath, kPath);
}

bool SelfSignedCert::generate(const SyncPath &certPath, const SyncPath &keyPath) {
    LOG_INFO(Log::instance()->getLogger(), "Generating self-signed certificate/key pair for local TLS IPC");

    UniqueEvpPkey pkey(EVP_RSA_gen(2048));
    if (!pkey) {
        LOG_ERROR(Log::instance()->getLogger(), "EVP_RSA_gen failed");
        return false;
    }

    UniqueX509 x509(X509_new());
    if (!x509) {
        LOG_ERROR(Log::instance()->getLogger(), "X509_new failed");
        return false;
    }

    if (!X509_set_version(x509.get(), 2)) {
        LOG_ERROR(Log::instance()->getLogger(), "X509_set_version failed");
        return false;
    }

    ASN1_INTEGER_set(X509_get_serialNumber(x509.get()), 1);

    if (!X509_gmtime_adj(X509_get_notBefore(x509.get()), 0)) {
        LOG_ERROR(Log::instance()->getLogger(), "X509_gmtime_adj (notBefore) failed");
        return false;
    }
    if (!X509_gmtime_adj(X509_get_notAfter(x509.get()), 60L * 60L * 24L * 365)) {
        LOG_ERROR(Log::instance()->getLogger(), "X509_gmtime_adj (notAfter) failed");
        return false;
    }

    X509_NAME *name = X509_get_subject_name(x509.get());
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char *>("kDrive-localhost"), -1, -1, 0);
    X509_set_issuer_name(x509.get(), name);

    if (!X509_set_pubkey(x509.get(), pkey.get())) {
        LOG_ERROR(Log::instance()->getLogger(), "X509_set_pubkey failed");
        return false;
    }

    if (!X509_sign(x509.get(), pkey.get(), EVP_sha256())) {
        LOG_ERROR(Log::instance()->getLogger(), "X509_sign failed");
        return false;
    }

    // Write certificate PEM
    {
        FILE *f = fopen(certPath.string().c_str(), "wb");
        if (!f) {
            LOG_ERROR(Log::instance()->getLogger(), "Failed to open cert file for writing: " << certPath.string());
            return false;
        }
        PEM_write_X509(f, x509.get());
        fclose(f);
        restrictPermissions(certPath);
    }

    // Write private key PEM (unencrypted)
    {
        FILE *f = fopen(keyPath.string().c_str(), "wb");
        if (!f) {
            LOG_ERROR(Log::instance()->getLogger(), "Failed to open key file for writing: " << keyPath.string());
            return false;
        }
        PEM_write_PrivateKey(f, pkey.get(), nullptr, nullptr, 0, nullptr, nullptr);
        fclose(f);
        restrictPermissions(keyPath);
    }

    LOG_INFO(Log::instance()->getLogger(), "Self-signed certificate and key written to: " << certPath.string());
    return true;
}

} // namespace KDC
