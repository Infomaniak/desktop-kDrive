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
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"

#include <log4cplus/loggingmacros.h>

namespace KDC {

LocalCopyJob::LocalCopyJob(const SyncPath &source, const SyncPath &dest) :
    _source(source),
    _dest(dest) {}

bool LocalCopyJob::canRun() {
    if (bypassCheck()) {
        return true;
    }

    // Check that we can copy the file in destination
    bool exists = false;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(_dest, exists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_dest, ioError).c_str());
        _exitInfo = ExitCode::SystemError;
        return false;
    }
    if (ioError == IoError::AccessDenied) {
        LOGW_WARN(_logger, L"Access denied to " << Path2WStr(_dest).c_str());
        _exitInfo = {ExitCode::SystemError, ExitCause::FileAccessError};
        return false;
    }

    if (exists) {
        LOGW_DEBUG(_logger, L"Item " << Path2WStr(_dest).c_str() << L" already exist. Aborting current sync and restart.");
        _exitInfo = {ExitCode::DataError, ExitCause::UnexpectedFileSystemEvent};
        return false;
    }

    // Check that source file still exists
    if (!IoHelper::checkIfPathExists(_source, exists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_source, ioError).c_str());
        _exitInfo = ExitCode::SystemError;
        return false;
    }
    if (ioError == IoError::AccessDenied) {
        LOGW_WARN(_logger, L"Access denied to " << Path2WStr(_source).c_str());
        _exitInfo = {ExitCode::SystemError, ExitCause::FileAccessError};
        return false;
    }

    if (!exists) {
        LOGW_DEBUG(_logger,
                   L"Item does not exist anymore. Aborting current sync and restart. - path=" << Path2WStr(_source).c_str());
        _exitInfo = {ExitCode::DataError, ExitCause::UnexpectedFileSystemEvent};
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
        _exitInfo = ExitCode::Ok;
    } catch (std::filesystem::filesystem_error &fsError) {
        LOGW_WARN(_logger, L"Failed to copy item " << Path2WStr(_source).c_str() << L" to " << Path2WStr(_dest).c_str() << L": "
                                                   << Utility::s2ws(fsError.what()).c_str() << L" (" << fsError.code().value()
                                                   << L")");
        _exitInfo = ExitCode::SystemError;
        if (IoHelper::stdError2ioError(fsError.code()) == IoError::AccessDenied) {
            _exitInfo.setCause(ExitCause::FileAccessError);
        }
    } catch (...) {
        LOGW_WARN(_logger, L"Failed to copy item " << Path2WStr(_source).c_str() << L" to " << Path2WStr(_dest).c_str()
                                                   << L": Unknown error");
        _exitInfo = ExitCode::SystemError;
    }
}

} // namespace KDC
