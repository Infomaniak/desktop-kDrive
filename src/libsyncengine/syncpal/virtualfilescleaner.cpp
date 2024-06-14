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

#include "virtualfilescleaner.h"

#include "db/syncdb.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"
#include "requests/exclusiontemplatecache.h"
#include "requests/parameterscache.h"

#include <log4cplus/loggingmacros.h>

namespace KDC {

VirtualFilesCleaner::VirtualFilesCleaner(const SyncPath &path, int syncDbId, std::shared_ptr<SyncDb> syncDb,
                                         bool (*vfsStatus)(int, const SyncPath &, bool &, bool &, bool &, int &),
                                         bool (*vfsClearFileAttributes)(int, const SyncPath &))
    : _logger(Log::instance()->getLogger()),
      _rootPath(path),
      _syncDbId(syncDbId),
      _syncDb(syncDb),
      _vfsStatus(vfsStatus),
      _vfsClearFileAttributes(vfsClearFileAttributes) {}

VirtualFilesCleaner::VirtualFilesCleaner(const SyncPath &path) : _logger(Log::instance()->getLogger()), _rootPath(path) {}

bool VirtualFilesCleaner::run() {
    // Clear xattr on root path
    _vfsClearFileAttributes(_syncDbId, _rootPath);
    return removePlaceholdersRecursivly(_rootPath);
}

bool VirtualFilesCleaner::removePlaceholdersRecursivly(const SyncPath &parentPath) {
    const SyncName rootPathStr = _rootPath.native();
    try {
        std::error_code ec;
        auto dirIt = std::filesystem::recursive_directory_iterator(
            parentPath, std::filesystem::directory_options::skip_permission_denied, ec);
        if (ec) {
            LOGW_WARN(Log::instance()->getLogger(),
                      "Error in removePlaceholdersRecursively: " << Utility::formatStdError(ec).c_str());
            return false;
        }

        for (; dirIt != std::filesystem::recursive_directory_iterator(); ++dirIt) {
#ifdef _WIN32
            // skip_permission_denied doesn't work on Windows
            try {
                bool dummy = dirIt->exists();
                (void)(dummy);
            } catch (std::filesystem::filesystem_error &) {
                dirIt.disable_recursion_pending();
                continue;
            }
#endif

            if (dirIt->path().native().length() > CommonUtility::maxPathLength()) {
                LOGW_DEBUG(Log::instance()->getLogger(), L"Ignore path=" << Path2WStr(dirIt->path()).c_str()
                                                                         << L" because size > "
                                                                         << CommonUtility::maxPathLength());
                dirIt.disable_recursion_pending();
                continue;
            }

            SyncName entryPathStr = dirIt->path().native();
            entryPathStr.erase(0, rootPathStr.length() + 1);  // +1 because of the first “/”

            if (ParametersCache::isExtendedLogEnabled()) {
                LOGW_DEBUG(_logger, L"VirtualFilesCleaner: processing item " << SyncName2WStr(entryPathStr).c_str());
            }

            SyncPath entryPath(entryPathStr);
            SyncPath relativePath = CommonUtility::relativePath(_rootPath, entryPath);
            bool isWarning = false;
            bool isExcluded = false;
            IoError ioError = IoErrorSuccess;
            const bool success =
                ExclusionTemplateCache::instance()->checkIfIsExcluded(_rootPath, relativePath, isWarning, isExcluded, ioError);
            if (!success || ioError != IoErrorSuccess) {
                LOGW_WARN(_logger, L"Error in ExclusionTemplateCache::checkIfIsExcluded: "
                                       << Utility::formatIoError(entryPath, ioError).c_str());
                continue;
            }
            if (isExcluded) {
                LOGW_DEBUG(Log::instance()->getLogger(), L"Ignore path=" << Path2WStr(dirIt->path()).c_str());
                dirIt.disable_recursion_pending();
                continue;
            }

            // Check file system
            bool isPlaceholder = false;
            bool isHydrated = false;
            bool isSyncing = false;
            int progress = 0;
            if (!_vfsStatus(_syncDbId, dirIt->path(), isPlaceholder, isHydrated, isSyncing, progress)) {
                LOGW_WARN(_logger, L"Error in vfsStatus for path=" << Path2WStr(dirIt->path()).c_str());
                _exitCode = ExitCodeSystemError;
                _exitCause = ExitCauseUnknown;
                return false;
            }

            if (!dirIt->is_directory() && isPlaceholder && isHydrated) {
                // Keep this file in file system
                if (ParametersCache::isExtendedLogEnabled()) {
                    LOGW_DEBUG(_logger, L"VirtualFilesCleaner: item " << SyncName2WStr(entryPathStr).c_str()
                                                                      << L" is a hydrated placeholder, keep it");
                }
            } else {
                if (!dirIt->is_directory()) {  // Keep folders
                    // Remove file from file system
                    if (ParametersCache::isExtendedLogEnabled()) {
                        LOGW_DEBUG(_logger, L"VirtualFilesCleaner: removing item " << SyncName2WStr(entryPathStr).c_str()
                                                                                   << L" from file system");
                    }

                    std::error_code ec;
                    if (!std::filesystem::remove(dirIt->path(), ec)) {
                        if (ec.value() != 0) {
                            LOGW_WARN(_logger, L"Failed to remove all " << SyncName2WStr(entryPathStr).c_str() << L": "
                                                                        << Utility::s2ws(ec.message()).c_str() << L" ("
                                                                        << ec.value() << L")");
                            _exitCode = ExitCodeSystemError;
                            _exitCause = ExitCauseFileAccessError;
                            return false;
                        }

                        LOGW_WARN(_logger, L"Failed to remove all " << SyncName2WStr(entryPathStr).c_str());
                        _exitCode = ExitCodeSystemError;
                        _exitCause = ExitCauseFileAccessError;
                        return false;
                    }

                    // Remove item from db
                    if (ParametersCache::isExtendedLogEnabled()) {
                        LOGW_DEBUG(_logger,
                                   L"VirtualFilesCleaner: removing item " << SyncName2WStr(entryPathStr).c_str() << L" from DB");
                    }

                    DbNodeId dbId = -1;
                    bool found = false;
                    if (!_syncDb->dbId(ReplicaSide::Local, entryPath, dbId, found)) {
                        LOG_WARN(_logger, "Error in SyncDb::dbId");
                        _exitCode = ExitCodeDbError;
                        _exitCause = ExitCauseDbAccessError;
                        return false;
                    }
                    if (!found) {
                        // We don't care that it is not found, we wanted to delete it anyway
                        continue;
                    }

                    // Remove node (and childs by cascade) from DB
                    if (!_syncDb->deleteNode(dbId, found)) {
                        LOG_WARN(_logger, "Error in SyncDb::deleteNode");
                        _exitCode = ExitCodeDbError;
                        _exitCause = ExitCauseDbAccessError;
                        return false;
                    }
                    if (!found) {
                        // We don't care that it is not found, we wanted to delete it anyway
                        continue;
                    }
                }
            }

            // Clear xattr
            _vfsClearFileAttributes(_syncDbId, dirIt->path());
        }
    } catch (std::filesystem::filesystem_error &e) {
        LOG_WARN(_logger, "Error caught in VirtualFilesCleaner::removePlaceholdersRecursivly: " << e.code() << " - " << e.what());
        return false;
    } catch (...) {
        LOG_WARN(_logger, "Error caught in VirtualFilesCleaner::removePlaceholdersRecursivly");
        return false;
    }

    return true;
}

bool VirtualFilesCleaner::removeDehydratedPlaceholders(std::vector<SyncPath> &failedToRemovePlaceholders) {
    bool ret = true;
    try {
        std::error_code ec;
        auto dirIt = std::filesystem::recursive_directory_iterator(
            _rootPath, std::filesystem::directory_options::skip_permission_denied, ec);
        if (ec) {
            LOGW_WARN(_logger, "Error in removeDehydratedPlaceholders: " << Utility::formatStdError(ec).c_str());
            return false;
        }
        for (; dirIt != std::filesystem::recursive_directory_iterator(); ++dirIt) {
#ifdef _WIN32
            // skip_permission_denied doesn't work on Windows
            try {
                bool dummy = dirIt->exists();
                (void)(dummy);
            } catch (std::filesystem::filesystem_error &) {
                dirIt.disable_recursion_pending();
                continue;
            }
#endif

            if (dirIt->path().native().length() > CommonUtility::maxPathLength()) {
                LOGW_WARN(_logger, L"Ignore path=" << Path2WStr(dirIt->path()).c_str() << L" because size > "
                                                   << CommonUtility::maxPathLength());
                dirIt.disable_recursion_pending();
                continue;
            }

            if (!dirIt->is_directory()) {
                bool isDehydrated = false;
                IoError ioError = IoErrorSuccess;
                const bool success = IoHelper::checkIfFileIsDehydrated(dirIt->path(), isDehydrated, ioError);
                if (!success || ioError == IoErrorNoSuchFileOrDirectory || ioError == IoErrorAccessDenied) {
                    LOGW_WARN(_logger, L"Error in IoHelper::checkIfFileIsDehydrated: "
                                           << Utility::formatIoError(dirIt->path(), ioError).c_str());
                    continue;
                }

                if (isDehydrated) {
                    SyncPath filePath = dirIt->path();
                    SyncName filePathStr = dirIt->path().native();

                    std::error_code ec;
                    if (!std::filesystem::remove(filePath, ec)) {
                        if (ec.value() != 0) {
                            LOGW_WARN(_logger, L"Failed to remove " << SyncName2WStr(filePathStr).c_str() << L": "
                                                                    << Utility::s2ws(ec.message()).c_str() << L" (" << ec.value()
                                                                    << L")");
                            _exitCode = ExitCodeSystemError;
                            _exitCause = ExitCauseFileAccessError;

                            failedToRemovePlaceholders.push_back(CommonUtility::relativePath(_rootPath, filePath));
                            ret = false;
                        }

                        LOGW_WARN(_logger, L"File does not exist " << SyncName2WStr(filePathStr).c_str());
                    }

                    if (ParametersCache::isExtendedLogEnabled()) {
                        LOGW_DEBUG(_logger,
                                   L"VFC removeDehydratedPlaceholders: removing item " << SyncName2WStr(filePathStr).c_str());
                    }
                }
            }
        }
    } catch (std::filesystem::filesystem_error &e) {
        LOG_WARN(_logger, "Error caught in VirtualFilesCleaner::removeDehydratedPlaceholders: " << e.code() << " - " << e.what());
        ret = false;
    } catch (...) {
        LOG_WARN(_logger, "Error caught in VirtualFilesCleaner::removeDehydratedPlaceholders");
        ret = false;
    }

    return ret;
}

}  // namespace KDC
