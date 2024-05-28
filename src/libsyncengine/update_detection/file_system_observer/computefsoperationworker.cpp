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

#include "computefsoperationworker.h"
#include "requests/parameterscache.h"
#include "requests/syncnodecache.h"
#include "requests/exclusiontemplatecache.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/io/iohelper.h"
#include "reconciliation/platform_inconsistency_checker/platforminconsistencycheckerutility.h"

namespace KDC {

ComputeFSOperationWorker::ComputeFSOperationWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name,
                                                   const std::string &shortName)
    : ISyncWorker(syncPal, name, shortName), _syncDb(syncPal->_syncDb) {}

void ComputeFSOperationWorker::execute() {
    ExitCode exitCode(ExitCodeUnknown);

    LOG_SYNCPAL_DEBUG(_logger, "Worker started: name=" << name().c_str());
    auto start = std::chrono::steady_clock::now();

    // Update the sync parameters
    bool ok = true;
    bool found = true;
    if (!ParmsDb::instance()->selectSync(_syncPal->syncDbId(), _sync, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in ParmsDb::selectSync");
        setExitCause(ExitCauseDbAccessError);
        exitCode = ExitCodeDbError;
        ok = false;
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "Sync not found");
        exitCode = ExitCodeDataError;
        ok = false;
    }
    if (!ok) {
        setDone(exitCode);
        LOG_SYNCPAL_DEBUG(_logger, "Worker stopped: name=" << name().c_str());
        return;
    }

    _syncPal->_localOperationSet->clear();
    _syncPal->_remoteOperationSet->clear();

    // Update SyncNode cache
    _syncPal->updateSyncNode();

    // Update unsynced list cache
    updateUnsyncedList();

    _fileSizeMismatchMap.clear();

    std::unordered_set<NodeId> localIdsSet;
    std::unordered_set<NodeId> remoteIdsSet;
    if (ok && !stopAsked()) {
        exitCode = exploreDbTree(localIdsSet, remoteIdsSet);
        ok = exitCode == ExitCodeOk;
    }

    if (ok && !stopAsked()) {
        exitCode = exploreSnapshotTree(ReplicaSideLocal, localIdsSet);
        ok = exitCode == ExitCodeOk;
        if (ok) {
            exitCode = exploreSnapshotTree(ReplicaSideRemote, remoteIdsSet);
        }
    }

    if (!ok || stopAsked()) {
        // Do not keep operations if there was an error or sync was stopped
        _syncPal->_localOperationSet->clear();
        _syncPal->_remoteOperationSet->clear();
    } else {
        exitCode = ExitCodeOk;
    }

    std::chrono::duration<double> elapsed_seconds = std::chrono::steady_clock::now() - start;
    LOG_SYNCPAL_INFO(_logger, "FS operation sets generated in: " << elapsed_seconds.count() << "s");

    setDone(exitCode);
    LOG_SYNCPAL_DEBUG(_logger, "Worker stopped: name=" << name().c_str());
}

ExitCode ComputeFSOperationWorker::exploreDbTree(std::unordered_set<NodeId> &localIdsSet,
                                                 std::unordered_set<NodeId> &remoteIdsSet) {
    bool found = false;
    std::unordered_set<DbNodeId> remainingDbIds;
    if (!_syncDb->dbIds(remainingDbIds, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::ids");
        setExitCause(ExitCauseDbAccessError);
        return ExitCodeDbError;
    } else if (!found) {
        LOG_SYNCPAL_DEBUG(_logger, "No items found in db");
        return ExitCodeOk;
    }

    // Explore the tree twice:
    // First compute operations only for directories
    // Then compute operations for files
    _dirPathToDeleteSet.clear();
    for (int i = 0; i <= 1; i++) {
        bool checkOnlyDir = i == 0;

        auto dbIt = remainingDbIds.begin();
        for (; dbIt != remainingDbIds.end();) {
            DbNodeId dbId = *dbIt;

            if (dbId == _syncPal->_syncDb->rootNode().nodeId()) {
                // Ignore root folder
                dbIt = remainingDbIds.erase(dbIt);
                continue;
            }

            if (stopAsked()) {
                return ExitCodeOk;
            }

            while (pauseAsked() || isPaused()) {
                if (!isPaused()) {
                    setPauseDone();
                }

                Utility::msleep(LOOP_PAUSE_SLEEP_PERIOD);

                if (unpauseAsked()) {
                    setUnpauseDone();
                }
            }

            DbNode dbNode;
            if (!_syncDb->node(dbId, dbNode, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::node");
                setExitCause(ExitCauseDbAccessError);
                return ExitCodeDbError;
            }
            if (!found) {
                LOG_SYNCPAL_DEBUG(_logger, "Failed to retrieve node for dbId=" << dbId);
                setExitCause(ExitCauseDbEntryNotFound);
                return ExitCodeDataError;
            }

            if (dbNode.nodeIdLocal().has_value()) {
                localIdsSet.insert(*dbNode.nodeIdLocal());
            }
            if (dbNode.nodeIdRemote().has_value()) {
                remoteIdsSet.insert(*dbNode.nodeIdRemote());
            }

            if (checkOnlyDir && dbNode.type() != NodeTypeDirectory) {
                // In first loop, we check only directory
                dbIt++;
                continue;
            }

            // Remove directory ID from list so 2nd iteration will be a bit faster
            dbIt = remainingDbIds.erase(dbIt);

            SyncPath localDbPath;
            SyncPath remoteDbPath;
            if (!_syncDb->path(dbId, localDbPath, remoteDbPath, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::path");
                setExitCause(ExitCauseDbAccessError);
                return ExitCodeDbError;
            }
            if (!found) {
                LOG_SYNCPAL_DEBUG(_logger, "Failed to retrieve node for dbId=" << dbId);
                setExitCause(ExitCauseDbEntryNotFound);
                return ExitCodeDataError;
            }

            for (int j = 0; j <= 1; j++) {
                ReplicaSide side = j == 0 ? ReplicaSideLocal : ReplicaSideRemote;
                SyncTime dbLastModified =
                    side == ReplicaSideLocal
                        ? (dbNode.lastModifiedLocal().has_value() ? dbNode.lastModifiedLocal().value() : 0)
                        : (dbNode.lastModifiedRemote().has_value() ? dbNode.lastModifiedRemote().value() : 0);
                NodeId nodeId = side == ReplicaSideLocal
                                    ? (dbNode.nodeIdLocal().has_value() ? dbNode.nodeIdLocal().value() : "")
                                    : (dbNode.nodeIdRemote().has_value() ? dbNode.nodeIdRemote().value() : "");
                if (nodeId.empty()) {
                    LOGW_SYNCPAL_WARN(_logger, Utility::s2ws(Utility::side2Str(side)).c_str()
                                                   << L" node ID empty for for dbId=" << dbId);
                    setExitCause(ExitCauseDbEntryNotFound);
                    return ExitCodeDataError;
                }

                SyncName dbName = side == ReplicaSideLocal ? dbNode.nameLocal() : dbNode.nameRemote();
                const SyncPath &dbPath = side == ReplicaSideLocal ? localDbPath : remoteDbPath;
                const std::shared_ptr<Snapshot> snapshot =
                    side == ReplicaSideLocal ? _syncPal->_localSnapshot : _syncPal->_remoteSnapshot;
                std::shared_ptr<FSOperationSet> opSet =
                    side == ReplicaSideLocal ? _syncPal->_localOperationSet : _syncPal->_remoteOperationSet;

                NodeId parentId;
                if (!_syncDb->parent(side, nodeId, parentId, found)) {
                    LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::parent");
                    setExitCause(ExitCauseDbAccessError);
                    return ExitCodeDbError;
                }
                if (!found) {
                    LOG_SYNCPAL_DEBUG(_logger, "Failed to retrieve node for dbId=" << nodeId.c_str());
                    setExitCause(ExitCauseDbEntryNotFound);
                    return ExitCodeDataError;
                }

                bool remoteItemUnsynced = false;
                bool movedIntoUnsyncedFolder = false;
                if (side == ReplicaSideRemote) {
                    // In case of a move inside an excluded folder, the item must be removed in this sync
                    if (isInUnsyncedList(nodeId, ReplicaSideRemote)) {
                        remoteItemUnsynced = true;
                        if (parentId != snapshot->parentId(nodeId)) {
                            movedIntoUnsyncedFolder = true;
                        }
                    }
                } else {
                    if (isInUnsyncedList(nodeId, ReplicaSideLocal)) {
                        continue;
                    }
                }

                if (!snapshot->exists(nodeId) || movedIntoUnsyncedFolder) {
                    if (!pathInDeletedFolder(dbPath)) {
                        // Check that the file/directory really does not exist on replica
                        bool isExcluded = false;
                        ExitCode exitCode = checkIfOkToDelete(side, dbPath, nodeId, isExcluded);
                        if (exitCode != ExitCodeOk) {
                            if (exitCode == ExitCodeNoWritePermission) {
                                // Blacklist node
                                _syncPal->blacklistTemporarily(nodeId, dbPath, side);
                                Error error(_syncPal->_syncDbId, "", "", NodeTypeDirectory, dbPath, ConflictTypeNone,
                                            InconsistencyTypeNone, CancelTypeNone, "", ExitCodeSystemError,
                                            ExitCauseFileAccessError);
                                _syncPal->addError(error);

                                // Update unsynced list cache
                                updateUnsyncedList();
                                continue;
                            } else {
                                return exitCode;
                            }
                        }

                        if (isExcluded) continue;   // Never generate operation on excluded file
                    }

                    if (isInUnsyncedList(snapshot, nodeId, side, true)) {
                        // Ignore operation
                        continue;
                    }

                    if (side == ReplicaSideLocal) {
                        SyncPath localPath = _syncPal->_localPath / dbPath;

                        // Do not propagate delete if path too long
                        size_t pathSize = localPath.native().size();
                        if (PlatformInconsistencyCheckerUtility::instance()->checkPathLength(pathSize, dbNode.type())) {
                            LOGW_SYNCPAL_WARN(_logger, L"Path length too big (" << pathSize << L" characters) for item "
                                                                                << Path2WStr(localPath).c_str()
                                                                                << L". Item is ignored.");
                            continue;
                        }

                        if (!snapshot->exists(nodeId)) {
                            bool exists = false;
                            IoError ioError = IoErrorSuccess;
                            if (!IoHelper::checkIfPathExists(localPath, exists, ioError)) {
                                LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: "
                                                       << Utility::formatIoError(localPath, ioError).c_str());
                                return ExitCodeSystemError;
                            }
                            if (exists) {
                                bool warn = false;
                                bool isExcluded = false;
                                const bool success = ExclusionTemplateCache::instance()->checkIfIsExcluded(
                                    _syncPal->_localPath, dbPath, warn, isExcluded, ioError);
                                if (!success) {
                                    LOGW_WARN(_logger, L"Error in ExclusionTemplateCache::checkIfIsExcluded: "
                                                           << Utility::formatIoError(localPath, ioError).c_str());
                                    return ExitCodeSystemError;
                                }
                                if (isExcluded) {
                                    // The item is excluded
                                    continue;
                                }
                            }
                        }
                    }

                    // Delete operation
                    FSOpPtr fsOp = std::make_shared<FSOperation>(OperationType::OperationTypeDelete, nodeId, dbNode.type(),
                                                                 dbNode.created().has_value() ? dbNode.created().value() : 0,
                                                                 dbLastModified, dbNode.size(), dbPath);
                    opSet->insertOp(fsOp);
                    logOperationGeneration(snapshot->side(), fsOp);

                    if (dbNode.type() == NodeTypeDirectory) {
                        addFolderToDelete(dbPath);
                    }
                    continue;
                }

                if (remoteItemUnsynced) {
                    // Ignore operations on unsynced items
                    continue;
                }

                SyncPath snapPath;
                if (!snapshot->path(nodeId, snapPath)) {
                    LOGW_SYNCPAL_WARN(_logger, L"Failed to retrieve path from snapshot for item "
                                                   << SyncName2WStr(dbName).c_str() << L" (" << Utility::s2ws(nodeId).c_str()
                                                   << L")");
                    setExitCause(ExitCauseInvalidSnapshot);
                    return ExitCodeDataError;
                }

                if (side == ReplicaSideLocal) {
                    // OS might fail to notify all delete events, therefore we check that the file still exists.
                    SyncPath absolutePath = _syncPal->_localPath / snapPath;
                    bool exists = false;
                    IoError ioError = IoErrorSuccess;
                    if (!IoHelper::checkIfPathExists(absolutePath, exists, ioError)) {
                        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: "
                                               << Utility::formatIoError(absolutePath, ioError).c_str());
                        return ExitCodeSystemError;
                    }
                    if (!exists) {
                        LOGW_DEBUG(_logger, L"Item does not exist anymore on local replica. Snapshot will be rebuilt - path="
                                                << Path2WStr(absolutePath).c_str());
                        setExitCause(ExitCauseInvalidSnapshot);
                        return ExitCodeDataError;
                    }
                } else {
                    // Check path length
                    if (isPathTooLong(snapPath, nodeId, snapshot->type(nodeId))) {
                        continue;
                    }
                }

                const SyncTime snapshotLastModified = snapshot->lastModified(nodeId);
                if (snapshotLastModified != dbLastModified && dbNode.type() == NodeType::NodeTypeFile) {
                    // Edit operation
                    FSOpPtr fsOp = std::make_shared<FSOperation>(OperationType::OperationTypeEdit, nodeId, NodeType::NodeTypeFile,
                                                                 snapshot->createdAt(nodeId), snapshotLastModified,
                                                                 snapshot->size(nodeId), snapPath);
                    opSet->insertOp(fsOp);
                    logOperationGeneration(snapshot->side(), fsOp);
                }

                bool movedOrRenamed = dbName != snapshot->name(nodeId) || parentId != snapshot->parentId(nodeId);
                if (movedOrRenamed) {
                    FSOpPtr fsOp = nullptr;
                    if (isInUnsyncedList(snapshot, nodeId, side)) {
                        // Delete operation
                        fsOp = std::make_shared<FSOperation>(OperationType::OperationTypeDelete
                                                             , nodeId
                                                             , dbNode.type()
                                                             ,snapshot->createdAt(nodeId)
                                                             , snapshotLastModified
                                                             ,snapshot->size(nodeId)
                                                             ,remoteDbPath  // We use the remotePath anyway here to display
                                                                                 // notifications with the real (remote) name
                                                             ,snapPath);
                    } else {
                        // Move operation
                        fsOp = std::make_shared<FSOperation>(OperationType::OperationTypeMove
                                                             , nodeId
                                                             , dbNode.type()
                                                             ,snapshot->createdAt(nodeId)
                                                             , snapshotLastModified
                                                             ,snapshot->size(nodeId)
                                                             ,remoteDbPath  // We use the remotePath anyway here to display
                                                                                 // notifications with the real (remote) name
                                                             ,snapPath);
                    }

                    opSet->insertOp(fsOp);
                    logOperationGeneration(snapshot->side(), fsOp);
                }
            }

            // Check file integrity
            ExitCode fileIntegrityOk = checkFileIntegrity(dbNode);
            if (fileIntegrityOk != ExitCodeOk) {
                return fileIntegrityOk;
            }
        }
    }

    return ExitCodeOk;
}

ExitCode ComputeFSOperationWorker::exploreSnapshotTree(ReplicaSide side, const std::unordered_set<NodeId> &idsSet) {
    const std::shared_ptr<Snapshot> snapshot = side == ReplicaSideLocal ? _syncPal->_localSnapshot : _syncPal->_remoteSnapshot;
    std::shared_ptr<FSOperationSet> opSet =
        side == ReplicaSideLocal ? _syncPal->_localOperationSet : _syncPal->_remoteOperationSet;

    std::unordered_set<NodeId> remainingDbIds;
    snapshot->ids(remainingDbIds);
    if (remainingDbIds.empty()) {
        LOG_SYNCPAL_DEBUG(_logger, "No items found in snapshot on side " << Utility::side2Str(side).c_str());
        return ExitCodeOk;
    }

    // Explore the tree twice:
    // First compute operations only for directories
    // Then compute operations for files
    for (int i = 0; i <= 1; i++) {
        bool checkOnlyDir = i == 0;

        auto snapIdIt = remainingDbIds.begin();
        for (; snapIdIt != remainingDbIds.end();) {
            if (stopAsked()) {
                return ExitCodeOk;
            }

            while (pauseAsked() || isPaused()) {
                if (!isPaused()) {
                    setPauseDone();
                }

                Utility::msleep(LOOP_PAUSE_SLEEP_PERIOD);

                if (unpauseAsked()) {
                    setUnpauseDone();
                }
            }

            if (*snapIdIt == snapshot->rootFolderId()) {
                // Ignore root folder
                snapIdIt = remainingDbIds.erase(snapIdIt);
                continue;  // Do not process root node
            }

            if (idsSet.find(*snapIdIt) != idsSet.end()) {
                // Do not process ids that are already in db
                snapIdIt = remainingDbIds.erase(snapIdIt);
                continue;
            }

            const NodeId nodeId = *snapIdIt;
            const NodeType type = snapshot->type(nodeId);
            if (checkOnlyDir && type != NodeTypeDirectory) {
                // In first loop, we check only directories
                snapIdIt++;
                continue;
            }

            // Remove directory ID from list so 2nd iteration will be a bit faster
            snapIdIt = remainingDbIds.erase(snapIdIt);

            if (snapshot->isOrphan(snapshot->parentId(nodeId))) {
                // Ignore orphans
                if (ParametersCache::instance()->parameters().extendedLog()) {
                    LOGW_SYNCPAL_DEBUG(_logger, L"Ignoring orphan node " << SyncName2WStr(snapshot->name(nodeId)).c_str() << L" ("
                                                                        << Utility::s2ws(nodeId).c_str() << L")");
                }
                continue;
            }

            SyncPath snapPath;
            if (!snapshot->path(nodeId, snapPath)) {
                LOGW_SYNCPAL_WARN(_logger, L"Failed to retrieve path from snapshot for item " << Utility::s2ws(nodeId).c_str());
                setExitCause(ExitCauseInvalidSnapshot);
                return ExitCodeDataError;
            }

            // Manage file exclusion
            const int64_t snapshotSize = snapshot->size(nodeId);
            if (isExcludedFromSync(snapshot, side, nodeId, snapPath, type, snapshotSize)) {
                continue;
            }

            if (side == ReplicaSideLocal) {
                // Check if a local file is hidden, hence excluded.
                bool isExcluded = false;
                IoError ioError = IoErrorSuccess;
                const bool success = ExclusionTemplateCache::instance()->checkIfIsAnExcludedHiddenFile(
                    _syncPal->_localPath, snapPath, isExcluded, ioError);
                if (!success || ioError != IoErrorSuccess || isExcluded) {
                    continue;
                }

                const bool isLink = _syncPal->_localSnapshot->isLink(nodeId);

                if (type == NodeTypeFile && !isLink) {
                    // On Windows, we receive CREATE event while the file is still being copied
                    // Do not start synchronizing the file while copying is in progress
                    const SyncPath absolutePath = _syncPal->_localPath / snapPath;
                    if (!IoHelper::isFileAccessible(absolutePath, ioError)) {
                        LOG_SYNCPAL_INFO(_logger, L"Item \"" << Path2WStr(absolutePath).c_str()
                                                             << L"\" is not ready. Synchronization postponed.");
                        continue;
                    }
                }
            }

            // Create operation
            FSOpPtr fsOp =
                std::make_shared<FSOperation>(OperationType::OperationTypeCreate, nodeId, type, snapshot->createdAt(nodeId),
                                              snapshot->lastModified(nodeId), snapshotSize, snapPath);
            opSet->insertOp(fsOp);
            logOperationGeneration(snapshot->side(), fsOp);
        }
    }

    return ExitCodeOk;
}

void ComputeFSOperationWorker::logOperationGeneration(const ReplicaSide side, const FSOpPtr fsOp) {
    if (!fsOp) {
        return;
    }
    if (!ParametersCache::instance()->parameters().extendedLog()) {
        return;
    }

    if (fsOp->operationType() == OperationTypeMove) {
        LOGW_SYNCPAL_DEBUG(_logger, L"Generate " << Utility::s2ws(Utility::side2Str(side)).c_str() << L" "
                                                 << Utility::s2ws(Utility::opType2Str(fsOp->operationType()).c_str())
                                                 << L" FS operation from "
                                                 << (fsOp->objectType() == NodeTypeDirectory ? L"dir \"" : L"file \"")
                                                 << Path2WStr(fsOp->path()).c_str() << L"\" to \""
                                                 << Path2WStr(fsOp->destinationPath()).c_str() << L"\" ("
                                                 << Utility::s2ws(fsOp->nodeId()).c_str() << L")");
        return;
    }

    LOGW_SYNCPAL_DEBUG(_logger, L"Generate " << Utility::s2ws(Utility::side2Str(side)).c_str() << L" "
                                             << Utility::s2ws(Utility::opType2Str(fsOp->operationType()).c_str())
                                             << L" FS operation on "
                                             << (fsOp->objectType() == NodeTypeDirectory ? L"dir \"" : L"file \"")
                                             << Path2WStr(fsOp->path()).c_str() << L"\" ("
                                             << Utility::s2ws(fsOp->nodeId()).c_str() << L")");
}

ExitCode ComputeFSOperationWorker::checkFileIntegrity(const DbNode &dbNode) {
    if (dbNode.type() == NodeType::NodeTypeFile && dbNode.nodeIdLocal().has_value() && dbNode.nodeIdRemote().has_value() &&
        dbNode.lastModifiedLocal().has_value()) {
        if (_fileSizeMismatchMap.find(dbNode.nodeIdLocal().value()) != _fileSizeMismatchMap.end()) {
            // Size mismatch already detected
            return ExitCodeOk;
        }

        if (!_syncPal->_localSnapshot->exists(dbNode.nodeIdLocal().value()) ||
            !_syncPal->_remoteSnapshot->exists(dbNode.nodeIdRemote().value())) {
            // Ignore if item does not exist
            return ExitCodeOk;
        }

        const bool localSnapshotIsLink = _syncPal->_localSnapshot->isLink(dbNode.nodeIdLocal().value());
        if (localSnapshotIsLink) {
            // Local and remote links sizes are not always the same (macOS aliases, Windows junctions)
            return ExitCodeOk;
        }

        int64_t localSnapshotSize = _syncPal->_localSnapshot->size(dbNode.nodeIdLocal().value());
        int64_t remoteSnapshotSize = _syncPal->_remoteSnapshot->size(dbNode.nodeIdRemote().value());
        SyncTime localSnapshotLastModified = _syncPal->_localSnapshot->lastModified(dbNode.nodeIdLocal().value());
        SyncTime remoteSnapshotLastModified = _syncPal->_remoteSnapshot->lastModified(dbNode.nodeIdRemote().value());

        // A mismatch is detected if all timestamps are equal but the sizes in snapshots differ.
        if (localSnapshotSize != remoteSnapshotSize && localSnapshotLastModified == dbNode.lastModifiedLocal().value() &&
            localSnapshotLastModified == remoteSnapshotLastModified) {
            SyncPath localSnapshotPath;
            if (!_syncPal->_localSnapshot->path(dbNode.nodeIdLocal().value(), localSnapshotPath)) {
                LOGW_SYNCPAL_WARN(_logger, L"Failed to retrieve path from snapshot for item "
                                               << SyncName2WStr(dbNode.nameLocal()).c_str() << L" ("
                                               << Utility::s2ws(dbNode.nodeIdLocal().value()).c_str() << L")");
                setExitCause(ExitCauseInvalidSnapshot);
                return ExitCodeDataError;
            }

            // OS might fail to notify all delete events, therefor we check that the file still exists
            SyncPath absoluteLocalPath = _syncPal->_localPath / localSnapshotPath;

            // No operations detected on this file but its size is not the same between remote and local replica
            // Remove it from local replica and download the remote version
            LOGW_SYNCPAL_DEBUG(_logger, L"File size mismatch for \"" << Path2WStr(absoluteLocalPath).c_str()
                                                                     << L"\". Remote version will be downloaded again.");
            _fileSizeMismatchMap.insert({dbNode.nodeIdLocal().value(), absoluteLocalPath});
#ifdef NDEBUG
            sentry_capture_event(sentry_value_new_message_event(SENTRY_LEVEL_WARNING, "ComputeFSOperationWorker::exploreDbTree",
                                                                "File size mismatch detected"));
#endif
        }
    }

    return ExitCodeOk;
}

bool ComputeFSOperationWorker::isExcludedFromSync(const std::shared_ptr<Snapshot> snapshot, const ReplicaSide side,
                                                  const NodeId &nodeId, const SyncPath &path, NodeType type, int64_t size) {
    if (isInUnsyncedList(snapshot, nodeId, side)) {
        if (ParametersCache::instance()->parameters().extendedLog()) {
            LOGW_SYNCPAL_DEBUG(_logger, L"Ignoring item " << Path2WStr(path).c_str() << L" (" << Utility::s2ws(nodeId).c_str()
                                                          << L") because it is not synced");
        }
        return true;
    }

    if (side == ReplicaSideRemote) {
        // Check path length
        if (isPathTooLong(path, nodeId, type)) {
            return true;
        }

        if (type == NodeTypeDirectory && isTooBig(snapshot, nodeId, size)) {
            if (ParametersCache::instance()->parameters().extendedLog()) {
                LOGW_SYNCPAL_DEBUG(_logger, L"Blacklisting item " << Path2WStr(path).c_str() << L" ("
                                                                  << Utility::s2ws(nodeId).c_str() << L") because it is too big");
            }
            return true;
        }
    } else {
        SyncPath absoluteFilePath = _syncPal->_localPath / path;

        // Check that file exists
        bool exists = false;
        IoError ioError = IoErrorSuccess;
        if (!IoHelper::checkIfPathExists(absoluteFilePath, exists, ioError)) {
            LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists for path="
                                   << Utility::formatIoError(absoluteFilePath, ioError).c_str());
            return true;
        }

        if (!exists) {
            LOGW_SYNCPAL_DEBUG(_logger, L"Ignore item " << Path2WStr(path).c_str() << L" (" << Utility::s2ws(nodeId).c_str()
                                                        << L") because it doesn't exist");
            return true;
        }
    }

    return false;
}

bool ComputeFSOperationWorker::isInUnsyncedList(const NodeId &nodeId, const ReplicaSide side) {
    auto &unsyncedList = side == ReplicaSideLocal ? _localTmpUnsyncedList : _remoteUnsyncedList;
    if (unsyncedList.size() == 0) {
        return false;
    }

    NodeId tmpNodeId = nodeId;
    bool found = false;
    do {
        if (unsyncedList.find(tmpNodeId) != unsyncedList.end()) {
            return true;
        }

        if (!_syncDb->parent(side, tmpNodeId, tmpNodeId, found)) {
            LOG_WARN(_logger, "Error in SyncDb::parent");
            break;
        }
    } while (found);

    return false;
}

bool ComputeFSOperationWorker::isInUnsyncedList(const std::shared_ptr<Snapshot> snapshot, const NodeId &nodeId,
                                                const ReplicaSide side, bool tmpListOnly /*= false*/) {
    auto &unsyncedList =
        side == ReplicaSideLocal ? _localTmpUnsyncedList : (tmpListOnly ? _remoteTmpUnsyncedList : _remoteUnsyncedList);
    auto &correspondingUnsyncedList =
        side == ReplicaSideLocal ? (tmpListOnly ? _remoteTmpUnsyncedList : _remoteUnsyncedList) : _localTmpUnsyncedList;

    if (unsyncedList.size() == 0 && correspondingUnsyncedList.size() == 0) {
        return false;
    }

    NodeId tmpNodeId = nodeId;
    while (!tmpNodeId.empty() && tmpNodeId != snapshot->rootFolderId()) {
        // Check if node is in black list or undecided list
        if (unsyncedList.find(tmpNodeId) != unsyncedList.end()) {
            return true;
        }

        // Check if node already exists on other side
        NodeId tmpCorrespondingNodeId;
        bool found = false;
        _syncPal->_syncDb->correspondingNodeId(side, tmpNodeId, tmpCorrespondingNodeId, found);
        if (found) {
            // Check if corresponding node is in black list or undecided list
            if (correspondingUnsyncedList.find(tmpCorrespondingNodeId) != correspondingUnsyncedList.end()) {
                return true;
            }
        }

        tmpNodeId = snapshot->parentId(tmpNodeId);
    }

    return false;
}

bool ComputeFSOperationWorker::isWhitelisted(const std::shared_ptr<Snapshot> snapshot, const NodeId &nodeId) {
    std::unordered_set<NodeId> whiteList;
    SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), SyncNodeTypeWhiteList, whiteList);

    NodeId tmpNodeId = nodeId;
    while (!tmpNodeId.empty() && tmpNodeId != snapshot->rootFolderId()) {
        if (whiteList.find(tmpNodeId) != whiteList.end()) {
            return true;
        }

        tmpNodeId = snapshot->parentId(tmpNodeId);
    }

    return false;
}

bool ComputeFSOperationWorker::isTooBig(const std::shared_ptr<Snapshot> remoteSnapshot, const NodeId &remoteNodeId,
                                        int64_t size) {
    if (isWhitelisted(remoteSnapshot, remoteNodeId)) {
        return false;
    }

    // Check if file already exist on local side
    NodeId localNodeId;
    bool found = false;
    _syncPal->_syncDb->correspondingNodeId(ReplicaSideRemote, remoteNodeId, localNodeId, found);
    if (found) {
        // We already synchronize the item locally, keep it
        return false;
    }

    // On first sync after migration from version under 3.4.0, the DB is empty but a big folder might as been whitelisted
    // Therefor check also with path
    SyncPath relativePath;
    if (remoteSnapshot->path(remoteNodeId, relativePath)) {
        localNodeId = _syncPal->_localSnapshot->itemId(relativePath);
        if (!localNodeId.empty()) {
            // We already synchronize the item locally, keep it
            return false;
        }
    }

    if (ParametersCache::instance()->parameters().useBigFolderSizeLimit() &&
        size > ParametersCache::instance()->parameters().bigFolderSizeLimitB() && _sync.virtualFileMode() == VirtualFileModeOff) {
        // Update undecided list
        std::unordered_set<NodeId> tmp;
        SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), SyncNodeTypeUndecidedList, tmp);

        // Delete all create operations that might have already been created on children
        deleteChildOpRecursively(remoteSnapshot, remoteNodeId, tmp);

        tmp.insert(remoteNodeId);
        SyncNodeCache::instance()->update(_syncPal->syncDbId(), SyncNodeTypeUndecidedList, tmp);

        // Update unsynced list cache
        updateUnsyncedList();

        return true;
    }

    return false;
}

bool ComputeFSOperationWorker::isPathTooLong(const SyncPath &path, const NodeId &nodeId, NodeType type) {
    SyncPath absolutePath = _syncPal->_localPath / path;
    size_t pathSize = absolutePath.native().size();
    if (PlatformInconsistencyCheckerUtility::instance()->checkPathLength(pathSize, type)) {
        LOGW_SYNCPAL_WARN(_logger, L"Path length too big (" << pathSize << L" characters) for item "
                                                            << Path2WStr(absolutePath).c_str() << L". Item is ignored.");

        Error err(_syncPal->syncDbId(), "", nodeId, type, path, ConflictTypeNone, InconsistencyTypePathLength);
        _syncPal->addError(err);

        return true;
    }
    return false;
}

ExitCode ComputeFSOperationWorker::checkIfOkToDelete(ReplicaSide side, const SyncPath &relativePath, const NodeId &nodeId, bool &isExcluded) {
    if (side != ReplicaSideLocal) return ExitCodeOk;

    if (!_syncPal->_localSnapshot->itemId(relativePath).empty()) {
        // Item with the same path but different ID exist
        // This is an Edit operation (Delete-Create)
        return ExitCodeOk;
    }

    const SyncPath absolutePath = _syncPal->_localPath / relativePath;
    bool exists = false;
    IoError ioError = IoErrorSuccess;
    if (!IoHelper::checkIfPathExistsWithSameNodeId(absolutePath, nodeId, exists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(absolutePath, ioError).c_str());
        setExitCause(ExitCauseFileAccessError);

        return ExitCodeSystemError;
    }

    if (!exists) return ExitCodeOk;

    bool readPermission = false;
    bool writePermission = false;
    bool execPermission = false;
    if (!IoHelper::getRights(absolutePath, readPermission, writePermission, execPermission, ioError)) {
        LOGW_WARN(_logger, L"Error in Utility::getRights for path=" << Path2WStr(absolutePath).c_str());
        setExitCause(ExitCauseFileAccessError);
        return ExitCodeSystemError;
    }

    if (!writePermission) {
        LOGW_DEBUG(_logger, L"Item " << Path2WStr(absolutePath).c_str() << L" doesn't have write permissions!");
        return ExitCodeNoWritePermission;
    }

    // Check if file is synced
    bool isWarning = false;
    ioError = IoErrorSuccess;
    const bool success =
        ExclusionTemplateCache::instance()->checkIfIsExcluded(_syncPal->_localPath, relativePath, isWarning, isExcluded, ioError);
    if (!success) {
        LOGW_WARN(_logger, L"Error in ExclusionTemplateCache::checkIfIsExcluded: "
                               << Utility::formatIoError(absolutePath, ioError).c_str());
        setExitCause(ExitCauseFileAccessError);
        return ExitCodeSystemError;
    }

    if (ioError == IoErrorAccessDenied) {
        LOGW_WARN(_logger, L"Item " << Path2WStr(absolutePath).c_str() << L" misses search permissions!");
        setExitCause(ExitCauseNoSearchPermission);
        return ExitCodeSystemError;
    }

    if (isExcluded) return ExitCodeOk;

    if (_syncPal->_localSnapshot->isOrphan(nodeId)) {
        // This can happen if the propagation of template exclusions has been unexpectedly interrupted.
        // This special handling should be removed once the app keeps track on such interruptions.
        return ExitCodeOk;
    }

    LOGW_SYNCPAL_DEBUG(_logger, L"Item " << Path2WStr(absolutePath).c_str()
                                         << L" still exists on local replica. Snapshot not up to date, restarting sync.");
#ifdef NDEBUG
    sentry_capture_event(sentry_value_new_message_event(SENTRY_LEVEL_WARNING, "ComputeFSOperationWorker::checkIfOkToDelete",
                                                        "Unwanted local delete operation averted"));
#endif

    setExitCause(ExitCauseInvalidSnapshot);

    return ExitCodeDataError;  // We need to rebuild the local snapshot from scratch.
}

void ComputeFSOperationWorker::deleteChildOpRecursively(const std::shared_ptr<Snapshot> remoteSnapshot,
                                                        const NodeId &remoteNodeId, std::unordered_set<NodeId> &tmpTooBigList) {
    std::unordered_set<NodeId> childrenIds;
    remoteSnapshot->getChildrenIds(remoteNodeId, childrenIds);

    for (auto &childId : childrenIds) {
        if (remoteSnapshot->type(childId) == NodeTypeDirectory) {
            deleteChildOpRecursively(remoteSnapshot, childId, tmpTooBigList);
        }
        _syncPal->_remoteOperationSet->removeOp(remoteNodeId, OperationTypeCreate);
        tmpTooBigList.erase(childId);
    }
}

void ComputeFSOperationWorker::updateUnsyncedList() {
    SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), SyncNodeTypeUndecidedList, _remoteUnsyncedList);
    std::unordered_set<NodeId> tmp;
    SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), SyncNodeTypeBlackList, tmp);
    _remoteUnsyncedList.merge(tmp);
    SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), SyncNodeTypeTmpRemoteBlacklist, _remoteTmpUnsyncedList);
    _remoteUnsyncedList.merge(_remoteTmpUnsyncedList);

    SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), SyncNodeTypeTmpLocalBlacklist,
                                         _localTmpUnsyncedList);  // Only tmp list on local side
}

void ComputeFSOperationWorker::addFolderToDelete(const SyncPath &path) {
    // Remove sub dirs
    auto dirPathToDeleteSetIt = _dirPathToDeleteSet.begin();
    while (dirPathToDeleteSetIt != _dirPathToDeleteSet.end()) {
        if (CommonUtility::isSubDir(path, *dirPathToDeleteSetIt)) {
            dirPathToDeleteSetIt = _dirPathToDeleteSet.erase(dirPathToDeleteSetIt);
        } else {
            dirPathToDeleteSetIt++;
        }
    }

    // Insert dir
    _dirPathToDeleteSet.insert(path);
}

bool ComputeFSOperationWorker::pathInDeletedFolder(const SyncPath &path) {
    for (auto dirPathToDeleteSetIt = _dirPathToDeleteSet.begin(); dirPathToDeleteSetIt != _dirPathToDeleteSet.end();
         ++dirPathToDeleteSetIt) {
        if (CommonUtility::isSubDir(*dirPathToDeleteSetIt, path)) {
            return true;
        }
    }

    return false;
}

}  // namespace KDC
