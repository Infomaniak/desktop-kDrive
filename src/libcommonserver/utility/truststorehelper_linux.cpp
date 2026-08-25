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

#include <array>
#include <cstdlib>
#include <cstdint>
#include <unistd.h>

namespace KDC {

namespace {

// Returns the number of certificate objects in the ctx's trust store.
// Useful to confirm that load_verify_locations() actually loaded something,
// since it can return 1 on an empty or malformed bundle file.
int64_t certCount(const SSL_CTX *ctx) {
    const X509_STORE *store = SSL_CTX_get_cert_store(ctx);
    if (!store) {
        return 0;
    }
    const STACK_OF(X509_OBJECT) *objs = X509_STORE_get0_objects(store);
    return objs ? sk_X509_OBJECT_num(objs) : 0;
}

// Try to load a CA bundle file and verify at least one cert was parsed.
// Returns true on success, false otherwise (cleans any partial load).
bool tryLoadBundleFile(SSL_CTX *ctx, const char *path) {
    if (!path || path[0] == '\0' || access(path, R_OK) != 0) {
        return false;
    }
    if (SSL_CTX_load_verify_locations(ctx, path, nullptr) == 1 && certCount(ctx) > 0) {
        return true;
    }
    return false;
}

// Try to load from a hashed cert directory. Unlike file loads, certs in a
// CA path directory are loaded lazily at handshake time, so certCount() is
// always 0 here — we can only trust the return value of load_verify_locations.
bool tryLoadBundleDir(SSL_CTX *ctx, const char *path) {
    if (!path || path[0] == '\0' || access(path, R_OK) != 0) {
        return false;
    }
    return SSL_CTX_load_verify_locations(ctx, nullptr, path) == 1;
}

// Well-known distro CA bundle paths (Debian/Ubuntu/Arch, RHEL/Fedora, SUSE, Alpine/musl).
constexpr std::array knownCaBundles = {
        "/etc/ssl/certs/ca-certificates.crt",
        "/etc/pki/tls/certs/ca-bundle.crt",
        "/etc/ssl/ca-bundle.pem",
        "/etc/ssl/cert.pem",
};

// Well-known hashed CApath directories (loaded lazily; cannot be cert-count verified).
constexpr std::array knownCaDirs = {
        "/etc/ssl/certs",
        "/etc/pki/tls/certs",
};

} // namespace

bool TrustStoreHelper::loadSystemCAs(SSL_CTX *ctx) {
    if (!ctx) {
        LOG_WARN(Log::instance()->getLogger(), "SSL_CTX is null");
        return false;
    }

    // Primary: SSL_CERT_FILE / SSL_CERT_DIR env vars (if set), then compiled-in
    // OPENSSLDIR defaults ("/etc/ssl" on the Conan Center package).
    // X509_get_default_cert_file() returns only the compiled-in path; it does
    // NOT resolve env vars (unlike set_default_verify_paths which does). So we
    // check the env vars ourselves using the standard OpenSSL env var names.

    // 1a. SSL_CERT_FILE env var override (highest priority)
    if (const char *envFile = getenv(X509_get_default_cert_file_env())) {
        if (tryLoadBundleFile(ctx, envFile)) {
            LOG_DEBUG(Log::instance()->getLogger(), "Loaded system CA certificates from SSL_CERT_FILE=" << envFile);
            return true;
        }
        LOG_WARN(Log::instance()->getLogger(),
                 "SSL_CERT_FILE=" << envFile << " set but no usable certificates loaded, continuing");
    }

    // 1b. SSL_CERT_DIR env var override
    if (const char *envDir = getenv(X509_get_default_cert_dir_env())) {
        if (tryLoadBundleDir(ctx, envDir)) {
            LOG_DEBUG(Log::instance()->getLogger(), "Loaded system CA certificates from SSL_CERT_DIR=" << envDir);
            return true;
        }
        LOG_WARN(Log::instance()->getLogger(), "SSL_CERT_DIR=" << envDir << " set but no usable certificates loaded, continuing");
    }

    // 1c. Compiled-in OPENSSLDIR default file (works on Ubuntu/Debian/Arch)
    if (const char *defaultFile = X509_get_default_cert_file()) {
        if (tryLoadBundleFile(ctx, defaultFile)) {
            LOG_DEBUG(Log::instance()->getLogger(), "Loaded system CA certificates from default path " << defaultFile);
            return true;
        }
    }

    // 1d. Compiled-in OPENSSLDIR default directory (hashed symlink dir, loaded lazily)
    if (const char *defaultDir = X509_get_default_cert_dir()) {
        if (tryLoadBundleDir(ctx, defaultDir)) {
            LOG_DEBUG(Log::instance()->getLogger(), "Loaded system CA certificates from default directory " << defaultDir);
            return true;
        }
    }

    // 2. Fallback: probe well-known distro paths for systems where the compiled-in
    // OPENSSLDIR doesn't match reality (RHEL/Fedora, SUSE, musl/Alpine).
    for (const char *path: knownCaBundles) {
        if (tryLoadBundleFile(ctx, path)) {
            LOG_DEBUG(Log::instance()->getLogger(), "Loaded system CA certificates from " << path);
            return true;
        }
    }

    // 3. Last resort: probe well-known hashed CApath directories.
    for (const char *path: knownCaDirs) {
        if (tryLoadBundleDir(ctx, path)) {
            LOG_DEBUG(Log::instance()->getLogger(), "Loaded system CA certificates from directory " << path);
            return true;
        }
    }

    LOG_ERROR(Log::instance()->getLogger(), "Failed to load system CA certificates from any known path or default");
    sentry::Handler::captureMessage(sentry::Level::Error, "TrustStoreHelper::loadSystemCAs",
                                    "Failed to load system CA certificates on Linux (no usable CA bundle found via "
                                    "SSL_CERT_FILE/SSL_CERT_DIR env vars, OpenSSL default paths, or known distro paths)");
    return false;
}

} // namespace KDC
