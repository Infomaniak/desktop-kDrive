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

#include "getappversionjob.h"
#include "utility/jsonparserutility.h"
#include "utility/utility.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

static const std::string prodVersionKey = "prod_version";
static const std::string applicationKey = "application";
static const std::string publishedVersionsKey = "published_versions";
static const std::string versionTypeProdKey = "production";
static const std::string versionTypeNextKey = "production-next";
static const std::string versionTypeBetaKey = "beta";
static const std::string versionTypeInternalKey = "internal";
static const std::string platformMacOsKey = "mac-os";
static const std::string platformWindowsKey = "windows";
static const std::string platformLinuxAmdKey = "linux-amd";
static const std::string platformLinuxArmKey = "linux-arm";
static const std::string tagKey = "tag";
static const std::string buildVersionKey = "build_version";
static const std::string buildMinOsVersionKey = "build_min_os_version";
static const std::string downloadUrlKey = "download_link";

GetAppVersionJob::GetAppVersionJob(const Platform platform, const std::string &appID) :
    GetAppVersionJob(platform, appID, {}) {}

GetAppVersionJob::GetAppVersionJob(const Platform platform, const std::string &appID, const std::vector<int> &userIdList) :
    _platform(platform),
    _appId(appID),
    _userIdList(userIdList) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
}

std::string GetAppVersionJob::toStr(const Platform platform) {
    switch (platform) {
        case Platform::MacOS:
            return platformMacOsKey;
        case Platform::Windows:
        case Platform::WindowsServer:
            return platformWindowsKey;
        case Platform::LinuxAMD:
            return platformLinuxAmdKey;
        case Platform::LinuxARM:
            return platformLinuxArmKey;
        default:
            return "unknown";
    }
}

VersionChannel toDistributionChannel(const std::string &str) {
    if (str == versionTypeProdKey) return VersionChannel::Prod;
    if (str == versionTypeNextKey) return VersionChannel::Next;
    if (str == versionTypeBetaKey) return VersionChannel::Beta;
    if (str == versionTypeInternalKey) return VersionChannel::Internal;
    return VersionChannel::Unknown;
}

std::string GetAppVersionJob::toStr(const VersionChannel channel) {
    switch (channel) {
        case VersionChannel::Prod:
            return versionTypeProdKey;
        case VersionChannel::Next:
            return versionTypeNextKey;
        case VersionChannel::Beta:
            return versionTypeBetaKey;
        case VersionChannel::Internal:
            return versionTypeInternalKey;
        default:
            return "unknown";
    }
}

std::string GetAppVersionJob::getSpecificUrl() {
    std::stringstream ss;
    ss << "/app-information/kstore-update/" << toStr(_platform) << "/com.infomaniak.drive/" << _appId;
    return ss.str();
}

std::string GetAppVersionJob::getContentType(bool &canceled) {
    canceled = false;
    return {};
}

void GetAppVersionJob::setQueryParameters(Poco::URI &uri, bool &canceled) {
    (void) canceled;
    for (const auto &id: _userIdList) {
        uri.addQueryParameter("user_ids[]", std::to_string(id));
    }
}

bool GetAppVersionJob::handleError(const std::string &, const Poco::URI &uri) {
    LOG_DEBUG(_logger, "Request failed: " << Utility::formatRequest(uri, _errorCode, _errorDescr));
    return false;
}

VersionChannel GetAppVersionJob::toDistributionChannel(const std::string &val) const {
    if (val == versionTypeProdKey) return VersionChannel::Prod;
    if (val == versionTypeNextKey) return VersionChannel::Next;
    if (val == versionTypeBetaKey) return VersionChannel::Beta;
    if (val == versionTypeInternalKey) return VersionChannel::Internal;
    return VersionChannel::Unknown;
}

bool GetAppVersionJob::handleResponse(std::istream &is) {
    std::string replyBody;
    getStringFromStream(is, replyBody);
    if (!AbstractNetworkJob::handleJsonResponse(replyBody)) return false;

    const Poco::JSON::Object::Ptr dataObj = JsonParserUtility::extractJsonObject(jsonRes(), dataKey);
    if (!dataObj) return false;

    std::string tmpStr;
    if (!JsonParserUtility::extractValue(dataObj, prodVersionKey, tmpStr)) return false;
    _prodVersionChannel = toDistributionChannel(tmpStr);

    const Poco::JSON::Object::Ptr applicationObj = JsonParserUtility::extractJsonObject(dataObj, applicationKey);
    if (!applicationObj) return false;

    const Poco::JSON::Array::Ptr publishedVersions = JsonParserUtility::extractArrayObject(applicationObj, publishedVersionsKey);
    if (!publishedVersions) return false;

    for (const auto &versionInfo: *publishedVersions) {
        const auto &obj = versionInfo.extract<Poco::JSON::Object::Ptr>();
        std::string versionType;
        if (!JsonParserUtility::extractValue(obj, typeKey, versionType)) return false;

        const VersionChannel channel = toDistributionChannel(versionType);
        _versionsInfo[channel].channel = channel;

        if (!JsonParserUtility::extractValue(obj, tagKey, _versionsInfo[channel].tag)) return false;
        if (!JsonParserUtility::extractValue(obj, buildVersionKey, _versionsInfo[channel].buildVersion)) return false;
        if (!JsonParserUtility::extractValue(obj, buildMinOsVersionKey, _versionsInfo[channel].buildMinOsVersion, false))
            return false;
        if (!JsonParserUtility::extractValue(obj, downloadUrlKey, _versionsInfo[channel].downloadUrl)) return false;

        if (!_versionsInfo[channel].isValid()) {
            LOG_WARN(_logger, "Missing mandatory value.");
            return false;
        }
    }

    return true;
}

} // namespace KDC
