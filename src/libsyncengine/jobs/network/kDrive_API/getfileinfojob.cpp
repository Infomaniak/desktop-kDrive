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

#include "getfileinfojob.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/utility/jsonparserutility.h"
#include "libsyncengine/jobs/network/kDrive_API/apitranslator.h"
#include "jobs/network/jobexceptions.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

GetFileInfoJob::GetFileInfoJob(const UserDbId userDbId, const DriveId driveId, RemoteNodeId nodeId) :
    AbstractTokenNetworkJob(ApiType::Drive, userDbId, 0, driveId),

    _nodeId(std::move(nodeId)) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;

    _trials = 1;

    if (const auto exitInfo = ApiTranslator::translateV2ToV3(userDbId, driveId, _nodeId); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ApiTranslator::translateV2ToV3: " << exitInfo);
        throw JobException("Translation error in GetFileInfoJob::GetFileInfoJob.");
    }
}


GetFileInfoJob::GetFileInfoJob(const DriveDbId driveDbId, RemoteNodeId nodeId) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, driveDbId, 0),
    _nodeId(std::move(nodeId)) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;

    _trials = 1;

    if (const auto exitInfo = ApiTranslator::translateV2ToV3(userDbId(), driveId(), _nodeId); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ApiTranslator::translateV2ToV3: " << exitInfo);
        throw JobException("Translation error in GetFileInfoJob::GetFileInfoJob.");
    }
}

ExitInfo GetFileInfoJob::handleResponse(std::istream &is) {
    if (const auto exitInfo = AbstractTokenNetworkJob::handleResponse(is); !exitInfo) return exitInfo;

    if (!jsonRes()) return ExitCode::Ok;

    const auto dataObj = jsonRes()->getObject(dataKey);
    if (!dataObj) return ExitCode::Ok;

    if (!JsonParserUtility::extractValue(dataObj, parentIdKey, _parentNodeId))
        return {ExitCode::BackError, ExitCause::MissingReplyData};

    if (!JsonParserUtility::extractValue(dataObj, createdAtKey, _creationTime))
        return {ExitCode::BackError, ExitCause::MissingReplyData};

    if (!JsonParserUtility::extractValue(dataObj, lastModifiedAtKey, _modificationTime))
        return {ExitCode::BackError, ExitCause::MissingReplyData};

    std::string tmp;
    if (!JsonParserUtility::extractValue(dataObj, typeKey, tmp)) return {ExitCode::BackError, ExitCause::MissingReplyData};

    if (tmp != dirKey && !JsonParserUtility::extractValue(dataObj, sizeKey, _size))
        return {ExitCode::BackError, ExitCause::MissingReplyData};

    if (!JsonParserUtility::extractValue(dataObj, lastModifiedByKey, _lastModifiedByUserId))
        return {ExitCode::BackError, ExitCause::MissingReplyData};

    constexpr bool mandatory = false;
    if (std::string symbolicLink; JsonParserUtility::extractValue(dataObj, symbolicLinkKey, symbolicLink, mandatory)) {
        _isLink = !symbolicLink.empty();
    }

    if (_withPath) {
        std::string relativePathStr;
        if (!JsonParserUtility::extractValue(dataObj, pathKey, relativePathStr)) {
            return {};
        }
        if (CommonUtility::startsWith(relativePathStr, "/")) {
            relativePathStr.erase(0, 1);
        }

        _path = relativePathStr;
    }

    return ExitCode::Ok;
}

ExitInfo GetFileInfoJob::handleError(const std::string &replyBody, const Poco::URI &uri) {
    using namespace Poco::Net;
    if (httpResponse().getStatus() == HTTPResponse::HTTP_FORBIDDEN ||
        httpResponse().getStatus() == HTTPResponse::HTTP_NOT_FOUND) {
        return ExitCode::Ok; // The file is not accessible or doesn't exist
    }

    return AbstractTokenNetworkJob::handleError(replyBody, uri);
}

std::string GetFileInfoJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/";
    str += _nodeId;

    return str;
}

void GetFileInfoJob::setQueryParameters(Poco::URI &uri) {
    if (_withPath) {
        uri.addQueryParameter("with", "path,capabilities");
    } else {
        uri.addQueryParameter("with", "capabilities");
    }
}

} // namespace KDC
