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

namespace KDC {

struct SentryUser {
    public:
        SentryUser() = default;
        SentryUser(const std::string_view &email, const std::string_view &username, const std::string_view &userId) :
            _email(email), _username(username), _userId(userId), defaultUser(false) {}
        explicit operator bool() const { return !defaultUser; }
        inline std::string_view email() const { return _email; };
        inline std::string_view username() const { return _username; };
        inline std::string_view userId() const { return _userId; };
        inline bool isDefault() const { return defaultUser; };

    private:
        std::string _email = "Not set";
        std::string _username = "Not set";
        std::string _userId = "Not set";
        bool defaultUser = true;
};
} // namespace KDC
