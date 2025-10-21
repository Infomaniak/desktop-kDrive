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

#include "localmovejob.h"

#include "libcommonserver/io/permissionsholder.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"

#include <log4cplus/loggingmacros.h>

namespace KDC {

LocalMoveJob::LocalMoveJob(const SyncPath &source, const SyncPath &dest) :
    _source(source),
    _dest(dest) {}

ExitInfo LocalMoveJob::canRun() {
    if (bypassCheck()) {
        return ExitCode::Ok;
    }

    LOGW_DEBUG(_logger, L"Move from: " << Utility::formatSyncPath(_source) << L" to: " << Utility::formatSyncPath(_dest));

    // If the paths are not identical except for case and encoding, check that the destination doesn't already exist
    bool isEqual = false;
    if (!Utility::checkIfEqualUpToCaseAndEncoding(_source, _dest, isEqual)) {
        LOG_WARN(_logger, "Error in Utility::checkIfEqualUpToCaseAndEncoding");
        return {ExitCode::SystemError, ExitCause::Unknown};
    }

    IoError ioError = IoError::Success;
    if (!isEqual) {
        // Check that we can move the file in destination
        bool exists = false;
        if (!IoHelper::checkIfPathExists(_dest, exists, ioError)) {
            LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_dest, ioError));
            return ExitCode::SystemError;
        }
        if (ioError == IoError::AccessDenied) {
            LOGW_WARN(_logger, L"Access denied to " << Utility::formatSyncPath(_dest));
            return {ExitCode::SystemError, ExitCause::FileAccessError};
        }

        if (exists) {
            LOGW_DEBUG(_logger, L"Item already exists: " << Utility::formatSyncPath(_dest));
            return {ExitCode::DataError, ExitCause::FileExists};
        }
    }

    // Check that the source file still exists.
    bool exists = false;
    if (!IoHelper::checkIfPathExists(_source, exists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_source, ioError));
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    if (!exists) {
        LOGW_DEBUG(_logger, L"Item does not exist anymore: " << Utility::formatSyncPath(_source));
        return {ExitCode::DataError, ExitCause::InvalidDestination};
    }

    return ExitCode::Ok;
}

ExitInfo LocalMoveJob::runJob() {
    if (const auto exitInfo = canRun(); !exitInfo) {
        return exitInfo;
    }

    // Make sure we are allowed to propagate the change
    PermissionsHolder _(_dest.parent_path(), _logger);

    std::error_code ec;
    std::filesystem::rename(_source, _dest, ec);

    if (ec.value() != 0) { // We consider this as a permission denied error
        LOGW_WARN(_logger, L"Failed to rename " << Utility::formatSyncPath(_source) << L" to " << Utility::formatSyncPath(_dest)
                                                << L": " << CommonUtility::s2ws(ec.message()) << L" (" << ec.value() << L")");
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    LOGW_INFO(_logger, L"Item with " << Utility::formatSyncPath(_source) << L" moved to " << Utility::formatSyncPath(_dest));
    return ExitCode::Ok;
}

} // namespace KDC
