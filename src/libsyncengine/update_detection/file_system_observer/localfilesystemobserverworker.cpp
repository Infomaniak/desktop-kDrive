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

#include "localfilesystemobserverworker.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"
#include "requests/parameterscache.h"
#include "requests/exclusiontemplatecache.h"

#include <log4cplus/loggingmacros.h>

#include <iostream>
#include <filesystem>

namespace KDC {

static const int defaultDiscoveryInterval = 60000; // 60sec
static const int waitForUpdateDelay = 1000; // 1sec

LocalFileSystemObserverWorker::LocalFileSystemObserverWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name,
                                                             const std::string &shortName) :
    FileSystemObserverWorker(syncPal, name, shortName, ReplicaSide::Local),
    _rootFolder(syncPal->localPath()) {}

LocalFileSystemObserverWorker::~LocalFileSystemObserverWorker() {
    LOG_SYNCPAL_DEBUG(_logger, "~LocalFileSystemObserverWorker");
}

void LocalFileSystemObserverWorker::start() {
    //    _checksumWorker = std::unique_ptr<ContentChecksumWorker>(new ContentChecksumWorker(_syncPal, "Content Checksum", "COCH",
    //    _snapshot)); _checksumWorker->start();

    FileSystemObserverWorker::start();

    _folderWatcher->start();
}

void LocalFileSystemObserverWorker::stop() {
    _folderWatcher->stop();

    FileSystemObserverWorker::stop();

    //    _checksumWorker->stop();
    //    _checksumWorker->waitForExit();
}

void LocalFileSystemObserverWorker::changesDetected(const std::list<std::pair<std::filesystem::path, OperationType>> &changes) {
    const std::lock_guard<std::recursive_mutex> lock(_recursiveMutex);

    // Warning: OperationType retrieved from FSEvent (macOS) seems to be unreliable in some cases. One event might contain
    // several operations. Only Delete event seems to be 100% reliable Move event from outside the synced dir to inside it will
    // be considered by the OS as move while must be considered by the synchronizer as Create.
    if (!_snapshot->isValid()) {
        // Snapshot generation is ongoing, queue the events and process them later
        _pendingFileEvents.insert(_pendingFileEvents.end(), changes.begin(), changes.end());
        return;
    }

    for (const auto &changedItem: changes) {
        if (stopAsked()) {
            _pendingFileEvents.clear();
            break;
        }

        // Raise flag _updating in order to wait 1sec without local changes before starting the sync
        _updating = true;
        _needUpdateTimerStart = std::chrono::steady_clock::now();

        OperationType opTypeFromOS = changedItem.second;
        const SyncPath absolutePath = changedItem.first.native();
        const SyncPath relativePath = CommonUtility::relativePath(_syncPal->localPath(), absolutePath);

        if (_syncPal->isTmpBlacklisted(relativePath, ReplicaSide::Local)) {
            _syncPal->removeItemFromTmpBlacklist(relativePath);
            if (opTypeFromOS == OperationType::Edit) {
                NodeId itemId = _snapshot->itemId(relativePath);
                if (!itemId.empty()) _snapshot->setLastModified(itemId, 0);
            }
        }
        // Check if exists with same nodeId
        if (opTypeFromOS == OperationType::Delete) {
            NodeId prevNodeId = _snapshot->itemId(relativePath);
            bool existsWithSameId = false;
            NodeId otherNodeId;
            IoError ioError = IoError::Success;
            if (!prevNodeId.empty()) {
                if (_syncPal->isTmpBlacklisted(prevNodeId, ReplicaSide::Local))
                    _syncPal->removeItemFromTmpBlacklist(relativePath);
                if (IoHelper::checkIfPathExistsWithSameNodeId(absolutePath, prevNodeId, existsWithSameId, otherNodeId, ioError) &&
                    !existsWithSameId) {
                    if (_snapshot->removeItem(prevNodeId)) {
                        LOGW_SYNCPAL_DEBUG(_logger, L"Item removed from local snapshot: "
                                                            << Utility::formatSyncPath(absolutePath).c_str() << L" ("
                                                            << Utility::s2ws(prevNodeId).c_str() << L")");
                    }
                    continue;
                }
            }
        }

        FileStat fileStat;
        IoError ioError = IoError::Success;
        if (!IoHelper::getFileStat(absolutePath, &fileStat, ioError)) {
            LOGW_SYNCPAL_WARN(_logger,
                              L"Error in IoHelper::getFileStat: " << Utility::formatIoError(absolutePath, ioError).c_str());
            invalidateSnapshot();
            return;
        }

        bool exists = true;
        if (ioError == IoError::AccessDenied) {
            LOGW_SYNCPAL_DEBUG(_logger,
                               L"Item: " << Utility::formatSyncPath(absolutePath).c_str() << L" misses search permissions!");
            sendAccessDeniedError(absolutePath);
            continue;
        } else if (ioError == IoError::NoSuchFileOrDirectory) {
            exists = false;
        }

        NodeId nodeId = std::to_string(fileStat.inode);
        NodeType nodeType = NodeType::Unknown;

        if (_syncPal->isTmpBlacklisted(nodeId, ReplicaSide::Local)) {
            _syncPal->removeItemFromTmpBlacklist(relativePath);
            if (opTypeFromOS == OperationType::Edit) {
                NodeId itemId = _snapshot->itemId(relativePath);
                if (!itemId.empty()) _snapshot->setLastModified(itemId, 0);
            }
        }

        bool isLink = false;
        if (exists) {
            ItemType itemType;
            if (!IoHelper::getItemType(absolutePath, itemType)) {
                LOGW_SYNCPAL_WARN(_logger,
                                  L"Error in IoHelper::getItemType: " << Utility::formatIoError(absolutePath, itemType.ioError));
                invalidateSnapshot();
                return;
            }
            if (itemType.ioError == IoError::AccessDenied) {
                LOGW_SYNCPAL_DEBUG(_logger, L"getItemType failed for item: "
                                                    << Utility::formatIoError(absolutePath, itemType.ioError)
                                                    << L". Blacklisting it temporarily");
                sendAccessDeniedError(absolutePath);
                continue;
            }

            nodeType = itemType.nodeType;
            isLink = itemType.linkType != LinkType::None;

            // Check if excluded by a file exclusion rule
            bool isWarning = false;
            bool toExclude = false;
            const bool success = ExclusionTemplateCache::instance()->checkIfIsExcluded(_syncPal->localPath(), relativePath,
                                                                                       isWarning, toExclude, ioError);
            if (!success) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in ExclusionTemplateCache::isExcluded: "
                                                   << Utility::formatIoError(absolutePath, ioError).c_str());
                invalidateSnapshot();
                return;
            }
            if (toExclude) {
                if (isWarning) {
                    Error error(_syncPal->syncDbId(), "", nodeId, nodeType, relativePath, ConflictType::None,
                                InconsistencyType::None, CancelType::ExcludedByTemplate);
                    _syncPal->addError(error);
                }

                // Check if item still exist in snapshot
                NodeId itemId = _snapshot->itemId(relativePath);
                if (!itemId.empty()) {
                    // Remove it from snapshot
                    _snapshot->removeItem(itemId);
                    LOGW_SYNCPAL_DEBUG(_logger, L"Item removed from sync because it's hidden: "
                                                        << Utility::formatSyncPath(absolutePath).c_str());
                } else {
                    LOGW_SYNCPAL_DEBUG(_logger, L"Item not processed because it's excluded: "
                                                        << Utility::formatSyncPath(absolutePath).c_str());
                }

                if (!_syncPal->vfsExclude(
                            absolutePath)) { // TODO : This class should never set any attribute or change anything on a file
                    LOGW_SYNCPAL_WARN(_logger, L"Error in vfsExclude: " << Utility::formatSyncPath(absolutePath).c_str());
                }

                continue;
            }
        }

        if (!exists) {
            // This is a delete operation
            // Get the ID from the snapshot
            NodeId itemId = _snapshot->itemId(relativePath);
            if (itemId.empty()) {
                // The file does not exist anymore, ignore it
                continue;
            }

            _syncPal->removeItemFromTmpBlacklist(itemId, ReplicaSide::Local);

            if (_snapshot->removeItem(itemId)) {
                LOGW_SYNCPAL_DEBUG(_logger, L"Item removed from local snapshot: " << Utility::formatSyncPath(absolutePath).c_str()
                                                                                  << L" (" << Utility::s2ws(itemId).c_str()
                                                                                  << L")");
            } else {
                LOGW_SYNCPAL_WARN(_logger, L"Fail to remove item: " << Utility::formatSyncPath(absolutePath).c_str() << L" ("
                                                                    << Utility::s2ws(itemId).c_str() << L")");
                invalidateSnapshot();
                return;
            }

            continue;
        }

        SyncPath parentPath = absolutePath.parent_path();
        NodeId parentNodeId;
        if (parentPath == _rootFolder) {
            parentNodeId = *_syncPal->_syncDb->rootNode().nodeIdLocal();
        } else {
            if (!IoHelper::getNodeId(parentPath, parentNodeId)) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in IoHelper::getNodeId for " << Utility::formatSyncPath(parentPath).c_str());
                continue;
            }
        }

        if (opTypeFromOS == OperationType::Edit) {
            // Filter out hydration/dehydration
            bool changed = false;
            const bool success = IoHelper::checkIfFileChanged(absolutePath, _snapshot->size(nodeId),
                                                              _snapshot->lastModified(nodeId), changed, ioError);
            if (!success) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in IoHelper::checkIfFileChanged: "
                                                   << Utility::formatIoError(absolutePath, ioError).c_str());
            }
            if (ioError == IoError::AccessDenied) {
                LOGW_SYNCPAL_DEBUG(_logger,
                                   L"Item: " << Utility::formatSyncPath(absolutePath).c_str() << L" misses search permissions!");
                sendAccessDeniedError(absolutePath);
            } else if (ioError == IoError::NoSuchFileOrDirectory) {
                continue;
            }

            if (!changed) {
#ifdef _WIN32
                bool isPlaceholder = false;
                bool isHydrated = false;
                bool isSyncing = false;
                int progress = 0;
                if (!_syncPal->vfsStatus(absolutePath, isPlaceholder, isHydrated, isSyncing, progress)) {
                    LOGW_SYNCPAL_WARN(_logger, L"Error in vfsStatus: " << Utility::formatSyncPath(absolutePath).c_str());
                    invalidateSnapshot();
                    return;
                }

                PinState pinstate = PinState::Unspecified;
                if (!_syncPal->vfsPinState(absolutePath, pinstate)) {
                    LOGW_SYNCPAL_WARN(_logger, L"Error in vfsPinState: " << Utility::formatSyncPath(absolutePath).c_str());
                    invalidateSnapshot();
                    return;
                }

                if (isPlaceholder) {
                    if ((isHydrated && pinstate == PinState::OnlineOnly) || (!isHydrated && pinstate == PinState::AlwaysLocal)) {
                        // Change status in order to start hydration/dehydration
                        // TODO : FileSystemObserver should not change file status, it should only monitor file system
                        if (!_syncPal->vfsFileStatusChanged(absolutePath, SyncFileStatus::Syncing)) {
                            LOGW_SYNCPAL_WARN(_logger, L"Error in SyncPal::vfsFileStatusChanged: "
                                                               << Utility::formatSyncPath(absolutePath).c_str());
                            invalidateSnapshot();
                            return;
                        }
                    }
                }

#else
                if (ParametersCache::isExtendedLogEnabled()) {
                    LOGW_SYNCPAL_DEBUG(_logger, L"Ignoring spurious edit notification on file: "
                                                        << Utility::formatSyncPath(absolutePath).c_str() << L" ("
                                                        << Utility::s2ws(nodeId).c_str() << L")");
                }
#endif

                continue;
            }
        }

        bool itemExistsInSnapshot = _snapshot->exists(nodeId);
        if (!itemExistsInSnapshot) {
            if (opTypeFromOS == OperationType::Delete) {
                // This is a delete operation but a file with the same name has been recreated immediately
                NodeId itemId = _snapshot->itemId(relativePath);
                if (_snapshot->removeItem(itemId)) {
                    LOGW_SYNCPAL_DEBUG(_logger, L"Item removed from local snapshot: "
                                                        << Utility::formatSyncPath(absolutePath).c_str() << L" ("
                                                        << Utility::s2ws(itemId).c_str() << L")");
                } else {
                    LOGW_SYNCPAL_WARN(_logger, L"Failed to remove item: " << Utility::formatSyncPath(absolutePath).c_str()
                                                                          << L" (" << Utility::s2ws(itemId).c_str() << L")");
                    invalidateSnapshot();
                    return;
                }
                continue;
            }

            if (_snapshot->pathExists(relativePath)) {
                NodeId previousItemId = _snapshot->itemId(relativePath);
                // If an item with same path already exist remove it from snapshot because its ID might have changed (i.e. the
                // file has been downloaded in the tmp folder then moved to override the existing one) The item will be inserted
                // below anyway
                if (_snapshot->removeItem(previousItemId)) {
                    LOGW_SYNCPAL_DEBUG(_logger, L"Item removed from local snapshot: "
                                                        << Utility::formatSyncPath(absolutePath).c_str() << L" ("
                                                        << Utility::s2ws(previousItemId).c_str() << L")");
                } else {
                    LOGW_SYNCPAL_WARN(_logger, L"Failed to delete item: " << Utility::formatSyncPath(absolutePath).c_str()
                                                                          << L" (" << Utility::s2ws(previousItemId).c_str()
                                                                          << L")");
                    invalidateSnapshot();
                    return;
                }
            }

            // This can be either Create, Move or Edit operation
            SnapshotItem item(nodeId, parentNodeId, absolutePath.filename().native(), fileStat.creationTime, fileStat.modtime,
                              nodeType, fileStat.size, isLink, true, true);

            if (!_snapshot->updateItem(item)) {
                LOGW_SYNCPAL_WARN(_logger, L"Failed to insert item: " << Utility::formatSyncPath(absolutePath).c_str() << L" ("
                                                                      << Utility::s2ws(nodeId).c_str() << L")");
                invalidateSnapshot();
                return;
            }

            if (ParametersCache::isExtendedLogEnabled()) {
                LOGW_SYNCPAL_DEBUG(_logger, L"Item inserted in local snapshot: " << Utility::formatSyncPath(absolutePath).c_str()
                                                                                 << L" (" << Utility::s2ws(nodeId).c_str()
                                                                                 << L") at " << fileStat.modtime);

                //                if (nodeType == NodeType::File) {
                //                    if (canComputeChecksum(absolutePath)) {
                //                        // Start asynchronous checkum generation
                //                        _checksumWorker->computeChecksum(nodeId, absolutePath);
                //                    }
                //                }
            }

            // Manage directories moved from outside the synchronized directory
            if (nodeType == NodeType::Directory) {
                if (absolutePath.native().length() > CommonUtility::maxPathLength()) {
                    LOGW_SYNCPAL_WARN(_logger, L"Ignore item: " << Utility::formatSyncPath(absolutePath).c_str()
                                                                << L" because size > " << CommonUtility::maxPathLength());
                    continue;
                }

                if (!exploreDir(absolutePath)) {
                    // Error while exploring directory, we need to invalidate the snapshot
                    invalidateSnapshot();
                    return;
                }
            }

            continue;
        }

        if (fileStat.modtime > _snapshot->lastModified(nodeId)) {
            // This is an edit operation
#ifdef __APPLE__
            if (_syncPal->vfsMode() == VirtualFileMode::Mac) {
                // Exclude spurious operations (for example setIcon)
                bool valid = true;
                ExitCode exitCode = isEditValid(nodeId, absolutePath, fileStat.modtime, valid);
                if (exitCode != ExitCode::Ok) {
                    LOGW_SYNCPAL_WARN(_logger, L"Error in LocalFileSystemObserverWorker::isEditValid");
                    invalidateSnapshot();
                    return;
                }

                if (!valid) {
                    continue;
                }
            }
#endif
        }

        // Update snapshot
        if (_snapshot->updateItem(SnapshotItem(nodeId, parentNodeId, absolutePath.filename().native(), fileStat.creationTime,
                                               fileStat.modtime, nodeType, fileStat.size, isLink, true, true))) {
            if (ParametersCache::isExtendedLogEnabled()) {
                LOGW_SYNCPAL_DEBUG(_logger, L"Item: " << Utility::formatSyncPath(absolutePath).c_str() << L" ("
                                                      << Utility::s2ws(nodeId).c_str() << L") updated in local snapshot at "
                                                      << fileStat.modtime);
            }

            if (nodeType == NodeType::File) {
                //                if (canComputeChecksum(absolutePath)) {
                //                    // Start asynchronous checkum generation
                //                    _checksumWorker->computeChecksum(nodeId, absolutePath);
                //                }
            }
        } else {
            LOGW_SYNCPAL_WARN(_logger, L"Failed to update item: " << Utility::formatSyncPath(absolutePath).c_str() << L" ("
                                                                  << Utility::s2ws(nodeId).c_str() << L")");
            invalidateSnapshot();
            return;
        }
    }
}

void LocalFileSystemObserverWorker::forceUpdate() {
    FileSystemObserverWorker::forceUpdate();
    _needUpdateTimerStart = std::chrono::steady_clock::now();
}

void LocalFileSystemObserverWorker::execute() {
    ExitCode exitCode(ExitCode::Unknown);

    LOG_SYNCPAL_DEBUG(_logger, "Worker started: name=" << name().c_str());

    auto timerStart = std::chrono::steady_clock::now();

    // Sync loop
    for (;;) {
        if (stopAsked()) {
            exitCode = ExitCode::Ok;
            invalidateSnapshot();
            break;
        }
        if (!_folderWatcher->exitInfo()) {
            exitCode = _folderWatcher->exitInfo().code();
            setExitCause(_folderWatcher->exitInfo().cause());
            invalidateSnapshot();
            break;
        }
        // We never pause this thread

        if (!isFolderWatcherReliable() &&
            (std::chrono::steady_clock::now() - timerStart).count() * 1000 > defaultDiscoveryInterval) {
            // The folder watcher became unreliable so fallback to static synchronization
            timerStart = std::chrono::steady_clock::now();
            invalidateSnapshot();
            exitCode = generateInitialSnapshot();
            if (exitCode != ExitCode::Ok) {
                LOG_SYNCPAL_DEBUG(_logger, "Error in generateInitialSnapshot: code=" << exitCode);
                break;
            }
        } else {
            if (!_snapshot->isValid()) {
                exitCode = generateInitialSnapshot();
                if (exitCode != ExitCode::Ok) {
                    LOG_SYNCPAL_DEBUG(_logger, "Error in generateInitialSnapshot: code=" << exitCode);
                    break;
                }
            }

            // Wait 1 sec after the last update
            if (_updating) {
                auto diff_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                                     _needUpdateTimerStart);
                if (diff_ms.count() > waitForUpdateDelay) {
                    const std::lock_guard<std::recursive_mutex> lk(_recursiveMutex);
                    _updating = false;
                }
            }
        }

        Utility::msleep(LOOP_EXEC_SLEEP_PERIOD);
    }

    setDone(exitCode);
    LOG_SYNCPAL_DEBUG(_logger, "Worker stopped: name=" << name().c_str());
}

ExitCode LocalFileSystemObserverWorker::generateInitialSnapshot() {
    LOG_SYNCPAL_INFO(_logger, "Starting local snapshot generation");
    auto start = std::chrono::steady_clock::now();

    _snapshot->init();
    _updating = true;

    ExitCode res = exploreDir(_rootFolder);

    std::chrono::duration<double> elapsed_seconds = std::chrono::steady_clock::now() - start;
    if (res == ExitCode::Ok && !stopAsked()) {
        _snapshot->setValid(true);
        LOG_SYNCPAL_INFO(_logger, "Local snapshot generated in: " << elapsed_seconds.count() << "s for " << _snapshot->nbItems()
                                                                  << " items");
    } else if (stopAsked()) {
        LOG_SYNCPAL_INFO(_logger, "Local snapshot generation stopped after: " << elapsed_seconds.count() << "s");
    } else {
        LOG_SYNCPAL_WARN(_logger, "Local snapshot generation failed after: " << elapsed_seconds.count() << "s");
    }

    const std::lock_guard<std::recursive_mutex> lock(_recursiveMutex);
    if (!_pendingFileEvents.empty()) {
        LOG_SYNCPAL_DEBUG(_logger, "Processing pending file events");
        changesDetected(_pendingFileEvents);
        _pendingFileEvents.clear();
        LOG_SYNCPAL_DEBUG(_logger, "Pending file events processed");
    }

    _updating = false;

    return res;
}

bool LocalFileSystemObserverWorker::canComputeChecksum(const SyncPath &absolutePath) {
    bool isPlaceholder = false;
    bool isHydrated = false;
    bool isSyncing = false;
    int progress = 0;
    if (!_syncPal->vfsStatus(absolutePath, isPlaceholder, isHydrated, isSyncing, progress)) {
        LOGW_WARN(_logger, L"Error in vfsStatus: " << Utility::formatSyncPath(absolutePath).c_str());
        return false;
    }

    return !isPlaceholder || (isHydrated && !isSyncing);
}

#ifdef __APPLE__

ExitCode LocalFileSystemObserverWorker::isEditValid(const NodeId &nodeId, const SyncPath &path, SyncTime lastModifiedLocal,
                                                    bool &valid) const {
    // If the item is a dehydrated placeholder, only metadata update are possible

    bool isPlaceholder = false;
    bool isHydrated = false;
    bool isSyncing = false;
    int progress = 0;
    if (!_syncPal->vfsStatus(path.native(), isPlaceholder, isHydrated, isSyncing, progress)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncPal::vfsStatus");
        return ExitCode::SystemError;
    }

    if (isPlaceholder && !isHydrated) {
        // Check if it is a metadata update
        DbNodeId dbNodeId;
        bool found;
        if (!_syncPal->_syncDb->dbId(ReplicaSide::Local, nodeId, dbNodeId, found)) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbId");
            return ExitCode::DbError;
        }
        if (!found) {
            LOGW_SYNCPAL_WARN(_logger, L"Node not found in node table for nodeId=" << Utility::s2ws(nodeId).c_str());
            return ExitCode::DataError;
        }

        DbNode dbNode;
        if (!_syncPal->_syncDb->node(dbNodeId, dbNode, found)) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::node");
            return ExitCode::DbError;
        }
        if (!found) {
            LOGW_SYNCPAL_WARN(_logger, L"Node not found in node table: nodeId=" << Utility::s2ws(nodeId).c_str());
            return ExitCode::DataError;
        }

        auto lastModifiedRemote = dbNode.lastModifiedRemote();
        valid = (!lastModifiedRemote || *lastModifiedRemote == lastModifiedLocal);
    } else {
        valid = true;
    }

    return ExitCode::Ok;
}

#endif

void LocalFileSystemObserverWorker::sendAccessDeniedError(const SyncPath &absolutePath) {
    LOGW_SYNCPAL_INFO(_logger, L"Access denied on item: " << Utility::formatSyncPath(absolutePath));

    const SyncPath relativePath = CommonUtility::relativePath(_syncPal->localPath(), absolutePath);
    bool isWarning = false;
    bool isExcluded = false;
    IoError ioError = IoError::Success;
    const bool success = ExclusionTemplateCache::instance()->checkIfIsExcluded(_syncPal->localPath(), relativePath, isWarning,
                                                                               isExcluded, ioError);
    if (!success) {
        LOGW_WARN(_logger,
                  L"Error in ExclusionTemplateCache::isExcluded: " << Utility::formatIoError(absolutePath, ioError).c_str());
        return;
    }
    if (isExcluded) {
        return;
    }
    _syncPal->handleAccessDeniedItem(relativePath);
}

ExitInfo LocalFileSystemObserverWorker::exploreDir(const SyncPath &absoluteParentDirPath) {
    // Check if root dir exists
    IoError ioError = IoError::Success;

    ItemType itemType;
    if (!IoHelper::getItemType(absoluteParentDirPath, itemType)) {
        LOGW_WARN(_logger,
                  L"Error in IoHelper::getItemType: " << Utility::formatIoError(absoluteParentDirPath, itemType.ioError).c_str());
        setExitCause(ExitCause::Unknown);
        return ExitCode::SystemError;
    }

    if (itemType.ioError == IoError::NoSuchFileOrDirectory) {
        LOGW_SYNCPAL_WARN(_logger,
                          L"Sync localpath: " << Utility::formatSyncPath(absoluteParentDirPath).c_str() << L" doesn't exist");
        setExitCause(ExitCause::SyncDirDoesntExist);
        return {ExitCode::SystemError, ExitCause::SyncDirDoesntExist};
    }

    if (itemType.ioError == IoError::AccessDenied) {
        LOGW_SYNCPAL_WARN(_logger, L"Sync localpath: " << Utility::formatSyncPath(absoluteParentDirPath).c_str()
                                                       << L" misses read permission");
        setExitCause(ExitCause::SyncDirAccesError);
        return {ExitCode::SystemError, ExitCause::SyncDirAccesError};
    }

    if (itemType.linkType != LinkType::None) {
        return ExitCode::Ok;
    }

    // Process all files
    try {
        IoHelper::DirectoryIterator dirIt;
        if (!IoHelper::getDirectoryIterator(absoluteParentDirPath, true, ioError, dirIt)) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in IoHelper::getDirectoryIterator: "
                                               << Utility::formatIoError(absoluteParentDirPath, ioError));
            setExitCause(ExitCause::Unknown);
            return ExitCode::SystemError;
        }

        if (ioError == IoError::NoSuchFileOrDirectory) {
            LOGW_SYNCPAL_WARN(_logger, L"Sync localpath: " << Utility::formatIoError(absoluteParentDirPath, ioError));
            setExitCause(ExitCause::SyncDirDoesntExist);
            return {ExitCode::SystemError, ExitCause::SyncDirDoesntExist};
        }

        if (ioError == IoError::AccessDenied) {
            LOGW_SYNCPAL_WARN(_logger, L"Sync localpath: " << Utility::formatIoError(absoluteParentDirPath, ioError));
            setExitCause(ExitCause::SyncDirAccesError);
            return {ExitCode::SystemError, ExitCause::SyncDirAccesError};
        }

        DirectoryEntry entry;
        bool endOfDirectory = false;
        while (dirIt.next(entry, endOfDirectory, ioError) && !endOfDirectory && ioError == IoError::Success) {
            if (ParametersCache::isExtendedLogEnabled()) {
                LOGW_SYNCPAL_DEBUG(_logger, L"Item: " << Utility::formatSyncPath(entry.path()) << L" found");
            }

            if (stopAsked()) {
                return ExitCode::Ok;
            }

            const SyncPath absolutePath = entry.path();
            const SyncPath relativePath = CommonUtility::relativePath(_syncPal->localPath(), absolutePath);

            bool toExclude = false;
            bool denyFullControl = false;
            bool isLink = false;

            // Check if the directory entry is managed
            bool isManaged = false;
            if (!Utility::checkIfDirEntryIsManaged(entry, isManaged, isLink, ioError)) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in Utility::checkIfDirEntryIsManaged: "
                                                   << Utility::formatIoError(absoluteParentDirPath, ioError));
                dirIt.disableRecursionPending();
                continue;
            }
            if (ioError == IoError::NoSuchFileOrDirectory) {
                LOGW_SYNCPAL_DEBUG(_logger, L"Directory entry does not exist anymore: "
                                                    << Utility::formatIoError(absoluteParentDirPath, ioError));
                dirIt.disableRecursionPending();
                continue;
            }
            if (ioError == IoError::AccessDenied) {
                LOGW_SYNCPAL_DEBUG(_logger, L"Directory misses search permission: "
                                                    << Utility::formatIoError(absoluteParentDirPath, ioError));
                dirIt.disableRecursionPending();
                sendAccessDeniedError(absolutePath);
                continue;
            }

            if (!isManaged) {
                LOGW_SYNCPAL_DEBUG(_logger, L"Directory entry is not managed: " << Utility::formatSyncPath(absolutePath));
                toExclude = true;
            }

            if (!toExclude) {
                // Check template exclusion
                bool isWarning = false;
                bool isExcluded = false;
                IoError ioError = IoError::Success;
                const bool success = ExclusionTemplateCache::instance()->checkIfIsExcluded(_syncPal->localPath(), relativePath,
                                                                                           isWarning, isExcluded, ioError);
                if (!success) {
                    LOGW_SYNCPAL_DEBUG(_logger, L"Error in ExclusionTemplateCache::isExcluded: "
                                                        << Utility::formatIoError(absolutePath, ioError).c_str());
                    dirIt.disableRecursionPending();
                    continue;
                }
                if (isExcluded) {
                    LOGW_SYNCPAL_INFO(_logger, L"Item: " << Utility::formatSyncPath(absolutePath).c_str()
                                                         << L" rejected because it's excluded");
                    toExclude = true;
                }
            }

            FileStat fileStat;
            NodeId nodeId;
            if (!toExclude) {
                IoError ioError = IoError::Success;
                if (!IoHelper::getFileStat(absolutePath, &fileStat, ioError)) {
                    LOGW_SYNCPAL_DEBUG(_logger, L"Error in IoHelper::getFileStat: "
                                                        << Utility::formatIoError(absolutePath, ioError).c_str());
                    dirIt.disableRecursionPending();
                    continue;
                }

                if (ioError == IoError::NoSuchFileOrDirectory) {
                    LOGW_SYNCPAL_DEBUG(_logger, L"Directory entry does not exist anymore: "
                                                        << Utility::formatSyncPath(absolutePath).c_str());
                    dirIt.disableRecursionPending();
                    continue;
                } else if (ioError == IoError::AccessDenied) {
                    LOGW_SYNCPAL_INFO(_logger, L"Item: " << Utility::formatSyncPath(absolutePath).c_str()
                                                         << L" rejected because access is denied");
                    sendAccessDeniedError(absolutePath);
                    toExclude = true;
                }
                nodeId = std::to_string(fileStat.inode);
            }

            if (toExclude) {
                if (!denyFullControl) {
                    if (!_syncPal->vfsExclude(
                                absolutePath)) { // TODO : This class should never set any attribute or change anything on a file
                        LOGW_SYNCPAL_WARN(_logger, L"Error in vfsExclude : " << Utility::formatSyncPath(absolutePath).c_str());
                    }
                }

                dirIt.disableRecursionPending();
                continue;
            }

            // Get parent folder id
            NodeId parentNodeId;
            if (absolutePath.parent_path() == _rootFolder) {
                parentNodeId = *_syncPal->_syncDb->rootNode().nodeIdLocal();
            } else {
                FileStat parentFileStat;
                IoError ioError = IoError::Success;
                if (!IoHelper::getFileStat(absolutePath.parent_path(), &parentFileStat, ioError)) {
                    LOGW_WARN(_logger, L"Error in IoHelper::getFileStat: "
                                               << Utility::formatIoError(absolutePath.parent_path(), ioError).c_str());
                    setExitCause(ExitCause::FileAccessError);
                    return ExitCode::SystemError;
                }

                if (ioError == IoError::NoSuchFileOrDirectory) {
                    LOGW_SYNCPAL_DEBUG(_logger, L"Directory doesn't exist anymore: "
                                                        << Utility::formatSyncPath(absolutePath.parent_path()).c_str());
                    dirIt.disableRecursionPending();
                    continue;
                } else if (ioError == IoError::AccessDenied) {
                    LOGW_SYNCPAL_DEBUG(_logger, L"Directory misses search permission: "
                                                        << Utility::formatSyncPath(absolutePath.parent_path()).c_str());
                    dirIt.disableRecursionPending();
                    sendAccessDeniedError(absolutePath);
                    continue;
                }

                parentNodeId = std::to_string(parentFileStat.inode);
            }

            if (!IoHelper::getItemType(absolutePath, itemType)) {
                LOGW_SYNCPAL_DEBUG(_logger, L"Error in IoHelper::getItemType: "
                                                    << Utility::formatIoError(absolutePath, itemType.ioError).c_str());
                dirIt.disableRecursionPending();
                continue;
            }
            if (itemType.ioError == IoError::AccessDenied) {
                LOGW_SYNCPAL_DEBUG(_logger, L"getItemType failed for item: "
                                                    << Utility::formatIoError(absoluteParentDirPath, ioError)
                                                    << L". Blacklisting it temporarily");
                sendAccessDeniedError(absolutePath);
            }

            SnapshotItem item(nodeId, parentNodeId, absolutePath.filename().native(), fileStat.creationTime, fileStat.modtime,
                              itemType.nodeType, fileStat.size, isLink, true, true);
            if (_snapshot->updateItem(item)) {
                if (ParametersCache::isExtendedLogEnabled()) {
                    LOGW_SYNCPAL_DEBUG(_logger, L"Item inserted in local snapshot: "
                                                        << Utility::formatSyncPath(absolutePath.filename()).c_str() << L" inode:"
                                                        << Utility::s2ws(nodeId.c_str()) << L" parent inode:"
                                                        << Utility::s2ws(parentNodeId).c_str() << L" createdAt:"
                                                        << fileStat.creationTime << L" modtime:" << fileStat.modtime << L" isDir:"
                                                        << (itemType.nodeType == NodeType::Directory) << L" size:"
                                                        << fileStat.size);
                }
            } else {
                LOGW_SYNCPAL_WARN(_logger, L"Failed to insert item: " << Utility::formatSyncPath(absolutePath.filename()).c_str()
                                                                      << L" into local snapshot!!!");
            }
        }
    } catch (std::filesystem::filesystem_error &e) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(),
                         "Error caught in LocalFileSystemObserverWorker::exploreDir: code=" << e.code() << " error=" << e.what());
        setExitCause(ExitCause::Unknown);
        return ExitCode::SystemError;
    } catch (...) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Error caught in LocalFileSystemObserverWorker::exploreDir");
        return ExitCode::SystemError;
    }

    return ExitCode::Ok;
}

} // namespace KDC
