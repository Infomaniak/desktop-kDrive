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

#include "localfilesystemobserverworker.h"
#include "libcommon/utility/utility.h"
#include "libcommon/log/sentry/ptraces.h"
#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"
#include "requests/parameterscache.h"
#include "requests/exclusiontemplatecache.h"
#include "snapshot/snapshotitem.h"
#include "utility/timerutility.h"

#include <log4cplus/loggingmacros.h>

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
    const std::lock_guard lock(_recursiveMutex);

    // Warning: OperationType retrieved from FSEvent (macOS) seems to be unreliable in some cases. One event might contain
    // several operations. Only Delete event seems to be 100% reliable Move event from outside the synced dir to inside it will
    // be considered by the OS as move while must be considered by the synchronizer as Create.
    if (!_snapshot->isValid()) {
        // Snapshot generation is ongoing, queue the events and process them later
        _pendingFileEvents.insert(_pendingFileEvents.end(), changes.begin(), changes.end());
        return;
    }

    for (const auto &[path, opTypeFromOS]: changes) {
        if (stopAsked()) {
            _pendingFileEvents.clear();
            break;
        }
        sentry::pTraces::scoped::LFSOChangeDetected perfMonitor(syncDbId());
        // Raise flag _updating in order to wait 1sec without local changes before starting the sync
        _updating = true;
        _needUpdateTimerStart = std::chrono::steady_clock::now();

        const SyncPath absolutePath = path.native();
        const SyncPath relativePath = CommonUtility::relativePath(_syncPal->localPath(), absolutePath);
        _syncPal->removeItemFromTmpBlacklist(relativePath);


        if (opTypeFromOS == OperationType::Delete) {
            // Check if exists with same nodeId
            NodeId prevNodeId = _snapshot->itemId(relativePath);
            if (!prevNodeId.empty()) {
                bool existsWithSameId = false;
                NodeId otherNodeId;
                if (auto checkError = IoError::Success;
                    IoHelper::checkIfPathExistsWithSameNodeId(absolutePath, prevNodeId, existsWithSameId, otherNodeId,
                                                              checkError) &&
                    !existsWithSameId) {
                    if (_snapshot->removeItem(prevNodeId)) {
                        LOGW_SYNCPAL_DEBUG(_logger, L"Item removed from local snapshot: " << Utility::formatSyncPath(absolutePath)
                                                                                          << L" (" << Utility::s2ws(prevNodeId)
                                                                                          << L")");
                    }
                    continue;
                }
            }
        }

        FileStat fileStat;
        auto ioError = IoError::Success;
        if (!IoHelper::getFileStat(absolutePath, &fileStat, ioError)) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in IoHelper::getFileStat: " << Utility::formatIoError(absolutePath, ioError));
            tryToInvalidateSnapshot();
            return;
        }

        bool exists = true;
        if (ioError == IoError::AccessDenied) {
            LOGW_SYNCPAL_DEBUG(_logger, L"Item: " << Utility::formatSyncPath(absolutePath) << L" misses search permissions!");
            sendAccessDeniedError(absolutePath);
            continue;
        } else if (ioError == IoError::NoSuchFileOrDirectory) {
            exists = false;
        }

        NodeId nodeId;
        auto nodeType = NodeType::Unknown;
        bool isLink = false;
        if (exists) {
            nodeId = std::to_string(fileStat.inode);
            ItemType itemType;
            if (!IoHelper::getItemType(absolutePath, itemType)) {
                LOGW_SYNCPAL_WARN(_logger,
                                  L"Error in IoHelper::getItemType: " << Utility::formatIoError(absolutePath, itemType.ioError));
                tryToInvalidateSnapshot();
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
            if (bool isWarning = false; ExclusionTemplateCache::instance()->isExcluded(relativePath, isWarning)) {
                if (isWarning) {
                    const Error error(_syncPal->syncDbId(), "", nodeId, nodeType, relativePath, ConflictType::None,
                                      InconsistencyType::None, CancelType::ExcludedByTemplate);
                    _syncPal->addError(error);
                }

                // Check if item still exist in snapshot
                if (const auto itemId = _snapshot->itemId(relativePath); !itemId.empty()) {
                    // Remove it from snapshot
                    (void) _snapshot->removeItem(itemId);
                    LOGW_SYNCPAL_DEBUG(_logger,
                                       L"Item removed from sync because it is hidden: " << Utility::formatSyncPath(absolutePath));
                } else {
                    LOGW_SYNCPAL_DEBUG(_logger,
                                       L"Item not processed because it is excluded: " << Utility::formatSyncPath(absolutePath));
                }

                _syncPal->vfs()->exclude(absolutePath);
                continue;
            }
        }

        if (!exists) {
            // This is a delete operation
            // Get the ID from the snapshot
            const auto itemId = _snapshot->itemId(relativePath);
            if (itemId.empty()) {
                // The file does not exist anymore, ignore it
                continue;
            }

            if (_snapshot->removeItem(itemId)) {
                LOGW_SYNCPAL_DEBUG(_logger, L"Item removed from local snapshot: " << Utility::formatSyncPath(absolutePath)
                                                                                  << L" (" << Utility::s2ws(itemId) << L")");
            } else {
                LOGW_SYNCPAL_WARN(_logger, L"Fail to remove item: " << Utility::formatSyncPath(absolutePath) << L" ("
                                                                    << Utility::s2ws(itemId) << L")");
                tryToInvalidateSnapshot();
                return;
            }

            continue;
        }

        const auto parentPath = absolutePath.parent_path();
        NodeId parentNodeId;
        if (parentPath == _rootFolder) {
            parentNodeId = *_syncPal->_syncDb->rootNode().nodeIdLocal();
        } else {
            if (!IoHelper::getNodeId(parentPath, parentNodeId)) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in IoHelper::getNodeId for " << Utility::formatSyncPath(parentPath));
                continue;
            }
        }

        if (opTypeFromOS == OperationType::Edit || opTypeFromOS == OperationType::Rights) {
            // Filter out hydration/dehydration
            bool changed = false;
            const bool success =
                    IoHelper::checkIfFileChanged(absolutePath, _snapshot->size(nodeId), _snapshot->lastModified(nodeId),
                                                 _snapshot->createdAt(nodeId), changed, ioError);
            if (!success) {
                LOGW_SYNCPAL_WARN(_logger,
                                  L"Error in IoHelper::checkIfFileChanged: " << Utility::formatIoError(absolutePath, ioError));
            }
            if (ioError == IoError::AccessDenied) {
                LOGW_SYNCPAL_DEBUG(_logger, L"Item: " << Utility::formatSyncPath(absolutePath) << L" misses search permissions!");
                sendAccessDeniedError(absolutePath);
            } else if (ioError == IoError::NoSuchFileOrDirectory) {
                continue;
            }

            if (!changed) {
#ifdef _WIN32
                VfsStatus vfsStatus;
                if (ExitInfo exitInfo = _syncPal->vfs()->status(absolutePath, vfsStatus); !exitInfo) {
                    LOGW_SYNCPAL_WARN(_logger,
                                      L"Error in vfsStatus: " << Utility::formatSyncPath(absolutePath) << L": " << exitInfo);
                    tryToInvalidateSnapshot();
                    return;
                }

                if (vfsStatus.isPlaceholder) {
                    const PinState pinState = _syncPal->vfs()->pinState(absolutePath);
                    if ((vfsStatus.isHydrated && pinState == PinState::OnlineOnly) ||
                        (!vfsStatus.isHydrated && pinState == PinState::AlwaysLocal)) {
                        // Change status in order to start hydration/dehydration
                        // TODO : FileSystemObserver should not change file status, it should only monitor file system
                        if (!_syncPal->vfs()->fileStatusChanged(absolutePath, SyncFileStatus::Syncing)) {
                            LOGW_SYNCPAL_WARN(_logger, L"Error in SyncPal::vfsFileStatusChanged: "
                                                               << Utility::formatSyncPath(absolutePath));
                            tryToInvalidateSnapshot();
                            return;
                        }
                    }
                }

#else
                if (ParametersCache::isExtendedLogEnabled()) {
                    LOGW_SYNCPAL_DEBUG(_logger, L"Ignoring spurious edit notification on file: "
                                                        << Utility::formatSyncPath(absolutePath) << L" (" << Utility::s2ws(nodeId)
                                                        << L")");
                }
#endif

                continue;
            }
        }

        if (const bool itemExistsInSnapshot = _snapshot->exists(nodeId); !itemExistsInSnapshot) {
            if (opTypeFromOS == OperationType::Delete) {
                // The node ID of the deleted item is different from `nodeId`. The latter is the identifier of an item with the
                // same path as the deleted item and that exists on the file system at the time of the last check. This situation
                // happens for instance if a file is deleted while another file with the same path has is recreated shortly
                // afterward. Typically, editors of the MS suite (xlsx, docx) or Adobe suite (pdf) perform a
                // Delete-followed-by-Create operation during a single edit.
                NodeId itemId = _snapshot->itemId(relativePath);
                if (_snapshot->removeItem(itemId)) {
                    LOGW_SYNCPAL_DEBUG(_logger, L"Item removed from local snapshot: " << Utility::formatSyncPath(absolutePath)
                                                                                      << L" (" << Utility::s2ws(itemId) << L")");
                } else {
                    LOGW_SYNCPAL_WARN(_logger, L"Failed to remove item: " << Utility::formatSyncPath(absolutePath) << L" ("
                                                                          << Utility::s2ws(itemId) << L")");
                    tryToInvalidateSnapshot();
                    return;
                }
                continue;
            }

            if (_snapshot->pathExists(relativePath)) {
                NodeId previousItemId = _snapshot->itemId(relativePath);
                // If an item with the same path already exists, remove it from snapshot because its ID might have changed (i.e.
                // the file has been downloaded in the tmp folder then moved to override the existing one). The item will be
                // inserted below anyway.
                if (_snapshot->removeItem(previousItemId)) {
                    LOGW_SYNCPAL_DEBUG(_logger, L"Item removed from local snapshot: " << Utility::formatSyncPath(absolutePath)
                                                                                      << L" (" << Utility::s2ws(previousItemId)
                                                                                      << L")");
                } else {
                    LOGW_SYNCPAL_WARN(_logger, L"Failed to delete item: " << Utility::formatSyncPath(absolutePath) << L" ("
                                                                          << Utility::s2ws(previousItemId) << L")");
                    tryToInvalidateSnapshot();
                    return;
                }
            }

            // This can be either Create, Move or Edit operation
            SnapshotItem item(nodeId, parentNodeId, absolutePath.filename().native(), fileStat.creationTime, fileStat.modtime,
                              nodeType, fileStat.size, isLink, true, true);

            if (!_snapshot->updateItem(item)) {
                LOGW_SYNCPAL_WARN(_logger, L"Failed to insert item: " << Utility::formatSyncPath(absolutePath) << L" ("
                                                                      << Utility::s2ws(nodeId) << L")");
                tryToInvalidateSnapshot();
                return;
            }

            if (ParametersCache::isExtendedLogEnabled()) {
                LOGW_SYNCPAL_DEBUG(_logger, L"Item inserted in local snapshot: " << Utility::formatSyncPath(absolutePath) << L" ("
                                                                                 << Utility::s2ws(nodeId) << L") at "
                                                                                 << fileStat.modtime);

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
                    LOGW_SYNCPAL_WARN(_logger, L"Ignore item: " << Utility::formatSyncPath(absolutePath) << L" because size > "
                                                                << CommonUtility::maxPathLength());
                    continue;
                }

                if (!exploreDir(absolutePath, true)) {
                    // Error while exploring directory, we need to invalidate the snapshot
                    tryToInvalidateSnapshot();
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
                    tryToInvalidateSnapshot();
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
                LOGW_SYNCPAL_DEBUG(_logger, L"Item: " << Utility::formatSyncPath(absolutePath) << L" (" << Utility::s2ws(nodeId)
                                                      << L") updated in local snapshot at " << fileStat.modtime);
            }

            if (nodeType == NodeType::File) {
                //                if (canComputeChecksum(absolutePath)) {
                //                    // Start asynchronous checkum generation
                //                    _checksumWorker->computeChecksum(nodeId, absolutePath);
                //                }
            }
        } else {
            LOGW_SYNCPAL_WARN(_logger, L"Failed to update item: " << Utility::formatSyncPath(absolutePath) << L" ("
                                                                  << Utility::s2ws(nodeId) << L")");
            tryToInvalidateSnapshot();
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
            tryToInvalidateSnapshot();
            break;
        }
        if (!_folderWatcher->exitInfo()) {
            LOG_SYNCPAL_WARN(_logger, "Error in FolderWatcher: " << _folderWatcher->exitInfo());
            exitCode = _folderWatcher->exitInfo().code();
            setExitCause(_folderWatcher->exitInfo().cause());
            tryToInvalidateSnapshot();
            break;
        }
        // We never pause this thread

        if (!isFolderWatcherReliable() &&
            (std::chrono::steady_clock::now() - timerStart).count() * 1000 > defaultDiscoveryInterval) {
            // The folder watcher became unreliable so fallback to static synchronization
            timerStart = std::chrono::steady_clock::now();
            tryToInvalidateSnapshot();
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
        if (_initializing) _initializing = false;
        Utility::msleep(LOOP_EXEC_SLEEP_PERIOD);
    }
    LOG_SYNCPAL_DEBUG(_logger, "Worker stopped: name=" << name().c_str());
    setDone(exitCode);
}

ExitCode LocalFileSystemObserverWorker::generateInitialSnapshot() {
    LOG_SYNCPAL_INFO(_logger, "Starting local snapshot generation");
    const TimerUtility timer;
    auto perfMonitor = sentry::pTraces::scoped::LFSOGenerateInitialSnapshot(syncDbId());

    _snapshot->init();
    _updating = true;

    ExitCode res = exploreDir(_rootFolder);

    if (res == ExitCode::Ok && !stopAsked()) {
        _snapshot->setValid(true);
        LOG_SYNCPAL_INFO(_logger, "Local snapshot generated in: " << timer.elapsed().count() << "s for " << _snapshot->nbItems()
                                                                  << " items");
        perfMonitor.stop();
    } else if (stopAsked()) {
        LOG_SYNCPAL_INFO(_logger, "Local snapshot generation stopped after: " << timer.elapsed().count() << "s");
    } else {
        LOG_SYNCPAL_WARN(_logger, "Local snapshot generation failed after: " << timer.elapsed().count() << "s");
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
    VfsStatus vfsStatus;
    if (const auto exitInfo = _syncPal->vfs()->status(absolutePath, vfsStatus); !exitInfo) {
        LOGW_WARN(_logger, L"Error in vfsStatus: " << Utility::formatSyncPath(absolutePath) << L": " << exitInfo);
        return exitInfo;
    }

    return !vfsStatus.isPlaceholder || (vfsStatus.isHydrated && !vfsStatus.isSyncing);
}

#ifdef __APPLE__

ExitCode LocalFileSystemObserverWorker::isEditValid(const NodeId &nodeId, const SyncPath &path, SyncTime lastModifiedLocal,
                                                    bool &valid) const {
    // If the item is a dehydrated placeholder, only metadata update are possible
    VfsStatus vfsStatus;
    if (!_syncPal->vfs()->status(path.native(), vfsStatus)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncPal::vfsStatus");
        return ExitCode::SystemError;
    }

    if (vfsStatus.isPlaceholder && !vfsStatus.isHydrated) {
        // Check if it is a metadata update
        DbNodeId dbNodeId = 0;
        bool found = false;
        if (!_syncPal->_syncDb->dbId(ReplicaSide::Local, nodeId, dbNodeId, found)) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbId");
            return ExitCode::DbError;
        }
        if (!found) {
            LOGW_SYNCPAL_WARN(_logger, L"Node not found in node table for nodeId=" << Utility::s2ws(nodeId));
            return ExitCode::DataError;
        }

        DbNode dbNode;
        if (!_syncPal->_syncDb->node(dbNodeId, dbNode, found)) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::node");
            return ExitCode::DbError;
        }
        if (!found) {
            LOGW_SYNCPAL_WARN(_logger, L"Node not found in node table: nodeId=" << Utility::s2ws(nodeId));
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
    if (ExclusionTemplateCache::instance()->isExcluded(relativePath)) {
        return;
    }
    (void) _syncPal->handleAccessDeniedItem(relativePath);
}

ExitInfo LocalFileSystemObserverWorker::exploreDir(const SyncPath &absoluteParentDirPath, bool fromChangeDetected) {
    // Check if root dir exists
    auto ioError = IoError::Success;

    ItemType itemType;
    if (!IoHelper::getItemType(absoluteParentDirPath, itemType)) {
        LOGW_WARN(_logger,
                  L"Error in IoHelper::getItemType: " << Utility::formatIoError(absoluteParentDirPath, itemType.ioError));
        setExitCause(ExitCause::Unknown);
        return ExitCode::SystemError;
    }

    if (itemType.ioError == IoError::NoSuchFileOrDirectory) {
        LOGW_SYNCPAL_WARN(_logger, L"Local " << Utility::formatSyncPath(absoluteParentDirPath) << L" doesn't exist");
        setExitCause(ExitCause::SyncDirDoesntExist);
        return {ExitCode::SystemError, ExitCause::SyncDirDoesntExist};
    }

    if (itemType.ioError == IoError::AccessDenied) {
        LOGW_SYNCPAL_WARN(_logger, L"Local " << Utility::formatSyncPath(absoluteParentDirPath) << L" misses read permission");
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
            LOGW_SYNCPAL_WARN(_logger, L"Local " << Utility::formatIoError(absoluteParentDirPath, ioError));
            setExitCause(ExitCause::SyncDirDoesntExist);
            return {ExitCode::SystemError, ExitCause::SyncDirDoesntExist};
        }

        if (ioError == IoError::AccessDenied) {
            LOGW_SYNCPAL_WARN(_logger, L"Local " << Utility::formatIoError(absoluteParentDirPath, ioError));
            setExitCause(ExitCause::SyncDirAccesError);
            return {ExitCode::SystemError, ExitCause::SyncDirAccesError};
        }
        DirectoryEntry entry;
        bool endOfDirectory = false;
        sentry::pTraces::counterScoped::LFSOExploreItem perfMonitor(fromChangeDetected, syncDbId());
        while (dirIt.next(entry, endOfDirectory, ioError) && !endOfDirectory && ioError == IoError::Success) {
            perfMonitor.start();

            if (ParametersCache::isExtendedLogEnabled()) {
                LOGW_SYNCPAL_DEBUG(_logger, L"Item: " << Utility::formatSyncPath(entry.path()) << L" found");
            }

            if (stopAsked()) {
                return ExitCode::Ok;
            }

            const auto &absolutePath = entry.path();
            const auto relativePath = CommonUtility::relativePath(_syncPal->localPath(), absolutePath);

            if (!IoHelper::getItemType(absolutePath, itemType)) {
                LOGW_SYNCPAL_DEBUG(_logger,
                                   L"Error in IoHelper::getItemType: " << Utility::formatIoError(absolutePath, itemType.ioError));
                dirIt.disableRecursionPending();
                continue;
            }
            if (itemType.ioError == IoError::AccessDenied) {
                LOGW_SYNCPAL_DEBUG(_logger, L"getItemType failed for item: "
                                                    << Utility::formatIoError(absoluteParentDirPath, ioError)
                                                    << L". Blacklisting it temporarily");
                sendAccessDeniedError(absolutePath);
            }

            bool toExclude = false;
            const bool isLink = itemType.linkType != LinkType::None;

            // Check if the directory entry is managed
            bool isManaged = false;
            if (!Utility::checkIfDirEntryIsManaged(entry, isManaged, ioError, itemType)) {
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
                if (ExclusionTemplateCache::instance()->isExcluded(relativePath)) {
                    LOGW_SYNCPAL_INFO(_logger,
                                      L"Item: " << Utility::formatSyncPath(absolutePath) << L" rejected because it is excluded");
                    toExclude = true;
                }
            }

            FileStat fileStat;
            NodeId nodeId;
            if (!toExclude) {
                if (!IoHelper::getFileStat(absolutePath, &fileStat, ioError)) {
                    LOGW_SYNCPAL_DEBUG(_logger,
                                       L"Error in IoHelper::getFileStat: " << Utility::formatIoError(absolutePath, ioError));
                    dirIt.disableRecursionPending();
                    continue;
                }

                if (ioError == IoError::NoSuchFileOrDirectory) {
                    LOGW_SYNCPAL_DEBUG(_logger,
                                       L"Directory entry does not exist anymore: " << Utility::formatSyncPath(absolutePath));
                    dirIt.disableRecursionPending();
                    continue;
                } else if (ioError == IoError::AccessDenied) {
                    LOGW_SYNCPAL_INFO(
                            _logger, L"Item: " << Utility::formatSyncPath(absolutePath) << L" rejected because access is denied");
                    sendAccessDeniedError(absolutePath);
                    toExclude = true;
                }
                nodeId = std::to_string(fileStat.inode);
            }

            if (toExclude) {
                dirIt.disableRecursionPending();
                continue;
            }

            // Get parent folder id
            NodeId parentNodeId;
            if (absolutePath.parent_path() == _rootFolder) {
                parentNodeId = *_syncPal->_syncDb->rootNode().nodeIdLocal();
            } else {
                parentNodeId = snapshot()->itemId(relativePath.parent_path());
                if (parentNodeId.empty()) {
                    FileStat parentFileStat;
                    if (!IoHelper::getFileStat(absolutePath.parent_path(), &parentFileStat, ioError)) {
                        LOGW_WARN(_logger, L"Error in IoHelper::getFileStat: "
                                                   << Utility::formatIoError(absolutePath.parent_path(), ioError));
                        setExitCause(ExitCause::FileAccessError);
                        return ExitCode::SystemError;
                    }

                    if (ioError == IoError::NoSuchFileOrDirectory) {
                        LOGW_SYNCPAL_DEBUG(_logger, L"Directory doesn't exist anymore: "
                                                            << Utility::formatSyncPath(absolutePath.parent_path()));
                        dirIt.disableRecursionPending();
                        continue;
                    } else if (ioError == IoError::AccessDenied) {
                        LOGW_SYNCPAL_DEBUG(_logger, L"Directory misses search permission: "
                                                            << Utility::formatSyncPath(absolutePath.parent_path()));
                        dirIt.disableRecursionPending();
                        sendAccessDeniedError(absolutePath);
                        continue;
                    }
                    parentNodeId = std::to_string(parentFileStat.inode);
                }
            }

            const SnapshotItem item(nodeId, parentNodeId, absolutePath.filename().native(), fileStat.creationTime,
                                    fileStat.modtime, itemType.nodeType, fileStat.size, isLink, true, true);
            if (_snapshot->updateItem(item)) {
                if (ParametersCache::isExtendedLogEnabled()) {
                    LOGW_SYNCPAL_DEBUG(_logger, L"Item inserted in local snapshot: "
                                                        << Utility::formatSyncPath(absolutePath.filename()) << L" inode:"
                                                        << Utility::s2ws(nodeId) << L" parent inode:"
                                                        << Utility::s2ws(parentNodeId) << L" createdAt:" << fileStat.creationTime
                                                        << L" modtime:" << fileStat.modtime << L" isDir:"
                                                        << (itemType.nodeType == NodeType::Directory) << L" size:"
                                                        << fileStat.size << L" isLink:" << isLink);
                }
            } else {
                LOGW_SYNCPAL_WARN(_logger, L"Failed to insert item: " << Utility::formatSyncPath(absolutePath.filename())
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
