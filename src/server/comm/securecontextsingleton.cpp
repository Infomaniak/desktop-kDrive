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

#include "securecontextsingleton.h"
#include "libcommonserver/utility/crypto.h"
#include "libcommonserver/utility/selfsignedcert.h"
#include "libcommonserver/log/log.h"

namespace KDC {

namespace {

bool applyPem(SSL_CTX *const sslCtx, const SelfSignedCert::Pem &pem) {
    UniqueBio certBio(BIO_new_mem_buf(pem.cert.data(), static_cast<int>(pem.cert.size())));
    UniqueBio keyBio(BIO_new_mem_buf(pem.key.data(), static_cast<int>(pem.key.size())));
    if (!certBio || !keyBio) {
        LOG_ERROR(Log::instance()->getLogger(), "Failed to create BIO: " << sslError());
        return false;
    }

    UniqueX509 cert(PEM_read_bio_X509(certBio.get(), nullptr, nullptr, nullptr));
    UniqueEvpPkey key(PEM_read_bio_PrivateKey(keyBio.get(), nullptr, nullptr, nullptr));
    if (!cert || !key) {
        LOG_ERROR(Log::instance()->getLogger(), "Failed to parse TLS material: " << sslError());
        return false;
    }

    if (SSL_CTX_use_certificate(sslCtx, cert.get()) != 1 || SSL_CTX_use_PrivateKey(sslCtx, key.get()) != 1 ||
        SSL_CTX_check_private_key(sslCtx) != 1) {
        LOG_ERROR(Log::instance()->getLogger(), "Failed to apply TLS material: " << sslError());
        return false;
    }

    return true;
}

} // namespace

Poco::Net::Context::Ptr SecureContextSingleton::instance() {
    static const Poco::Net::Context::Ptr ctx = createContext();
    return ctx;
}

Poco::Net::Context::Ptr SecureContextSingleton::createContext() {
    SelfSignedCert::Pem pem;
    if (!SelfSignedCert::generateAndPublishServerCert(pem)) {
        throw Poco::RuntimeException("Unable to obtain TLS material for local IPC");
    }

    SelfSignedCert::Pem clientPem;
    if (!SelfSignedCert::generateAndPublishClientCert(clientPem)) {
        throw Poco::RuntimeException("Unable to obtain TLS client material for local IPC");
    }

    Poco::Net::Context::Ptr ctx(
            new Poco::Net::Context(Poco::Net::Context::TLS_SERVER_USE, "", "", "", Poco::Net::Context::VERIFY_STRICT));
    ctx->requireMinimumProtocol(Poco::Net::Context::PROTO_TLSV1_2);

    if (!applyPem(ctx->sslContext(), pem)) {
        throw Poco::RuntimeException("Failed to load in-memory TLS certificate/key");
    }

    // Trust the client certificate so the server can verify the GUI's identity during the TLS handshake.
    try {
        std::istringstream certStream(clientPem.cert);
        const Poco::Crypto::X509Certificate clientCert(certStream);
        ctx->addCertificateAuthority(clientCert);
    } catch (const Poco::Exception &e) {
        LOG_ERROR(Log::instance()->getLogger(), "Failed to add client certificate as CA: " << e.displayText());
        throw Poco::RuntimeException("Failed to load client certificate as trusted CA");
    }

    return ctx;
}

} // namespace KDC
