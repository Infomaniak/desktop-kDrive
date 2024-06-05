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

#include "excludelistpropagator.h"
#include "requests/exclusiontemplatecache.h"
#include "requests/parameterscache.h"
#include "libcommon/utility/utility.h"

namespace KDC {

ExcludeListPropagator::ExcludeListPropagator(std::shared_ptr<SyncPal> syncPal) : _syncPal(syncPal) {
    LOG_DEBUG(Log::instance()->getLogger(), "ExcludeListPropagator created " << jobId());
}

ExcludeListPropagator::~ExcludeListPropagator() {
    LOG_DEBUG(Log::instance()->getLogger(), "ExcludeListPropagator destroyed " << jobId());
}

void ExcludeListPropagator::runJob() {
    LOG_SYNCPAL_DEBUG(Log::instance()->getLogger(), "ExcludeListPropagator started " << jobId());

    ExitCode exitCode = checkItems();
    if (exitCode != ExitCodeOk) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Error in ExcludeListPropagator::checkItems");
        _exitCode = exitCode;
    }

    LOG_SYNCPAL_DEBUG(Log::instance()->getLogger(), "ExcludeListPropagator ended");
    _exitCode = ExitCodeOk;
}

int ExcludeListPropagator::syncDbId() const {
    return _syncPal->syncDbId();
}

ExitCode ExcludeListPropagator::checkItems() {
    try {
        std::error_code ec;
        auto dirIt = std::filesystem::recursive_directory_iterator(
            _syncPal->_localPath, std::filesystem::directory_options::skip_permission_denied, ec);
        if (ec) {
            LOGW_SYNCPAL_DEBUG(Log::instance()->getLogger(), "Error in checkItems: " << Utility::formatStdError(ec).c_str());
            return ExitCodeSystemError;
        }

        for (; dirIt != std::filesystem::recursive_directory_iterator(); ++dirIt) {
            if (isAborted()) {
                LOG_SYNCPAL_INFO(Log::instance()->getLogger(), "ExcludeListPropagator aborted " << jobId());
                return ExitCodeOk;
            }
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
                LOGW_SYNCPAL_WARN(Log::instance()->getLogger(), L"Ignore " << Utility::formatSyncPath(dirIt->path()).c_str()
                                                                           << L" because size > "
                                                                           << CommonUtility::maxPathLength());
                dirIt.disable_recursion_pending();
                continue;
            }

            const SyncPath relativePath = CommonUtility::relativePath(_syncPal->_localPath, dirIt->path());
            bool isWarning = false;
            bool isExcluded = false;
            IoError ioError = IoErrorSuccess;
            const bool success = ExclusionTemplateCache::instance()->checkIfIsExcluded(_syncPal->_localPath, relativePath,
                                                                                       isWarning, isExcluded, ioError);
            if (!success) {
                LOGW_SYNCPAL_WARN(Log::instance()->getLogger(), L"Error in ExclusionTemplateCache::checkIfIsExcluded: "
                                                                    << Utility::formatIoError(dirIt->path(), ioError).c_str());
                return ExitCodeSystemError;
            } else if (isExcluded) {
                if (isWarning) {
                    NodeId localNodeId = _syncPal->snapshot(ReplicaSideLocal)->itemId(relativePath);
                    NodeType localNodeType = _syncPal->snapshot(ReplicaSideLocal)->type(localNodeId);
                    Error error(_syncPal->syncDbId(), "", localNodeId, localNodeType, relativePath, ConflictTypeNone,
                                InconsistencyTypeNone, CancelTypeExcludedByTemplate);
                    _syncPal->addError(error);
                }
                // Find dbId from the entry path
                DbNodeId dbNodeId = -1;
                bool found = false;
                if (!_syncPal->_syncDb->dbId(ReplicaSideLocal, relativePath, dbNodeId, found)) {
                    LOGW_SYNCPAL_WARN(Log::instance()->getLogger(),
                                      L"Error in SyncDb::dbId for path=" << Path2WStr(relativePath).c_str());
                    return ExitCodeDbError;
                }

                if (!found) continue;

                // Remove node (and children by cascade) from DB
                if (ParametersCache::instance()->parameters().extendedLog()) {
                    LOGW_SYNCPAL_DEBUG(Log::instance()->getLogger(), L"Removing node "
                                                                         << Path2WStr(relativePath).c_str()
                                                                         << L" from DB because it is excluded from sync");
                }

                if (!_syncPal->_syncDb->deleteNode(dbNodeId, found)) {
                    LOGW_SYNCPAL_WARN(Log::instance()->getLogger(),
                                      L"Error in SyncDb::deleteNode for " << Utility::formatSyncPath(relativePath).c_str());
                    return ExitCodeDbError;
                }
                if (!found) {
                    LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Failed to delete node ID for dbNodeId=" << dbNodeId);
                    return ExitCodeDataError;
                }
            }
        }
    } catch (std::filesystem::filesystem_error &e) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(),
                         "Error caught in ExcludeListPropagator::checkItems: " << e.code() << " - " << e.what());
        return ExitCodeSystemError;
    } catch (...) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Error caught in ExcludeListPropagator::checkItems");
        return ExitCodeSystemError;
    }

    return ExitCodeOk;
}

}  // namespace KDC
