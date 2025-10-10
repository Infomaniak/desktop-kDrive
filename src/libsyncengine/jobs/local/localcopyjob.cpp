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

#include "localcopyjob.h"

#include "libcommonserver/io/permissionsholder.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"

#include <log4cplus/loggingmacros.h>

namespace KDC {

LocalCopyJob::LocalCopyJob(const SyncPath &source, const SyncPath &dest) :
    _source(source),
    _dest(dest) {}

ExitInfo LocalCopyJob::canRun() {
    if (bypassCheck()) {
        return ExitCode::Ok;
    }

    // Check that we can copy the file in destination
    bool exists = false;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(_dest, exists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_dest, ioError));
        return ExitCode::SystemError;
    }
    if (ioError == IoError::AccessDenied) {
        LOGW_WARN(_logger, L"Access denied to " << Path2WStr(_dest));
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    if (exists) {
        LOGW_DEBUG(_logger, L"Item " << Path2WStr(_dest) << L" already exist. Aborting current sync and restart.");
        return {ExitCode::DataError, ExitCause::FileExists};
    }

    // Check that source file still exists
    if (!IoHelper::checkIfPathExists(_source, exists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_source, ioError));
        return ExitCode::SystemError;
    }
    if (ioError == IoError::AccessDenied) {
        LOGW_WARN(_logger, L"Access denied to " << Path2WStr(_source));
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    if (!exists) {
        LOGW_DEBUG(_logger, L"Item does not exist anymore. Aborting current sync and restart. Item with "
                                    << Utility::formatSyncPath(_source));
        return {ExitCode::DataError, ExitCause::NotFound};
    }

    return ExitCode::Ok;
}

ExitInfo LocalCopyJob::runJob() {
    if (const auto exitInfo = canRun(); !exitInfo) {
        return exitInfo;
    }

    // Make sure we are allowed to propagate the change
    PermissionsHolder _(_dest.parent_path(), _logger);

    try {
        std::filesystem::copy(_source, _dest);
        LOGW_INFO(_logger, L"Item " << Path2WStr(_source) << L" copied to " << Path2WStr(_dest));
    } catch (std::filesystem::filesystem_error &fsError) {
        LOGW_WARN(_logger, L"Failed to copy item " << Path2WStr(_source) << L" to " << Path2WStr(_dest) << L": "
                                                   << CommonUtility::s2ws(fsError.what()) << L" (" << fsError.code().value()
                                                   << L")");
        exitInfo = ExitCode::SystemError;
        if (IoHelper::stdError2ioError(fsError.code()) == IoError::AccessDenied) {
            exitInfo.setCause(ExitCause::FileAccessError);
        }
    } catch (...) {
        LOGW_WARN(_logger, L"Failed to copy item " << Utility::formatSyncPath(_source) << L" to "
                                                   << Utility::formatSyncPath(_dest) << L": Unknown error");
        exitInfo = ExitCode::SystemError;
    }
    return exitInfo;
}

} // namespace KDC
