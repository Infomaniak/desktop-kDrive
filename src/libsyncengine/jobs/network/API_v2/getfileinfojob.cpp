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

#include "getfileinfojob.h"
#include "libcommonserver/utility/utility.h"
#include "libcommon/utility/jsonparserutility.h"

namespace KDC {

GetFileInfoJob::GetFileInfoJob(int userDbId, int driveId, const NodeId &nodeId)
    : AbstractTokenNetworkJob(ApiType::Drive, userDbId, 0, 0, driveId), _nodeId(nodeId) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
    _trials = 1;
}

GetFileInfoJob::GetFileInfoJob(int driveDbId, const NodeId &nodeId)
    : AbstractTokenNetworkJob(ApiType::Drive, 0, 0, driveDbId, 0), _nodeId(nodeId) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
    _trials = 1;
}

bool GetFileInfoJob::handleResponse(std::istream &is) {
    if (!AbstractTokenNetworkJob::handleResponse(is)) return false;

    if (!jsonRes()) return true;

    Poco::JSON::Object::Ptr dataObj = jsonRes()->getObject(dataKey);
    if (!dataObj) return true;

    if (!JsonParserUtility::extractValue(dataObj, parentIdKey, _parentNodeId)) {
        return false;
    }
    if (!JsonParserUtility::extractValue(dataObj, createdAtKey, _creationTime)) {
        return false;
    }
    if (!JsonParserUtility::extractValue(dataObj, lastModifiedAtKey, _modtime)) {
        return false;
    }
    std::string tmp;
    if (!JsonParserUtility::extractValue(dataObj, typeKey, tmp)) {
        return false;
    }
    const bool isDir = tmp == dirKey;
    if (!isDir) {
        if (!JsonParserUtility::extractValue(dataObj, sizeKey, _size)) {
            return false;
        }
    }

    std::string symbolicLink;
    if (JsonParserUtility::extractValue(dataObj, symbolicLinkKey, symbolicLink, false)) {
        _isLink = !symbolicLink.empty();
    }

    if (_withPath) {
        std::string str;
        if (!JsonParserUtility::extractValue(dataObj, pathKey, str)) {
            return false;
        }
        if (Utility::startsWith(str, "/")) {
            str.erase(0, 1);
        }
        _path = str;
    }

    return true;
}

bool GetFileInfoJob::handleError(std::istream &is, const Poco::URI &uri) {
    if (_resHttp.getStatus() == Poco::Net::HTTPResponse::HTTP_FORBIDDEN ||
        _resHttp.getStatus() == Poco::Net::HTTPResponse::HTTP_NOT_FOUND) {
        // The file is not accessible or doesn't exist
        return true;
    } else {
        return AbstractTokenNetworkJob::handleError(is, uri);
    }
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

}  // namespace KDC
