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

#include "renamejob.h"
#include "libcommonserver/utility/utility.h"

namespace KDC {

RenameJob::RenameJob(int driveDbId, const NodeId &remoteFileId, const SyncPath &absoluteFinalPath) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, 0, driveDbId, 0), _remoteFileId(remoteFileId),
    _absoluteFinalPath(absoluteFinalPath) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_POST;
}

RenameJob::~RenameJob() {
    if (_vfsForceStatus && _vfsStatus && !_absoluteFinalPath.empty()) {
        VfsStatus vfsStatus;
        if (const auto exitInfo = _vfsStatus(_absoluteFinalPath, vfsStatus); !exitInfo) {
            LOGW_WARN(_logger, L"Error in vfsStatus for path=" << Path2WStr(_absoluteFinalPath) << L" : " << exitInfo);
        }

        vfsStatus.isSyncing = false;
        vfsStatus.progress = 0;
        if (const auto exitInfo = _vfsForceStatus(_absoluteFinalPath, vfsStatus); !exitInfo) {
            LOGW_WARN(_logger, L"Error in vfsForceStatus for path=" << Path2WStr(_absoluteFinalPath) << L" : " << exitInfo);
        }
    }
}

std::string RenameJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/";
    str += _remoteFileId;
    str += "/rename";
    return str;
}

ExitInfo RenameJob::setData() {
    Poco::JSON::Object json;
    SyncName name = _absoluteFinalPath.filename().native();
    json.set("name", name);

    std::stringstream ss;
    json.stringify(ss);
    _data = ss.str();
    return ExitCode::Ok;
}

} // namespace KDC
