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

#include "getsizejob.h"
#include "libcommon/utility/jsonparserutility.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

GetSizeJob::GetSizeJob(int userDbId, int driveId, const NodeId &nodeId) :
    AbstractTokenNetworkJob(ApiType::Drive, userDbId, 0, 0, driveId),
    _nodeId(nodeId),
    _size(0) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
}

GetSizeJob::GetSizeJob(int driveDbId, const NodeId &nodeId) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, 0, driveDbId, 0),
    _nodeId(nodeId) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
}

ExitInfo GetSizeJob::handleResponse(std::istream &is) {
    if (const auto exitInfo = AbstractTokenNetworkJob::handleResponse(is); !exitInfo) {
        return exitInfo;
    }

    if (jsonRes()) {
        Poco::JSON::Object::Ptr dataObj = jsonRes()->getObject(dataKey);
        if (dataObj) {
            if (!JsonParserUtility::extractValue(dataObj, sizeKey, _size)) {
                return {};
            }
        }
    }

    return ExitCode::Ok;
}

ExitInfo GetSizeJob::handleError(const std::string &replyBody, const Poco::URI &uri) {
    if (httpResponse().getStatus() == Poco::Net::HTTPResponse::HTTP_FORBIDDEN) {
        // Access to the directory is forbidden
        return ExitCode::Ok;
    }
    return AbstractTokenNetworkJob::handleError(replyBody, uri);
}

std::string GetSizeJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/";
    str += _nodeId;
    str += "/sizes";
    return str;
}

} // namespace KDC
