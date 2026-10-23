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

#pragma once

#include "libcommonserver/commonserverlib.h"

#include <openssl/ssl.h>

namespace KDC {

/// Rejects TLS chains containing a certificate whose key is known to be compromised.
///
/// The trust store helper only feeds root CAs to OpenSSL, so the blocks applied by the OS
/// (revocation, disallowed certificates) never reach our verification. This list fills the gap
/// for compromised intermediates that still chain to a trusted root.
struct COMMONSERVER_EXPORT CertificateBlocklist {
        /// Install the chain check on a client SSL_CTX. Poco's own verify callback is kept.
        static void install(SSL_CTX *ctx);

        /// Callback given to SSL_CTX_set_cert_verify_callback: runs the regular OpenSSL
        /// verification, then rejects the validated chain if one of its certificates is blocked.
        /// @return 1 if the chain is accepted, 0 otherwise.
        static int verifyChain(X509_STORE_CTX *storeCtx, void *arg);

        /// @return true if the SHA-256 of the certificate's SubjectPublicKeyInfo is blocked.
        static bool isBlocked(const X509 *cert);
};

} // namespace KDC
