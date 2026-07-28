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

#include <string>

namespace KDC {

class COMMONSERVER_EXPORT SelfSignedCert {
    public:
        struct Pem {
                std::string cert;
                std::string key;
        };

        /// Read the certificate/key pair from the keychain, generating and storing
        /// a new one if absent.
        /// @return true on success, false otherwise.
        static bool loadOrGenerate(Pem &pem);

    private:
        static constexpr char keyKeychainKey[] = "kdrive_ipc_tls_key";

        static bool generate(Pem &pem);
};

} // namespace KDC
