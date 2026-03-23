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

#include "getdriveuserinfojob.h"

#include "libcommonserver/utility/jsonparserutility.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

GetDriveUserInfoJob::GetDriveUserInfoJob(const int32_t userDbId, const int32_t driveId, const int32_t userId) :
    AbstractTokenNetworkJob(ApiType::Drive, userDbId, 0, 0, driveId),
    _targetUserId(userId) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
    _apiVersion = 3;
}

GetDriveUserInfoJob::GetDriveUserInfoJob(const int32_t driveDbId, const int32_t userId) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, 0, driveDbId, 0),
    _targetUserId(userId) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
    _apiVersion = 3;
}

ExitInfo GetDriveUserInfoJob::handleJsonResponse(const std::string &replyBody) {
    if (const auto exitInfo = AbstractTokenNetworkJob::handleJsonResponse(replyBody); !exitInfo) return exitInfo;

    const auto dataArray = jsonRes()->getArray(dataKey);
    if (!dataArray || dataArray->empty()) {
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    }

    const auto dataObj = dataArray->getObject(0);
    if (!dataObj) {
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    }

    if (!JsonParserUtility::extractValue(dataObj, displayNameKey, _name)) {
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    }

    if (!JsonParserUtility::extractValue(dataObj, emailKey, _email)) {
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    }

    if (!JsonParserUtility::extractValue(dataObj, avatarKey, _avatarUrl)) {
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    }

    return ExitCode::Ok;
}

std::string GetDriveUserInfoJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/users";
    return str;
}

void GetDriveUserInfoJob::setQueryParameters(Poco::URI &uri) {
    uri.addQueryParameter("user_ids[]", std::to_string(_targetUserId));
}

} // namespace KDC
