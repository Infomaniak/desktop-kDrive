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

#include "abstractnetworkjob.h"
#include "jobs/network/networkjobsparams.h"
#include "login/login.h"

#include <unordered_map>

namespace KDC {

class AbstractTokenNetworkJob : public AbstractNetworkJob {
        struct ExitHandler {
                ExitCause exitCause{ExitCauseUnknown};
                std::string debugMessage;
        };

    public:
        typedef enum { ApiDrive, ApiDriveByUser, ApiProfile, ApiNotifyDrive } ApiType;

        AbstractTokenNetworkJob(ApiType apiType, int userDbId, int userId, int driveDbId, int driveId, bool returnJson = true);
        AbstractTokenNetworkJob(ApiType apiType, bool returnJson = true);
        virtual ~AbstractTokenNetworkJob() override = default;

        bool hasErrorApi(std::string *errorCode = nullptr, std::string *errorDescr = nullptr);
        ExitCause getExitCause();
        inline std::string octetStreamRes() { return _octetStreamRes; }
        inline Poco::JSON::Object::Ptr jsonRes() { return _jsonRes; }

        static void updateLoginByUserDbId(const Login &login, int userDbId);

        inline static void clearCacheForUser(int userDbId) { _userToApiKeyMap.erase(userDbId); }
        inline static void clearCacheForDrive(int driveDbId) { _driveToApiKeyMap.erase(driveDbId); }

        bool refreshToken();
        long tokenUpdateDurationFromNow();

    protected:
        virtual std::string getSpecificUrl() override;
        virtual std::string getContentType(bool &canceled) override;

        virtual bool handleResponse(std::istream &is) override;
        virtual bool handleError(std::istream &is, const Poco::URI &uri) override;
        virtual bool handleJsonResponse(std::istream &is);
        virtual bool handleJsonResponse(std::string &str);
        virtual bool handleOctetStreamResponse(std::istream &is);


        int userId() const { return _userId; }
        int driveId() const { return _driveId; }

        Poco::JSON::Object::Ptr _jsonRes{nullptr};
        std::string _octetStreamRes;

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
        std::string _errorCode;
        std::string _errorDescr;
        Poco::JSON::Object::Ptr _error{nullptr};

        std::string loadToken();

        virtual std::string getUrl() override;
        bool handleUnauthorizedResponse();
        bool handleNotFoundResponse();
        bool defaultBackErrorHandling(NetworkErrorCode errorCode, const Poco::URI &uri);
};

}  // namespace KDC
