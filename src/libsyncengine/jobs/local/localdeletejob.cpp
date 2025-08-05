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

#include "localdeletejob.h"
#include "../network/API_v2/getfileinfojob.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"
#include "requests/parameterscache.h"

#include <log4cplus/loggingmacros.h>

#include <utility/utility.h>

#if defined(KD_WINDOWS)
#include <objbase.h>
#endif

namespace KDC {

LocalDeleteJob::Path::Path(const SyncPath &path) :
    _path(path) {};

bool LocalDeleteJob::Path::endsWith(SyncPath &&ending) const {
    if (!_path.empty() && ending.empty()) return false;

    SyncPath path = _path;

    while (!ending.empty() && !path.empty()) {
        if (ending.filename() != path.filename()) return false;
        ending = ending.parent_path();
        path = path.parent_path();
    }

    return !path.empty() || ending.empty();
};

bool LocalDeleteJob::matchRelativePaths(const SyncPath &targetPath, const SyncPath &localRelativePath,
                                        const SyncPath &remoteRelativePath) {
    if (targetPath.empty()) return localRelativePath == remoteRelativePath;

    // Case of an advanced synchronization
    return Path(remoteRelativePath).endsWith(SyncPath(targetPath.filename()) / localRelativePath);
}

LocalDeleteJob::LocalDeleteJob(const SyncPalInfo &syncPalInfo, const SyncPath &relativePath, bool isDehydratedPlaceholder,
                               NodeId remoteId, bool forceToTrash /* = false */) :
    _absolutePath(syncPalInfo.localPath / relativePath),
    _syncInfo(syncPalInfo),
    _relativePath(relativePath),
    _isDehydratedPlaceholder(isDehydratedPlaceholder),
    _remoteNodeId(remoteId),
    _forceToTrash(forceToTrash) {}

LocalDeleteJob::LocalDeleteJob(const SyncPath &absolutePath) :
    _absolutePath(absolutePath) {
    setBypassCheck(true);
}

LocalDeleteJob::~LocalDeleteJob() {}


bool LocalDeleteJob::findRemoteItem(SyncPath &remoteItemPath) const {
    bool found = true;
    remoteItemPath.clear();

    // The item must be absent of remote replica for the job to run
    GetFileInfoJob job(syncInfo().driveDbId, _remoteNodeId);
    job.setWithPath(true);
    job.runSynchronously();

    if (job.hasHttpError()) {
        using namespace Poco::Net;
        if (job.getStatusCode() == HTTPResponse::HTTP_FORBIDDEN || job.getStatusCode() == HTTPResponse::HTTP_NOT_FOUND) {
            found = false;
            LOGW_DEBUG(_logger, L"Item: " << Utility::formatSyncPath(_absolutePath).c_str()
                                          << L" not found on remote replica. This is normal and expected.");
        }
        remoteItemPath = job.path();
    }

    return found;
}

bool LocalDeleteJob::canRun() {
    if (bypassCheck()) {
        return true;
    }

    // The item must exist locally for the job to run
    bool exists = false;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(_absolutePath, exists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_absolutePath, ioError));
        _exitInfo = ExitCode::SystemError;
        return false;
    }
    if (ioError == IoError::AccessDenied) {
        LOGW_WARN(_logger, L"Access denied to " << Utility::formatSyncPath(_absolutePath));
        _exitInfo = {ExitCode::SystemError, ExitCause::FileAccessError};
        return false;
    }

    if (!exists) {
        LOGW_DEBUG(_logger, L"Item does not exist anymore: " << Utility::formatSyncPath(_absolutePath));
        _exitInfo = {ExitCode::DataError, ExitCause::NotFound};
        return false;
    }

    if (_remoteNodeId.empty()) {
        LOG_WARN(_logger, "Remote node ID is empty");
        _exitInfo = {ExitCode::SystemError, ExitCause::FileAccessError};
        return false;
    }

    // Check if the item we want to delete locally has a remote counterpart.

    SyncPath remoteRelativePath;
    const bool remoteItemIsFound = findRemoteItem(remoteRelativePath);
    if (!remoteItemIsFound) return true; // Safe deletion.

    // Check whether the remote item has been moved.
    // If the remote item has been moved into a blacklisted folder, then this Delete job is created and
    // the local item should be deleted.
    // Note: the other remote move operations are not relevant: they generate Move jobs.

    SyncPath normalizedPath;
    if (!Utility::normalizedSyncPath(_relativePath, normalizedPath)) {
        LOGW_WARN(_logger, L"Error in Utility::normalizedSyncPath: " << Utility::formatSyncPath(_relativePath));
        _exitInfo = {ExitCode::SystemError, ExitCause::FileAccessError};
        return false;
    }

    if (matchRelativePaths(syncInfo().targetPath, normalizedPath, remoteRelativePath)) {
        // Item is found at the same path on remote
        LOGW_DEBUG(_logger, L"Item with " << Utility::formatSyncPath(_absolutePath).c_str()
                                          << L" still exists on remote replica. Aborting current sync and restarting.");
        _exitInfo = {ExitCode::DataError, ExitCause::InvalidSnapshot}; // We need to rebuild the remote snapshot from scratch
        return false;
    }

    return true;
}

void LocalDeleteJob::handleTrashMoveOutcome(bool success) {
    if (!success) {
        LOGW_WARN(_logger,
                  L"Failed to move item: " << Utility::formatSyncPath(_absolutePath) << L" to trash. Trying hard delete.");
    } else if (ParametersCache::isExtendedLogEnabled()) {
        LOGW_DEBUG(_logger, L"Item with " << Utility::formatSyncPath(_absolutePath) << L" was moved to trash");
    }
}

bool LocalDeleteJob::moveToTrash() {
    const bool success = Utility::moveItemToTrash(_absolutePath);
    handleTrashMoveOutcome(success);
    return success;
}

void LocalDeleteJob::runJob() {
    if (!canRun()) return;

    const bool tryMoveToTrash =
            (ParametersCache::instance()->parameters().moveToTrash() && !_isDehydratedPlaceholder) || _forceToTrash;

    _exitInfo = ExitCode::Ok;
    if (tryMoveToTrash && moveToTrash()) return;

    LOGW_DEBUG(_logger, L"Delete item with " << Utility::formatSyncPath(_absolutePath));
    std::error_code ec;
    std::filesystem::remove_all(_absolutePath, ec);
    if (ec) {
        LOGW_WARN(_logger, L"Failed to delete item with path " << Utility::formatStdError(_absolutePath, ec));
        if (IoHelper::stdError2ioError(ec) == IoError::AccessDenied) {
            _exitInfo = {ExitCode::SystemError, ExitCause::FileAccessError};
        } else {
            _exitInfo = ExitCode::SystemError;
        }
        return;
    }

    if (ParametersCache::isExtendedLogEnabled()) {
        LOGW_INFO(_logger, L"Item: " << Utility::formatSyncPath(_absolutePath) << L" deleted");
    }
}

} // namespace KDC
