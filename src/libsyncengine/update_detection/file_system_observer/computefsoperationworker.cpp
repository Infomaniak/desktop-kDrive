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

#include "localfilesystemobserverworker.h"

namespace KDC {

ComputeFSOperationWorker::ComputeFSOperationWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name,
                                                   const std::string &shortName)
    : ISyncWorker(syncPal, name, shortName),
      _syncDb(syncPal->_syncDb),
      _localSnapshot(syncPal->_localSnapshot),
      _remoteSnapshot(syncPal->_remoteSnapshot) {}

ComputeFSOperationWorker::ComputeFSOperationWorker(const std::shared_ptr<SyncDb> testSyncDb,
                                                   const std::shared_ptr<Snapshot> testLocalSnapshot,
                                                   const std::shared_ptr<Snapshot> testRemoteSnapshot, const std::string &name,
                                                   const std::string &shortName)
    : ISyncWorker(nullptr, name, shortName, true),
      _syncDb(testSyncDb),
      _localSnapshot(testLocalSnapshot),
      _remoteSnapshot(testRemoteSnapshot) {}

void ComputeFSOperationWorker::execute() {
    ExitCode exitCode(ExitCode::Unknown);

    LOG_SYNCPAL_DEBUG(_logger, "Worker started: name=" << name().c_str());
    auto start = std::chrono::steady_clock::now();

    // Update the sync parameters
    bool ok = true;
    bool found = true;
    if (!ParmsDb::instance()->selectSync(_syncPal->syncDbId(), _sync, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in ParmsDb::selectSync");
        setExitCause(ExitCause::DbAccessError);
        exitCode = ExitCode::DbError;
        ok = false;
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "Sync not found");
        exitCode = ExitCode::DataError;
        ok = false;
    }
    if (!ok) {
        setDone(exitCode);
        LOG_SYNCPAL_DEBUG(_logger, "Worker stopped: name=" << name().c_str());
        return;
    }

    _syncPal->operationSet(ReplicaSide::Local)->clear();
    _syncPal->operationSet(ReplicaSide::Remote)->clear();

    // Update SyncNode cache
    _syncPal->updateSyncNode();

    // Update unsynced list cache
    updateUnsyncedList();

    _fileSizeMismatchMap.clear();

    std::unordered_set<NodeId> localIdsSet;
    std::unordered_set<NodeId> remoteIdsSet;
    if (ok && !stopAsked()) {
        exitCode = exploreDbTree(localIdsSet, remoteIdsSet);
        ok = exitCode == ExitCode::Ok;
    }

    if (ok && !stopAsked()) {
        exitCode = exploreSnapshotTree(ReplicaSide::Local, localIdsSet);
        ok = exitCode == ExitCode::Ok;
        if (ok) {
            exitCode = exploreSnapshotTree(ReplicaSide::Remote, remoteIdsSet);
        }
    }

    if (!ok || stopAsked()) {
        // Do not keep operations if there was an error or sync was stopped
        _syncPal->operationSet(ReplicaSide::Local)->clear();
        _syncPal->operationSet(ReplicaSide::Remote)->clear();
    } else {
        exitCode = ExitCode::Ok;
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
        setExitCause(ExitCause::DbAccessError);
        return ExitCode::DbError;
    } else if (!found) {
        LOG_SYNCPAL_DEBUG(_logger, "No items found in db");
        return ExitCode::Ok;
    }

    // Explore the tree twice:
    // First compute operations only for directories
    // Then compute operations for files
    _dirPathToDeleteSet.clear();
    for (int i = 0; i <= 1; i++) {
        bool checkOnlyDir = i == 0;

        auto dbIt = remainingDbIds.begin();
        while (dbIt != remainingDbIds.end()) {
            DbNodeId dbId = *dbIt;

            if (dbId == _syncPal->_syncDb->rootNode().nodeId()) {
                // Ignore root folder
                dbIt = remainingDbIds.erase(dbIt);
                continue;
            }

            if (stopAsked()) {
                return ExitCode::Ok;
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
                setExitCause(ExitCause::DbAccessError);
                return ExitCode::DbError;
            }
            if (!found) {
                LOG_SYNCPAL_DEBUG(_logger, "Failed to retrieve node for dbId=" << dbId);
                setExitCause(ExitCause::DbEntryNotFound);
                return ExitCode::DataError;
            }

            if (dbNode.nodeIdLocal().has_value()) {
                localIdsSet.insert(*dbNode.nodeIdLocal());
            }
            if (dbNode.nodeIdRemote().has_value()) {
                remoteIdsSet.insert(*dbNode.nodeIdRemote());
            }

            if (checkOnlyDir && dbNode.type() != NodeType::Directory) {
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
                setExitCause(ExitCause::DbAccessError);
                return ExitCode::DbError;
            }
            if (!found) {
                LOG_SYNCPAL_DEBUG(_logger, "Failed to retrieve node for dbId=" << dbId);
                setExitCause(ExitCause::DbEntryNotFound);
                return ExitCode::DataError;
            }

            for (int j = 0; j <= 1; j++) {
                ReplicaSide side = j == 0 ? ReplicaSide::Local : ReplicaSide::Remote;
                SyncTime dbLastModified = 0;
                NodeId nodeId;
                if (side == ReplicaSide::Local) {
                    dbLastModified = dbNode.lastModifiedLocal().has_value() ? dbNode.lastModifiedLocal().value() : 0;
                    nodeId = dbNode.nodeIdLocal().has_value() ? dbNode.nodeIdLocal().value() : "";
                } else {
                    dbLastModified = dbNode.lastModifiedRemote().has_value() ? dbNode.lastModifiedRemote().value() : 0;
                    nodeId = dbNode.nodeIdRemote().has_value() ? dbNode.nodeIdRemote().value() : "";
                }
                if (nodeId.empty()) {
                    LOGW_SYNCPAL_WARN(_logger, Utility::s2ws(Utility::side2Str(side)).c_str()
                                                   << L" node ID empty for for dbId=" << dbId);
                    setExitCause(ExitCause::DbEntryNotFound);
                    return ExitCode::DataError;
                }

                SyncName dbName = side == ReplicaSide::Local ? dbNode.nameLocal() : dbNode.nameRemote();
                const SyncPath &dbPath = side == ReplicaSide::Local ? localDbPath : remoteDbPath;
                const std::shared_ptr<Snapshot> snapshot = _syncPal->snapshot(side, true);
                std::shared_ptr<FSOperationSet> opSet = _syncPal->operationSet(side);

                NodeId parentId;
                if (!_syncDb->parent(side, nodeId, parentId, found)) {
                    LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::parent");
                    setExitCause(ExitCause::DbAccessError);
                    return ExitCode::DbError;
                }
                if (!found) {
                    LOG_SYNCPAL_DEBUG(_logger, "Failed to retrieve node for dbId=" << nodeId.c_str());
                    setExitCause(ExitCause::DbEntryNotFound);
                    return ExitCode::DataError;
                }

                bool remoteItemUnsynced = false;
                bool movedIntoUnsyncedFolder = false;
                if (side == ReplicaSide::Remote) {
                    // In case of a move inside an excluded folder, the item must be removed in this sync
                    if (isInUnsyncedList(nodeId, ReplicaSide::Remote)) {
                        remoteItemUnsynced = true;
                        if (parentId != snapshot->parentId(nodeId)) {
                            movedIntoUnsyncedFolder = true;
                        }
                    }
                } else {
                    if (isInUnsyncedList(nodeId, ReplicaSide::Local)) {
                        continue;
                    }
                }

                if (!snapshot->exists(nodeId) || movedIntoUnsyncedFolder) {
                    if (!pathInDeletedFolder(dbPath)) {
                        // Check that the file/directory really does not exist on replica
                        bool isExcluded = false;
                        if (const ExitCode exitCode = checkIfOkToDelete(side, dbPath, nodeId, isExcluded);
                            exitCode != ExitCode::Ok) {
                            if (exitCode == ExitCode::NoWritePermission) {
                                // Blacklist node
                                _syncPal->blacklistTemporarily(nodeId, dbPath, side);
                                Error error(_syncPal->_syncDbId, "", "", NodeType::Directory, dbPath, ConflictType::None,
                                            InconsistencyType::None, CancelType::None, "", ExitCode::SystemError,
                                            ExitCause::FileAccessError);
                                _syncPal->addError(error);

                                // Update unsynced list cache
                                updateUnsyncedList();
                                continue;
                            } else {
                                return exitCode;
                            }
                        }

                        if (isExcluded) continue;  // Never generate operation on excluded file
                    }

                    if (isInUnsyncedList(snapshot, nodeId, side, true)) {
                        // Ignore operation
                        continue;
                    }

                    bool checkTemplate = side == ReplicaSide::Remote;
                    if (side == ReplicaSide::Local) {
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

                            if (IoError ioError = IoError::Success; !IoHelper::checkIfPathExists(localPath, exists, ioError)) {
                                LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: "
                                                       << Utility::formatIoError(localPath, ioError).c_str());
                                return ExitCode::SystemError;
                            }
                            checkTemplate = exists;
                        }
                    }

                    if (checkTemplate) {
                        IoError ioError = IoError::Success;
                        bool warn = false;
                        bool isExcluded = false;
                        const bool success = ExclusionTemplateCache::instance()->checkIfIsExcluded(_syncPal->_localPath, dbPath,
                                                                                                   warn, isExcluded, ioError);
                        if (!success) {
                            LOGW_WARN(_logger, L"Error in ExclusionTemplateCache::checkIfIsExcluded: "
                                                   << Utility::formatIoError(dbPath, ioError).c_str());
                            return ExitCode::SystemError;
                        }
                        if (isExcluded) {
                            // The item is excluded
                            continue;
                        }
                    }

                    // Delete operation
                    FSOpPtr fsOp = std::make_shared<FSOperation>(OperationType::Delete, nodeId, dbNode.type(),
                                                                 dbNode.created().has_value() ? dbNode.created().value() : 0,
                                                                 dbLastModified, dbNode.size(), dbPath);
                    opSet->insertOp(fsOp);
                    logOperationGeneration(snapshot->side(), fsOp);

                    if (dbNode.type() == NodeType::Directory) {
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
                    setExitCause(ExitCause::InvalidSnapshot);
                    return ExitCode::DataError;
                }

                if (side == ReplicaSide::Local && !_testing) {
                    // OS might fail to notify all delete events, therefore we check that the file still exists.
                    SyncPath absolutePath = _syncPal->_localPath / snapPath;
                    bool exists = false;
                    if (IoError ioError = IoError::Success; !IoHelper::checkIfPathExists(absolutePath, exists, ioError)) {
                        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: "
                                               << Utility::formatIoError(absolutePath, ioError).c_str());
                        return ExitCode::SystemError;
                    }
                    if (!exists) {
                        LOGW_DEBUG(_logger, L"Item does not exist anymore on local replica. Snapshot will be rebuilt - path="
                                                << Path2WStr(absolutePath).c_str());
                        setExitCause(ExitCause::InvalidSnapshot);
                        return ExitCode::DataError;
                    }
                } else {
                    // Check path length
                    if (isPathTooLong(snapPath, nodeId, snapshot->type(nodeId))) {
                        continue;
                    }
                }

                const SyncTime snapshotLastModified = snapshot->lastModified(nodeId);
                if (snapshotLastModified != dbLastModified && dbNode.type() == NodeType::File) {
                    // Edit operation
                    FSOpPtr fsOp =
                        std::make_shared<FSOperation>(OperationType::Edit, nodeId, NodeType::File, snapshot->createdAt(nodeId),
                                                      snapshotLastModified, snapshot->size(nodeId), snapPath);
                    opSet->insertOp(fsOp);
                    logOperationGeneration(snapshot->side(), fsOp);
                }

                const bool movedOrRenamed =
                    dbName != Utility::normalizedSyncName(snapshot->name(nodeId)) || parentId != snapshot->parentId(nodeId);
                if (movedOrRenamed) {
                    FSOpPtr fsOp = nullptr;
                    if (isInUnsyncedList(snapshot, nodeId, side)) {
                        // Delete operation
                        fsOp = std::make_shared<FSOperation>(OperationType::Delete, nodeId, dbNode.type(),
                                                             snapshot->createdAt(nodeId), snapshotLastModified,
                                                             snapshot->size(nodeId),
                                                             remoteDbPath  // We use the remotePath anyway here to display
                                                             // notifications with the real (remote) name
                                                             ,
                                                             snapPath);
                    } else {
                        // Move operation
                        fsOp =
                            std::make_shared<FSOperation>(OperationType::Move, nodeId, dbNode.type(), snapshot->createdAt(nodeId),
                                                          snapshotLastModified, snapshot->size(nodeId),
                                                          remoteDbPath  // We use the remotePath anyway here to display
                                                          // notifications with the real (remote) name
                                                          ,
                                                          snapPath);
                    }

                    opSet->insertOp(fsOp);
                    logOperationGeneration(snapshot->side(), fsOp);
                }
            }

            // Check file integrity
            ExitCode fileIntegrityOk = checkFileIntegrity(dbNode);
            if (fileIntegrityOk != ExitCode::Ok) {
                return fileIntegrityOk;
            }
        }
    }

    return ExitCode::Ok;
}

ExitCode ComputeFSOperationWorker::exploreSnapshotTree(ReplicaSide side, const std::unordered_set<NodeId> &idsSet) {
    const std::shared_ptr<Snapshot> snapshot = _syncPal->snapshot(side, true);
    std::shared_ptr<FSOperationSet> opSet = _syncPal->operationSet(side);

    std::unordered_set<NodeId> remainingDbIds;
    snapshot->ids(remainingDbIds);
    if (remainingDbIds.empty()) {
        LOG_SYNCPAL_DEBUG(_logger, "No items found in snapshot on side " << Utility::side2Str(side).c_str());
        return ExitCode::Ok;
    }

    // Explore the tree twice:
    // First compute operations only for directories
    // Then compute operations for files
    for (int i = 0; i <= 1; i++) {
        bool checkOnlyDir = i == 0;

        auto snapIdIt = remainingDbIds.begin();
        while (snapIdIt != remainingDbIds.end()) {
            if (stopAsked()) {
                return ExitCode::Ok;
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
            if (checkOnlyDir && type != NodeType::Directory) {
                // In first loop, we check only directories
                snapIdIt++;
                continue;
            }

            // Remove directory ID from list so 2nd iteration will be a bit faster
            snapIdIt = remainingDbIds.erase(snapIdIt);

            if (snapshot->isOrphan(snapshot->parentId(nodeId))) {
                // Ignore orphans
                if (ParametersCache::isExtendedLogEnabled()) {
                    LOGW_SYNCPAL_DEBUG(_logger, L"Ignoring orphan node " << SyncName2WStr(snapshot->name(nodeId)).c_str() << L" ("
                                                                         << Utility::s2ws(nodeId).c_str() << L")");
                }
                continue;
            }

            SyncPath snapPath;
            if (!snapshot->path(nodeId, snapPath)) {
                LOGW_SYNCPAL_WARN(_logger, L"Failed to retrieve path from snapshot for item " << Utility::s2ws(nodeId).c_str());
                setExitCause(ExitCause::InvalidSnapshot);
                return ExitCode::DataError;
            }

            // Manage file exclusion
            const int64_t snapshotSize = snapshot->size(nodeId);
            if (isExcludedFromSync(snapshot, side, nodeId, snapPath, type, snapshotSize)) {
                continue;
            }

            if (side == ReplicaSide::Local) {
                // Check if a local file is hidden, hence excluded.
                bool isExcluded = false;
                IoError ioError = IoError::Success;
                const bool success = ExclusionTemplateCache::instance()->checkIfIsExcludedBecauseHidden(
                    _syncPal->_localPath, snapPath, isExcluded, ioError);
                if (!success || ioError != IoError::Success || isExcluded) {
                    if (_testing && ioError == IoError::NoSuchFileOrDirectory) {
                        // Files does exist in test, this fine, ignore ioError.
                    } else {
                        continue;
                    }
                }

                // TODO : this portion of code aimed to wait for a file to be available locally before starting to synchronize it
                // For example, on Windows, when copying a big file inside the sync folder, the creation event is received
                // immediately but the copy will take some time. Therefor, the file will appear locked during the copy.
                // However, this will also block the update of file locked by an application during its edition (Microsoft Office,
                // Open Office, ...)
                //                const bool isLink = _syncPal->snapshot(ReplicaSide::Local, true)->isLink(nodeId);
                //
                //                if (type == NodeType::File && !isLink) {
                //                    // On Windows, we receive CREATE event while the file is still being copied
                //                    // Do not start synchronizing the file while copying is in progress
                //                    const SyncPath absolutePath = _syncPal->_localPath / snapPath;
                //                    if (!IoHelper::isFileAccessible(absolutePath, ioError)) {
                //                        LOG_SYNCPAL_INFO(_logger, L"Item \"" << Path2WStr(absolutePath).c_str()
                //                                                             << L"\" is not ready. Synchronization postponed.");
                //                        continue;
                //                    }
                //                }
            }

            // Create operation
            FSOpPtr fsOp = std::make_shared<FSOperation>(OperationType::Create, nodeId, type, snapshot->createdAt(nodeId),
                                                         snapshot->lastModified(nodeId), snapshotSize, snapPath);
            opSet->insertOp(fsOp);
            logOperationGeneration(snapshot->side(), fsOp);
        }
    }

    return ExitCode::Ok;
}

void ComputeFSOperationWorker::logOperationGeneration(const ReplicaSide side, const FSOpPtr fsOp) {
    if (!fsOp) {
        return;
    }
    if (!ParametersCache::isExtendedLogEnabled()) {
        return;
    }

    if (fsOp->operationType() == OperationType::Move) {
        LOGW_SYNCPAL_DEBUG(
            _logger, L"Generate " << Utility::s2ws(Utility::side2Str(side)).c_str() << L" "
                                  << Utility::s2ws(Utility::opType2Str(fsOp->operationType()).c_str()) << L" FS operation from "
                                  << (fsOp->objectType() == NodeType::Directory ? L"dir \"" : L"file \"")
                                  << Path2WStr(fsOp->path()).c_str() << L"\" to \"" << Path2WStr(fsOp->destinationPath()).c_str()
                                  << L"\" (" << Utility::s2ws(fsOp->nodeId()).c_str() << L")");
        return;
    }

    LOGW_SYNCPAL_DEBUG(
        _logger, L"Generate " << Utility::s2ws(Utility::side2Str(side)).c_str() << L" "
                              << Utility::s2ws(Utility::opType2Str(fsOp->operationType()).c_str()) << L" FS operation on "
                              << (fsOp->objectType() == NodeType::Directory ? L"dir \"" : L"file \"")
                              << Path2WStr(fsOp->path()).c_str() << L"\" (" << Utility::s2ws(fsOp->nodeId()).c_str() << L")");
}

ExitCode ComputeFSOperationWorker::checkFileIntegrity(const DbNode &dbNode) {
    if (!CommonUtility::isFileSizeMismatchDetectionEnabled() || dbNode.type() != NodeType::File ||
        !dbNode.nodeIdLocal().has_value() || !dbNode.nodeIdRemote().has_value() || !dbNode.lastModifiedLocal().has_value()) {
        return ExitCode::Ok;
    }

    if (_fileSizeMismatchMap.contains(dbNode.nodeIdLocal().value())) {
        // Size mismatch already detected
        return ExitCode::Ok;
    }

    if (!_syncPal->snapshot(ReplicaSide::Local, true)->exists(dbNode.nodeIdLocal().value()) ||
        !_syncPal->snapshot(ReplicaSide::Remote, true)->exists(dbNode.nodeIdRemote().value())) {
        // Ignore if item does not exist
        return ExitCode::Ok;
    }

    if (const bool localSnapshotIsLink = _syncPal->snapshot(ReplicaSide::Local, true)->isLink(dbNode.nodeIdLocal().value());
        localSnapshotIsLink) {
        // Local and remote links sizes are not always the same (macOS aliases, Windows junctions)
        return ExitCode::Ok;
    }

    int64_t localSnapshotSize = _syncPal->snapshot(ReplicaSide::Local, true)->size(dbNode.nodeIdLocal().value());
    int64_t remoteSnapshotSize = _syncPal->snapshot(ReplicaSide::Remote, true)->size(dbNode.nodeIdRemote().value());
    SyncTime localSnapshotLastModified = _syncPal->snapshot(ReplicaSide::Local, true)->lastModified(dbNode.nodeIdLocal().value());
    SyncTime remoteSnapshotLastModified =
        _syncPal->snapshot(ReplicaSide::Remote, true)->lastModified(dbNode.nodeIdRemote().value());

    // A mismatch is detected if all timestamps are equal but the sizes in snapshots differ.
    if (localSnapshotSize != remoteSnapshotSize && localSnapshotLastModified == dbNode.lastModifiedLocal().value() &&
        localSnapshotLastModified == remoteSnapshotLastModified) {
        SyncPath localSnapshotPath;
        if (!_syncPal->snapshot(ReplicaSide::Local, true)->path(dbNode.nodeIdLocal().value(), localSnapshotPath)) {
            LOGW_SYNCPAL_WARN(_logger, L"Failed to retrieve path from snapshot for item "
                                           << SyncName2WStr(dbNode.nameLocal()).c_str() << L" ("
                                           << Utility::s2ws(dbNode.nodeIdLocal().value()).c_str() << L")");
            setExitCause(ExitCause::InvalidSnapshot);
            return ExitCode::DataError;
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

    return ExitCode::Ok;
}

bool ComputeFSOperationWorker::isExcludedFromSync(const std::shared_ptr<Snapshot> snapshot, const ReplicaSide side,
                                                  const NodeId &nodeId, const SyncPath &path, NodeType type, int64_t size) {
    if (isInUnsyncedList(snapshot, nodeId, side)) {
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_SYNCPAL_DEBUG(_logger, L"Ignoring item " << Path2WStr(path).c_str() << L" (" << Utility::s2ws(nodeId).c_str()
                                                          << L") because it is not synced");
        }
        return true;
    }

    if (side == ReplicaSide::Remote) {
        // Check path length
        if (isPathTooLong(path, nodeId, type)) {
            return true;
        }

        if (type == NodeType::Directory && isTooBig(snapshot, nodeId, size)) {
            if (ParametersCache::isExtendedLogEnabled()) {
                LOGW_SYNCPAL_DEBUG(_logger, L"Blacklisting item " << Path2WStr(path).c_str() << L" ("
                                                                  << Utility::s2ws(nodeId).c_str() << L") because it is too big");
            }
            return true;
        }
    } else {
        if (!_testing) {
            SyncPath absoluteFilePath = _syncPal->_localPath / path;

            // Check that file exists
            bool exists = false;
            IoError ioError = IoError::Success;
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
    }

    return false;
}

bool ComputeFSOperationWorker::isInUnsyncedList(const NodeId &nodeId, const ReplicaSide side) {
    auto &unsyncedList = side == ReplicaSide::Local ? _localTmpUnsyncedList : _remoteUnsyncedList;
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
        side == ReplicaSide::Local ? _localTmpUnsyncedList : (tmpListOnly ? _remoteTmpUnsyncedList : _remoteUnsyncedList);
    auto &correspondingUnsyncedList =
        side == ReplicaSide::Local ? (tmpListOnly ? _remoteTmpUnsyncedList : _remoteUnsyncedList) : _localTmpUnsyncedList;

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
    SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), SyncNodeType::WhiteList, whiteList);

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
    _syncPal->_syncDb->correspondingNodeId(ReplicaSide::Remote, remoteNodeId, localNodeId, found);
    if (found) {
        // We already synchronize the item locally, keep it
        return false;
    }

    // On first sync after migration from version under 3.4.0, the DB is empty but a big folder might as been whitelisted
    // Therefor check also with path
    SyncPath relativePath;
    if (remoteSnapshot->path(remoteNodeId, relativePath)) {
        localNodeId = _syncPal->snapshot(ReplicaSide::Local, true)->itemId(relativePath);
        if (!localNodeId.empty()) {
            // We already synchronize the item locally, keep it
            return false;
        }
    }

    if (ParametersCache::instance()->parameters().useBigFolderSizeLimit() &&
        size > ParametersCache::instance()->parameters().bigFolderSizeLimitB() &&
        _sync.virtualFileMode() == VirtualFileMode::Off) {
        // Update undecided list
        std::unordered_set<NodeId> tmp;
        SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), SyncNodeType::UndecidedList, tmp);

        // Delete all create operations that might have already been created on children
        deleteChildOpRecursively(remoteSnapshot, remoteNodeId, tmp);

        tmp.insert(remoteNodeId);
        SyncNodeCache::instance()->update(_syncPal->syncDbId(), SyncNodeType::UndecidedList, tmp);

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

        Error err(_syncPal->syncDbId(), "", nodeId, type, path, ConflictType::None, InconsistencyType::PathLength);
        _syncPal->addError(err);

        return true;
    }
    return false;
}

ExitCode ComputeFSOperationWorker::checkIfOkToDelete(ReplicaSide side, const SyncPath &relativePath, const NodeId &nodeId,
                                                     bool &isExcluded) {
    if (side != ReplicaSide::Local) return ExitCode::Ok;

    if (!_syncPal->snapshot(ReplicaSide::Local, true)->itemId(relativePath).empty()) {
        // Item with the same path but different ID exist
        // This is an Edit operation (Delete-Create)
        return ExitCode::Ok;
    }

    const SyncPath absolutePath = _syncPal->_localPath / relativePath;
    bool existsWithSameId = false;
    NodeId otherNodeId;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExistsWithSameNodeId(absolutePath, nodeId, existsWithSameId, otherNodeId, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExistsWithSameNodeId: "
                               << Utility::formatIoError(absolutePath, ioError).c_str());

        if (ioError == IoError::InvalidFileName) {
            // Observed on MacOSX under special circumstances; see getItemType unit test edge cases.
            setExitCause(ExitCause::InvalidName);
        } else
            setExitCause(ExitCause::FileAccessError);

        return ExitCode::SystemError;
    }

    if (!existsWithSameId) return ExitCode::Ok;

    bool readPermission = false;
    bool writePermission = false;
    bool execPermission = false;
    if (!IoHelper::getRights(absolutePath, readPermission, writePermission, execPermission, ioError)) {
        LOGW_WARN(_logger, L"Error in Utility::getRights for path=" << Path2WStr(absolutePath).c_str());
        setExitCause(ExitCause::FileAccessError);
        return ExitCode::SystemError;
    }

    if (!writePermission) {
        LOGW_DEBUG(_logger, L"Item " << Path2WStr(absolutePath).c_str() << L" doesn't have write permissions!");
        return ExitCode::NoWritePermission;
    }

    // Check if file is synced
    bool isWarning = false;
    ioError = IoError::Success;
    const bool success =
        ExclusionTemplateCache::instance()->checkIfIsExcluded(_syncPal->_localPath, relativePath, isWarning, isExcluded, ioError);
    if (!success) {
        LOGW_WARN(_logger,
                  L"Error in ExclusionTemplateCache::isExcluded: " << Utility::formatIoError(absolutePath, ioError).c_str());
        setExitCause(ExitCause::FileAccessError);
        return ExitCode::SystemError;
    }

    if (ioError == IoError::AccessDenied) {
        LOGW_WARN(_logger, L"Item " << Path2WStr(absolutePath).c_str() << L" misses search permissions!");
        setExitCause(ExitCause::NoSearchPermission);
        return ExitCode::SystemError;
    }

    if (isExcluded) return ExitCode::Ok;

    if (_syncPal->snapshot(ReplicaSide::Local, true)->isOrphan(nodeId)) {
        // This can happen if the propagation of template exclusions has been unexpectedly interrupted.
        // This special handling should be removed once the app keeps track on such interruptions.
        return ExitCode::Ok;
    }

    LOGW_SYNCPAL_DEBUG(_logger, L"Item " << Path2WStr(absolutePath).c_str()
                                         << L" still exists on local replica. Snapshot not up to date, restarting sync.");
#ifdef NDEBUG
    sentry_capture_event(sentry_value_new_message_event(SENTRY_LEVEL_WARNING, "ComputeFSOperationWorker::checkIfOkToDelete",
                                                        "Unwanted local delete operation averted"));
#endif

    setExitCause(ExitCause::InvalidSnapshot);

    return ExitCode::DataError;  // We need to rebuild the local snapshot from scratch.
}

void ComputeFSOperationWorker::deleteChildOpRecursively(const std::shared_ptr<Snapshot> remoteSnapshot,
                                                        const NodeId &remoteNodeId, std::unordered_set<NodeId> &tmpTooBigList) {
    std::unordered_set<NodeId> childrenIds;
    remoteSnapshot->getChildrenIds(remoteNodeId, childrenIds);

    for (auto &childId : childrenIds) {
        if (remoteSnapshot->type(childId) == NodeType::Directory) {
            deleteChildOpRecursively(remoteSnapshot, childId, tmpTooBigList);
        }
        _syncPal->_remoteOperationSet->removeOp(remoteNodeId, OperationType::Create);
        tmpTooBigList.erase(childId);
    }
}

void ComputeFSOperationWorker::updateUnsyncedList() {
    SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), SyncNodeType::UndecidedList, _remoteUnsyncedList);
    std::unordered_set<NodeId> tmp;
    SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), SyncNodeType::BlackList, tmp);
    _remoteUnsyncedList.merge(tmp);
    SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), SyncNodeType::TmpRemoteBlacklist, _remoteTmpUnsyncedList);
    _remoteUnsyncedList.merge(_remoteTmpUnsyncedList);

    SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), SyncNodeType::TmpLocalBlacklist,
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
