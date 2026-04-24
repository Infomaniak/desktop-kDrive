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

#include "getdriveslistjob.h"

#include "utility/jsonparserutility.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

const std::string driveKey = "drive";

GetDrivesListJob::GetDrivesListJob(const UserDbId userDbId) :
    AbstractTokenNetworkJob(ApiType::DriveByUser, userDbId, 0, 0) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
    _apiVersion = 2;
}

void GetDrivesListJob::setQueryParameters(Poco::URI &uri) {
    uri.addQueryParameter("roles[]", "admin");
    uri.addQueryParameter("roles[]", "user");
    uri.addQueryParameter("with", "drive,drive.account");
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
        const auto dataObj = it->extract<Poco::JSON::Object::Ptr>();

        DriveId driveId = -1;
        if (!JsonParserUtility::extractValue(dataObj, driveIdKey, driveId)) {
            return {ExitCode::BackError, ExitCause::MissingReplyData};
        }

        UserId userId = -1;
        if (!JsonParserUtility::extractValue(dataObj, idKey, userId)) {
            return {ExitCode::BackError, ExitCause::MissingReplyData};
        }

        AccountId accountId = -1;
        std::string accountName;
        const auto driveObj = dataObj->getObject(driveKey);
        if (!driveObj) {
            LOG_ERROR(Log::instance()->getLogger(), "Missing drive info!");
            return {ExitCode::BackError, ExitCause::MissingReplyData};
        }

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
        if (!JsonParserUtility::extractValue(dataObj, driveNameKey, driveName)) {
            return {ExitCode::BackError, ExitCause::MissingReplyData};
        }

        std::string colorHex;
        if (Poco::JSON::Object::Ptr prefObj = dataObj->getObject(preferenceKey)) {
            if (!JsonParserUtility::extractValue(prefObj, colorKey, colorHex, false)) {
                return {ExitCode::BackError, ExitCause::MissingReplyData};
            }
        }
        DriveAvailableInfo driveInfo(driveId, userId, accountId, QString::fromStdString(accountName),
                                     QString::fromStdString(driveName), QString::fromStdString(colorHex));
        _availableDrives.push_back(driveInfo);
    }

    return ExitCode::Ok;
}

} // namespace KDC
