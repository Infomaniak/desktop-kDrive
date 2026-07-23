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
#include "libcommon/utility/types.h"

#include <filesystem>

namespace KDC {

class COMMONSERVER_EXPORT SelfSignedCert {
    public:
        static constexpr char certFileName[] = "server_cert.pem";
        static constexpr char keyFileName[] = "server_key.pem";

        /// @return Path where the certificate PEM is (or will be) stored.
        static SyncPath certPath();

        /// @return Path where the private-key PEM is (or will be) stored.
        static SyncPath keyPath();

        /// Generate a new RSA self-signed certificate/key pair if none exists yet.
        /// If files already exist they are left untouched.
        /// @return true on success, false otherwise.
        static bool generateIfNeeded();

    private:
        static bool generate(const SyncPath &certPath, const SyncPath &keyPath);
};

} // namespace KDC
