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

#include "getfileinfojob.h"
#include "libcommonserver/utility/utility.h"
#include "libcommon/utility/jsonparserutility.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

GetFileInfoJob::GetFileInfoJob(const int userDbId, const int driveId, const NodeId &nodeId) :
    AbstractTokenNetworkJob(ApiType::Drive, userDbId, 0, 0, driveId),
    _nodeId(nodeId) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
    _trials = 1;
}

GetFileInfoJob::GetFileInfoJob(const int driveDbId, const NodeId &nodeId) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, 0, driveDbId, 0),
    _nodeId(nodeId) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
    _trials = 1;
}

ExitInfo GetFileInfoJob::handleResponse(std::istream &is) {
    if (const auto exitCode = AbstractTokenNetworkJob::handleResponse(is); !exitCode) return exitCode;

    if (!jsonRes()) return ExitCode::Ok;

    const auto dataObj = jsonRes()->getObject(dataKey);
    if (!dataObj) return ExitCode::Ok;

    if (!JsonParserUtility::extractValue(dataObj, parentIdKey, _parentNodeId)) {
        return {};
    }
    if (!JsonParserUtility::extractValue(dataObj, createdAtKey, _creationTime)) {
        return {};
    }
    if (!JsonParserUtility::extractValue(dataObj, lastModifiedAtKey, _modificationTime)) {
        return {};
    }
    std::string tmp;
    if (!JsonParserUtility::extractValue(dataObj, typeKey, tmp)) {
        return {};
    }
    if (tmp != dirKey) {
        if (!JsonParserUtility::extractValue(dataObj, sizeKey, _size)) {
            return {};
        }
    }

    if (std::string symbolicLink; JsonParserUtility::extractValue(dataObj, symbolicLinkKey, symbolicLink, false)) {
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
        uri.addQueryParameter("with", "path");
    }
}

} // namespace KDC
