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

#include "localcreatedirjob.h"
#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"

#include <log4cplus/loggingmacros.h>

namespace KDC {

LocalCreateDirJob::LocalCreateDirJob(const SyncPath &destFilepath) : _destFilePath(destFilepath) {}

bool LocalCreateDirJob::canRun() {
    if (bypassCheck()) {
        return true;
    }

    // Check that we can create the directory here
    bool exists = false;
    IoError ioError = IoErrorSuccess;
    if (!IoHelper::checkIfPathExists(_destFilePath, exists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_destFilePath, ioError).c_str());
        _exitCode = ExitCodeSystemError;
        _exitCause = ExitCauseFileAccessError;
        return false;
    }

    if (exists) {
        LOGW_DEBUG(_logger, L"Directory " << Path2WStr(_destFilePath).c_str() << L" already exist.");
        _exitCode = ExitCodeDataError;
        _exitCause = ExitCauseFileAlreadyExist;
        return false;
    }

    return true;
}

void LocalCreateDirJob::runJob() {
    if (!canRun()) {
        return;
    }

    IoError ioError = IoErrorSuccess;
    if (IoHelper::createDirectory(_destFilePath, ioError)) {
        if (isExtendedLog()) {
            LOGW_DEBUG(_logger, L"Directory " << Path2WStr(_destFilePath).c_str() << L" created");
        }
        _exitCode = ExitCodeOk;
    }

    if (ioError == IoErrorAccessDenied) {
        LOGW_WARN(_logger, L"Search permission missing for path=" << Path2WStr(_destFilePath).c_str());
        _exitCode = ExitCodeSystemError;
        _exitCause = ExitCauseNoSearchPermission;
        return;
    }

    if (ioError != IoErrorSuccess) {  // Unexpected error
        LOGW_WARN(_logger, L"Failed to create directory: " << Utility::formatIoError(_destFilePath, ioError).c_str());
        _exitCode = ExitCodeSystemError;
        _exitCause = ExitCauseFileAccessError;
        return;
    }

    if (_exitCode == ExitCodeOk) {
        FileStat filestat;
        bool exists = false;
        if (!IoHelper::getFileStat(_destFilePath, &filestat, exists, ioError)) {
            LOGW_WARN(_logger, L"Error in IoHelper::getFileStat: " << Utility::formatIoError(_destFilePath, ioError).c_str());
            // TODO: handle error appropriately.
        }
        _nodeId = std::to_string(filestat.inode);
        _modtime = filestat.modtime;
    }
}

}  // namespace KDC
