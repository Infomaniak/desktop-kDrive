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

#include "copytodirectoryjob.h"
#include "libcommon/utility/jsonparserutility.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

CopyToDirectoryJob::CopyToDirectoryJob(int driveDbId, const NodeId &remoteFileId, const NodeId &remoteDestId,
                                       const SyncName &newName) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, 0, driveDbId, 0),
    _remoteFileId(remoteFileId),
    _remoteDestId(remoteDestId),
    _newName(newName) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_POST;
}

bool CopyToDirectoryJob::handleResponse(std::istream &is) {
    if (!AbstractTokenNetworkJob::handleResponse(is)) {
        return false;
    }

    NodeId res;
    if (jsonRes()) {
        Poco::JSON::Object::Ptr dataObj = jsonRes()->getObject(dataKey);
        if (dataObj) {
            if (!JsonParserUtility::extractValue(dataObj, idKey, _nodeId)) {
                return false;
            }
            if (!JsonParserUtility::extractValue(dataObj, lastModifiedAtKey, _modtime)) {
                return false;
            }
        }
    }

    return true;
}

std::string CopyToDirectoryJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/";
    str += _remoteFileId;
    str += "/copy/";
    str += _remoteDestId;
    return str;
}

ExitInfo CopyToDirectoryJob::setData() {
    Poco::JSON::Object json;
    json.set("name", _newName);

    std::stringstream ss;
    json.stringify(ss);
    _data = ss.str();
    return ExitCode::Ok;
}

} // namespace KDC
