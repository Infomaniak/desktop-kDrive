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

#include "blacklistpropagator.h"

#include "jobs/local/synclocaldeletejob.h"
#include "jobs/local/localmovejob.h"
#include "libcommon/utility/utility.h" // Path2WStr
#include "libcommon/io/iohelper.h"
#include "libcommonserver/utility/utility.h"
#include "reconciliation/platform_inconsistency_checker/platforminconsistencycheckerutility.h"
#include "requests/syncnodecache.h"
#include "requests/parameterscache.h"

namespace KDC {

BlacklistPropagator::BlacklistPropagator(std::shared_ptr<SyncPal> syncPal) :
    _syncPal(syncPal) {
    LOG_SYNCPAL_DEBUG(Log::instance()->getLogger(), "BlacklistPropagator created");
}

BlacklistPropagator::~BlacklistPropagator() {
    LOG_SYNCPAL_DEBUG(Log::instance()->getLogger(), "BlacklistPropagator destroyed");
}

ExitInfo BlacklistPropagator::runJob() {
    LOG_SYNCPAL_DEBUG(Log::instance()->getLogger(), "BlacklistPropagator started");

    // Select sync
    bool found = true;
    if (!ParmsDb::instance()->selectSync(_syncPal->syncDbId(), _sync, found)) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectSync");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Sync not found");
        return ExitCode::DataError;
    }

    if (const auto exitInfo = checkNodes(); !exitInfo) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Error in BlacklistPropagator::checkNodes");
        return exitInfo;
    }

    LOG_SYNCPAL_DEBUG(Log::instance()->getLogger(), "BlacklistPropagator ended");
    return ExitCode::Ok;
}

ExitInfo BlacklistPropagator::checkNodes() {
    _syncPal->setSyncHasFullyCompleted(false);

    NodeSet blackList;
    SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), SyncNodeType::BlackList, blackList);

    if (blackList.empty()) {
        LOG_SYNCPAL_DEBUG(Log::instance()->getLogger(), "Blacklist is empty");
        return ExitCode::Ok;
    }

    bool noItemToRemoveFound = true;
    ExitInfo exitInfo(ExitCode::Unknown);
    for (auto &remoteNodeId: blackList) {
        if (isAborted()) {
            LOG_SYNCPAL_INFO(Log::instance()->getLogger(), "BlacklistPropagator aborted " << jobId());
            return ExitCode::Ok;
        }

        // Check if item still exist
        DbNodeId dbId;
        bool found = false;
        if (!_syncPal->syncDb()->dbId(ReplicaSide::Remote, remoteNodeId, dbId, found)) {
            LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Error in SyncDb::dbId");
            exitInfo = ExitCode::DbError;
            break;
        }

        if (found) {
            noItemToRemoveFound = false;

            NodeId localNodeId;
            if (!_syncPal->syncDb()->correspondingNodeId(ReplicaSide::Remote, remoteNodeId, localNodeId, found)) {
                LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Error in SyncDb::correspondingNodeId");
                exitInfo = ExitCode::DbError;
                break;
            }
            if (!found) {
                LOG_SYNCPAL_WARN(Log::instance()->getLogger(),
                                 "Corresponding node ID not found for remote ID = " << remoteNodeId);
                exitInfo = ExitCode::DataError;
                break;
            }

            exitInfo = removeItem(localNodeId, remoteNodeId, dbId).code();
            if (!exitInfo) break;
        }
    }

    if (noItemToRemoveFound) {
        exitInfo = ExitCode::Ok;
    }

    return exitInfo;
}

ExitInfo BlacklistPropagator::cancelHydration(const SyncPath &absoluteLocalPath) {
    bool directoryIterationException = false;
    auto ioError = IoError::Success;
    IoHelper::DirectoryIterator dirIt;
    bool endOfDir = false;
    DirectoryEntry entry;

    try {
        if (!IoHelper::recursiveDirectoryIterator(absoluteLocalPath, dirIt)) {
            LOGW_WARN(_logger, L"Error in IoHelper::recursiveDirectoryIterator");
            return ExitCode::SystemError;
        }

        while (dirIt.next(entry, endOfDir, ioError) && !endOfDir) {
            if (isAborted()) return ExitCode::Ok;

            const SyncPath &absoluteLocalPath_ = entry.path();
            // Check if the directory entry is managed
            bool isManaged = true;
            auto managedDirEntryError = IoError::Success;
            if (!Utility::checkIfDirEntryIsManaged(entry, isManaged, managedDirEntryError)) {
                LOGW_SYNCPAL_WARN(Log::instance()->getLogger(), L"Error in Utility::checkIfDirEntryIsManaged: "
                                                                        << CommonUtility::formatSyncPath(absoluteLocalPath_));
                dirIt.disableRecursionPending();
                continue;
            }

            if (managedDirEntryError == IoError::NoSuchFileOrDirectory) {
                LOGW_SYNCPAL_DEBUG(Log::instance()->getLogger(), L"Directory entry does not exist anymore:"
                                                                         << CommonUtility::formatSyncPath(absoluteLocalPath_));
                dirIt.disableRecursionPending();
                continue;
            }

            if (ioError == IoError::AccessDenied) {
                LOGW_SYNCPAL_DEBUG(Log::instance()->getLogger(),
                                   L"Directory misses search permission: " << CommonUtility::formatSyncPath(absoluteLocalPath_));
                dirIt.disableRecursionPending();
                continue;
            }

            if (!isManaged) {
                LOGW_SYNCPAL_DEBUG(Log::instance()->getLogger(),
                                   L"Directory entry is not managed: " << CommonUtility::formatSyncPath(absoluteLocalPath_));
                dirIt.disableRecursionPending();
                continue;
            }

            LOGW_SYNCPAL_DEBUG(Log::instance()->getLogger(),
                               L"Cancel hydration: " << CommonUtility::formatSyncPath(absoluteLocalPath_));
            _syncPal->vfs()->cancelHydrate(entry.path());
        }
    } catch (const std::filesystem::filesystem_error &e) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(),
                         "Exception caught in BlacklistPropagator::removeItem: code=" << e.code() << " error=" << e.what());
        directoryIterationException = true;
    } catch (...) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Exception caught in BlacklistPropagator::removeItem.");
        directoryIterationException = true;
    }

    if (const auto interruptionExitInfo =
                IoHelper::checkDirectoryIteratorInterruption(endOfDir, ioError, entry, directoryIterationException);
        !interruptionExitInfo) {
        return interruptionExitInfo;
    }

    LOGW_SYNCPAL_DEBUG(Log::instance()->getLogger(),
                       L"Cancelling hydration of " << CommonUtility::formatSyncPath(absoluteLocalPath));

    _syncPal->vfs()->cancelHydrate(absoluteLocalPath);

    return ExitCode::Ok;
}

ExitInfo BlacklistPropagator::removeItem(const NodeId &localNodeId, const NodeId &remoteNodeId, DbNodeId dbId) {
    // Get path from nodeId
    SyncPath localPath;
    SyncPath remotePath;
    bool found = false;
    if (!_syncPal->syncDb()->path(dbId, localPath, remotePath, found)) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Error in SyncDb::path");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_SYNCPAL_INFO(Log::instance()->getLogger(), "Node not found for id = " << dbId);
        return ExitCode::DataError;
    }

    const SyncPath absoluteLocalPath = _sync.localPath() / localPath;
    const bool liteSyncActivated = _syncPal->vfsMode() != VirtualFileMode::Off;

    if (liteSyncActivated) {
        if (const auto cancellationExitInfo = cancelHydration(absoluteLocalPath); !cancellationExitInfo) {
            return cancellationExitInfo;
        }
    }

    // Remove item from filesystem
    bool exists = false;
    if (auto ioError = IoError::Success; !IoHelper::checkIfPathExists(absoluteLocalPath, exists, ioError)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in IoHelper::checkIfPathExists for " << CommonUtility::formatIoError(absoluteLocalPath, ioError));
        return ExitCode::SystemError;
    }

    if (exists) {
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_SYNCPAL_DEBUG(Log::instance()->getLogger(), L"Removing item with "
                                                                     << CommonUtility::formatSyncPath(localPath) << L" ("
                                                                     << CommonUtility::s2ws(localNodeId)
                                                                     << L") on local replica because it is blacklisted.");
        }

        SyncLocalDeleteJob job(_syncPal, localPath, liteSyncActivated, remoteNodeId);
        job.setBypassCheck(true);
        job.runSynchronously();
        if (!job.exitInfo()) {
            LOGW_SYNCPAL_WARN(Log::instance()->getLogger(),
                              L"Failed to remove item with " << CommonUtility::formatSyncPath(absoluteLocalPath) << L" ("
                                                             << CommonUtility::s2ws(localNodeId)
                                                             << L") removed from local replica. It will not be blacklisted.");

            SyncPath destPath;
            PlatformInconsistencyCheckerUtility::renameLocalFile(
                    absoluteLocalPath, PlatformInconsistencyCheckerUtility::SuffixType::Blacklisted, &destPath);

            Error err(_syncPal->syncDbId(), "", "", NodeType::Directory, absoluteLocalPath, ConflictType::None,
                      InconsistencyType::None, CancelType::MoveToBinFailed, destPath);
            _syncPal->addError(err);
        } else {
            LOGW_SYNCPAL_DEBUG(Log::instance()->getLogger(), L"Item with " << CommonUtility::formatSyncPath(absoluteLocalPath)
                                                                           << L" (" << CommonUtility::s2ws(localNodeId)
                                                                           << L") removed from local replica.");
        }
    }

    // Remove node (and children by cascade) from DB
    if (!_syncPal->syncDb()->deleteNode(dbId, found)) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Error in SyncDb::deleteNode");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Node not found in node table for dbId=" << dbId);
        return ExitCode::DataError;
    }

    return ExitCode::Ok;
}

} // namespace KDC
