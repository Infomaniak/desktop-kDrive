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

#include "excludelistpropagator.h"
#include "update_detection/file_system_observer/filesystemobserverworker.h"
#include "requests/exclusiontemplatecache.h"
#include "requests/parameterscache.h"
#include "libcommon/utility/utility.h"

namespace KDC {

ExcludeListPropagator::ExcludeListPropagator(std::shared_ptr<SyncPal> syncPal) :
    _syncPal(syncPal) {
    LOG_DEBUG(Log::instance()->getLogger(), "ExcludeListPropagator created " << jobId());
}

ExcludeListPropagator::~ExcludeListPropagator() {
    LOG_DEBUG(Log::instance()->getLogger(), "ExcludeListPropagator destroyed " << jobId());
}

void ExcludeListPropagator::runJob() {
    LOG_SYNCPAL_DEBUG(Log::instance()->getLogger(), "ExcludeListPropagator started " << jobId());

    ExitCode exitCode = checkItems();
    if (exitCode != ExitCode::Ok) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Error in ExcludeListPropagator::checkItems");
        _exitInfo = exitCode;
    }

    LOG_SYNCPAL_DEBUG(Log::instance()->getLogger(), "ExcludeListPropagator ended");
    _exitInfo = ExitCode::Ok;
}

int ExcludeListPropagator::syncDbId() const {
    return _syncPal->syncDbId();
}

ExitCode ExcludeListPropagator::checkItems() {
    try {
        std::error_code ec;
        auto dirIt = std::filesystem::recursive_directory_iterator(
                _syncPal->localPath(), std::filesystem::directory_options::skip_permission_denied, ec);
        if (ec) {
            LOGW_SYNCPAL_DEBUG(Log::instance()->getLogger(), L"Error in checkItems: " << Utility::formatStdError(ec));
            return ExitCode::SystemError;
        }

        for (; dirIt != std::filesystem::recursive_directory_iterator(); ++dirIt) {
            if (isAborted()) {
                LOG_SYNCPAL_INFO(Log::instance()->getLogger(), "ExcludeListPropagator aborted " << jobId());
                return ExitCode::Ok;
            }
#if defined(KD_WINDOWS)
            // skip_permission_denied doesn't work on Windows
            try {
                bool dummy = dirIt->exists();
                (void) (dummy);
            } catch (std::filesystem::filesystem_error &) {
                dirIt.disable_recursion_pending();
                continue;
            }
#endif

            if (dirIt->path().native().length() > CommonUtility::maxPathLength()) {
                LOGW_SYNCPAL_WARN(Log::instance()->getLogger(), L"Ignore " << Utility::formatSyncPath(dirIt->path())
                                                                           << L" because size > "
                                                                           << CommonUtility::maxPathLength());
                dirIt.disable_recursion_pending();
                continue;
            }

            const SyncPath relativePath = CommonUtility::relativePath(_syncPal->localPath(), dirIt->path());
            if (bool isWarning = false; ExclusionTemplateCache::instance()->isExcluded(relativePath, isWarning)) {
                if (isWarning) {
                    NodeId localNodeId = _syncPal->liveSnapshot(ReplicaSide::Local).itemId(relativePath);
                    NodeType localNodeType = localNodeId.empty() ? NodeType::Unknown
                                                                 : _syncPal->liveSnapshot(ReplicaSide::Local).type(localNodeId);
                    Error error(_syncPal->syncDbId(), "", localNodeId, localNodeType, relativePath, ConflictType::None,
                                InconsistencyType::None, CancelType::ExcludedByTemplate);
                    _syncPal->addError(error);
                }
                // Find dbId from the entry path
                DbNodeId dbNodeId = -1;
                bool found = false;
                if (!_syncPal->syncDb()->dbId(ReplicaSide::Local, relativePath, dbNodeId, found)) {
                    LOGW_SYNCPAL_WARN(Log::instance()->getLogger(),
                                      L"Error in SyncDb::dbId for path=" << Utility::formatSyncPath(relativePath));
                    return ExitCode::DbError;
                }

                if (!found) continue;

                // Remove node (and children by cascade) from DB
                if (ParametersCache::isExtendedLogEnabled()) {
                    LOGW_SYNCPAL_DEBUG(Log::instance()->getLogger(), L"Removing node "
                                                                             << Utility::formatSyncPath(relativePath)
                                                                             << L" from DB because it is excluded from sync");
                }

                if (!_syncPal->syncDb()->deleteNode(dbNodeId, found)) {
                    LOGW_SYNCPAL_WARN(Log::instance()->getLogger(),
                                      L"Error in SyncDb::deleteNode for " << Utility::formatSyncPath(relativePath));
                    return ExitCode::DbError;
                }
                if (!found) {
                    LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Failed to delete node ID for dbNodeId=" << dbNodeId);
                    return ExitCode::DataError;
                }
            }
        }
    } catch (std::filesystem::filesystem_error &e) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(),
                         "Error caught in ExcludeListPropagator::checkItems: code=" << e.code() << " error=" << e.what());
        return ExitCode::SystemError;
    } catch (...) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Error caught in ExcludeListPropagator::checkItems");
        return ExitCode::SystemError;
    }

    return ExitCode::Ok;
}

} // namespace KDC
