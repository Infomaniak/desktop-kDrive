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
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(_destFilePath, exists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_destFilePath, ioError).c_str());
        _exitCode = ExitCode::SystemError;
        _exitCause = ExitCause::Unknown;
        return false;
    }
    if (ioError == IoError::AccessDenied) {
        LOGW_WARN(_logger, L"Access denied to " << Utility::formatSyncPath(_destFilePath).c_str());
        _exitCode = ExitCode::SystemError;
        _exitCause = ExitCause::FileAccessError;
        return false;
    }

    if (exists) {
        LOGW_DEBUG(_logger, L"Directory: " << Utility::formatSyncPath(_destFilePath).c_str() << L" already exist.");
        _exitCode = ExitCode::DataError;
        _exitCause = ExitCause::FileAlreadyExist;
        return false;
    }

    return true;
}

void LocalCreateDirJob::runJob() {
    if (!canRun()) {
        return;
    }

    IoError ioError = IoError::Success;
    if (IoHelper::createDirectory(_destFilePath, ioError) && ioError == IoError::Success) {
        if (isExtendedLog()) {
            LOGW_DEBUG(_logger, L"Directory: " << Utility::formatSyncPath(_destFilePath).c_str() << L" created");
        }
        _exitCode = ExitCode::Ok;
    }

    if (ioError == IoError::AccessDenied) {
        LOGW_WARN(_logger, L"Search permission missing: =" << Utility::formatSyncPath(_destFilePath).c_str());
        _exitCode = ExitCode::SystemError;
        _exitCause = ExitCause::FileAccessError;
        return;
    }

    if (ioError != IoError::Success) { // Unexpected error
        LOGW_WARN(_logger, L"Failed to create directory: " << Utility::formatIoError(_destFilePath, ioError).c_str());
        _exitCode = ExitCode::SystemError;
        _exitCause = ExitCause::Unknown;
        return;
    }

    if (_exitCode == ExitCode::Ok) {
        FileStat filestat;
        if (!IoHelper::getFileStat(_destFilePath, &filestat, ioError)) {
            LOGW_WARN(_logger, L"Error in IoHelper::getFileStat: " << Utility::formatIoError(_destFilePath, ioError).c_str());
            _exitCode = ExitCode::SystemError;
            _exitCause = ExitCause::Unknown;
            return;
        }

        if (ioError == IoError::NoSuchFileOrDirectory) {
            LOGW_WARN(_logger, L"Item does not exist anymore: " << Utility::formatSyncPath(_destFilePath).c_str());
            _exitCode = ExitCode::DataError;
            _exitCause = ExitCause::InvalidSnapshot;
            return;
        } else if (ioError == IoError::AccessDenied) {
            LOGW_WARN(_logger, L"Item misses search permission: " << Utility::formatSyncPath(_destFilePath).c_str());
            _exitCode = ExitCode::SystemError;
            _exitCause = ExitCause::FileAccessError;
            return;
        }

        _nodeId = std::to_string(filestat.inode);
        _modtime = filestat.modtime;
    }
}

} // namespace KDC
