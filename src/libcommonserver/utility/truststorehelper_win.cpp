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

#include <openssl/x509.h>
#include <openssl/pem.h>

#include <Windows.h>
#include <wincrypt.h>

#include <vector>

namespace KDC {

namespace {

// Extract DER-encoded certificates from the Windows "ROOT" system certificate store.
std::vector<std::vector<unsigned char>> extractSystemRootCAs() {
    std::vector<std::vector<unsigned char>> certs;

    HCERTSTORE hStore = CertOpenSystemStore(0, L"ROOT");
    if (!hStore) {
        LOG_WARN(Log::instance()->getLogger(), "CertOpenSystemStore(ROOT) failed (error=" << GetLastError() << ")");
        return {};
    }

    PCCERT_CONTEXT pCertCtx = nullptr;
    while ((pCertCtx = CertEnumCertificatesInStore(hStore, pCertCtx)) != nullptr) {
        if (pCertCtx->pbCertEncoded && pCertCtx->cbCertEncoded > 0) {
            (void) certs.emplace_back(pCertCtx->pbCertEncoded, pCertCtx->pbCertEncoded + pCertCtx->cbCertEncoded);
        }
    }

    (void) CertCloseStore(hStore, 0);
    return certs;
}

} // namespace

bool TrustStoreHelper::loadSystemCAs(SSL_CTX *ctx) {
    if (!ctx) {
        LOG_WARN(Log::instance()->getLogger(), "SSL_CTX is null");
        return false;
    }

    auto derCerts = extractSystemRootCAs();
    if (derCerts.empty()) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Failed to extract system root CAs from Windows certificate store, falling back to OpenSSL default paths");
        sentry::Handler::captureMessage(sentry::Level::Error, "TrustStoreHelper::loadSystemCAs",
                                        "Failed to extract root CAs from Windows certificate store "
                                        "(OpenSSL default paths fallback is unreliable on this platform)");
        // This line is only useful so we have a default path for the context
        // We still return false because we failed to load any CA and verification would very likely fail
        SSL_CTX_set_default_verify_paths(ctx);
        return false;
    }

    X509_STORE *store = SSL_CTX_get_cert_store(ctx);
    if (!store) {
        store = X509_STORE_new();
        if (!store) {
            LOG_WARN(Log::instance()->getLogger(), "Failed to create X509_STORE");
            return false;
        }
        SSL_CTX_set_cert_store(ctx, store);
    }

    int addedCount = 0;
    int failedCount = 0;
    for (const auto &der: derCerts) {
        const unsigned char *data = der.data();
        X509 *cert = d2i_X509(nullptr, &data, static_cast<long>(der.size()));
        if (!cert) {
            failedCount++;
            continue;
        }
        if (X509_STORE_add_cert(store, cert) != 1) {
            failedCount++;
            LOG_DEBUG(Log::instance()->getLogger(), "Skipped a certificate that X509_STORE_add_cert rejected");
        } else {
            addedCount++;
        }
        X509_free(cert);
    }

    if (addedCount == 0) {
        LOG_ERROR(Log::instance()->getLogger(), "Failed to add any CA certificate from Windows certificate store ("
                                                        << derCerts.size() << " extracted, " << failedCount << " failed)");
        sentry::Handler::captureMessage(sentry::Level::Error, "TrustStoreHelper::loadSystemCAs",
                                        "Failed to add any CA certificate from Windows certificate store (" +
                                                std::to_string(derCerts.size()) + " extracted, 0 added)");
        return false;
    }

    if (failedCount > 0) {
        LOG_WARN(Log::instance()->getLogger(), "Loaded CA certificates from Windows certificate store with partial failures ("
                                                       << addedCount << " added, " << failedCount << " failed of "
                                                       << derCerts.size() << ")");
        sentry::Handler::captureMessage(sentry::Level::Warning, "TrustStoreHelper::loadSystemCAs",
                                        "Partial CA load from Windows certificate store (" + std::to_string(addedCount) +
                                                " added, " + std::to_string(failedCount) + " failed of " +
                                                std::to_string(derCerts.size()) + ")");
    } else {
        LOG_INFO(Log::instance()->getLogger(), "Loaded " << addedCount << " CA certificates from Windows certificate store");
    }

    return true;
}

} // namespace KDC
