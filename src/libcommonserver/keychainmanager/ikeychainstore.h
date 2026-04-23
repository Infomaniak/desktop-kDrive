/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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

#include <string>
#include <memory>

namespace KDC {

/**
 * @brief Interface for keychain storage operations.
 * Allows dependency injection for testing purposes.
 */
class IKeychainStore {
    public:
        virtual ~IKeychainStore() = default;

        /**
         * @brief Stores a password in the keychain.
         * @param package The package identifier
         * @param service The service identifier
         * @param key The key to store
         * @param password The password to store
         * @param error Output error message if operation fails
         * @return true on success, false on failure
         */
        virtual bool setPassword(const std::string &package, const std::string &service, const std::string &key,
                                 const std::string &password, std::string &error) = 0;

        /**
         * @brief Retrieves a password from the keychain.
         * @param package The package identifier
         * @param service The service identifier
         * @param key The key to retrieve
         * @param password Output parameter for the retrieved password
         * @param found Output parameter indicating if the key was found
         * @param error Output error message if operation fails
         * @return true on success (even if not found), false on error
         */
        virtual bool getPassword(const std::string &package, const std::string &service, const std::string &key,
                                 std::string &password, bool &found, std::string &error) = 0;

        /**
         * @brief Deletes a password from the keychain.
         * @param package The package identifier
         * @param service The service identifier
         * @param key The key to delete
         * @param error Output error message if operation fails
         * @return true on success, false on failure
         */
        virtual bool deletePassword(const std::string &package, const std::string &service, const std::string &key,
                                    std::string &error) = 0;
};

} // namespace KDC
