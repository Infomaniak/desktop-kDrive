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

#include "getappversionjob.h"

#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/jsonparserutility.h"
#include "utility/utility.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

static const std::string channelKey = "channel";
static const std::string tagKey = "tag";
static const std::string buildVersionKey = "build_version";
static const std::string downloadUrlKey = "download_link";
static const std::string buildMinOsVersionKey = "build_min_os_version";
static const std::string applicationMinVersionKey = "min_version";
static const std::string checksumKey = "checksum";

GetAppVersionJob::GetAppVersionJob(const DistributionChannel currentChannel, const std::string &appID) :
    GetAppVersionJob(currentChannel, appID, {}) {}

GetAppVersionJob::GetAppVersionJob(const DistributionChannel currentChannel, const std::string &appID,
                                   const std::vector<UserId> &userIdList) :
    AbstractTokenNetworkJob(userIdList.empty() ? ApiType::InternalUnauthenticated : ApiType::Internal),
    _currentChannel(currentChannel),
    _appId(appID),
    _userIdList(userIdList) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
    _apiVersion = 1;
}

std::string GetAppVersionJob::getSpecificUrl() {
    constexpr auto kStoreAuthenticatedEndpoint = "/app-information/applications/version";
    constexpr auto kStoreUnauthenticatedEndpoint = "/app-information/applications/version/no-auth";
    return getApiType() == ApiType::Internal ? kStoreAuthenticatedEndpoint : kStoreUnauthenticatedEndpoint;
}

void GetAppVersionJob::setQueryParameters(Poco::URI &uri) {
    uri.addQueryParameter("appId", _appId);
    uri.addQueryParameter("channel", toString(_currentChannel));
    uri.addQueryParameter("platform", toString(CommonUtility::platform()));
    uri.addQueryParameter("os_version", CommonUtility::osVersion());
    uri.addQueryParameter("store", "kStore");
    uri.addQueryParameter("name", "com.infomaniak.drive");
    for (const auto &id: _userIdList) {
        uri.addQueryParameter("user_ids[]", std::to_string(id));
    }
}

ExitInfo GetAppVersionJob::handleError(const std::string &, const Poco::URI &uri) {
    LOG_DEBUG(_logger, "Request failed: " << Utility::formatRequest(uri, _backError.code(), _backError.description()));
    return {ExitCode::BackError, ExitCause::HttpErr};
}

ExitInfo GetAppVersionJob::handleResponse(std::istream &is) {
    std::string replyBody;
    getStringFromStream(is, replyBody);
    if (const auto exitCode = AbstractNetworkJob::handleJsonResponse(replyBody); !exitCode) return exitCode;

    const Poco::JSON::Object::Ptr dataObj = JsonParserUtility::extractJsonObject(jsonRes(), dataKey);
    if (!dataObj) return {ExitCode::BackError, ExitCause::MissingReplyData};

    std::string channelStr;
    if (!JsonParserUtility::extractValue(dataObj, channelKey, channelStr))
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    _versionsInfo.channel = toDistributionChannel(channelStr);
    if (!JsonParserUtility::extractValue(dataObj, tagKey, _versionsInfo.tag))
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    if (!JsonParserUtility::extractValue(dataObj, buildVersionKey, _versionsInfo.buildVersion))
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    if (!JsonParserUtility::extractValue(dataObj, buildMinOsVersionKey, _versionsInfo.minOsVersion))
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    if (!JsonParserUtility::extractValue(dataObj, downloadUrlKey, _versionsInfo.downloadUrl))
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    if (!JsonParserUtility::extractValue(dataObj, checksumKey, _versionsInfo.checksum))
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    if (!JsonParserUtility::extractValue(dataObj, applicationMinVersionKey, _versionsInfo.minAppVersion))
        return {ExitCode::BackError, ExitCause::MissingReplyData};

    return ExitCode::Ok;
}

} // namespace KDC
