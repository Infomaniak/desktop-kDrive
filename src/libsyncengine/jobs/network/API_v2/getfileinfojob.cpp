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

bool GetFileInfoJob::handleResponse(std::istream &is) {
    if (!AbstractTokenNetworkJob::handleResponse(is)) return false;

    if (!jsonRes()) return true;

    const auto dataObj = jsonRes()->getObject(dataKey);
    if (!dataObj) return true;

    if (!JsonParserUtility::extractValue(dataObj, parentIdKey, _parentNodeId)) {
        return false;
    }
    if (!JsonParserUtility::extractValue(dataObj, createdAtKey, _creationTime)) {
        return false;
    }
    if (!JsonParserUtility::extractValue(dataObj, lastModifiedAtKey, _modificationTime)) {
        return false;
    }
    std::string tmp;
    if (!JsonParserUtility::extractValue(dataObj, typeKey, tmp)) {
        return false;
    }
    if (tmp != dirKey) {
        if (!JsonParserUtility::extractValue(dataObj, sizeKey, _size)) {
            return false;
        }
    }

    if (std::string symbolicLink; JsonParserUtility::extractValue(dataObj, symbolicLinkKey, symbolicLink, false)) {
        _isLink = !symbolicLink.empty();
    }

    if (_withPath) {
        std::string relativePathStr;
        if (!JsonParserUtility::extractValue(dataObj, pathKey, relativePathStr)) {
            return false;
        }
        if (Utility::startsWith(relativePathStr, "/")) {
            relativePathStr.erase(0, 1);
        }

        _path = relativePathStr;
    }

    return true;
}

bool GetFileInfoJob::handleError(std::istream &is, const Poco::URI &uri) {
    using namespace Poco::Net;
    if (_resHttp.getStatus() == HTTPResponse::HTTP_FORBIDDEN || _resHttp.getStatus() == HTTPResponse::HTTP_NOT_FOUND) {
        return true; // The file is not accessible or doesn't exist
    }

    return AbstractTokenNetworkJob::handleError(is, uri);
}

std::string GetFileInfoJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/";
    str += _nodeId;
    return str;
}

void GetFileInfoJob::setQueryParameters(Poco::URI &uri, bool &canceled) {
    if (_withPath) {
        uri.addQueryParameter("with", "path");
    }
    canceled = false;
}

} // namespace KDC
