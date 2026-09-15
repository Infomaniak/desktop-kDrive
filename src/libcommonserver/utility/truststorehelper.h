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

/// Loads the platform-native CA trust store into the OpenSSL SSL_CTX used by Poco.
///
/// On macOS and Windows the bundled OpenSSL build does not ship a CA bundle and
/// `loadDefaultCAs=true` only checks paths compiled into the OpenSSL binary
/// (e.g. `/usr/local/ssl/cert.pem` on macOS), which do not exist on end-user
/// machines.  This helper extracts the native trust store (macOS keychain /
/// Windows certificate store / Linux ca-certificates) and feeds it into the
/// SSL_CTX so that peer verification actually works.
struct COMMONSERVER_EXPORT TrustStoreHelper {
        /// Load platform-native root CAs into the given SSL_CTX.
        /// Returns true on success, false on error.
        static bool loadSystemCAs(SSL_CTX *ctx);
};

} // namespace KDC
