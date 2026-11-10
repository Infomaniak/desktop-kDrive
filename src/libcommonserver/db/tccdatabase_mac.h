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

#include <cstdint>
#include <optional>
#include <string>

namespace KDC {

// Reads the system TCC database, which only processes granted the Full Disk Access authorization can open.
struct TccDatabase {
        /**
         * @brief Read the authorization value of `client` for `service`, and log the outcome with the file information of the
         * database.
         * @return The `auth_value` of the matching row, or nothing when the database cannot be read or has no such row.
         */
        static std::optional<int32_t> authValue(const std::string &service, const std::string &client, std::string &errorDescr);
};

} // namespace KDC
