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

#include "jobs/network/abstractnetworkjob.h"
#include "jobs/network/networkjobsparams.h"
#include "login/login.h"

#include <unordered_map>

namespace KDC {

class AbstractTokenNetworkJob : public AbstractNetworkJob {
        struct ExitHandler {
                ExitCause exitCause{ExitCause::Unknown};
                std::string debugMessage;
        };

    public:
        struct DbError : std::runtime_error {
                using std::runtime_error::runtime_error;
        };

        struct DataError : std::runtime_error {
                using std::runtime_error::runtime_error;
        };

        struct TokenError : std::runtime_error {
                using std::runtime_error::runtime_error;
        };

        enum class ApiType {
            Drive,
            DriveByUser,
            Profile,
            NotifyDrive,
            Desktop
        };

        /// @throw std::runtime_error
        /// @throw DbError
        /// @throw DataError
        /// @throw TokenError
        AbstractTokenNetworkJob(ApiType apiType, int userDbId, int userId, int driveDbId, int driveId, bool returnJson = true);
        explicit AbstractTokenNetworkJob(ApiType apiType, bool returnJson = true);
        ~AbstractTokenNetworkJob() override = default;

        ExitCause getExitCause() const;

        static void updateLoginByUserDbId(const Login &login, int userDbId);

        inline static void clearCacheForUser(int userDbId) { _userToApiKeyMap.erase(userDbId); }
        inline static void clearCacheForDrive(int driveDbId) { _driveToApiKeyMap.erase(driveDbId); }

        bool refreshToken();
        long tokenUpdateDurationFromNow();

        static ExitCode exception2ExitCode(const std::exception &e);

    protected:
        std::string getSpecificUrl() override;
        std::string getContentType(bool &canceled) override;

        bool handleResponse(std::istream &is) override;
        bool handleError(std::istream &is, const Poco::URI &uri) override;
        bool handleJsonResponse(std::istream &is) override;

        [[nodiscard]] int userId() const { return _userId; }
        [[nodiscard]] int driveId() const { return _driveId; }
        ApiType getApiType() const { return _apiType; }

    private:
        // User cache: <userDbId, <Login, userId>>
        static std::unordered_map<int, std::pair<std::shared_ptr<Login>, int>> _userToApiKeyMap;

        // Drive cache: <driveDbId, <userDbId, driveId>>
        static std::unordered_map<int, std::pair<int, int>> _driveToApiKeyMap;

        ApiType _apiType;
        int _userDbId;
        int _userId;
        int _driveDbId;
        int _driveId;
        bool _returnJson;
        std::string _token;

        bool _accessTokenAlreadyRefreshed = false;

        std::string loadToken();

        std::string getUrl() override;
        bool handleUnauthorizedResponse();
        bool defaultBackErrorHandling(NetworkErrorCode errorCode, const Poco::URI &uri);
};

} // namespace KDC
