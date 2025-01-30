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

#include "movejob.h"

#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"

namespace KDC {

MoveJob::MoveJob(int driveDbId, const SyncPath &destFilepath, const NodeId &fileId, const NodeId &destDirId,
                 const SyncName &name /*= ""*/) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, 0, driveDbId, 0), _destFilepath(destFilepath), _fileId(fileId),
    _destDirId(destDirId), _name(name) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_POST;
}

MoveJob::~MoveJob() {
    if (_vfsForceStatus && _vfsStatus) {
        VfsStatus vfsStatus;
        if (const auto exitInfo = _vfsStatus(_destFilepath, vfsStatus); !exitInfo) {
            LOGW_WARN(_logger, L"Error in vfsStatus for path=" << Path2WStr(_destFilepath) << L" : " << exitInfo);
        }

        vfsStatus.isSyncing = false;
        vfsStatus.progress = 100;
        if (const auto exitInfo = _vfsForceStatus(_destFilepath, vfsStatus);
            !exitInfo) { // TODO : to be refactored, some parameters are used on macOS only
            LOGW_WARN(_logger, L"Error in vfsForceStatus for path=" << Path2WStr(_destFilepath) << L" : " << exitInfo);
        }
    }
}

bool MoveJob::canRun() {
    if (bypassCheck()) {
        return true;
    }

    // Check that we still have to move the folder
    bool exists;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(_destFilepath, exists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_destFilepath, ioError).c_str());
        _exitCode = ExitCode::SystemError;
        _exitCause = ExitCause::Unknown;
        return false;
    }
    if (ioError == IoError::AccessDenied) {
        LOGW_WARN(_logger, L"Access denied to " << Utility::formatSyncPath(_destFilepath));
        _exitCode = ExitCode::SystemError;
        _exitCause = ExitCause::FileAccessError;
        return false;
    }

    if (!exists) {
        LOGW_DEBUG(_logger, L"File " << Path2WStr(_destFilepath).c_str()

                                     << L" is not in its destination folder. Aborting current sync and restart.");
        _exitCode = ExitCode::DataError; // Data error so the snapshots will be re-created
        _exitCause = ExitCause::UnexpectedFileSystemEvent;
        return false;
    }

    return true;
}

std::string MoveJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/";
    str += _fileId;
    str += "/move/";
    str += _destDirId;
    return str;
}

ExitInfo MoveJob::setData() {
    Poco::JSON::Object json;
    if (!_name.empty()) {
        json.set("name", _name);

        std::stringstream ss;
        json.stringify(ss);
        _data = ss.str();
    }
    return ExitCode::Ok;
}

} // namespace KDC
