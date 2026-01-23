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

#include "synclocaldeletejob.h"

#include "jobs/network/kDrive_API/getfileinfojob.h"
#include "jobs/network/kDrive_API/itemsexistjob.h"
#include "libcommon/io/permissionsholder.h"
#include "libcommon/io/iohelper.h"
#include "libcommonserver/utility/utility.h"
#include "requests/parameterscache.h"

#include <log4cplus/loggingmacros.h>

#include <utility/utility.h>

#if defined(KD_WINDOWS)
#include <objbase.h>
#endif

namespace KDC {

SyncLocalDeleteJob::Path::Path(const SyncPath &path) :
    _path(path) {};

bool SyncLocalDeleteJob::Path::endsWith(SyncPath &&ending) const {
    if (!_path.empty() && ending.empty()) return false;

    SyncPath path = _path;

    while (!ending.empty() && !path.empty()) {
        if (ending.filename() != path.filename()) return false;
        ending = ending.parent_path();
        path = path.parent_path();
    }

    return !path.empty() || ending.empty();
};

bool SyncLocalDeleteJob::matchRelativePaths(const SyncPath &targetPath, const SyncPath &localRelativePath,
                                            const SyncPath &remoteRelativePath) {
    if (targetPath.empty()) return localRelativePath == remoteRelativePath;

    // Case of an advanced synchronization
    return Path(remoteRelativePath).endsWith(SyncPath(targetPath.filename()) / localRelativePath);
}

SyncLocalDeleteJob::SyncLocalDeleteJob(const std::shared_ptr<SyncPal> syncPal, const SyncPath &relativePath,
                                       bool liteSyncIsEnabled, const NodeId &remoteId, bool forceToTrash /* = false */) :
    GenericLocalDeleteJob(syncPal ? syncPal->localPath() / relativePath : ""),
    _liteSyncIsEnabled(liteSyncIsEnabled),
    _syncPal(syncPal),
    _relativeLocalPath(relativePath),
    _remoteNodeId(remoteId),
    _forceToTrash(forceToTrash) {}

SyncLocalDeleteJob::SyncLocalDeleteJob(const std::shared_ptr<SyncPal> syncPal, const SyncPath &absolutePath) :
    GenericLocalDeleteJob(absolutePath),
    _syncPal(syncPal) {
    setBypassCheck(true);
}

bool SyncLocalDeleteJob::findRemoteItem(SyncPath &remoteItemPath) const {
    bool found = true;
    remoteItemPath.clear();

    // The item must be absent of remote replica for the job to run
    GetFileInfoJob job(_syncPal->driveDbId(), _remoteNodeId);
    job.setWithPath(true);
    job.runSynchronously();
    remoteItemPath = job.path();
    if (job.hasHttpError()) {
        using namespace Poco::Net;
        if (job.getStatusCode() == HTTPResponse::HTTP_FORBIDDEN || job.getStatusCode() == HTTPResponse::HTTP_NOT_FOUND) {
            found = false;
            LOGW_DEBUG(_logger, L"Item: " << CommonUtility::formatSyncPath(absolutePath())
                                          << L" not found on remote replica. This is normal and expected.");
        }
    }

    return found;
}

ExitInfo SyncLocalDeleteJob::checkIfRemoteFileHasBeenMoved() {
    SyncPath remoteRelativePath;
    const bool remoteItemIsFound = findRemoteItem(remoteRelativePath);
    if (!remoteItemIsFound) return ExitCode::Ok; // Safe deletion.

    SyncPath normalizedPath;
    if (!Utility::normalizedSyncPath(_relativeLocalPath, normalizedPath)) {
        LOGW_WARN(_logger, L"Error in Utility::normalizedSyncPath: " << CommonUtility::formatSyncPath(_relativeLocalPath));
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    if (matchRelativePaths(_syncPal->syncInfo().targetPath, normalizedPath, remoteRelativePath)) {
        // Item is found at the same path on remote
        LOGW_DEBUG(_logger, L"Item with " << CommonUtility::formatSyncPath(absolutePath()).c_str()
                                          << L" still exists on remote replica. Aborting current sync and restarting.");
        return {ExitCode::DataError, ExitCause::InvalidSnapshot}; // We need to rebuild the remote snapshot from scratch
    }

    return ExitCode::Ok;
}

ExitInfo SyncLocalDeleteJob::canRun() {
    if (bypassCheck()) {
        return ExitCode::Ok;
    }

    // The item must exist locally for the job to run
    bool exists = false;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(absolutePath(), exists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << CommonUtility::formatIoError(absolutePath(), ioError));
        return ExitCode::SystemError;
    }
    if (ioError == IoError::AccessDenied) {
        LOGW_WARN(_logger, L"Access denied to " << CommonUtility::formatSyncPath(absolutePath()));
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    if (!exists) {
        LOGW_DEBUG(_logger, L"Item does not exist anymore: " << CommonUtility::formatSyncPath(absolutePath()));
        return {ExitCode::DataError, ExitCause::NotFound};
    }

    if (_remoteNodeId.empty()) {
        LOG_WARN(_logger, "Remote node ID is empty");
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    // Check if the item we want to delete locally has a remote counterpart.
    ItemsExistJob existsJob(_syncPal->syncInfo().driveDbId, {_remoteNodeId});
    existsJob.runSynchronously();
    if (!existsJob.exists(_remoteNodeId, ioError) && ioError == IoError::NoSuchFileOrDirectory)
        return ExitCode::Ok; // Safe deletion.

    // Check whether the remote item has been moved.
    // If the remote item has been moved into a blacklisted folder, then this Delete job is created and
    // the local item should be deleted.
    return checkIfRemoteFileHasBeenMoved();
}

namespace {
bool isFileDehydrated(const SyncPath &localPath, log4cplus::Logger logger) {
    bool isDehydrated = false;
    if (auto errorOnHydrationCheck = IoError::Success;
        !IoHelper::checkIfFileIsDehydrated(localPath, isDehydrated, errorOnHydrationCheck) ||
        errorOnHydrationCheck != IoError::Success) {
        LOGW_WARN(logger, L"Error in IoHelper::checkIfFileIsDehydrated: "
                                  << CommonUtility::formatIoError(localPath, errorOnHydrationCheck));
    }

    return isDehydrated;
}
} // namespace

ExitInfo SyncLocalDeleteJob::handleLiteSyncFile(const SyncPath &path) {
    if (isFileDehydrated(path, _logger)) return hardDelete(path);

    return moveToTrashOrHardDeleteIfNeeded(path);
}

ExitInfo SyncLocalDeleteJob::deleteFromDB(const SyncPath &relativeLocalPath) {
    bool found = false;
    DbNodeId dbId = 0;
    if (!_syncPal->syncDb()->dbId(ReplicaSide::Local, relativeLocalPath, dbId, found)) {
        LOGW_ERROR(_logger, L"Failed to get DB ID for " << CommonUtility::formatSyncPath(relativeLocalPath));
        return {ExitCode::DbError, ExitCause::DbAccessError};
    }
    if (!found) {
        LOGW_ERROR(_logger, L"Node DB ID not found for " << CommonUtility::formatSyncPath(relativeLocalPath));
        return {ExitCode::DataError, ExitCause::DbEntryNotFound};
    }

    // Remove item (and children by cascade) from DB
    if (!_syncPal->syncDb()->deleteNode(dbId, found)) {
        LOG_ERROR(_logger, "Failed to remove node " << dbId << " from DB");
        return {ExitCode::DbError, ExitCause::DbAccessError};
    }
    if (!found) {
        LOG_ERROR(_logger, "Node DB ID " << dbId << " not found");
        return {ExitCode::DataError, ExitCause::DbEntryNotFound};
    }

    if (ParametersCache::isExtendedLogEnabled()) {
        LOGW_DEBUG(_logger, L"Item removed from DB: " << CommonUtility::formatSyncPath(relativeLocalPath));
    }

    return ExitCode::Ok;
}

ExitInfo SyncLocalDeleteJob::hardDeleteDehydratedPlaceholders() {
    IoError ioError = IoError::Success;
    IoHelper::DirectoryIterator dir;
    if (!IoHelper::getDirectoryIterator(absolutePath(), true, ioError, dir)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in DirectoryIterator: " << CommonUtility::formatIoError(absolutePath(), ioError));
    }

    DirectoryEntry entry;
    bool endOfDirectory = false;
    while (dir.next(entry, endOfDirectory, ioError) && !endOfDirectory) {
        if ((entry.is_symlink() || entry.is_regular_file()) && isFileDehydrated(entry.path(), _logger)) {
            auto exitInfo = hardDelete(entry.path());
            if (!exitInfo) return exitInfo;

            const auto relativeLocalPath = CommonUtility::relativePath(_syncPal->localPath(), entry.path());
            exitInfo = deleteFromDB(relativeLocalPath);
            if (!exitInfo) return exitInfo;
        }
    }

    if (!endOfDirectory) {
        LOGW_WARN(_logger, L"Error in DirectoryIterator: " << CommonUtility::formatIoError(absolutePath(), ioError));
        return {ExitCode::SystemError, ExitCause::FileOrDirectoryCorrupted};
    }

    return ExitCode::Ok;
}


ExitInfo SyncLocalDeleteJob::moveToTrash() {
    if (!_liteSyncIsEnabled) return moveToTrashOrHardDeleteIfNeeded(absolutePath());

    bool isDirectory = false;
    auto ioErrorCheckIfIsDirectory = IoError::Success;
    if (const bool success = IoHelper::checkIfIsDirectory(absolutePath(), isDirectory, ioErrorCheckIfIsDirectory); !success) {
        LOGW_WARN(_logger, L"Failed to check if path is a directory: "
                                   << CommonUtility::formatIoError(absolutePath(), ioErrorCheckIfIsDirectory));

        return hardDelete(absolutePath());
    }

    if (!isDirectory) return handleLiteSyncFile(absolutePath());

    if (const auto exitInfo = hardDeleteDehydratedPlaceholders(); !exitInfo) return exitInfo;

    return moveToTrashOrHardDeleteIfNeeded(absolutePath());
}

ExitInfo SyncLocalDeleteJob::runJob() {
    if (!_syncPal) {
        LOG_ERROR(_logger, "`_syncPal` is null!");
        return ExitCode::LogicError;
    }
    if (const auto exitInfo = canRun(); !exitInfo) return exitInfo;

    // Make sure we are allowed to propagate the change
    PermissionsHolder permsHolder(absolutePath().parent_path(), _logger);
    PermissionsHolder permsHolder2(absolutePath(), _logger);

    if (const bool tryMoveToTrash = ParametersCache::instance()->parameters().moveToTrash(); tryMoveToTrash || _forceToTrash) {
        return moveToTrash();
    }

    return hardDelete(absolutePath());
}

} // namespace KDC
