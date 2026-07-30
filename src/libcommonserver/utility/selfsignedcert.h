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

        /// Generate a fresh certificate/key pair and publish the certificate to the keychain,
        /// so that the GUI can pin it. The private key never leaves this process.
        /// Must run before the port is published, otherwise a GUI starting in between would pin
        /// the previous certificate.
        /// @return true on success, false otherwise.
        static bool generateAndPublish(Pem &pem);

    private:
        static bool generate(Pem &pem);
};

} // namespace KDC
