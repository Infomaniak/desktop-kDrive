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

#include "localcopyjob.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"

#include <log4cplus/loggingmacros.h>

namespace KDC {

LocalCopyJob::LocalCopyJob(const SyncPath &source, const SyncPath &dest) : _source(source), _dest(dest) {}

bool LocalCopyJob::canRun() {
    if (bypassCheck()) {
        return true;
    }

    // Check that we can copy the file in destination
    bool exists = false;
    IoError ioError = IoErrorSuccess;
    if (!IoHelper::checkIfPathExists(_dest, exists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_dest, ioError).c_str());
        _exitCode = ExitCodeSystemError;
        _exitCause = ExitCauseFileAccessError;
        return false;
    }

    if (exists) {
        LOGW_DEBUG(_logger, L"Item " << Path2WStr(_dest).c_str() << L" already exist. Aborting current sync and restart.");

        _exitCode = ExitCodeNeedRestart;
        _exitCause = ExitCauseUnexpectedFileSystemEvent;
        return false;
    }

    // Check that source file still exists
    if (!IoHelper::checkIfPathExists(_source, exists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_source, ioError).c_str());
        _exitCode = ExitCodeSystemError;
        _exitCause = ExitCauseFileAccessError;
        return false;
    }

    if (!exists) {
        LOGW_DEBUG(_logger,
                   L"Item does not exist anymore. Aborting current sync and restart. - path=" << Path2WStr(_source).c_str());
        _exitCode = ExitCodeNeedRestart;
        _exitCause = ExitCauseUnexpectedFileSystemEvent;
        return false;
    }

    return true;
}

void LocalCopyJob::runJob() {
    if (!canRun()) {
        return;
    }

    try {
        std::filesystem::copy(_source, _dest);
        LOGW_INFO(_logger, L"Item " << Path2WStr(_source).c_str() << L" copied to " << Path2WStr(_dest).c_str());
        _exitCode = ExitCodeOk;
    } catch (std::filesystem::filesystem_error &fsError) {
        LOGW_WARN(_logger, L"Failed to copy item " << Path2WStr(_source).c_str() << L" to " << Path2WStr(_dest).c_str() << L": "
                                                   << Utility::s2ws(fsError.what()).c_str() << L" (" << fsError.code() << L")");
        _exitCode = ExitCodeSystemError;
        _exitCause = ExitCauseFileAccessError;
    } catch (...) {
        LOGW_WARN(_logger, L"Failed to copy item " << Path2WStr(_source).c_str() << L" to " << Path2WStr(_dest).c_str()
                                                   << L": Unknown error");
        _exitCode = ExitCodeSystemError;
        _exitCause = ExitCauseFileAccessError;
    }
}

}  // namespace KDC
