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

#include "libcommonserver/utility/selfsignedcert.h"

#include <Poco/Net/Context.h>
#include <Poco/Crypto/X509Certificate.h>
#include <Poco/Crypto/RSAKey.h>
#include <Poco/Exception.h>

#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/rsa.h>

#include <sstream>

namespace KDC {

namespace {

bool applyPem(SSL_CTX *sslCtx, const SelfSignedCert::Pem &pem) {
    std::istringstream certStream(pem.cert);
    std::istringstream keyStream(pem.key);

    Poco::Crypto::X509Certificate cert(certStream);
    Poco::Crypto::RSAKey key(nullptr, &keyStream, "");

    return SSL_CTX_use_certificate(sslCtx, const_cast<X509 *>(cert.certificate())) == 1 &&
           SSL_CTX_use_RSAPrivateKey(sslCtx, const_cast<RSA *>(key.impl()->getRSA())) == 1 &&
           SSL_CTX_check_private_key(sslCtx) == 1;
}

} // namespace

Poco::Net::Context::Ptr SecureContextSingleton::instance() {
    static Poco::Net::Context::Ptr ctx = createContext();
    return ctx;
}

Poco::Net::Context::Ptr SecureContextSingleton::createContext() {
    SelfSignedCert::Pem pem;
    if (!SelfSignedCert::loadOrGenerate(pem)) {
        throw Poco::RuntimeException("Unable to obtain TLS material for local IPC");
    }

    Poco::Net::Context::Ptr ctx(
            new Poco::Net::Context(Poco::Net::Context::TLS_SERVER_USE, "", "", "", Poco::Net::Context::VERIFY_NONE));
    ctx->requireMinimumProtocol(Poco::Net::Context::PROTO_TLSV1_2);

    if (!applyPem(ctx->sslContext(), pem)) {
        throw Poco::RuntimeException("Failed to load in-memory TLS certificate/key");
    }
    return ctx;
}

} // namespace KDC
