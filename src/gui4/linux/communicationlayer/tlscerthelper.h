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
#include <QSslKey>
#include <string>

namespace KDC {

/**
 * Read-only accessor for TLS certificates and keys stored in the OS keychain.
 * The server generates and publishes all TLS material; the client only reads.
 * Uses the same package/service as KeyChainStorage; the keychainKey maps to the
 * keychain "user" slot.
 */
class TLSCertHelper {
    public:
        TLSCertHelper() = delete;

        /**
         * Reads and parses the server certificate from the keychain.
         * @param certificate The output QSslCertificate to hold the parsed certificate.
         * @return true if the certificate was successfully read and parsed, false otherwise.
         */
        static bool readCertificate(QSslCertificate &certificate);

        /**
         * Reads and parses the client certificate from the keychain.
         * @param certificate The output QSslCertificate to hold the parsed client certificate.
         * @return true if the certificate was successfully read and parsed, false otherwise.
         */
        static bool readClientCertificate(QSslCertificate &certificate);

        /**
         * Reads and parses the client private key from the keychain.
         * @param key The output QSslKey to hold the parsed client private key.
         * @return true if the key was successfully read and parsed, false otherwise.
         */
        static bool readClientKey(QSslKey &key);

    private:
        static bool readPemFromKeychain(const std::string &keychainKey, std::string &outPem);
};

} // namespace KDC
