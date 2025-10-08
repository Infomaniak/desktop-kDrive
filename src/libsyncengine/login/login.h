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

#include "libcommon/keychainmanager/apitoken.h"
#include "libcommon/utility/types.h"

#include <unordered_map>
#include <chrono>

#include <log4cplus/logger.h>

namespace KDC {

class Login {
    public:
        struct LoginInfo {
                LoginInfo &operator=(const LoginInfo &info);

                std::mutex _mutex;
                std::chrono::time_point<std::chrono::steady_clock> _lastTokenUpdateTime;
        };

        Login();
        Login(const std::string &keychainKey);
        virtual ~Login();

        /*
         * Retrieve the API tokens
         */
        ExitInfo requestToken(const std::string &authorizationCode, const std::string &codeVerifier = "");
        ExitInfo refreshToken();

        inline bool hasToken() const { return !_apiToken.accessToken().empty(); }
        long tokenUpdateDurationFromNow() const;

        inline const ApiToken &apiToken() const { return _apiToken; }
        inline void setApiToken(ApiToken &apiToken) { _apiToken = apiToken; }
        inline const std::string &keychainKey() const { return _keychainKey; }
        inline void setKeychainKey(std::string &keychain) { _keychainKey = keychain; }
        inline bool hasError() const { return !_error.empty(); }
        inline void setError(const std::string &error) { _error = error; }
        inline std::string error() const { return _error; }
        inline void setErrorDescr(const std::string &errorDescr) { _errorDescr = errorDescr; }
        inline std::string errorDescr() const { return _errorDescr; }

    private:
        log4cplus::Logger _logger;
        ApiToken _apiToken;
        std::string _keychainKey;
        std::string _error;
        std::string _errorDescr;

        static std::unordered_map<int, LoginInfo> _info;

        static ExitCode refreshToken(const std::string &keychainKey, ApiToken &apiToken, std::string &error,
                                     std::string &errorDescr);
};

} // namespace KDC
