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

#include "renamejob.h"
#include "libcommonserver/utility/utility.h"

namespace KDC {

RenameJob::RenameJob(int driveDbId, const NodeId &remoteFileId, const SyncPath &absoluteFinalPath)
    : AbstractTokenNetworkJob(ApiDrive, 0, 0, driveDbId, 0), _remoteFileId(remoteFileId), _absoluteFinalPath(absoluteFinalPath) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_POST;
}

RenameJob::~RenameJob() {
    if (_vfsForceStatus && _vfsStatus && !_absoluteFinalPath.empty()) {
        bool isPlaceholder = false;
        bool isHydrated = false;
        bool isSyncing = false;
        int progress = 0;
        if (!_vfsStatus(_absoluteFinalPath, isPlaceholder, isHydrated, isSyncing, progress)) {
            LOGW_WARN(_logger, L"Error in vfsStatus for path=" << Path2WStr(_absoluteFinalPath).c_str());
        }

        if (!_vfsForceStatus(_absoluteFinalPath, false, 0, isHydrated)) {
            LOGW_WARN(_logger, L"Error in vfsForceStatus for path=" << Path2WStr(_absoluteFinalPath).c_str());
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

void RenameJob::setData(bool &canceled) {
    Poco::JSON::Object json;
    SyncName name = _absoluteFinalPath.filename().native();
    json.set("name", name);

    std::stringstream ss;
    json.stringify(ss);
    _data = ss.str();
    canceled = false;
}

}  // namespace KDC
