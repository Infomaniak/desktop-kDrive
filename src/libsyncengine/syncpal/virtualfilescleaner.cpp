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

#include "virtualfilescleaner.h"

#include "db/syncdb.h"
#include "requests/exclusiontemplatecache.h"
#include "requests/parameterscache.h"

#include "libcommon/utility/utility.h"

#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"

#include <log4cplus/loggingmacros.h>

namespace KDC {

VirtualFilesCleaner::VirtualFilesCleaner(const SyncPath &path, std::shared_ptr<SyncDb> syncDb, const std::shared_ptr<Vfs> vfs) :
    _logger(Log::instance()->getLogger()),
    _rootPath(path),
    _syncDb(syncDb),
    _vfs(vfs) {}

VirtualFilesCleaner::VirtualFilesCleaner(const SyncPath &path) :
    _logger(Log::instance()->getLogger()),
    _rootPath(path) {}

bool VirtualFilesCleaner::run() {
    // Clear xattr on root path
    assert(_vfs);
    _vfs->clearFileAttributes(_rootPath);
    return removePlaceholdersRecursively(_rootPath);
}

bool VirtualFilesCleaner::removePlaceholdersRecursively(const SyncPath &parentPath) {
    bool directoryIterationException = false;
    IoError ioError = IoError::Success;
    IoHelper::DirectoryIterator dirIt;
    bool endOfDir = false;
    DirectoryEntry entry;

    if (!IoHelper::getRecursiveDirectoryIterator(parentPath, ioError, dirIt)) {
        LOGW_WARN(_logger, L"Error in IoHelper::getRecursiveDirectoryIterator: " << Utility::formatIoError(parentPath, ioError));
        _exitInfo = IoHelper::directoryIteratorExitCode(ioError);
        return false;
    }

    while (dirIt.next(entry, endOfDir, ioError) && !endOfDir) {
        if (!folderCanBeProcessed(entry)) {
            dirIt.disableRecursionPending();
            continue;
        }
        const SyncPath &absolutePath = entry.path();
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_DEBUG(_logger, L"VirtualFilesCleaner: processing item " << Utility::formatSyncPath(absolutePath));
        }

        const SyncPath relativePath = CommonUtility::relativePath(_rootPath, absolutePath);
        if (ExclusionTemplateCache::instance()->isExcluded(relativePath)) {
            LOGW_DEBUG(_logger, L"Ignore " << Utility::formatSyncPath(absolutePath) << L" because it is excluded");
            dirIt.disableRecursionPending();
            continue;
        }

        // Check file system
        VfsStatus vfsStatus;
        assert(_vfs && "Missing VFS.");
        _exitInfo = _vfs->status(absolutePath, vfsStatus);
        if (!_exitInfo) {
            LOGW_WARN(_logger, L"Error in vfsStatus for " << Utility::formatSyncPath(absolutePath) << L": " << _exitInfo);
            return false;
        }

        bool shouldBeKeptOnDisk = false;
        if (auto entryIoError = IoError::Success;
            !checkIfItemShouldStayOnDisk(entry, vfsStatus, shouldBeKeptOnDisk, entryIoError)) {
            LOGW_DEBUG(_logger, L"Error in checkIfItemShouldStayOnDisk " << Utility::formatIoError(entry.path(), entryIoError));
            if (IoHelper::isExpectedError(entryIoError)) continue;

            _exitInfo = IoHelper::directoryIteratorExitCode(entryIoError);
            return false;
        }

        if (shouldBeKeptOnDisk) {
            // Keep file on file system.
            if (ParametersCache::isExtendedLogEnabled()) {
                LOGW_DEBUG(_logger, L"VirtualFilesCleaner: item with " << Utility::formatSyncPath(absolutePath)
                                                                       << L" is either a folder, a hydrated placeholder or a "
                                                                          L"file that is not synchronized yet. Keeping it.");
            }
        } else { // Remove file from file system.
            if (ParametersCache::isExtendedLogEnabled()) {
                LOGW_DEBUG(_logger, L"VirtualFilesCleaner: removing item " << Utility::formatSyncPath(absolutePath)
                                                                           << L" from file system");
            }

            if (auto tmpIoError = IoError::Unknown; !IoHelper::deleteItem(entry.path(), tmpIoError)) {
                LOGW_WARN(_logger, L"Error in IoHelper::deleteItem: " << Utility::formatIoError(absolutePath, tmpIoError));
                _exitInfo = {ExitCode::SystemError, ExitCause::FileAccessError};
                return false;
            }

            // Remove item from db
            if (ParametersCache::isExtendedLogEnabled()) {
                LOGW_DEBUG(_logger,
                           L"VirtualFilesCleaner: removing item " << Utility::formatSyncPath(absolutePath) << L" from DB");
            }

            DbNodeId dbId = -1;
            bool found = false;
            if (!_syncDb->dbId(ReplicaSide::Local, relativePath, dbId, found)) {
                LOG_WARN(_logger, "Error in SyncDb::dbId");
                _exitInfo = {ExitCode::DbError, ExitCause::DbAccessError};
                return false;
            }
            if (!found) {
                // We don't care that it is not found, we wanted to delete it anyway
                continue;
            }

            // Remove node and its children by cascade from DB
            if (!_syncDb->deleteNode(dbId, found)) {
                LOG_WARN(_logger, "Error in SyncDb::deleteNode");
                _exitInfo = {ExitCode::DbError, ExitCause::DbAccessError};
                return false;
            }
            if (!found) {
                // We don't care that it is not found, we wanted to delete it anyway
                continue;
            }
        }

        // Clear xattr
        assert(_vfs);
        _vfs->clearFileAttributes(absolutePath);
    }

    if (ioError != IoError::Success) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in DirectoryIterator: " << Utility::formatIoError(parentPath, ioError));
        directoryIterationException = true;
    }

    const bool success = (ioError == IoError::Success) && endOfDir && !directoryIterationException;
    if (!success) {
        _exitInfo = IoHelper::directoryIteratorExitCode(ioError);
    }

    return success;
}

bool VirtualFilesCleaner::folderCanBeProcessed(const DirectoryEntry &directoryEntry) {
    if (directoryEntry.path().native().length() > CommonUtility::maxPathLength()) {
        LOGW_WARN(_logger, L"Ignore " << Utility::formatSyncPath(directoryEntry.path()) << L" because size > "
                                      << CommonUtility::maxPathLength());
        return false;
    }

    return true;
}

bool VirtualFilesCleaner::checkFileType(const std::filesystem::directory_entry &entry, bool &hasFileType, IoError &ioError) {
    hasFileType = false;
    ioError = IoError::Success;

    std::error_code ec;
    const auto isSymlink = entry.is_symlink(ec);
    if (ec.value()) {
        LOGW_WARN(_logger,
                  L"Error in std::filesystem::directory_entry::is_symlink " << Utility::formatStdError(entry.path(), ec));
        ioError = IoHelper::stdError2ioError(ec);
        return false;
    }

    if (isSymlink) {
        hasFileType = true;
        return true;
    }

    const auto isDirectory = entry.is_directory(ec);
    if (ec.value()) {
        LOGW_WARN(_logger,
                  L"Error in std::filesystem::directory_entry::is_directory " << Utility::formatStdError(entry.path(), ec));
        ioError = IoHelper::stdError2ioError(ec);
        return false;
    }

    hasFileType = !isDirectory;
    return true;
}

bool VirtualFilesCleaner::checkIfItemShouldStayOnDisk(const std::filesystem::directory_entry &entry, const VfsStatus &vfsStatus,
                                                      bool &shouldStayOnDisk, IoError &ioError) {
    shouldStayOnDisk = false;
    ioError = IoError::Success;

    auto hasFileType = false;
    if (auto tmpIoError = IoError::Success; !checkFileType(entry, hasFileType, tmpIoError)) {
        LOGW_WARN(_logger, L"Error in checkFileType " << Utility::formatIoError(entry.path(), tmpIoError));
        ioError = tmpIoError;
        return false;
    }

    if (!hasFileType) {
        shouldStayOnDisk = true; // Folders are kept on disk.
        return true;
    }

    if (vfsStatus.isPlaceholder && vfsStatus.isHydrated) {
        shouldStayOnDisk = true; // Hydrated placeholders are kept on disk.
        return true;
    }

    if (!vfsStatus.isPlaceholder) {
        shouldStayOnDisk = true; // Non-placeholder files are kept on disk.
        return true;
    }

    return true;
}

bool VirtualFilesCleaner::removeDehydratedPlaceholders(std::vector<SyncPath> &failedToRemovePlaceholders) {
    bool directoryIterationException = false;

    IoHelper::DirectoryIterator dirIt;
    if (auto ioError = IoError::Success; !IoHelper::getRecursiveDirectoryIterator(_rootPath, ioError, dirIt)) {
        LOGW_WARN(_logger, L"Error in IoHelper::getRecursiveDirectoryIterator: " << Utility::formatIoError(_rootPath, ioError));
        _exitInfo = IoHelper::directoryIteratorExitCode(ioError);
        return false;
    }

    DirectoryEntry entry;
    bool endOfDir = false;
    IoError iterationIoError = IoError::Success;
    while (dirIt.next(entry, endOfDir, iterationIoError) && !endOfDir) {
        if (!folderCanBeProcessed(entry)) {
            dirIt.disableRecursionPending();
            continue;
        }

        auto hasFileType = false;
        if (auto tmpIoError = IoError::Success; !checkFileType(entry, hasFileType, tmpIoError)) {
            LOGW_WARN(_logger, L"Error in checkFileType " << Utility::formatIoError(entry.path(), tmpIoError));
            if (IoHelper::isExpectedError(tmpIoError)) continue;

            _exitInfo = IoHelper::directoryIteratorExitCode(tmpIoError);
            return false;
        }

        if (!hasFileType) continue;

        auto isDehydrated = false;
        auto ioError = IoError::Success;
        if (const bool success = IoHelper::checkIfFileIsDehydrated(entry.path(), isDehydrated, ioError);
            !success || ioError == IoError::NoSuchFileOrDirectory || ioError == IoError::AccessDenied) {
            LOGW_WARN(_logger, L"Error in IoHelper::checkIfFileIsDehydrated: " << Utility::formatIoError(entry.path(), ioError));
            continue;
        }

        if (!isDehydrated) continue;

        if (auto tmpIoError = IoError::Success; !IoHelper::deleteItem(entry.path(), tmpIoError)) {
            LOGW_WARN(_logger, L"Failed to remove " << Utility::formatIoError(entry.path(), tmpIoError));
            _exitInfo = IoHelper::directoryIteratorExitCode(tmpIoError);

            failedToRemovePlaceholders.push_back(CommonUtility::relativePath(_rootPath, entry.path()));
        }

        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_DEBUG(_logger,
                       L"VFC removeDehydratedPlaceholders: removing item with " << Utility::formatSyncPath(entry.path()));
        }
    }

    if (iterationIoError != IoError::Success) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in DirectoryIterator: " << Utility::formatIoError(_rootPath, iterationIoError));
        directoryIterationException = true;
    }

    const bool iterationSuccess = (iterationIoError == IoError::Success) && endOfDir && !directoryIterationException;
    if (!iterationSuccess) {
        _exitInfo = IoHelper::directoryIteratorExitCode(iterationIoError);
    }

    return iterationSuccess && failedToRemovePlaceholders.empty();
}

} // namespace KDC
