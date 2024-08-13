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

#include "getappversionjob.h"
#include "utility/jsonparserutility.h"

#include <config.h>

namespace KDC {
GetAppVersionJob::GetAppVersionJob(const Platform platform, const std::string &appID) : _platform(platform), _appId(appID) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
}

std::string platformToStr(Platform platform) {
    switch (platform) {
        case Platform::MacOS:
            return platformMacOsKey;
        case Platform::Windows:
            return platformWindowsKey;
        case Platform::LinuxAMD:
            return platformLinuxAmdKey;
        case Platform::LinuxARM:
            return platformLinuxArmKey;
        default:
            return "unknown";
    }
}

std::string GetAppVersionJob::getSpecificUrl() {
    std::string str = std::format("/app-information/kstore-update/{}/com.infomaniak.drive/{}", platformToStr(_platform), _appId);
    return str;
}
std::string GetAppVersionJob::getContentType(bool &canceled) {
    canceled = false;
    return {};
}

bool GetAppVersionJob::handleError(std::istream &is, const Poco::URI &uri) {
    // TODO
    return false;
}

DistributionChannel GetAppVersionJob::toDistributionChannel(const std::string &val) const {
    if (val == versionTypeProdKey) {
        return DistributionChannel::Prod;
    }
    if (val == versionTypeNextKey) {
        return DistributionChannel::Next;
    }
    if (val == versionTypeBetaKey) {
        return DistributionChannel::Beta;
    }
    if (val == versionTypeInternalKey) {
        return DistributionChannel::Internal;
    }
    return DistributionChannel::Unknown;
}

bool GetAppVersionJob::handleResponse(std::istream &is) {
    if (!AbstractNetworkJob::handleJsonResponse(is)) {
        return false;
    }

    const Poco::JSON::Object::Ptr dataObj = JsonParserUtility::extractJsonObject(jsonRes(), dataKey);
    if (!dataObj) return false;

    const Poco::JSON::Object::Ptr applicationObj = JsonParserUtility::extractJsonObject(dataObj, applicationKey);
    if (!applicationObj) return false;

    const Poco::JSON::Array::Ptr publishedVersions = JsonParserUtility::extractArrayObject(applicationObj, publishedVersionsKey);
    if (!publishedVersions) return false;

    for (const auto &versionInfo : *publishedVersions) {
        const auto &obj = versionInfo.extract<Poco::JSON::Object::Ptr>();
        std::string versionType;
        if (!JsonParserUtility::extractValue(obj, typeKey, versionType)) {
            return false;
        }

        const DistributionChannel channel = toDistributionChannel(versionType);
        if (!JsonParserUtility::extractValue(obj, tagKey, _versionInfo[channel].tag)) {
            return false;
        }
        if (!JsonParserUtility::extractValue(obj, changeLogKey, _versionInfo[channel].changeLog)) {
            return false;
        }
        if (!JsonParserUtility::extractValue(obj, buildVersionKey, _versionInfo[channel].buildVersion)) {
            return false;
        }
        if (!JsonParserUtility::extractValue(obj, buildMinOsVersionKey, _versionInfo[channel].buildMinOsVersion)) {
            return false;
        }
        if (!JsonParserUtility::extractValue(obj, downloadUrlKey, _versionInfo[channel].downloadUrl)) {
            return false;
        }

        if (!_versionInfo[channel].isValid()) {
            LOG_WARN(_logger, "Missing mandatory value.");
            return false;
        }
    }

    return true;
}
}  // namespace KDC
