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

#include "getdriveslistjob.h"

#include "utility/jsonparserutility.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

GetDrivesListJob::GetDrivesListJob(int userDbId) :
    AbstractTokenNetworkJob(ApiType::DriveByUser, userDbId, 0, 0, 0) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
}

void GetDrivesListJob::setQueryParameters(Poco::URI &uri) {
    uri.addQueryParameter("roles[]", "admin");
    uri.addQueryParameter("roles[]", "user");
    uri.addQueryParameter("with", "account");
}

std::string GetDrivesListJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/users/current/drives";
    return str;
}

ExitInfo GetDrivesListJob::handleJsonResponse(const std::string &replyBody) {
    if (const auto exitInfo = AbstractTokenNetworkJob::handleJsonResponse(replyBody); !exitInfo) return exitInfo;

    const auto dataArray = jsonRes()->getArray(dataKey);
    if (!dataArray) {
        LOG_ERROR(Log::instance()->getLogger(), "Unable to read available drives info");
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    }
    if (dataArray->empty()) {
        LOG_INFO(Log::instance()->getLogger(), "No available drives!");
        return ExitCode::Ok;
    }

    for (auto it = dataArray->begin(); it != dataArray->end(); ++it) {
        const auto driveObj = it->extract<Poco::JSON::Object::Ptr>();

        int driveId = -1;
        if (!JsonParserUtility::extractValue(driveObj, driveIdKey, driveId)) {
            return {ExitCode::BackError, ExitCause::MissingReplyData};
        }

        int userId = -1;
        if (!JsonParserUtility::extractValue(driveObj, idKey, userId)) {
            return {ExitCode::BackError, ExitCause::MissingReplyData};
        }

        int accountId = -1;
        std::string accountName;
        const auto accountObj = driveObj->getObject(accountKey);
        if (!accountObj) {
            LOG_ERROR(Log::instance()->getLogger(), "Missing account info!");
            return {ExitCode::BackError, ExitCause::MissingReplyData};
        }
        if (!JsonParserUtility::extractValue(accountObj, idKey, accountId)) {
            return {ExitCode::BackError, ExitCause::MissingReplyData};
        }
        if (!JsonParserUtility::extractValue(accountObj, nameKey, accountName)) {
            return {ExitCode::BackError, ExitCause::MissingReplyData};
        }

        std::string driveName;
        if (!JsonParserUtility::extractValue(driveObj, driveNameKey, driveName)) {
            return {ExitCode::BackError, ExitCause::MissingReplyData};
        }

        std::string colorHex;
        if (Poco::JSON::Object::Ptr prefObj = driveObj->getObject(preferenceKey)) {
            if (!JsonParserUtility::extractValue(prefObj, colorKey, colorHex, false)) {
                return {ExitCode::BackError, ExitCause::MissingReplyData};
            }
        }
        DriveAvailableInfo driveInfo(driveId, userId, accountId, accountName, driveName, colorHex);
        _availableDrives.push_back(driveInfo);
    }

    return ExitCode::Ok;
}


// // Search user in DB
// User user;
// bool found = false;
// if (!ParmsDb::instance()->selectUserByUserId(userId, user, found)) {
//     LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectUserByUserId");
//     return ExitCode::DbError;
// }
// if (found) {
//     driveInfo.setUserDbId(user.dbId());
// }
//
// list.insert(driveId, driveInfo);

} // namespace KDC
