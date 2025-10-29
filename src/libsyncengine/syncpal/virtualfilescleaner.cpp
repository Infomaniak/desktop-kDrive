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

#include "virtualfilescleaner.h"

#include "db/syncdb.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"
#include "requests/exclusiontemplatecache.h"
#include "requests/parameterscache.h"

#include <log4cplus/loggingmacros.h>

namespace KDC {

VirtualFilesCleaner::VirtualFilesCleaner(const SyncPath &path, std::shared_ptr<SyncDb> syncDb, const std::shared_ptr<Vfs> &vfs) :
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

namespace {
bool hasFileType(const std::filesystem::directory_entry &entry) {
    return entry.is_symlink() || (!entry.is_directory());
}
} // namespace

bool VirtualFilesCleaner::removePlaceholdersRecursively(const SyncPath &parentPath) {
    const SyncName rootPathStr = _rootPath.native();
    bool directoryIterationException = false;

    IoError ioError = IoError::Success;
    IoHelper::DirectoryIterator dirIt(parentPath, true, ioError);
    bool endOfDir = false;
    DirectoryEntry entry;

    try {
        if (!recursiveDirectoryIterator(parentPath, dirIt)) {
            LOGW_WARN(_logger, L"Error in VirtualFilesCleaner::recursiveDirectoryIterator");
            return false;
        }

        while (dirIt.next(entry, endOfDir, ioError) && !endOfDir && ioError == IoError::Success) {
            if (!folderCanBeProcessed(entry)) {
                dirIt.disableRecursionPending();
                continue;
            }
            const SyncPath &absolutePath = entry.path();
            SyncPath relativePath = CommonUtility::relativePath(_rootPath, absolutePath);

            if (ParametersCache::isExtendedLogEnabled()) {
                LOGW_DEBUG(_logger, L"VirtualFilesCleaner: processing item " << Utility::formatSyncPath(absolutePath));
            }
            if (ExclusionTemplateCache::instance()->isExcluded(relativePath)) {
                LOGW_DEBUG(_logger, L"Ignore " << Utility::formatSyncPath(absolutePath) << L" because it is excluded");
                dirIt.disableRecursionPending();
                continue;
            }

            // Check file system
            VfsStatus vfsStatus;
            assert(_vfs);
            if (ExitInfo exitInfo = _vfs->status(absolutePath, vfsStatus); !exitInfo) {
                LOGW_WARN(_logger, L"Error in vfsStatus for " << Utility::formatSyncPath(absolutePath) << L": " << exitInfo);
                _exitCode = exitInfo.code();
                _exitCause = exitInfo.cause();
                return false;
            }

            if (const bool dirItemHasFileType = hasFileType(entry);
                dirItemHasFileType && vfsStatus.isPlaceholder && vfsStatus.isHydrated) {
                // Keep this file in file system
                if (ParametersCache::isExtendedLogEnabled()) {
                    LOGW_DEBUG(_logger, L"VirtualFilesCleaner: item " << Utility::formatSyncPath(absolutePath)
                                                                      << L" is a hydrated placeholder, keep it");
                }
            } else if (dirItemHasFileType) { // Keep folders
                // Remove file from file system
                if (ParametersCache::isExtendedLogEnabled()) {
                    LOGW_DEBUG(_logger, L"VirtualFilesCleaner: removing item " << Utility::formatSyncPath(absolutePath)
                                                                               << L" from file system");
                }

                if (std::error_code removeEc; !std::filesystem::remove(entry.path(), removeEc)) {
                    if (removeEc) {
                        LOGW_WARN(_logger, L"Failed to remove all for " << Utility::formatStdError(absolutePath, removeEc));
                        _exitCode = ExitCode::SystemError;
                        _exitCause = ExitCause::FileAccessError;
                        return false;
                    }

                    LOGW_WARN(_logger, L"Failed to remove all " << Utility::formatSyncPath(absolutePath));
                    _exitCode = ExitCode::SystemError;
                    _exitCause = ExitCause::FileAccessError;
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
                    _exitCode = ExitCode::DbError;
                    _exitCause = ExitCause::DbAccessError;
                    return false;
                }
                if (!found) {
                    // We don't care that it is not found, we wanted to delete it anyway
                    continue;
                }

                // Remove node and its children by cascade from DB
                if (!_syncDb->deleteNode(dbId, found)) {
                    LOG_WARN(_logger, "Error in SyncDb::deleteNode");
                    _exitCode = ExitCode::DbError;
                    _exitCause = ExitCause::DbAccessError;
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
    } catch (std::filesystem::filesystem_error &e) {
        LOG_WARN(_logger, "Exception caught in VirtualFilesCleaner::removePlaceholdersRecursively: code=" << e.code() << " error="
                                                                                                          << e.what());
        directoryIterationException = true;
    } catch (...) {
        LOG_WARN(_logger, "Exception caught in VirtualFilesCleaner::removePlaceholdersRecursively");
        directoryIterationException = true;
    }

    if (ioError != IoError::Success) {
        LOGW_WARN(_logger, L"Error in IoHelper::DirectoryIterator: " << Utility::formatIoError(entry.path(), ioError));
    }

    if (!endOfDir) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in DirectoryIterator: " << Utility::formatIoError(entry.path(), ioError));
    }

    const bool success = (ioError == IoError::Success) && endOfDir && !directoryIterationException;
    if (!success) {
        _exitCode = ExitCode::SystemError;
        _exitCause = ExitCause::FileOrDirectoryCorrupted;
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

bool VirtualFilesCleaner::recursiveDirectoryIterator(const SyncPath &path, IoHelper::DirectoryIterator &dirIt) {
    IoError ioError = IoError::Success;
    dirIt = IoHelper::DirectoryIterator(path, true, ioError);
    if (ioError != IoError::Success) {
        LOGW_WARN(_logger, L"Error in IoHelper::DirectoryIterator: " << Utility::formatIoError(path, ioError));
        return false;
    }

    return true;
}

bool VirtualFilesCleaner::removeDehydratedPlaceholders(std::vector<SyncPath> &failedToRemovePlaceholders) {
    bool ret = true;
    try {
        std::filesystem::recursive_directory_iterator dirIt;
        if (!recursiveDirectoryIterator(_rootPath, dirIt)) {
            LOGW_WARN(_logger, L"Error in VirtualFilesCleaner::recursiveDirectoryIterator");
            return false;
        }
        for (; dirIt != std::filesystem::recursive_directory_iterator(); ++dirIt) {
            if (!folderCanBeProcessed(dirIt)) {
                dirIt.disable_recursion_pending();
                continue;
            }

            if (hasFileType(*dirIt)) {
                bool isDehydrated = false;
                IoError ioError = IoError::Success;
                const bool success = IoHelper::checkIfFileIsDehydrated(dirIt->path(), isDehydrated, ioError);
                if (!success || ioError == IoError::NoSuchFileOrDirectory || ioError == IoError::AccessDenied) {
                    LOGW_WARN(_logger,
                              L"Error in IoHelper::checkIfFileIsDehydrated: " << Utility::formatIoError(dirIt->path(), ioError));
                    continue;
                }

                if (isDehydrated) {
                    const SyncPath filePath = dirIt->path();
                    if (std::error_code ec; !std::filesystem::remove(filePath, ec)) {
                        if (ec) {
                            LOGW_WARN(_logger, L"Failed to remove " << Utility::formatStdError(filePath, ec));
                            _exitCode = ExitCode::SystemError;
                            _exitCause = ExitCause::FileAccessError;

                            failedToRemovePlaceholders.push_back(CommonUtility::relativePath(_rootPath, filePath));
                            ret = false;
                        }

                        LOGW_WARN(_logger, L"File does not exist: " << Utility::formatSyncPath(filePath));
                    }

                    if (ParametersCache::isExtendedLogEnabled()) {
                        LOGW_DEBUG(_logger,
                                   L"VFC removeDehydratedPlaceholders: removing item with " << Utility::formatSyncPath(filePath));
                    }
                }
            }
        }
    } catch (std::filesystem::filesystem_error &e) {
        LOG_WARN(_logger, "Exception caught in VirtualFilesCleaner::removeDehydratedPlaceholders: code=" << e.code() << " error="
                                                                                                         << e.what());
        ret = false;
    } catch (...) {
        LOG_WARN(_logger, "Exception caught in VirtualFilesCleaner::removeDehydratedPlaceholders");
        ret = false;
    }

    return ret;
}

} // namespace KDC
