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

#include "duplicatejob.h"
#include "libcommonserver/utility/utility.h"
#include "libcommon/utility/jsonparserutility.h"

namespace KDC {

DuplicateJob::DuplicateJob(int driveDbId, const NodeId &remoteFileId, const SyncPath &absoluteFinalPath) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, 0, driveDbId, 0), _remoteFileId(remoteFileId),
    _absoluteFinalPath(absoluteFinalPath) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_POST;
}

DuplicateJob::~DuplicateJob() {
    if (_vfsForceStatus && _vfsStatus && !_absoluteFinalPath.empty()) {
        bool isPlaceholder = false;
        bool isHydrated = false;
        bool isSyncing = false;
        int progress = 0;
        if (ExitInfo exitInfo = _vfsStatus(_absoluteFinalPath, isPlaceholder, isHydrated, isSyncing, progress); !exitInfo) {
            LOGW_WARN(_logger, L"Error in vfsStatus for path=" << Path2WStr(_absoluteFinalPath) << L" : " << exitInfo);
        }

        if (ExitInfo exitInfo = _vfsForceStatus(_absoluteFinalPath, false, 0, false); !exitInfo) {
            LOGW_WARN(_logger, L"Error in vfsForceStatus for path=" << Path2WStr(_absoluteFinalPath) << L" : " << exitInfo);
        }
    }
}

bool DuplicateJob::handleResponse(std::istream &is) {
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

std::string DuplicateJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/";
    str += _remoteFileId;
    str += "/duplicate";
    return str;
}

ExitInfo DuplicateJob::setData() {
    Poco::JSON::Object json;
    SyncName name = _absoluteFinalPath.filename().native();
    json.set("name", name);

    std::stringstream ss;
    json.stringify(ss);
    _data = ss.str();
    return ExitCode::Ok;
}

} // namespace KDC
