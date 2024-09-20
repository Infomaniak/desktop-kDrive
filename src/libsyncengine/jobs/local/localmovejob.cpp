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

#include "localmovejob.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"

#include <log4cplus/loggingmacros.h>

namespace KDC {

LocalMoveJob::LocalMoveJob(const SyncPath &source, const SyncPath &dest) : _source(source), _dest(dest) {}

bool LocalMoveJob::canRun() {
    if (bypassCheck()) {
        return true;
    }

    std::error_code ec;
    IoError ioError = IoError::Success;
    if (!Utility::isEqualInsensitive(_source, _dest)) {
        // Check that we can move the file in destination
        bool exists = false;
        if (!IoHelper::checkIfPathExists(_dest, exists, ioError)) {
            LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_dest, ioError).c_str());
            _exitCode = ExitCode::SystemError;
            _exitCause = ExitCause::FileAccessError;
            return false;
        }

        if (exists) {
            LOGW_DEBUG(_logger, L"Item " << Path2WStr(_dest).c_str() << L" already exist. Aborting current sync and restart.");
            _exitCode = ExitCode::NeedRestart;
            _exitCause = ExitCause::UnexpectedFileSystemEvent;
            return false;
        }
    }

    // Check that the source file still exists.
    bool exists = false;
    if (!IoHelper::checkIfPathExists(_source, exists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_source, ioError).c_str());
        _exitCode = ExitCode::SystemError;
        _exitCause = ExitCause::FileAccessError;
        return false;
    }

    if (!exists) {
        LOGW_DEBUG(_logger,
                   L"Item does not exist anymore. Aborting current sync and restart. - path=" << Path2WStr(_source).c_str());
        _exitCode = ExitCode::NeedRestart;
        _exitCause = ExitCause::UnexpectedFileSystemEvent;
        return false;
    }

    return true;
}

void LocalMoveJob::runJob() {
    if (!canRun()) {
        return;
    }

    std::error_code ec;
    std::filesystem::rename(_source, _dest, ec);

    if (ec.value() != 0) {
        LOGW_WARN(_logger, L"Failed to rename " << Path2WStr(_source).c_str() << L" to " << Path2WStr(_dest).c_str() << L": "
                                                << Utility::s2ws(ec.message()).c_str() << L" (" << ec.value() << L")");
        _exitCode = ExitCode::SystemError;
        _exitCause = ExitCause::FileAccessError;
        return;
    }

    LOGW_INFO(_logger, L"Item " << Path2WStr(_source).c_str() << L" moved to " << Path2WStr(_dest).c_str());
    _exitCode = ExitCode::Ok;
}

} // namespace KDC
