/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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
#include "requests/parameterscache.h"

#include "libcommonserver/io/permissionsgiver.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"

#include <log4cplus/loggingmacros.h>

#include <utility/utility.h>

#if defined(KD_WINDOWS)
#include <objbase.h>
#endif

namespace KDC {

bool SyncLocalDeleteJob::matchRelativePaths(const SyncPath &remoteTargetPath, const SyncPath &localRelativePath,
                                            const SyncPath &remoteRelativePath) {
    // Standard synchronization case: the target path is empty and the local and remote relative paths should match.
    if (remoteTargetPath.empty()) return remoteRelativePath == localRelativePath;

    // Case of an advanced synchronization.
    // The remote target path is of the form /path/to/target_folder where to root is the remote drive root.
    // We remove the "/" at the beginning to compare it with a reconstructed relative path.
    const auto relativeRemoteTargetPath = std::filesystem::relative(remoteTargetPath, remoteTargetPath.root_path());

    if (relativeRemoteTargetPath.begin() == relativeRemoteTargetPath.end() || relativeRemoteTargetPath == SyncPath{"."})
        return remoteRelativePath == localRelativePath;

    return remoteRelativePath == relativeRemoteTargetPath / localRelativePath;
}

SyncLocalDeleteJob::SyncLocalDeleteJob(const std::shared_ptr<SyncPal> syncPal, const SyncPath &relativePath,
                                       const bool liteSyncIsEnabled, RemoteNodeId remoteNodeId,
                                       ForceToTrash forceToTrash /* = ForceToTrash::No */) :
    GenericLocalDeleteJob(syncPal ? syncPal->localPath() / relativePath : ""),
    _liteSyncIsEnabled(liteSyncIsEnabled),
    _syncPal(syncPal),
    _relativeLocalPath(relativePath),
    _remoteNodeId(std::move(remoteNodeId)),
    _forceToTrash(forceToTrash == ForceToTrash::Yes) {}

SyncLocalDeleteJob::SyncLocalDeleteJob(const std::shared_ptr<SyncPal> syncPal, const SyncPath &absoluteLocalPath) :
    GenericLocalDeleteJob(absoluteLocalPath),
    _syncPal(syncPal) {
    setBypassCheck(true);
}

bool SyncLocalDeleteJob::findRemoteItemRelativePath(SyncPath &remoteItemPath) const {
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
            LOGW_DEBUG(_logger, L"Item: " << Utility::formatSyncPath(absoluteLocalPath())
                                          << L" not found on remote replica. This is normal and expected.");
        }
    }

    return found;
}

ExitInfo SyncLocalDeleteJob::checkIfRemoteFileHasBeenMoved() {
    SyncPath remoteRelativePath;

    if (const bool remoteItemIsFound = findRemoteItemRelativePath(remoteRelativePath); !remoteItemIsFound) {
        return ExitCode::Ok; // Safe deletion.
    }

    SyncPath normalizedRelativeLocalPath;
    if (!Utility::normalizedSyncPath(_relativeLocalPath, normalizedRelativeLocalPath)) {
        LOGW_WARN(_logger, L"Error in Utility::normalizedSyncPath: " << Utility::formatSyncPath(_relativeLocalPath));

        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    if (matchRelativePaths(_syncPal->syncInfo().targetPath, normalizedRelativeLocalPath, remoteRelativePath)) {
        // The item was found at the same path on remote drive.
        LOGW_DEBUG(_logger, L"Item with " << Utility::formatSyncPath(absoluteLocalPath()).c_str()
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
    if (!IoHelper::checkIfPathExists(absoluteLocalPath(), exists, ioError, IoHelper::PathCheckOption::Insensitive)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(absoluteLocalPath(), ioError));
        return ExitCode::SystemError;
    }
    if (ioError == IoError::AccessDenied) {
        LOGW_WARN(_logger, L"Access denied to " << Utility::formatSyncPath(absoluteLocalPath()));
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    if (!exists) {
        LOGW_DEBUG(_logger, L"Item does not exist anymore: " << Utility::formatSyncPath(absoluteLocalPath()));
        return {ExitCode::DataError, ExitCause::NotFound};
    }

    if (_remoteNodeId.empty()) {
        LOG_WARN(_logger, "Remote node ID is empty");
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    // Check if the item we want to delete locally has a remote counterpart.
    ItemsExistJob existsJob(_syncPal->syncInfo().driveDbId, {_remoteNodeId});
    if (ExitInfo exitInfo = existsJob.runSynchronously(); !exitInfo) {
        LOG_WARN(_logger, "Error in ItemsExistJob: " << exitInfo);

        return exitInfo;
    }

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
        LOGW_WARN(logger,
                  L"Error in IoHelper::checkIfFileIsDehydrated: " << Utility::formatIoError(localPath, errorOnHydrationCheck));
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
        LOGW_ERROR(_logger, L"Failed to get DB ID for " << Utility::formatSyncPath(relativeLocalPath));
        return {ExitCode::DbError, ExitCause::DbAccessError};
    }
    if (!found) {
        LOGW_ERROR(_logger, L"Node DB ID not found for " << Utility::formatSyncPath(relativeLocalPath));
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
        LOGW_DEBUG(_logger, L"Item removed from DB: " << Utility::formatSyncPath(relativeLocalPath));
    }

    return ExitCode::Ok;
}

ExitInfo SyncLocalDeleteJob::hardDeleteDehydratedPlaceholders() {
    IoError ioError = IoError::Success;
    IoHelper::DirectoryIterator dir;
    if (!IoHelper::getDirectoryIterator(absoluteLocalPath(), true, ioError, dir)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in DirectoryIterator: " << Utility::formatIoError(absoluteLocalPath(), ioError));
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
        LOGW_WARN(_logger, L"Error in DirectoryIterator: " << Utility::formatIoError(absoluteLocalPath(), ioError));
        return {ExitCode::SystemError, ExitCause::FileOrDirectoryCorrupted};
    }

    return ExitCode::Ok;
}


ExitInfo SyncLocalDeleteJob::moveToTrash() {
    if (!_liteSyncIsEnabled) return moveToTrashOrHardDeleteIfNeeded(absoluteLocalPath());

    bool isDirectory = false;
    auto ioErrorCheckIfIsDirectory = IoError::Success;
    if (const bool success = IoHelper::checkIfIsDirectory(absoluteLocalPath(), isDirectory, ioErrorCheckIfIsDirectory);
        !success) {
        LOGW_WARN(_logger, L"Failed to check if path is a directory: "
                                   << Utility::formatIoError(absoluteLocalPath(), ioErrorCheckIfIsDirectory));

        return hardDelete(absoluteLocalPath());
    }

    if (!isDirectory) return handleLiteSyncFile(absoluteLocalPath());

    if (const auto exitInfo = hardDeleteDehydratedPlaceholders(); !exitInfo) return exitInfo;

    return moveToTrashOrHardDeleteIfNeeded(absoluteLocalPath());
}

ExitInfo SyncLocalDeleteJob::runJob() {
    if (!_syncPal) {
        LOG_ERROR(_logger, "`_syncPal` is null!");
        return ExitCode::LogicError;
    }
    if (const auto exitInfo = canRun(); !exitInfo) return exitInfo;

    // Make sure we are allowed to propagate the change
    PermissionsGiver permsGiver(absoluteLocalPath().parent_path(), _logger);
    PermissionsGiver permsGiver2(absoluteLocalPath(), _logger);

    if (const bool tryMoveToTrash = ParametersCache::instance()->parameters().moveToTrash(); tryMoveToTrash || _forceToTrash) {
        return moveToTrash();
    }

    return hardDelete(absoluteLocalPath());
}

} // namespace KDC
