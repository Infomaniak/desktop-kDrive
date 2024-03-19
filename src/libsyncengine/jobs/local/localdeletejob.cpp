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

#include "localdeletejob.h"
#include "jobs/network/getfileinfojob.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"
#include "requests/parameterscache.h"

#include <log4cplus/loggingmacros.h>

#include <utility/utility.h>

#ifdef _WIN32
#include "objbase.h"
#endif

namespace KDC {

LocalDeleteJob::LocalDeleteJob(int driveDbId, const SyncPath &syncPath, const SyncPath &relativePath,
                               bool isDehydratedPlaceholder, NodeId remoteId, bool forceToTrash /* = false */)
    : _driveDbId(driveDbId),
      _syncPath(syncPath),
      _relativePath(relativePath),
      _absolutePath(syncPath / relativePath),
      _isDehydratedPlaceholder(isDehydratedPlaceholder),
      _remoteNodeId(remoteId),
      _forceToTrash(forceToTrash) {
#ifdef _WIN32
    CoInitialize(NULL);
#endif
}

LocalDeleteJob::LocalDeleteJob(const SyncPath &absolutePath) : _absolutePath(absolutePath) {
#ifdef _WIN32
    CoInitialize(NULL);
#endif
    setBypassCheck(true);
}

LocalDeleteJob::~LocalDeleteJob() {
#ifdef _WIN32
    CoUninitialize();
#endif
}

bool LocalDeleteJob::canRun() {
    if (bypassCheck()) {
        return true;
    }

    // The item must exist locally for the job to run
    bool exists;
    IoError ioError = IoErrorSuccess;
    if (!IoHelper::checkIfPathExists(_absolutePath, exists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_absolutePath, ioError).c_str());
        _exitCode = ExitCodeSystemError;
        _exitCause = ExitCauseFileAccessError;
        return false;
    }

    if (!exists) {
        LOGW_DEBUG(_logger, L"Item does not exist anymore. Aborting current sync and restart. - path="
                                << Path2WStr(_absolutePath).c_str());
        _exitCode = ExitCodeNeedRestart;
        _exitCause = ExitCauseUnexpectedFileSystemEvent;
        return false;
    }

    if (_remoteNodeId.empty()) {
        LOG_WARN(_logger, "Remote node ID is empty");
        _exitCode = ExitCodeSystemError;
        _exitCause = ExitCauseFileAccessError;
        return false;
    }

    // The item must be absent of remote replica for the job to run
    GetFileInfoJob job(_driveDbId, _remoteNodeId);
    job.setWithPath(true);
    job.runSynchronously();
    bool itemFound = true;
    if (job.hasHttpError()) {
        if (job.getStatusCode() == Poco::Net::HTTPResponse::HTTP_FORBIDDEN ||
            job.getStatusCode() == Poco::Net::HTTPResponse::HTTP_NOT_FOUND) {
            itemFound = false;
            LOGW_DEBUG(_logger, L"Item " << Path2WStr(_absolutePath).c_str()
                                         << L" not found on remote replica. This is normal and expected.");
        }
    }

    if (itemFound) {
        // Verify that that the item has not moved
        // For item moved in a blacklisted folder, we need to delete them even if they still exist on remote replica
        if (_relativePath == job.path().relative_path()) {
            // Item is found at the same path on remote
            LOGW_DEBUG(_logger, L"Item " << Path2WStr(_absolutePath).c_str()
                                         << L" still exist on remote replica. Aborting current sync and restart.");
            _exitCode = ExitCodeDataError;  // We need to rebuild the remote snapshot from scratch
            _exitCause = ExitCauseInvalidSnapshot;
            return false;
        }
    }

    return true;
}

void LocalDeleteJob::runJob() {
    if (!canRun()) {
        return;
    }

    if ((ParametersCache::instance()->parameters().moveToTrash() && !_isDehydratedPlaceholder) || _forceToTrash) {
        bool success = Utility::moveItemToTrash(_absolutePath);
        _exitCode = ExitCodeOk;
        if (!success) {
            LOGW_WARN(_logger, L"Failed to move item " << Path2WStr(_absolutePath).c_str() << L" to trash");
            _exitCode = ExitCodeSystemError;
            _exitCause = ExitCauseMoveToTrashFailed;
            return;
        }
        if (ParametersCache::instance()->parameters().extendedLog()) {
            LOGW_DEBUG(_logger, L"Item " << Path2WStr(_absolutePath).c_str() << L" was moved to trash");
        }
    } else {
        LOGW_DEBUG(_logger, L"Delete item " << Path2WStr(_absolutePath).c_str());
        std::error_code ec;
        if (std::filesystem::remove_all(_absolutePath, ec) > 0) {
            LOGW_INFO(_logger, L"Item " << Path2WStr(_absolutePath).c_str() << L" deleted");
            _exitCode = ExitCodeOk;
        } else {
            LOGW_WARN(_logger, L"Failed to delete item " << Path2WStr(_absolutePath).c_str() << L": "
                                                         << Utility::s2ws(ec.message()).c_str() << L" (" << ec.value() << L")");
            _exitCode = ExitCodeSystemError;
            _exitCause = ExitCauseFileAccessError;
            return;
        }
    }
}

}  // namespace KDC
