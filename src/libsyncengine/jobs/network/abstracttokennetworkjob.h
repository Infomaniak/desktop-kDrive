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

#include "jobs/network/abstractnetworkjob.h"
#include "jobs/network/networkjobsparams.h"
#include "login/login.h"

#include "libparms/db/account.h"
#include "libparms/db/drive.h"

#include <unordered_map>

namespace KDC {

class AbstractTokenNetworkJob : public AbstractNetworkJob {
        struct ExitHandler {
                ExitCause exitCause{ExitCause::Unknown};
                std::string debugMessage;
        };

    public:
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
        /// @throw InvalidArgumentError
        AbstractTokenNetworkJob(ApiType apiType, UserDbId userDbId, UserId userId, DriveDbId driveDbId, DriveId driveId,
                                bool returnJson = true);
        explicit AbstractTokenNetworkJob(ApiType apiType, bool returnJson = true);
        ~AbstractTokenNetworkJob() override = default;

        ExitCause getExitCause() const;
        DriveDbId driveDbId() const { return _driveDbId; };

        static void updateLoginByUserDbId(const Login &login, UserDbId userDbId);

        static void clearCache();

        ExitInfo refreshToken();
        long tokenUpdateDurationFromNow();

    protected:
        std::string getSpecificUrl() override;
        std::string contentType() override;

        ExitInfo handleResponse(std::istream &is) override;
        ExitInfo handleError(const std::string &replyBody, const Poco::URI &uri) override;
        ExitInfo handleJsonResponse(
                const std::string &replyBody) override; // TODO : this method should be private and called for every job.

        [[nodiscard]] UserId userId() const { return _userId; }
        [[nodiscard]] DriveId driveId() const { return _driveId; }
        [[nodiscard]] ApiType getApiType() const { return _apiType; }

    private:
        struct LoginEntry {
                std::shared_ptr<Login> login;
                UserId userId{0};
        };
        using UserCache = std::unordered_map<UserDbId, LoginEntry>;
        static UserCache _userToApiKeyMap;
        static std::recursive_mutex _cacheMutex;
        struct UserEntry {
                UserDbId userDbId{0};
                DriveId driveId{0};
        };
        using DriveCache = std::unordered_map<DriveDbId, UserEntry>;
        static DriveCache _driveToApiKeyMap;

        ApiType _apiType{ApiType::Drive};
        UserDbId _userDbId{0};
        UserId _userId{0};
        DriveDbId _driveDbId{0};
        DriveId _driveId{0};
        bool _returnJson{true};
        ApiToken _apiToken;

        bool _accessTokenAlreadyRefreshed{false};

        virtual ApiToken loadApiToken();

        std::string getUrl() override;
        ExitInfo handleUnauthorizedResponse();
        void defaultBackErrorHandling(NetworkErrorCode errorCode, const Poco::URI &uri, ExitCause &exitCause);

        // Load user information, including the API token, based on the record associated `_driveDbId`, provided it does exist.
        void loadUserInfoFromDriveDbId();

        // Load user information, including the API token, based on the value of `_userDbId`, assuming it has been set.
        void loadUserInfoFromUserDbId();

        ApiToken retrieveApiTokenFromUserCache();
        Account getAccount(const Drive &drive) const;
        Drive getDrive(DriveDbId driveDbId) const;

        /// @throw InvalidArgumentError
        void checkParametersValidity();

        friend class TestServerRequests;
};

} // namespace KDC
