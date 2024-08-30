/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#include <string>

namespace KDC {

struct SentryUser {
    public:
        SentryUser() = default;
        SentryUser(const std::string_view &email, const std::string_view &username, const std::string_view &userId)
            : email(email), username(username), userId(userId) {}

        inline std::string_view getEmail() const { return email; };
        inline std::string_view getUsername() const { return username; };
        inline std::string_view getUserId() const { return userId; };

    private:
        std::string email = "Not set";
        std::string username = "Not set";
        std::string userId = "Not set";
};
}  // namespace KDC