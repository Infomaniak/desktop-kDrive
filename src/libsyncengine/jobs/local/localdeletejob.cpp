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
#include "libcommonserver/io/permissionsholder.h"
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

LocalDeleteJob::LocalDeleteJob(const std::shared_ptr<SyncPal> syncPal, const SyncPath &relativePath, bool liteSyncIsEnabled,
                               const NodeId &remoteId, bool forceToTrash /* = false */) :
    _absolutePath(syncPal ? syncPal->localPath() / relativePath : ""),
    _liteSyncIsEnabled(liteSyncIsEnabled),
    _syncPal(syncPal),
    _relativeLocalPath(relativePath),
    _remoteNodeId(remoteId),
    _forceToTrash(forceToTrash) {}

LocalDeleteJob::LocalDeleteJob(const SyncPath &absolutePath, const std::shared_ptr<SyncPal> syncPal /*= nullptr*/) :
    _absolutePath(absolutePath),
    _syncPal(syncPal) {
    setBypassCheck(true);
}

LocalDeleteJob::~LocalDeleteJob() {}


bool LocalDeleteJob::findRemoteItem(SyncPath &remoteItemPath) const {
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
            LOGW_DEBUG(_logger, L"Item: " << Utility::formatSyncPath(_absolutePath).c_str()
                                          << L" not found on remote replica. This is normal and expected.");
        }
    }

    return found;
}

ExitInfo LocalDeleteJob::canRun() {
    if (bypassCheck()) {
        return ExitCode::Ok;
    }

    // The item must exist locally for the job to run
    bool exists = false;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(_absolutePath, exists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_absolutePath, ioError));
        return ExitCode::SystemError;
    }
    if (ioError == IoError::AccessDenied) {
        LOGW_WARN(_logger, L"Access denied to " << Utility::formatSyncPath(_absolutePath));
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    if (!exists) {
        LOGW_DEBUG(_logger, L"Item does not exist anymore: " << Utility::formatSyncPath(_absolutePath));
        return {ExitCode::DataError, ExitCause::NotFound};
    }

    if (_remoteNodeId.empty()) {
        LOG_WARN(_logger, "Remote node ID is empty");
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    // Check if the item we want to delete locally has a remote counterpart.

    SyncPath remoteRelativePath;
    const bool remoteItemIsFound = findRemoteItem(remoteRelativePath);
    if (!remoteItemIsFound) return ExitCode::Ok; // Safe deletion.

    // Check whether the remote item has been moved.
    // If the remote item has been moved into a blacklisted folder, then this Delete job is created and
    // the local item should be deleted.
    // Note: the other remote move operations are not relevant: they generate Move jobs.

    SyncPath normalizedPath;
    if (!Utility::normalizedSyncPath(_relativeLocalPath, normalizedPath)) {
        LOGW_WARN(_logger, L"Error in Utility::normalizedSyncPath: " << Utility::formatSyncPath(_relativeLocalPath));
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    if (matchRelativePaths(_syncPal->syncInfo().targetPath, normalizedPath, remoteRelativePath)) {
        // Item is found at the same path on remote
        LOGW_DEBUG(_logger, L"Item with " << Utility::formatSyncPath(_absolutePath).c_str()
                                          << L" still exists on remote replica. Aborting current sync and restarting.");
        return {ExitCode::DataError, ExitCause::InvalidSnapshot}; // We need to rebuild the remote snapshot from scratch
    }

    return ExitCode::Ok;
}

ExitInfo LocalDeleteJob::hardDelete(const SyncPath &path) {
    LOGW_DEBUG(_logger, L"Try to hard delete item with " << Utility::formatSyncPath(path));

    std::error_code ec;
    (void) std::filesystem::remove_all(path, ec);
    if (ec) {
        LOGW_WARN(_logger, L"Failed to delete item with " << Utility::formatStdError(_absolutePath, ec));
        if (IoHelper::stdError2ioError(ec) == IoError::AccessDenied) {
            return {ExitCode::SystemError, ExitCause::FileAccessError};
        }

        return ExitCode::SystemError;
    }

    if (ParametersCache::isExtendedLogEnabled()) {
        LOGW_INFO(_logger, L"Item: " << Utility::formatSyncPath(path) << L" deleted.");
    }

    return ExitCode::Ok;
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

ExitInfo LocalDeleteJob::handleLiteSyncFile(const SyncPath &path) {
    if (isFileDehydrated(path, _logger)) return hardDelete(path);

    return moveToTrashOrHardDeleteIfNeeded(path);
}

ExitInfo LocalDeleteJob::deleteFromDB(const SyncPath &relativeLocalPath) {
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

ExitInfo LocalDeleteJob::hardDeleteDehydratedPlaceholders() {
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
            auto exitInfo = hardDelete(entry.path());
            if (!exitInfo) return exitInfo;

            const auto relativeLocalPath = CommonUtility::relativePath(_syncPal->localPath(), entry.path());
            exitInfo = deleteFromDB(relativeLocalPath);
            if (!exitInfo) return exitInfo;
        }
    }

    if (!endOfDirectory) {
        LOGW_WARN(_logger, L"Error in DirectoryIterator: " << Utility::formatIoError(_absolutePath, ioError));
        return {ExitCode::SystemError, ExitCause::FileOrDirectoryCorrupted};
    }

    return ExitCode::Ok;
}

ExitInfo LocalDeleteJob::moveToTrashOrHardDeleteIfNeeded(const SyncPath &path) {
    if (const bool moveToTrashSuccess = IoHelper::moveItemToTrash(path); !moveToTrashSuccess) {
        LOGW_WARN(_logger, L"Failed to move item: " << Utility::formatSyncPath(path) << L" to trash. Trying hard delete.");
        return hardDelete(path);
    }

    if (ParametersCache::isExtendedLogEnabled()) {
        LOGW_DEBUG(_logger, L"Item with " << Utility::formatSyncPath(path) << L" was moved to trash.");
    }

    return ExitCode::Ok;
}

ExitInfo LocalDeleteJob::moveToTrash() {
    if (!_liteSyncIsEnabled) return moveToTrashOrHardDeleteIfNeeded(_absolutePath);

    bool isDirectory = false;
    auto ioErrorCheckIfIsDirectory = IoError::Success;
    if (const bool success = IoHelper::checkIfIsDirectory(_absolutePath, isDirectory, ioErrorCheckIfIsDirectory); !success) {
        LOGW_WARN(_logger, L"Failed to check if path is a directory: "
                                   << Utility::formatIoError(_absolutePath, ioErrorCheckIfIsDirectory));

        return hardDelete(_absolutePath);
    }

    if (!isDirectory) return handleLiteSyncFile(_absolutePath);

    if (const auto exitInfo = hardDeleteDehydratedPlaceholders(); !exitInfo) return exitInfo;

    return moveToTrashOrHardDeleteIfNeeded(_absolutePath);
}

ExitInfo LocalDeleteJob::runJob() {
    if (const auto exitInfo = canRun(); !exitInfo) return exitInfo;

    // Make sure we are allowed to propagate the change
    PermissionsHolder permsHolder(_absolutePath.parent_path(), _logger);
    PermissionsHolder permsHolder2(_absolutePath, _logger);

    if (const bool tryMoveToTrash = ParametersCache::instance()->parameters().moveToTrash();
        _syncPal && (tryMoveToTrash || _forceToTrash))
        return moveToTrash();

    return hardDelete(_absolutePath);
}

} // namespace KDC
