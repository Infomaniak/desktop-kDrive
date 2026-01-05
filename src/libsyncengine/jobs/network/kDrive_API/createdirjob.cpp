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

#include "createdirjob.h"
#include "libcommonserver/utility/utility.h"
#include "libcommon/utility/jsonparserutility.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

CreateDirJob::CreateDirJob(const std::shared_ptr<Vfs> &vfs, int driveDbId, const SyncPath &filepath, const NodeId &parentId,
                           const SyncName &name, const std::string &color /*= ""*/) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, 0, driveDbId, 0),
    _filePath(filepath),
    _parentDirId(parentId),
    _name(name),
    _color(color),
    _vfs(vfs) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_POST;
}

CreateDirJob::CreateDirJob(const std::shared_ptr<Vfs> &vfs, int driveDbId, const NodeId &parentId, const SyncName &name) :
    CreateDirJob(vfs, driveDbId, "", parentId, name) {}

CreateDirJob::~CreateDirJob() {
    if (_filePath.empty() || !_vfs) return;
    if (const ExitInfo exitInfo = _vfs->setPinState(_filePath, PinState::AlwaysLocal); !exitInfo) {
        LOGW_WARN(_logger,
                  L"Error in CreateDirJob::vfsSetPinState for " << Utility::formatSyncPath(_filePath) << L" : " << exitInfo);
    }

    if (const ExitInfo exitInfo =
                _vfs->forceStatus(_filePath, VfsStatus({.isHydrated = true, .isSyncing = false, .progress = 0}));
        !exitInfo) {
        LOGW_WARN(_logger,
                  L"Error in CreateDirJob::vfsForceStatus for " << Utility::formatSyncPath(_filePath) << L" : " << exitInfo);
    }
}
std::string CreateDirJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/";
    str += _parentDirId;
    str += "/directory";
    return str;
}

ExitInfo CreateDirJob::setData() {
    Poco::JSON::Object json;
    json.set("name", _name);
    if (!_color.empty()) {
        json.set("color", _color);
    }

    std::stringstream ss;
    json.stringify(ss);
    _data = ss.str();
    return ExitCode::Ok;
}

ExitInfo CreateDirJob::handleResponse(std::istream &is) {
    if (const auto exitInfo = AbstractTokenNetworkJob::handleResponse(is); !exitInfo) {
        return exitInfo;
    }

    if (jsonRes()) {
        if (const auto dataObj = jsonRes()->getObject(dataKey); dataObj) {
            if (!JsonParserUtility::extractValue(dataObj, idKey, _nodeId)) {
                return {};
            }
            if (!JsonParserUtility::extractValue(dataObj, lastModifiedAtKey, _modtime)) {
                return {};
            }
        }

        if (!_filePath.empty() && _vfs) {
            constexpr VfsStatus vfsStatus({.isHydrated = true, .isSyncing = false, .progress = 0});
            if (const auto exitInfo = _vfs->forceStatus(_filePath, vfsStatus); !exitInfo) {
                LOGW_WARN(_logger, L"Error in CreateDirJob::_vfsForceStatus for " << Utility::formatSyncPath(_filePath) << L" : "
                                                                                  << exitInfo);
            }
        }
    }

    return ExitCode::Ok;
}

} // namespace KDC
