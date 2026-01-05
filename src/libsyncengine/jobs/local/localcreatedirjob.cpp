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

#include "localcreatedirjob.h"

#include "libcommonserver/io/permissionsholder.h"
#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"

#include <log4cplus/loggingmacros.h>

namespace KDC {

LocalCreateDirJob::LocalCreateDirJob(const SyncPath &destFilepath, bool readOnly /*= false*/) :
    _destFilePath(destFilepath),
    _readOnly(readOnly) {}

ExitInfo LocalCreateDirJob::canRun() {
    if (bypassCheck()) {
        return ExitCode::Ok;
    }

    // Check that we can create the directory here
    bool exists = false;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(_destFilePath, exists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_destFilePath, ioError));
        return ExitCode::SystemError;
    }
    if (ioError == IoError::AccessDenied) {
        LOGW_WARN(_logger, L"Access denied to " << Utility::formatSyncPath(_destFilePath));
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    if (exists) {
        LOGW_DEBUG(_logger, L"Directory: " << Utility::formatSyncPath(_destFilePath) << L" already exist.");
        return {ExitCode::DataError, ExitCause::FileExists};
    }

    return ExitCode::Ok;
}

ExitInfo LocalCreateDirJob::runJob() {
    if (const auto exitInfo = canRun(); !exitInfo) {
        return exitInfo;
    }

    // Make sure we are allowed to propagate the change
    PermissionsHolder _(_destFilePath.parent_path(), _logger);

    IoError ioError = IoError::Success;
    if (IoHelper::createDirectory(_destFilePath, false, ioError) && ioError == IoError::Success) {
        if (isExtendedLog()) {
            LOGW_DEBUG(_logger, L"Directory: " << Utility::formatSyncPath(_destFilePath) << L" created");
        }
    }
    if (ioError == IoError::AccessDenied) {
        LOGW_WARN(_logger, L"Search permission missing: =" << Utility::formatSyncPath(_destFilePath));
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }
    if (ioError != IoError::Success) { // Unexpected error
        LOGW_WARN(_logger, L"Failed to create directory: " << Utility::formatIoError(_destFilePath, ioError));
        return ExitCode::SystemError;
    }

    FileStat filestat;
    if (!IoHelper::getFileStat(_destFilePath, &filestat, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::getFileStat: " << Utility::formatIoError(_destFilePath, ioError));
        return ExitCode::SystemError;
    }

    if (ioError == IoError::NoSuchFileOrDirectory) {
        LOGW_WARN(_logger, L"Item does not exist anymore: " << Utility::formatSyncPath(_destFilePath));
        return {ExitCode::DataError, ExitCause::InvalidSize};
    } else if (ioError == IoError::AccessDenied) {
        LOGW_WARN(_logger, L"Item misses search permission: " << Utility::formatSyncPath(_destFilePath));
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    if (_readOnly) {
        if (IoHelper::setReadOnly(_destFilePath) != IoError::Success) {
            LOGW_WARN(_logger, L"Failed to set read-only rights: " << Utility::formatSyncPath(_destFilePath));
        }
    }

    _nodeId = std::to_string(filestat.inode);
    _modtime = filestat.modificationTime;
    _creationTime = filestat.creationTime;
    return ExitCode::Ok;
}

} // namespace KDC
