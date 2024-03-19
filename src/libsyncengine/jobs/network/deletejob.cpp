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

#include "deletejob.h"

#include "requests/parameterscache.h"
#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"  // Path2WStr

namespace KDC {

DeleteJob::DeleteJob(int driveDbId, const NodeId &fileId, const SyncPath &absoluteLocalFilepath)
    : AbstractTokenNetworkJob(ApiDrive, 0, 0, driveDbId, 0), _fileId(fileId), _absoluteLocalFilepath(absoluteLocalFilepath) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_DELETE;
}

DeleteJob::~DeleteJob() {}

bool DeleteJob::canRun() {
    if (bypassCheck()) {
        return true;
    }

    // The item must be absent on local replica for the job to run
    bool exists;
    IoError ioError = IoErrorSuccess;
    if (!IoHelper::checkIfPathExistsWithSameNodeId(_absoluteLocalFilepath, _fileId, exists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists for path="
                               << Utility::formatIoError(_absoluteLocalFilepath, ioError).c_str());
        _exitCode = ExitCodeSystemError;
        _exitCause = ExitCauseFileAccessError;
        return false;
    }

    if (exists) {
        FileStat filestat;
        bool exists = false;
        IoError ioError = IoErrorSuccess;
        const bool fileStatSuccess = IoHelper::getFileStat(_absoluteLocalFilepath, &filestat, exists, ioError);
        if (!ParametersCache::instance()->parameters().syncHiddenFiles() && fileStatSuccess && filestat.isHidden) {
            // The item is hidden, remove it from sync
            return true;
        }
        if (!fileStatSuccess) {
            LOGW_WARN(_logger,
                      L"Error in IoHelper::getFileStat: " << Utility::formatIoError(_absoluteLocalFilepath, ioError).c_str());
        }

        LOGW_DEBUG(_logger, L"Item " << Path2WStr(_absoluteLocalFilepath).c_str()
                                     << L" still exist on local replica. Aborting current sync and restart.");
        _exitCode = ExitCodeDataError;  // Data error so the snapshots will be re-created
        _exitCause = ExitCauseUnexpectedFileSystemEvent;
        return false;
    }

    return true;
}

std::string DeleteJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/";
    str += _fileId;
    return str;
}

}  // namespace KDC
