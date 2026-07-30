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

#include <log4cplus/loggingmacros.h>

#include <openssl/x509.h>
#include <openssl/pem.h>

#include <Security/Security.h>
#include <Security/SecCertificate.h>
#include <Security/SecTrust.h>
#include <CoreFoundation/CoreFoundation.h>

#include <vector>

namespace KDC {

namespace {

// Extract DER-encoded certificates from the macOS system root trust store
// using the public SecTrustCopyAnchorCertificates API.
std::vector<std::vector<unsigned char>> extractSystemRootCAs() {
    std::vector<std::vector<unsigned char>> certs;

    CFArrayRef anchors = nullptr;
    OSStatus status = SecTrustCopyAnchorCertificates(&anchors);
    if (status != errSecSuccess || !anchors) {
        LOG_WARN(Log::instance()->getLogger(), "SecTrustCopyAnchorCertificates failed (OSStatus=" << status << ")");
        return {};
    }

    CFIndex count = CFArrayGetCount(anchors);
    for (CFIndex i = 0; i < count; i++) {
        SecCertificateRef certRef = static_cast<SecCertificateRef>(CFArrayGetValueAtIndex(anchors, i));
        if (!certRef) continue;

        CFDataRef certData = SecCertificateCopyData(certRef);
        if (!certData) continue;

        const UInt8 *dataPtr = CFDataGetBytePtr(certData);
        CFIndex dataLen = CFDataGetLength(certData);
        if (dataPtr && dataLen > 0) {
            certs.emplace_back(dataPtr, dataPtr + dataLen);
        }
        CFRelease(certData);
    }

    CFRelease(anchors);
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
                 "Failed to extract system root CAs from keychain, falling back to OpenSSL default paths");
        return SSL_CTX_set_default_verify_paths(ctx) == 1;
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

    for (const auto &der: derCerts) {
        const unsigned char *data = der.data();
        X509 *cert = d2i_X509(nullptr, &data, static_cast<long>(der.size()));
        if (cert) {
            if (X509_STORE_add_cert(store, cert) != 1) {
                LOG_DEBUG(Log::instance()->getLogger(), "Skipped a certificate that X509_STORE_add_cert rejected");
            }
            X509_free(cert);
        }
    }

    LOG_INFO(Log::instance()->getLogger(), "Loaded CA certificates from macOS keychain");
    return true;
}

} // namespace KDC
