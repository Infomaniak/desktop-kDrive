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

#include "truststorehelper.h"

#include "log/log.h"

#include "libcommon/log/sentry/handler.h"

#include <log4cplus/loggingmacros.h>

#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <unistd.h>

namespace KDC {

namespace {

// Returns the number of certificate objects in the ctx's trust store.
// Useful to confirm that load_verify_locations() actually loaded something,
// since it can return 1 on an empty or malformed bundle file.
long certCount(const SSL_CTX *ctx) {
    const X509_STORE *store = SSL_CTX_get_cert_store(ctx);
    if (!store) {
        return 0;
    }
    const STACK_OF(X509_OBJECT) *objs = X509_STORE_get0_objects(store);
    return objs ? sk_X509_OBJECT_num(objs) : 0;
}

// Well-known distro CA bundle paths (Debian/Ubuntu/Arch, RHEL/Fedora, SUSE, Alpine/musl).
constexpr const char *knownCaBundles[] = {
        "/etc/ssl/certs/ca-certificates.crt",
        "/etc/pki/tls/certs/ca-bundle.crt",
        "/etc/ssl/ca-bundle.pem",
        "/etc/ssl/cert.pem",
};

} // namespace

bool TrustStoreHelper::loadSystemCAs(SSL_CTX *ctx) {
    if (!ctx) {
        LOG_WARN(Log::instance()->getLogger(), "SSL_CTX is null");
        return false;
    }

    // Primary: compiled-in OpenSSL default paths.  X509_get_default_cert_file/dir()
    // resolve SSL_CERT_FILE / SSL_CERT_DIR env vars first, then fall back to
    // OPENSSLDIR (compiled as "/etc/ssl" on the Conan Center package).
    // We use load_verify_locations() instead of set_default_verify_paths() so
    // that certs are actually parsed immediately and we can verify the count.
    if (const char *defaultFile = X509_get_default_cert_file()) {
        if (access(defaultFile, R_OK) == 0 && SSL_CTX_load_verify_locations(ctx, defaultFile, nullptr) == 1 &&
            certCount(ctx) > 0) {
            LOG_DEBUG(Log::instance()->getLogger(), "Loaded system CA certificates from default path " << defaultFile);
            return true;
        }
    }
    if (const char *defaultDir = X509_get_default_cert_dir()) {
        if (access(defaultDir, R_OK) == 0 && SSL_CTX_load_verify_locations(ctx, nullptr, defaultDir) == 1 && certCount(ctx) > 0) {
            LOG_DEBUG(Log::instance()->getLogger(), "Loaded system CA certificates from default directory " << defaultDir);
            return true;
        }
    }

    // Fallback: probe well-known distro paths for systems where the compiled-in
    // OPENSSLDIR doesn't match reality (RHEL/Fedora, SUSE, musl/Alpine).
    for (const char *path: knownCaBundles) {
        if (access(path, R_OK) != 0) {
            continue;
        }

        if (SSL_CTX_load_verify_locations(ctx, path, nullptr) == 1 && certCount(ctx) > 0) {
            LOG_DEBUG(Log::instance()->getLogger(), "Loaded system CA certificates from " << path);
            return true;
        }

        LOG_WARN(Log::instance()->getLogger(),
                 "SSL_CTX_load_verify_locations produced no usable certificates for " << path << ", trying next candidate");
    }

    LOG_ERROR(Log::instance()->getLogger(), "Failed to load system CA certificates from any known path or default");
    sentry::Handler::captureMessage(sentry::Level::Error, "TrustStoreHelper::loadSystemCAs",
                                    "Failed to load system CA certificates on Linux (no usable CA bundle found via "
                                    "OpenSSL default paths or known distro paths)");
    return false;
}

} // namespace KDC
