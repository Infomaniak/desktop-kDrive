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

#pragma once

#include <list>
#include <string>

#include <stdint.h>

namespace KDC {

class ApiToken {
    public:
        ApiToken();
        ApiToken(const std::string &data);

        inline const std::string &rawData() const { return _rawData; }
        inline const std::string &accessToken() const { return _accessToken; }
        inline void setAccessToken(const std::string &newAccessToken) { _accessToken = newAccessToken; }
        inline const std::string &refreshToken() const { return _refreshToken; }
        inline void setRefreshToken(const std::string &newRefreshToken) { _refreshToken = newRefreshToken; }
        inline const std::string &tokenType() const { return _tokenType; }
        inline void setTokenType(const std::string &newTokenType) { _tokenType = newTokenType; }
        inline uint64_t expiresIn() const { return _expiresIn; }
        inline void setExpiresIn(uint64_t newExpiresIn) { _expiresIn = newExpiresIn; }
        inline int userId() const { return _userId; }
        inline void setUserId(int newUserId) { _userId = newUserId; }
        inline const std::string &scope() const { return _scope; }
        inline void setScope(const std::string &newScope) { _scope = newScope; }

        /**
         * Reconstruct a JSON string based on the current values of the attributes.
         * Especially useful in tests where we only inject the access token, without all the unnecessary information.
         * @return JSON string containing all token information.
         */
        std::string reconstructJsonString();

    private:
        std::string _rawData;

        std::string _accessToken;
        std::string _refreshToken;
        std::string _tokenType;
        uint64_t _expiresIn = 0;
        int _userId = 0;
        std::string _scope;
};

}  // namespace KDC
