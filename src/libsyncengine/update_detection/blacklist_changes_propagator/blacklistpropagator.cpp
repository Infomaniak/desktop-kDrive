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

#include "jobs/local/localdeletejob.h"
#include "jobs/local/localmovejob.h"
#include "libcommon/utility/utility.h" // Path2WStr
#include "libcommonserver/io/iohelper.h"
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

void BlacklistPropagator::runJob() {
    LOG_SYNCPAL_DEBUG(Log::instance()->getLogger(), "BlacklistPropagator started");

    // Select sync
    bool found = true;
    if (!ParmsDb::instance()->selectSync(_syncPal->syncDbId(), _sync, found)) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectSync");
        _exitInfo = ExitCode::DbError;
        return;
    }
    if (!found) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Sync not found");
        _exitInfo = ExitCode::DataError;
        return;
    }

    ExitCode exitCode = checkNodes();
    if (exitCode != ExitCode::Ok) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Error in BlacklistPropagator::checkNodes");
        _exitInfo = exitCode;
        return;
    }

    LOG_SYNCPAL_DEBUG(Log::instance()->getLogger(), "BlacklistPropagator ended");
    _exitInfo = ExitCode::Ok;
}

ExitCode BlacklistPropagator::checkNodes() {
    ExitCode exitCode(ExitCode::Unknown);

    _syncPal->setSyncHasFullyCompleted(false);

    NodeSet blackList;
    SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), SyncNodeType::BlackList, blackList);

    if (blackList.empty()) {
        LOG_SYNCPAL_DEBUG(Log::instance()->getLogger(), "Blacklist is empty");
        return ExitCode::Ok;
    }

    bool noItemToRemoveFound = true;
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
            exitCode = ExitCode::DbError;
            break;
        }

        if (found) {
            noItemToRemoveFound = false;

            NodeId localNodeId;
            if (!_syncPal->syncDb()->correspondingNodeId(ReplicaSide::Remote, remoteNodeId, localNodeId, found)) {
                LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Error in SyncDb::correspondingNodeId");
                exitCode = ExitCode::DbError;
                break;
            }
            if (!found) {
                LOG_SYNCPAL_WARN(Log::instance()->getLogger(),
                                 "Corresponding node ID not found for remote ID = " << remoteNodeId);
                exitCode = ExitCode::DataError;
                break;
            }

            exitCode = removeItem(localNodeId, remoteNodeId, dbId);
            if (exitCode != ExitCode::Ok) {
                break;
            }
        }
    }

    if (noItemToRemoveFound) {
        exitCode = ExitCode::Ok;
    }

    return exitCode;
}

namespace {
bool isFileDehydrated(const bool isLiteSyncActivated, const SyncPath &localPath, log4cplus::Logger logger) {
    if (!isLiteSyncActivated) return false;

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

ExitCode BlacklistPropagator::removeItem(const NodeId &localNodeId, const NodeId &remoteNodeId, DbNodeId dbId) {
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

    // Cancel hydration
    const bool liteSyncActivated = _syncPal->vfsMode() != VirtualFileMode::Off;
    if (liteSyncActivated) {
        try {
            std::error_code ec;
            auto dirIt = std::filesystem::recursive_directory_iterator(
                    absoluteLocalPath, std::filesystem::directory_options::skip_permission_denied, ec);
            if (ec) {
                LOGW_SYNCPAL_WARN(Log::instance()->getLogger(),
                                  L"Error in BlacklistPropagator::removeItem :" << Utility::formatStdError(ec));
                return ExitCode::SystemError;
            }
            for (; dirIt != std::filesystem::recursive_directory_iterator(); ++dirIt) {
                if (isAborted()) {
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

                SyncPath absoluteLocalPath_ = dirIt->path();

                // Check if the directory entry is managed
                bool isManaged = true;
                IoError ioError = IoError::Success;
                if (!Utility::checkIfDirEntryIsManaged(*dirIt, isManaged, ioError)) {
                    LOGW_SYNCPAL_WARN(Log::instance()->getLogger(), L"Error in Utility::checkIfDirEntryIsManaged: "
                                                                            << Utility::formatSyncPath(absoluteLocalPath_));
                    dirIt.disable_recursion_pending();
                    continue;
                }

                if (ioError == IoError::NoSuchFileOrDirectory) {
                    LOGW_SYNCPAL_DEBUG(Log::instance()->getLogger(),
                                       L"Directory entry does not exist anymore:" << Utility::formatSyncPath(absoluteLocalPath_));
                    dirIt.disable_recursion_pending();
                    continue;
                }

                if (ioError == IoError::AccessDenied) {
                    LOGW_SYNCPAL_DEBUG(Log::instance()->getLogger(),
                                       L"Directory misses search permission: " << Utility::formatSyncPath(absoluteLocalPath_));
                    dirIt.disable_recursion_pending();
                    continue;
                }

                if (!isManaged) {
                    LOGW_SYNCPAL_DEBUG(Log::instance()->getLogger(),
                                       L"Directory entry is not managed: " << Utility::formatSyncPath(absoluteLocalPath_));
                    dirIt.disable_recursion_pending();
                    continue;
                }

                LOGW_SYNCPAL_DEBUG(Log::instance()->getLogger(),
                                   L"Cancel hydration: " << Utility::formatSyncPath(absoluteLocalPath_));
                _syncPal->vfs()->cancelHydrate(dirIt->path());
            }
        } catch (const std::filesystem::filesystem_error &e) {
            LOG_SYNCPAL_WARN(Log::instance()->getLogger(),
                             "Exception caught in BlacklistPropagator::removeItem: code=" << e.code() << " error=" << e.what());
            return ExitCode::SystemError;
        } catch (...) {
            LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Exception caught in BlacklistPropagator::removeItem.");
            return ExitCode::SystemError;
        }

        LOGW_SYNCPAL_DEBUG(Log::instance()->getLogger(), L"Cancel hydration: " << Utility::formatSyncPath(absoluteLocalPath));
        _syncPal->vfs()->cancelHydrate(absoluteLocalPath);
    }

    // Remove item from filesystem
    bool exists = false;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(absoluteLocalPath, exists, ioError)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in IoHelper::checkIfPathExists for path=" << Utility::formatIoError(absoluteLocalPath, ioError));
        return ExitCode::SystemError;
    }

    if (exists) {
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_SYNCPAL_DEBUG(Log::instance()->getLogger(), L"Removing item with "
                                                                     << Utility::formatSyncPath(localPath) << L" ("
                                                                     << CommonUtility::s2ws(localNodeId)
                                                                     << L") on local replica because it is blacklisted.");
        }

        const bool isDehydrated = isFileDehydrated(liteSyncActivated, absoluteLocalPath, Log::instance()->getLogger());
        LocalDeleteJob job(_syncPal->syncInfo(), localPath, isDehydrated, remoteNodeId);
        job.setBypassCheck(true);
        job.runSynchronously();
        if (!job.exitInfo()) {
            LOGW_SYNCPAL_WARN(Log::instance()->getLogger(),
                              L"Failed to remove item with " << Utility::formatSyncPath(absoluteLocalPath) << L" ("
                                                             << CommonUtility::s2ws(localNodeId)
                                                             << L") removed from local replica. It will not be blacklisted.");

            SyncPath destPath;
            PlatformInconsistencyCheckerUtility::renameLocalFile(
                    absoluteLocalPath, PlatformInconsistencyCheckerUtility::SuffixType::Blacklisted, &destPath);

            Error err(_syncPal->syncDbId(), "", "", NodeType::Directory, absoluteLocalPath, ConflictType::None,
                      InconsistencyType::None, CancelType::MoveToBinFailed, destPath);
            _syncPal->addError(err);
        } else {
            LOGW_SYNCPAL_DEBUG(Log::instance()->getLogger(), L"Item with " << Utility::formatSyncPath(absoluteLocalPath) << L" ("
                                                                           << CommonUtility::s2ws(localNodeId)
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
