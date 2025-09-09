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
#include "jobs/network/kDrive_API/getfileinfojob.h"
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
    _path(path){};

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

LocalDeleteJob::LocalDeleteJob(const SyncPalInfo &syncPalInfo, const SyncPath &relativePath, bool liteIsSyncEnabled,
                               const NodeId &remoteId, bool forceToTrash /* = false */) :
    _absolutePath(syncPalInfo.localPath / relativePath),
    _liteSyncIsEnabled(liteIsSyncEnabled),
    _syncInfo(syncPalInfo),
    _relativePath(relativePath),
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

void LocalDeleteJob::handleTrashMoveOutcome(const bool success, const SyncPath &path) {
    if (!success) {
        LOGW_WARN(_logger, L"Failed to move item: " << Utility::formatSyncPath(path) << L" to trash. Trying hard delete.");
        hardDelete(path);
    } else if (ParametersCache::isExtendedLogEnabled()) {
        LOGW_DEBUG(_logger, L"Item with " << Utility::formatSyncPath(path) << L" was moved to trash");
    }
}

void LocalDeleteJob::hardDelete(const SyncPath &path) {
    LOGW_DEBUG(_logger, L"Try to hard delete item with " << Utility::formatSyncPath(path));

    std::error_code ec;
    (void) std::filesystem::remove_all(path, ec);
    if (ec) {
        LOGW_WARN(_logger, L"Failed to delete item with path " << Utility::formatStdError(path, ec));
        if (IoHelper::stdError2ioError(ec) == IoError::AccessDenied) {
            _exitInfo = {ExitCode::SystemError, ExitCause::FileAccessError};
            return;
        }
        _exitInfo = ExitCode::SystemError;
        return;
    }

    if (ParametersCache::isExtendedLogEnabled()) {
        LOGW_INFO(_logger, L"Item: " << Utility::formatSyncPath(path) << L" deleted.");
    }
}

namespace {
bool isFileDehydrated(const SyncPath &localPath, log4cplus::Logger logger) {
    bool isDehydrated = false;
    if (auto errorOnHydrationCheck = IoError::Success;
        !IoHelper::checkIfFileIsDehydrated(localPath, isDehydrated, errorOnHydrationCheck) ||
        errorOnHydrationCheck != IoError::Success) {
        LOGW_WARN(logger,
                  L"Error in IoHelper::checkIfFileIsDehydrated: " << Utility::formatIoError(localPath, errorOnHydrationCheck));
    }

    return isDehydrated;
}
} // namespace

void LocalDeleteJob::handleLiteSyncFile(const SyncPath &path, bool &moveToTrashOutcome) {
    if (isFileDehydrated(path, _logger)) {
        hardDelete(path);
    } else {
        const bool outcome = Utility::moveItemToTrash(path);
        handleTrashMoveOutcome(outcome, path);
        moveToTrashOutcome = moveToTrashOutcome && outcome;
    }
}

void LocalDeleteJob::hardDeleteDehydratedPlaceholders() {
    IoError ioError = IoError::Success;
    IoHelper::DirectoryIterator dir;
    if (!IoHelper::getDirectoryIterator(_absolutePath, true, ioError, dir)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in DirectoryIterator: " << Utility::formatIoError(_absolutePath, ioError));
    }

    DirectoryEntry entry;
    bool endOfDirectory = false;
    while (dir.next(entry, endOfDirectory, ioError) && !endOfDirectory) {
        if ((entry.is_symlink() || entry.is_regular_file()) && isFileDehydrated(entry.path(), _logger)) {
            hardDelete(entry.path());
        }
    }

    if (!endOfDirectory) {
        LOGW_WARN(_logger, L"Error in DirectoryIterator: " << Utility::formatIoError(_absolutePath, ioError));
    }
}

bool LocalDeleteJob::moveToTrash() {
    if (!_liteSyncIsEnabled) {
        const bool moveToTrashSuccess = Utility::moveItemToTrash(_absolutePath);
        handleTrashMoveOutcome(moveToTrashSuccess, _absolutePath);

        return moveToTrashSuccess;
    }

    bool isDirectory = false;
    auto ioErrorCheckIfIsDirectory = IoError::Success;
    if (const bool success = IoHelper::checkIfIsDirectory(_absolutePath, isDirectory, ioErrorCheckIfIsDirectory); !success) {
        LOGW_WARN(_logger, L"Failed to check if path is a directory: "
                                   << Utility::formatIoError(_absolutePath, ioErrorCheckIfIsDirectory));

        hardDelete(_absolutePath);
        return false;
    }

    bool moveToTrashOutcome = true;
    if (!isDirectory) {
        handleLiteSyncFile(_absolutePath, moveToTrashOutcome);
        return moveToTrashOutcome;
    }

    hardDeleteDehydratedPlaceholders();

    moveToTrashOutcome = Utility::moveItemToTrash(_absolutePath);
    handleTrashMoveOutcome(moveToTrashOutcome, _absolutePath);

    return moveToTrashOutcome;
}

void LocalDeleteJob::runJob() {
    _exitInfo = ExitCode::Ok; //

    if (!canRun()) return;

    if (const bool tryMoveToTrash = (ParametersCache::instance()->parameters().moveToTrash()) || _forceToTrash)
        (void) moveToTrash();
    else
        hardDelete(_absolutePath);
}

} // namespace KDC
