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
#include "libcommon/io/filestat.h"
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

ExitInfo ExcludeListPropagator::runJob() {
    LOG_SYNCPAL_DEBUG(Log::instance()->getLogger(), "ExcludeListPropagator started " << jobId());

    if (const auto exitInfo = checkItems(); !exitInfo) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Error in ExcludeListPropagator::checkItems");
        return exitInfo;
    }

    LOG_SYNCPAL_DEBUG(Log::instance()->getLogger(), "ExcludeListPropagator ended");
    return ExitCode::Ok;
}

int ExcludeListPropagator::syncDbId() const {
    return _syncPal->syncDbId();
}

ExitInfo ExcludeListPropagator::checkItem(const DirectoryEntry &entry) {
    const SyncPath &relativePath = CommonUtility::relativePath(_syncPal->localPath(), entry.path());

    bool isWarning = false;
    const bool isExcluded = ExclusionTemplateCache::instance()->isExcluded(relativePath, isWarning);
    if (!isExcluded) return ExitCode::Ok;

    if (isWarning) {
        IoError ioError = IoError::Success;
        NodeId localNodeId;
        ItemType localNodeType;
        bool found = false;

        if (IoHelper::getNodeId(entry.path(), localNodeId) && IoHelper::getItemType(entry.path(), localNodeType)) {
            Error error(_syncPal->syncDbId(), localNodeId, NodeId(), localNodeType.nodeType, relativePath, ConflictType::None,
                        InconsistencyType::None, CancelType::ExcludedByTemplate);
            _syncPal->addError(error);
        } else {
            LOGW_SYNCPAL_WARN(Log::instance()->getLogger(),
                              L"Error in SyncDb::getNodeId or SyncDb::getItemType for path=" << CommonUtility::formatSyncPath(relativePath));
        }
    }

    // Find dbId from the entry path
    DbNodeId dbNodeId = -1;
    bool found = false;
    if (!_syncPal->syncDb()->dbId(ReplicaSide::Local, relativePath, dbNodeId, found)) {
        LOGW_SYNCPAL_WARN(Log::instance()->getLogger(),
                          L"Error in SyncDb::dbId for path=" << CommonUtility::formatSyncPath(relativePath));
        return ExitCode::DbError;
    }

    if (!found) return ExitCode::Ok;

    // Remove node (and children by cascade) from DB
    if (ParametersCache::isExtendedLogEnabled()) {
        LOGW_SYNCPAL_DEBUG(Log::instance()->getLogger(), L"Removing node " << CommonUtility::formatSyncPath(relativePath)
                                                                           << L" from DB because it is excluded from sync");
    }

    if (!_syncPal->syncDb()->deleteNode(dbNodeId, found)) {
        LOGW_SYNCPAL_WARN(Log::instance()->getLogger(),
                          L"Error in SyncDb::deleteNode for " << CommonUtility::formatSyncPath(relativePath));
        return ExitCode::DbError;
    }

    if (!found) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Failed to delete node ID for dbNodeId=" << dbNodeId);
        return ExitCode::DataError;
    }

    return ExitCode::Ok;
}

ExitInfo ExcludeListPropagator::checkItems() {
    bool directoryIterationException = false;
    auto ioError = IoError::Success;
    IoHelper::DirectoryIterator dirIt;
    bool endOfDir = false;
    DirectoryEntry entry;

    try {
        if (!IoHelper::recursiveDirectoryIterator(_syncPal->localPath(), dirIt)) {
            LOGW_WARN(_logger, L"Error in IoHelper::recursiveDirectoryIterator");
            return ExitCode::SystemError;
        }

        while (dirIt.next(entry, endOfDir, ioError) && !endOfDir && ioError == IoError::Success) {
            if (isAborted()) {
                LOG_SYNCPAL_INFO(Log::instance()->getLogger(), "ExcludeListPropagator aborted " << jobId());
                return ExitCode::Ok;
            }

            if (entry.path().native().length() > CommonUtility::maxPathLength()) {
                LOGW_SYNCPAL_WARN(Log::instance()->getLogger(), L"Ignore " << CommonUtility::formatSyncPath(entry.path())
                                                                           << L" because size > "
                                                                           << CommonUtility::maxPathLength());
                dirIt.disableRecursionPending();
                continue;
            }

            if (const auto checkItemExitInfo = checkItem(entry); !checkItemExitInfo) return checkItemExitInfo;
        }
    } catch (std::filesystem::filesystem_error &e) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(),
                         "Error caught in ExcludeListPropagator::checkItems: code=" << e.code() << " error=" << e.what());
        directoryIterationException = true;
    } catch (...) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Error caught in ExcludeListPropagator::checkItems");
        directoryIterationException = true;
    }

    return IoHelper::checkDirectoryIteratorInterruption(endOfDir, ioError, entry, directoryIterationException);
}

} // namespace KDC
