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

#include <openssl/ssl.h>

namespace KDC {

bool TrustStoreHelper::loadSystemCAs(SSL_CTX *ctx) {
    if (!ctx) {
        LOG_WARN(Log::instance()->getLogger(), "SSL_CTX is null");
        return false;
    }

    // The Conan Center OpenSSL on Linux is compiled with OPENSSLDIR="/etc/ssl",
    // so SSL_CTX_set_default_verify_paths() already knows where to find the system CA
    // bundle (/etc/ssl/certs/ and /etc/ssl/cert.pem). This is equivalent to
    // loadDefaultCAs=true in the Poco Context constructor.
    if (SSL_CTX_set_default_verify_paths(ctx) != 1) {
        LOG_WARN(Log::instance()->getLogger(), "Failed to load system default CA paths");
        return false;
    }

    LOG_DEBUG(Log::instance()->getLogger(), "Loaded system CA certificates via OpenSSL default verify paths");
    return true;
}

} // namespace KDC
