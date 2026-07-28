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


#include <QSslCertificate>
#include <string>

namespace KDC {

/**
 * Read-only accessor for the public TLS certificate stored in the OS keychain.
 * Uses the same package/service as KeyChainStorage; the keychainKey maps to the
 * keychain "user" slot.
 */
class CertReader {
    public:
        explicit CertReader(std::string keychainKey);

        /**
         * Reads the PEM string from the keychain.
         * @param found set to false if the entry does not exist (not an error).
         * @return the PEM string, or empty on absence/failure.
         */
        std::string readPem(bool &found) const;

        /** Reads and parses into a QSslCertificate. Null cert on absence/failure. */
        QSslCertificate readCertificate(bool &found) const;

    private:
        std::string _keychainKey;
};

} // namespace KDC
