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

#include "ikeychainstore.h"

namespace KDC {

/**
 * @brief Real keychain storage implementation using the keychain library.
 */
class KeychainStore : public IKeychainStore {
    public:
        KeychainStore() = default;
        ~KeychainStore() override = default;

        bool setPassword(const std::string &package, const std::string &service, const std::string &key,
                         const std::string &password, std::string &error) override;

        bool getPassword(const std::string &package, const std::string &service, const std::string &key, std::string &password,
                         bool &found, std::string &error) override;

        bool deletePassword(const std::string &package, const std::string &service, const std::string &key,
                            std::string &error) override;
};

} // namespace KDC
