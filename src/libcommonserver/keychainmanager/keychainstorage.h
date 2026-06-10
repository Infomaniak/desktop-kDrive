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

#include "libcommon.h"

#include <string>

namespace KDC {

class COMMON_EXPORT IKeyChainStorage {
    public:
        virtual ~IKeyChainStorage() = default;

        virtual bool writePassword(const std::string &keychainKey, const std::string &rawData) = 0;
        virtual bool readPassword(const std::string &keychainKey, std::string &data, bool &found) = 0;
        virtual bool deletePassword(const std::string &keychainKey) = 0;
};

class COMMON_EXPORT KeyChainStorage : public IKeyChainStorage {
    public:
        bool writePassword(const std::string &keychainKey, const std::string &rawData) override;
        bool readPassword(const std::string &keychainKey, std::string &data, bool &found) override;
        bool deletePassword(const std::string &keychainKey) override;
};

} // namespace KDC
