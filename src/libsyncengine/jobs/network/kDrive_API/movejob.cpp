/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "jobs/network/jobexceptions.h"
#include "jobs/network/kDrive_API/apitranslator.h"

#include "libcommonserver/io/iohelper.h"

#include "libcommonserver/utility/utility.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

MoveJob::MoveJob(const std::shared_ptr<Vfs> vfs, const DriveDbId driveDbId, SyncPath destFilepath, RemoteNodeId fileId,
                 RemoteNodeId destDirId, SyncName name /*= ""*/) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, driveDbId, 0),
    _destFilepath(std::move(destFilepath)),
    _fileId(std::move(fileId)),
    _destDirId(std::move(destDirId)),
    _name(std::move(name)),
    _vfs(vfs) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_POST;
    _apiVersion = 3;
    if (const auto exitInfo = ApiTranslator::translateV2ToV3(userDbId(), driveId(), _destDirId); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ApiTranslator::translateV2ToV3: " << exitInfo);
        throw JobException("Translation error in MoveJob::MoveJob.");
    }
}

MoveJob::~MoveJob() {
    if (!_vfs) return;

    VfsStatus vfsStatus;
    if (const ExitInfo exitInfo = _vfs->status(_destFilepath, vfsStatus); !exitInfo) {
        LOGW_WARN(_logger, L"Error in vfsStatus for " << Utility::formatSyncPath(_destFilepath) << L": " << exitInfo);
    }

    vfsStatus.isSyncing = false;
    vfsStatus.progress = 100;
    if (const ExitInfo exitInfo = _vfs->forceStatus(_destFilepath, vfsStatus);
        !exitInfo) { // TODO : to be refactored, some parameters are used on macOS only
        LOGW_WARN(_logger, L"Error in vfsForceStatus for " << Utility::formatSyncPath(_destFilepath) << L": " << exitInfo);
    }
}

ExitInfo MoveJob::canRun() {
    if (bypassCheck()) return ExitCode::Ok;

    // Check that we still have to move the folder
    bool exists = false;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(_destFilepath, exists, ioError, IoHelper::PathCheckOption::Insensitive)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_destFilepath, ioError));
        return ExitCode::SystemError;
    }

    if (ioError == IoError::AccessDenied) {
        LOGW_WARN(_logger, L"Access denied to " << Utility::formatSyncPath(_destFilepath));
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    if (!exists) {
        LOGW_DEBUG(_logger, L"File " << Path2WStr(_destFilepath).c_str()
                                     << L" is not in its destination folder. Aborting current sync and restart.");
        return {ExitCode::DataError, ExitCause::InvalidDestination};
    }

    return ExitCode::Ok;
}

std::string MoveJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/";
    str += _fileId;
    str += "/move/";
    str += _destDirId;

    return str;
}

void MoveJob::setQueryParameters(Poco::URI &uri) {
    uri.addQueryParameter(conflictKey, conflictErrorValue);
}

ExitInfo MoveJob::setData() {
    if (!_name.empty()) {
        Poco::JSON::Object json;
        (void) json.set("name", _name);

        std::stringstream ss;
        json.stringify(ss);
        _data = ss.str();
    }

    return ExitCode::Ok;
}

} // namespace KDC
