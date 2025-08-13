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

#include "computefsoperationworker.h"
#include "requests/parameterscache.h"
#include "requests/syncnodecache.h"
#include "requests/exclusiontemplatecache.h"
#include "libcommon/utility/utility.h"
#include "libcommon/log/sentry/ptraces.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/io/iohelper.h"
#include "reconciliation/platform_inconsistency_checker/platforminconsistencycheckerutility.h"

namespace KDC {

ComputeFSOperationWorker::ComputeFSOperationWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name,
                                                   const std::string &shortName) :
    ISyncWorker(syncPal, name, shortName),
    _syncDbReadOnlyCache(syncPal->syncDb()->cache()) {
    // Resolution for the modification time is 2s on FAT filesystems:
    // https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-filetime
    if (CommonUtility::isFAT(_syncPal->localPath())) _timeDifferenceThresholdForEdit = 1;
}

ComputeFSOperationWorker::ComputeFSOperationWorker(SyncDbReadOnlyCache &testSyncDbReadOnlyCache, const std::string &name,
                                                   const std::string &shortName) :
    ISyncWorker(nullptr, name, shortName, std::chrono::seconds(0), true),
    _syncDbReadOnlyCache(testSyncDbReadOnlyCache) {}

void ComputeFSOperationWorker::postponeCreateOperationsOnReusedIds() {
    for (const auto &localId: _localReusedIds) {
        _syncPal->removeLocalOperation(localId, OperationType::Create);
        deleteLocalDescendantCreateOps(localId);
        SyncPath localPath;
        bool ignore = false;
        (void) _syncPal->snapshot(ReplicaSide::Local)->path(localId, localPath, ignore);
        LOGW_SYNCPAL_DEBUG(_logger, L"Postponing the creation of local item with id='"
                                            << CommonUtility::s2ws(localId) << L"' and " << Utility::formatSyncPath(localPath)
                                            << L" and its descendants because this item, or one of its ancestors, has reused the "
                                               L"identifier of a deleted item.");
    }

    _localReusedIds.clear();
}

void ComputeFSOperationWorker::execute() {
    ExitCode exitCode(ExitCode::Unknown);

    LOG_SYNCPAL_DEBUG(_logger, "Worker started: name=" << name());
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
        LOG_SYNCPAL_DEBUG(_logger, "Worker stopped: name=" << name());
        setDone(exitCode);
        return;
    }

    _syncPal->operationSet(ReplicaSide::Local)->clear();
    _syncPal->operationSet(ReplicaSide::Remote)->clear();

    // Update SyncNode cache
    updateSyncNode();

    // Update unsynced list cache
    updateUnsyncedList();

    _fileSizeMismatchMap.clear();

    NodeIdSet localIdsSet;
    NodeIdSet remoteIdsSet;
    if (!stopAsked()) {
        sentry::pTraces::scoped::InferChangesFromDb perfMonitor(syncDbId());
        exitCode = inferChangesFromDb(localIdsSet, remoteIdsSet);
        ok = exitCode == ExitCode::Ok;
        if (ok) perfMonitor.stop();
    }
    if (ok && !stopAsked()) {
        sentry::pTraces::scoped::ExploreLocalSnapshot perfMonitor(syncDbId());
        exitCode = exploreSnapshotTree(ReplicaSide::Local, localIdsSet);
        ok = exitCode == ExitCode::Ok;
        if (ok) perfMonitor.stop();
    }
    if (ok && !stopAsked()) {
        sentry::pTraces::scoped::ExploreRemoteSnapshot perfMonitor(syncDbId());
        exitCode = exploreSnapshotTree(ReplicaSide::Remote, remoteIdsSet);
        ok = exitCode == ExitCode::Ok;
        if (ok) perfMonitor.stop();
    }

    const std::chrono::duration<double> elapsedSeconds = std::chrono::steady_clock::now() - start;

    if (!ok || stopAsked()) {
        // Do not keep operations if there was an error or sync was stopped
        _syncPal->operationSet(ReplicaSide::Local)->clear();
        _syncPal->operationSet(ReplicaSide::Remote)->clear();
        LOG_SYNCPAL_INFO(_logger, "FS operation aborted after: " << elapsedSeconds.count() << "s");

    } else {
        /* If the current snapshot state does not reveal any operation, we store the current revision number.
         * On the next call to compute filesystem operations, only items from the snapshot that were modified in a higher
         * revision will be processed. It is important not to update the CFSO's lastSyncedRevision if an operation is detected,
         * otherwise, we may fail to detect it again if, for any reason, the propagation of the change fails.
         */

        if (_syncPal->operationSet(ReplicaSide::Local)->nbOps() == 0) {
            _lastLocalSnapshotSyncedRevision = _syncPal->snapshot(ReplicaSide::Local)->revision();
        }
        if (_syncPal->operationSet(ReplicaSide::Remote)->nbOps() == 0) {
            _lastRemoteSnapshotSyncedRevision = _syncPal->snapshot(ReplicaSide::Remote)->revision();
        }
        exitCode = ExitCode::Ok;
        LOG_SYNCPAL_INFO(_logger, "FS operation sets generated in: " << elapsedSeconds.count() << "s");
    }

    postponeCreateOperationsOnReusedIds();

    LOG_SYNCPAL_DEBUG(_logger, "Worker stopped: name=" << name());
    setDone(exitCode);
}

// Remove deleted nodes from sync_node table & cache
ExitCode ComputeFSOperationWorker::updateSyncNode(const SyncNodeType syncNodeType) {
    NodeSet nodeIdSet;
    if (auto exitCode = SyncNodeCache::instance()->syncNodes(syncDbId(), syncNodeType, nodeIdSet); exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Error in SyncNodeCache::syncNodes");
        return exitCode;
    }

    std::erase_if(nodeIdSet, [this, &syncNodeType](auto &item) {
        const bool ok = _syncPal->snapshot(CommonUtility::syncNodeTypeSide(syncNodeType))->exists(item);
        return !ok;
    });

    if (auto exitCode = SyncNodeCache::instance()->update(syncDbId(), syncNodeType, nodeIdSet); exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Error in SyncNodeCache::update");
        return exitCode;
    }

    return ExitCode::Ok;
}

ExitCode ComputeFSOperationWorker::updateSyncNode() {
    for (auto syncNodeTypeIdx = toInt(SyncNodeType::WhiteList); syncNodeTypeIdx <= toInt(SyncNodeType::UndecidedList);
         syncNodeTypeIdx++) {
        const auto syncNodeType = static_cast<SyncNodeType>(syncNodeTypeIdx);

        if (auto exitCode = updateSyncNode(syncNodeType); exitCode != ExitCode::Ok) {
            LOG_WARN(Log::instance()->getLogger(), "Error in SyncPal::updateSyncNode for syncNodeType=" << syncNodeType);
            return exitCode;
        }
    }

    return ExitCode::Ok;
}

ExitCode ComputeFSOperationWorker::inferChangeFromDbNode(const ReplicaSide side, const DbNode &dbNode,
                                                         const SyncPath &localDbPath, const SyncPath &remoteDbPath) {
    assert(side != ReplicaSide::Unknown);

    const NodeId &nodeId = dbNode.nodeId(side);
    if (nodeId.empty()) {
        LOGW_SYNCPAL_WARN(_logger, side << L" node ID empty for for dbId=" << dbNode.nodeId());
        setExitCause(ExitCause::DbEntryNotFound);
        return ExitCode::DataError;
    }

    const auto dbModificationTime = dbNode.lastModified(side);
    const auto dbName = dbNode.name(side);
    const auto &dbPath = side == ReplicaSide::Local ? localDbPath : remoteDbPath;
    const auto snapshot = _syncPal->snapshot(side);
    const auto opSet = _syncPal->operationSet(side);

    NodeId parentNodeid;
    bool parentNodeIsFoundInDb = false;
    if (!_syncDbReadOnlyCache.parent(side, nodeId, parentNodeid, parentNodeIsFoundInDb)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::parent");
        setExitCause(ExitCause::DbAccessError);
        return ExitCode::DbError;
    }
    if (!parentNodeIsFoundInDb) {
        LOG_SYNCPAL_DEBUG(_logger, "Failed to retrieve node for dbId=" << nodeId);
        setExitCause(ExitCause::DbEntryNotFound);
        return ExitCode::DataError;
    }

    // Detect DELETE
    bool remoteItemUnsynced = false;
    bool movedIntoUnsyncedFolder = false;
    const auto nodeExistsInSnapshot = snapshot->exists(nodeId);
    bool nodeIdReused = false;
#if defined(KD_LINUX)
    isReusedNodeId(nodeId, dbNode, snapshot, nodeIdReused);
#endif

    if (side == ReplicaSide::Remote) {
        // In case of a move inside an excluded folder, the item must be removed in this sync
        if (isInUnsyncedListParentSearchInDb(nodeId, ReplicaSide::Remote)) {
            remoteItemUnsynced = true;
            if (nodeExistsInSnapshot && parentNodeid != snapshot->parentId(nodeId)) {
                movedIntoUnsyncedFolder = true;
            }
        }
    } else if (isInUnsyncedListParentSearchInDb(nodeId, ReplicaSide::Local)) {
        return ExitCode::Ok;
    }

    if (!nodeExistsInSnapshot || movedIntoUnsyncedFolder || nodeIdReused) {
        bool isInDeletedFolder = false;
        if (!checkIfPathIsInDeletedFolder(dbPath, isInDeletedFolder)) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in SyncPal::checkIfPathIsInDeletedFolder: " << Utility::formatSyncPath(dbPath));
            return ExitCode::SystemError;
        }

        if (!isInDeletedFolder) {
            // Check that the file/directory really does not exist on replica
            bool isExcluded = false;
            if (const ExitInfo exitInfo = checkIfOkToDelete(side, dbPath, nodeId, isExcluded); !exitInfo) {
                // Can happen only on local side
                return ExitCode::SystemError;
            }

            if (isExcluded) return ExitCode::Ok; // Never generate operation on excluded file
        }

        if (isInUnsyncedListParentSearchInSnapshot(snapshot, nodeId, side)) {
            // Ignore operation
            return ExitCode::Ok;
        }

        bool checkTemplate = side == ReplicaSide::Remote;
        if (side == ReplicaSide::Local) {
            const SyncPath localPath = _syncPal->localPath() / dbPath;

            // Do not propagate delete if the path is too long.
            const size_t pathSize = localPath.native().size();
            if (PlatformInconsistencyCheckerUtility::instance()->isPathTooLong(pathSize)) {
                LOGW_SYNCPAL_WARN(_logger, L"Path length too long (" << pathSize << L" characters) for item with "
                                                                     << Utility::formatSyncPath(localPath)
                                                                     << L". Item is ignored.");
                return ExitCode::Ok;
            }

            if (!nodeExistsInSnapshot) {
                bool exists = false;
                IoError ioError = IoError::Success;
                if (!IoHelper::checkIfPathExists(localPath, exists, ioError) || ioError == IoError::AccessDenied) {
                    LOGW_SYNCPAL_WARN(_logger,
                                      L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(localPath, ioError));
                    return ExitCode::SystemError;
                }
                checkTemplate = exists;
            }
        }

        // Exits if the item is excluded.
        if (checkTemplate && ExclusionTemplateCache::instance()->isExcluded(dbPath)) return ExitCode::Ok;

        // Delete operation
        const auto fsOp = std::make_shared<FSOperation>(OperationType::Delete, nodeId, dbNode.type(),
                                                        dbNode.created().has_value() ? dbNode.created().value() : 0,
                                                        dbModificationTime, dbNode.size(), dbPath);
        opSet->insertOp(fsOp);
        logOperationGeneration(snapshot->side(), fsOp);

        if (nodeIdReused) (void) _localReusedIds.insert(nodeId);

        if (dbNode.type() == NodeType::Directory && !addFolderToDelete(dbPath)) {
            LOGW_SYNCPAL_WARN(_logger,
                              L"Error in ComputeFSOperationWorker::addFolderToDelete: " << Utility::formatSyncPath(dbPath));
            return ExitCode::SystemError;
        }
        return ExitCode::Ok;
    }

    if (remoteItemUnsynced) {
        // Ignore operations on unsynced items
        return ExitCode::Ok;
    }

    // Load snapshot path
    SyncPath snapshotPath;
    if (bool ignore = false; !snapshot->path(nodeId, snapshotPath, ignore)) {
        if (ignore) {
            notifyIgnoredItem(nodeId, snapshotPath, dbNode.type());
            return ExitCode::Ok;
        }
        LOGW_SYNCPAL_WARN(_logger, L"Failed to retrieve path from snapshot for item " << SyncName2WStr(dbName) << L" ("
                                                                                      << CommonUtility::s2ws(nodeId) << L")");
        setExitCause(ExitCause::InvalidSnapshot);
        return ExitCode::DataError;
    }

    if (side == ReplicaSide::Remote && isPathTooLong(snapshotPath, nodeId, snapshot->type(nodeId))) {
        return ExitCode::Ok;
    }

    // Detect EDIT
    const SyncTime snapshotModificationTime = snapshot->lastModified(nodeId);
    // On FAT filesystems, the time resolution for modification time is 2 seconds. Therefor, we ignore EDIT operations if the
    // difference between modification time in DB and the one on the filesystem is less or equal to 1sec.
    const bool modifiedTimeDiffIsEnough = abs(snapshotModificationTime - dbModificationTime) > _timeDifferenceThresholdForEdit;
    const SyncTime snapshotCreatedAt = snapshot->createdAt(nodeId);
    const SyncTime dbCreatedAt = dbNode.created().has_value() ? dbNode.created().value() : 0;
    const auto sameSize = snapshot->isLink(nodeId) || snapshot->size(nodeId) == dbNode.size();
    // Size can differ for links between remote and local replica, do not check it in that case
    if (dbNode.type() == NodeType::File &&
        (modifiedTimeDiffIsEnough || !sameSize || (snapshotCreatedAt != dbCreatedAt && side == ReplicaSide::Local))) {
        // Edit operation
        const auto fsOp = std::make_shared<FSOperation>(OperationType::Edit, nodeId, NodeType::File, snapshot->createdAt(nodeId),
                                                        snapshotModificationTime, snapshot->size(nodeId), snapshotPath);
        opSet->insertOp(fsOp);
        logOperationGeneration(snapshot->side(), fsOp);
    }

    // Detect MOVE
    if (const auto snapshotName = snapshot->name(nodeId); dbName != snapshotName || parentNodeid != snapshot->parentId(nodeId)) {
        FSOpPtr fsOp = nullptr;
        if (isInUnsyncedListParentSearchInSnapshot(snapshot, nodeId, side)) {
            // Delete operation
            fsOp = std::make_shared<FSOperation>(OperationType::Delete, nodeId, dbNode.type(), snapshot->createdAt(nodeId),
                                                 snapshotModificationTime, snapshot->size(nodeId), dbPath);
        } else {
            // Move operation
            fsOp = std::make_shared<FSOperation>(OperationType::Move, nodeId, dbNode.type(), snapshot->createdAt(nodeId),
                                                 snapshotModificationTime, snapshot->size(nodeId), dbPath, snapshotPath);
        }

        opSet->insertOp(fsOp);
        logOperationGeneration(snapshot->side(), fsOp);
    }

    return ExitCode::Ok;
}


ExitCode ComputeFSOperationWorker::inferChangesFromDb(const NodeType nodeType, NodeIdSet &localIdsSet, NodeIdSet &remoteIdsSet,
                                                      NodeIdsSet &remainingNodesIds) {
    for (auto nodesIdsIt = remainingNodesIds.begin(); nodesIdsIt != remainingNodesIds.end();) {
        if (nodesIdsIt->dbNodeId == _syncPal->syncDb()->rootNode().nodeId()) {
            nodesIdsIt = remainingNodesIds.erase(nodesIdsIt);
            continue; // Ignore root folder
        }

        if (stopAsked()) {
            return ExitCode::Ok;
        }

        if (!hasChangedSinceLastSeen(*nodesIdsIt)) {
            (void) localIdsSet.insert(nodesIdsIt->localNodeId);
            (void) remoteIdsSet.insert(nodesIdsIt->remoteNodeId);
            nodesIdsIt = remainingNodesIds.erase(nodesIdsIt);
            continue;
        }

        DbNode dbNode;
        bool dbNodeIsFound = false;
        const auto dbNodeIds = *nodesIdsIt;
        if (!_syncDbReadOnlyCache.node(dbNodeIds.dbNodeId, dbNode, dbNodeIsFound)) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::node");
            setExitCause(ExitCause::DbAccessError);
            return ExitCode::DbError;
        }
        if (!dbNodeIsFound) {
            // The node is not anymore in the DB (one of its parents has been blacklisted)
            nodesIdsIt = remainingNodesIds.erase(nodesIdsIt);
            continue;
        }


        if (dbNode.type() != nodeType) {
            ++nodesIdsIt;
            continue;
        }

        if (dbNode.nodeIdLocal()) (void) localIdsSet.insert(*dbNode.nodeIdLocal());
        if (dbNode.nodeIdRemote()) (void) remoteIdsSet.insert(*dbNode.nodeIdRemote());
        nodesIdsIt = remainingNodesIds.erase(nodesIdsIt);

        SyncPath localDbPath;
        SyncPath remoteDbPath;
        bool dbPathsAreFound = false;
        if (!_syncDbReadOnlyCache.path(dbNodeIds.dbNodeId, localDbPath, remoteDbPath, dbPathsAreFound)) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::path");
            setExitCause(ExitCause::DbAccessError);
            return ExitCode::DbError;
        }
        if (!dbPathsAreFound) {
            LOG_SYNCPAL_WARN(_logger, "Failed to retrieve node for dbId=" << dbNodeIds.dbNodeId);
            setExitCause(ExitCause::DbEntryNotFound);
            return ExitCode::DataError;
        }

        for (const auto side: std::array<ReplicaSide, 2>{ReplicaSide::Local, ReplicaSide::Remote}) {
            if (const auto inferExitCode = inferChangeFromDbNode(side, dbNode, localDbPath, remoteDbPath);
                inferExitCode != ExitCode::Ok) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in ComputeFSOperationWorker::inferChangeFromDbNode: side="
                                                   << side << L" " << Utility::formatSyncPath(localDbPath) << L" code="
                                                   << inferExitCode);
                if (side == ReplicaSide::Local && inferExitCode == ExitCode::SystemError) {
                    if (const ExitInfo exitInfo = blacklistItem(localDbPath); !exitInfo) {
                        LOG_SYNCPAL_WARN(_logger, "Error in ComputeFSOperationWorker::blacklistItem");
                    }
                    break;
                }
                return inferExitCode;
            }
        }

        if (const auto fileIntegrityExitCode = checkFileIntegrity(dbNode); fileIntegrityExitCode != ExitCode::Ok) {
            return fileIntegrityExitCode;
        }
    }

    return ExitCode::Ok;
}

ExitCode ComputeFSOperationWorker::inferChangesFromDb(NodeIdSet &localIdsSet, NodeIdSet &remoteIdsSet) {
    bool dbIdsArefound = false;
    NodeIdsSet remainingNodesIds;
    if (!_syncDbReadOnlyCache.ids(remainingNodesIds, dbIdsArefound)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::ids");
        setExitCause(ExitCause::DbAccessError);
        return ExitCode::DbError;
    } else if (!dbIdsArefound) {
        LOG_SYNCPAL_DEBUG(_logger, "No items found in db");
        return ExitCode::Ok;
    }

    // First, detect changes only for directories: this populates exclusion lists.
    // Second, detect changes for files. This is made faster thanks to exclusion lists.
    _dirPathToDeleteSet.clear();
    for (const auto nodeType: std::array<NodeType, 2>{NodeType::Directory, NodeType::File}) {
        if (const auto exitCode = inferChangesFromDb(nodeType, localIdsSet, remoteIdsSet, remainingNodesIds);
            exitCode != ExitCode::Ok) {
            return exitCode;
        }
    }

    return ExitCode::Ok;
}

ExitCode ComputeFSOperationWorker::exploreSnapshotTree(ReplicaSide side, const NodeIdSet &idsSet) {
    const std::shared_ptr<const Snapshot> snapshot = _syncPal->snapshot(side);
    std::shared_ptr<FSOperationSet> opSet = _syncPal->operationSet(side);

    NodeIdSet remainingDbIds;
    snapshot->ids(remainingDbIds);
    if (remainingDbIds.empty()) {
        LOG_SYNCPAL_DEBUG(_logger, "No items found in snapshot on side " << side);
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

            if (*snapIdIt == snapshot->rootFolderId()) {
                // Ignore root folder
                snapIdIt = remainingDbIds.erase(snapIdIt);
                continue; // Do not process root node
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
                    LOGW_SYNCPAL_DEBUG(_logger, L"Ignoring orphan node " << SyncName2WStr(snapshot->name(nodeId)) << L" ("
                                                                         << CommonUtility::s2ws(nodeId) << L")");
                }
                continue;
            }

            SyncPath snapshotPath;
            if (bool ignore = false; !snapshot->path(nodeId, snapshotPath, ignore)) {
                if (ignore) {
                    notifyIgnoredItem(nodeId, snapshotPath, type);
                    continue;
                }

                LOG_SYNCPAL_WARN(_logger, "Failed to retrieve path from snapshot for item " << nodeId);
                setExitCause(ExitCause::InvalidSnapshot);
                return ExitCode::DataError;
            }

            // Manage file exclusion
            const int64_t snapshotSize = snapshot->size(nodeId);
            if (isExcludedFromSync(snapshot, side, nodeId, snapshotPath, type, snapshotSize)) {
                continue;
            }

            // Create operation
            auto fsOp = std::make_shared<FSOperation>(OperationType::Create, nodeId, type, snapshot->createdAt(nodeId),
                                                      snapshot->lastModified(nodeId), snapshotSize, snapshotPath);
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

    std::wstringstream ss;
    ss << L"Generate " << side << L" " << fsOp->operationType() << L" FS operation. ";
    ss << L"type=" << (fsOp->objectType() == NodeType::Directory ? L"dir" : L"file");
    if (fsOp->operationType() == OperationType::Move) {
        ss << L", from " << Utility::formatSyncPath(fsOp->path()) << L", to " << Utility::formatSyncPath(fsOp->destinationPath());
    } else {
        ss << L", " << Utility::formatSyncPath(fsOp->path());
    }
    ss << L", id=" << CommonUtility::s2ws(fsOp->nodeId());
    ss << L", last modification time=" << fsOp->lastModified();
    ss << L", created at=" << fsOp->createdAt();
    ss << L", size=" << fsOp->size();

    LOGW_SYNCPAL_DEBUG(_logger, ss.str())
}

ExitCode ComputeFSOperationWorker::checkFileIntegrity(const DbNode &dbNode) {
    if (dbNode.type() != NodeType::File) {
        return ExitCode::Ok;
    }

    if (!CommonUtility::isFileSizeMismatchDetectionEnabled()) {
        return ExitCode::Ok;
    }

    if (!dbNode.nodeIdLocal().has_value() || !dbNode.nodeIdRemote().has_value() || !dbNode.lastModifiedLocal().has_value()) {
        return ExitCode::Ok;
    }

    if (_fileSizeMismatchMap.contains(dbNode.nodeIdLocal().value())) {
        // Size mismatch already detected
        return ExitCode::Ok;
    }

    if (!_syncPal->snapshot(ReplicaSide::Local)->exists(dbNode.nodeIdLocal().value()) ||
        !_syncPal->snapshot(ReplicaSide::Remote)->exists(dbNode.nodeIdRemote().value())) {
        // Ignore if item does not exist
        return ExitCode::Ok;
    }

    if (const bool localSnapshotIsLink = _syncPal->snapshot(ReplicaSide::Local)->isLink(dbNode.nodeIdLocal().value());
        localSnapshotIsLink) {
        // Local and remote links sizes are not always the same (macOS aliases, Windows junctions)
        return ExitCode::Ok;
    }

    int64_t localSnapshotSize = _syncPal->snapshot(ReplicaSide::Local)->size(dbNode.nodeIdLocal().value());
    int64_t remoteSnapshotSize = _syncPal->snapshot(ReplicaSide::Remote)->size(dbNode.nodeIdRemote().value());
    SyncTime localsnapshotModificationTime = _syncPal->snapshot(ReplicaSide::Local)->lastModified(dbNode.nodeIdLocal().value());
    SyncTime remotesnapshotModificationTime =
            _syncPal->snapshot(ReplicaSide::Remote)->lastModified(dbNode.nodeIdRemote().value());

    // A mismatch is detected if all timestamps are equal but the sizes in snapshots differ.
    if (localSnapshotSize != remoteSnapshotSize && localsnapshotModificationTime == dbNode.lastModifiedLocal().value() &&
        localsnapshotModificationTime == remotesnapshotModificationTime) {
        SyncPath localSnapshotPath;
        if (bool ignore = false;
            !_syncPal->snapshot(ReplicaSide::Local)->path(dbNode.nodeIdLocal().value(), localSnapshotPath, ignore)) {
            if (ignore) {
                notifyIgnoredItem(dbNode.nodeIdLocal().value(), localSnapshotPath, dbNode.type());
                return ExitCode::Ok;
            }

            LOGW_SYNCPAL_WARN(_logger, L"Failed to retrieve path from snapshot for item "
                                               << SyncName2WStr(dbNode.nameLocal()) << L" ("
                                               << CommonUtility::s2ws(dbNode.nodeIdLocal().value()) << L")");
            setExitCause(ExitCause::InvalidSnapshot);
            return ExitCode::DataError;
        }

        // OS might fail to notify all delete events, therefor we check that the file still exists
        SyncPath absoluteLocalPath = _syncPal->localPath() / localSnapshotPath;

        // No operations detected on this file but its size is not the same between remote and local replica
        // Remove it from local replica and download the remote version
        LOGW_SYNCPAL_DEBUG(_logger, L"File size mismatch for \"" << Path2WStr(absoluteLocalPath)
                                                                 << L"\". Remote version will be downloaded again.");
        _fileSizeMismatchMap.insert({dbNode.nodeIdLocal().value(), absoluteLocalPath});
        sentry::Handler::captureMessage(sentry::Level::Warning, "ComputeFSOperationWorker::exploreDbTree",
                                        "File size mismatch detected");
    }

    return ExitCode::Ok;
}

bool ComputeFSOperationWorker::isExcludedFromSync(const std::shared_ptr<const Snapshot> snapshot, const ReplicaSide side,
                                                  const NodeId &nodeId, const SyncPath &path, NodeType type, int64_t size) {
    if (isInUnsyncedListParentSearchInSnapshot(snapshot, nodeId, side)) {
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_SYNCPAL_DEBUG(_logger, L"Ignoring item " << Path2WStr(path) << L" (" << CommonUtility::s2ws(nodeId)
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
                LOGW_SYNCPAL_DEBUG(_logger, L"Blacklisting item with " << Utility::formatSyncPath(path) << L" ("
                                                                       << CommonUtility::s2ws(nodeId)
                                                                       << L") because it is too big");
            }
            return true;
        }
    }
    return false;
}

bool ComputeFSOperationWorker::isInUnsyncedListParentSearchInDb(const NodeId &nodeId, const ReplicaSide side) const {
    if (_localTmpUnsyncedList.empty() && _remoteTmpUnsyncedList.empty() && _remoteUnsyncedList.empty()) {
        return false;
    }

    NodeId tmpNodeId = nodeId;
    bool found = false;
    do {
        // Check if node already exists on other side
        NodeId otherTmpNodeId;
        _syncDbReadOnlyCache.correspondingNodeId(side, tmpNodeId, otherTmpNodeId, found);

        if (const NodeId localId = side == ReplicaSide::Local ? tmpNodeId : otherTmpNodeId;
            !localId.empty() && _localTmpUnsyncedList.contains(localId)) {
            return true;
        }
        if (const NodeId remoteId = side == ReplicaSide::Remote ? tmpNodeId : otherTmpNodeId;
            !remoteId.empty() && (_remoteTmpUnsyncedList.contains(remoteId) || _remoteUnsyncedList.contains(remoteId))) {
            return true;
        }

        if (!_syncDbReadOnlyCache.parent(side, tmpNodeId, tmpNodeId, found)) {
            LOG_WARN(_logger, "Error in SyncDb::parent");
            break;
        }
    } while (found);

    return false;
}

bool ComputeFSOperationWorker::isInUnsyncedListParentSearchInSnapshot(const std::shared_ptr<const Snapshot> snapshot,
                                                                      const NodeId &nodeId, const ReplicaSide side) const {
    if (_localTmpUnsyncedList.empty() && _remoteTmpUnsyncedList.empty() && _remoteUnsyncedList.empty()) {
        return false;
    }

    NodeId tmpNodeId = nodeId;
    while (!tmpNodeId.empty() && tmpNodeId != snapshot->rootFolderId()) {
        // Check if node already exists on other side
        NodeId otherTmpNodeId;
        bool found = false;
        _syncDbReadOnlyCache.correspondingNodeId(side, tmpNodeId, otherTmpNodeId, found);

        if (const NodeId localId = side == ReplicaSide::Local ? tmpNodeId : otherTmpNodeId;
            !localId.empty() && _localTmpUnsyncedList.contains(localId)) {
            return true;
        }
        if (const NodeId remoteId = side == ReplicaSide::Remote ? tmpNodeId : otherTmpNodeId;
            !remoteId.empty() && (_remoteTmpUnsyncedList.contains(remoteId) || _remoteUnsyncedList.contains(remoteId))) {
            return true;
        }

        tmpNodeId = snapshot->parentId(tmpNodeId);
    }

    return false;
}

bool ComputeFSOperationWorker::isWhitelisted(const std::shared_ptr<const Snapshot> snapshot, const NodeId &nodeId) const {
    NodeIdSet whiteList;
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

bool ComputeFSOperationWorker::isTooBig(const std::shared_ptr<const Snapshot> remoteSnapshot, const NodeId &remoteNodeId,
                                        int64_t size) {
    if (isWhitelisted(remoteSnapshot, remoteNodeId)) {
        return false;
    }

    // Check if file already exist on local side
    NodeId localNodeId;
    bool found = false;
    _syncDbReadOnlyCache.correspondingNodeId(ReplicaSide::Remote, remoteNodeId, localNodeId, found);
    if (found) {
        // We already synchronize the item locally, keep it
        return false;
    }

    // On first sync after migration from version under 3.4.0, the DB is empty but a big folder might as been whitelisted
    // Therefor check also with path
    SyncPath relativePath;
    if (bool ignore = false; remoteSnapshot->path(remoteNodeId, relativePath, ignore)) {
        if (ignore) {
            notifyIgnoredItem(remoteNodeId, relativePath, remoteSnapshot->type(remoteNodeId));
            return false;
        }

        localNodeId = _syncPal->snapshot(ReplicaSide::Local)->itemId(relativePath);
        if (!localNodeId.empty()) {
            // We already synchronize the item locally, keep it
            return false;
        }
    }

    if (ParametersCache::instance()->parameters().useBigFolderSizeLimit() &&
        size > ParametersCache::instance()->parameters().bigFolderSizeLimitB() &&
        _sync.virtualFileMode() == VirtualFileMode::Off) {
        // Update undecided list
        NodeIdSet tmp;
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

bool ComputeFSOperationWorker::isPathTooLong(const SyncPath &path, const NodeId &nodeId, NodeType type) const {
    const SyncPath absolutePath = _syncPal->localPath() / path;
    const size_t pathSize = absolutePath.native().size();
    if (PlatformInconsistencyCheckerUtility::instance()->isPathTooLong(pathSize)) {
        LOGW_SYNCPAL_WARN(_logger, L"Path length too long (" << pathSize << L" characters) for item with "
                                                             << Utility::formatSyncPath(absolutePath) << L". Item is ignored.");

        Error err(_syncPal->syncDbId(), "", nodeId, type, path, ConflictType::None, InconsistencyType::PathLength);
        _syncPal->addError(err);

        return true;
    }
    return false;
}

#if defined(KD_LINUX)
void ComputeFSOperationWorker::isReusedNodeId(const NodeId &localNodeId, const DbNode &dbNode,
                                              const std::shared_ptr<const Snapshot> &snapshot, bool &isReused) const {
    isReused = false;
    // Check if the node is in the liveSnapshot
    if (snapshot->side() != ReplicaSide::Local || !snapshot->exists(localNodeId)) {
        return;
    }

    // Check if the node type has changed
    if (snapshot->type(localNodeId) != NodeType::Unknown && dbNode.type() != NodeType::Unknown &&
        snapshot->type(localNodeId) != dbNode.type()) {
        isReused = true;
        LOGW_SYNCPAL_DEBUG(_logger, L"Node type has changed for " << CommonUtility::s2ws(localNodeId) << L" from "
                                                                  << dbNode.type() << L" to " << snapshot->type(localNodeId));
        return;
    }

    /* The nodeId will be considered as reused if each of the following has changed :
     * - the creation date,
     * - the modification date,
     * - the size,
     * - the path (this is needed as some software might delete and recreate a file when saving it, wich will change all the
     *             previous properties, but the file is still the same)
     */

    // Check if the creation date has changed
    if (snapshot->createdAt(localNodeId) == dbNode.created().value()) {
        return;
    }

    // Check if the node name has changed:
    if (snapshot->name(localNodeId) == dbNode.nameLocal()) {
        return;
    }

    // For a directory, the last modified date in db is not updated when a child is added or removed, but only when the directory
    // is renamed. Therefore, it does not make sense to check the last modified date for directories here.
    if (snapshot->type(localNodeId) == NodeType::Directory) {
        isReused = true;
        LOGW_SYNCPAL_DEBUG(_logger, L"Creation date (old: "
                                            << dbNode.created().value() << L" / new: " << snapshot->createdAt(localNodeId)
                                            << L") and name (old: " << Utility::formatSyncName(dbNode.nameLocal()) << L" / new: "
                                            << Utility::formatSyncName(snapshot->name(localNodeId)) << L") changed for "
                                            << CommonUtility::s2ws(localNodeId) << L". Node is reused.");
        return;
    }

    // Check if the mtime has changed
    if (snapshot->lastModified(localNodeId) == dbNode.lastModified(ReplicaSide::Local)) {
        return;
    }

    // Check if the node size has changed
    if (snapshot->size(localNodeId) == dbNode.size()) {
        return;
    }

    LOGW_SYNCPAL_DEBUG(_logger, L"Size (old: "
                                        << dbNode.size() << L" / new: " << snapshot->size(localNodeId)
                                        << L"), creation date and modification date (old: " << dbNode.created().value() << L" | "
                                        << dbNode.lastModified(ReplicaSide::Local) << L" / new: "
                                        << snapshot->createdAt(localNodeId) << L" | " << snapshot->lastModified(localNodeId)
                                        << L") and name (old: " << Utility::formatSyncName(dbNode.nameLocal()) << L" / new: "
                                        << Utility::formatSyncName(snapshot->name(localNodeId)) << L") have all changed for "
                                        << CommonUtility::s2ws(localNodeId) << L". Node is reused.");
    isReused = true;
}
#endif

ExitInfo ComputeFSOperationWorker::checkIfOkToDelete(const ReplicaSide side, const SyncPath &relativePath, const NodeId &nodeId,
                                                     bool &isExcluded) {
    if (side != ReplicaSide::Local) return ExitCode::Ok;

    if (!_syncPal->snapshot(ReplicaSide::Local)->itemId(relativePath).empty()) {
        // Item with the same path but different ID exist
        // This is an Edit operation (Delete-Create)
        return ExitCode::Ok;
    }

    const SyncPath absolutePath = _syncPal->localPath() / relativePath;
    bool existsWithSameId = false;
    NodeId otherNodeId;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExistsWithSameNodeId(absolutePath, nodeId, existsWithSameId, otherNodeId, ioError)) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in IoHelper::checkIfPathExistsWithSameNodeId: "
                                           << Utility::formatIoError(absolutePath, ioError));

        if (ioError == IoError::InvalidFileName) {
            // Observed on MacOSX under special circumstances; see getItemType unit test edge cases.
            setExitCause(ExitCause::InvalidName);
            return {ExitCode::SystemError, ExitCause::InvalidName};

        } else if (ioError == IoError::AccessDenied) {
            setExitCause(ExitCause::FileAccessError);
            return {ExitCode::SystemError, ExitCause::FileAccessError};
        }
        setExitCause(ExitCause::Unknown);
        return ExitCode::SystemError;
    }

    if (!existsWithSameId) return ExitCode::Ok;

    // Check if file is synced
    if (ExclusionTemplateCache::instance()->isExcluded(relativePath)) {
        isExcluded = true;
        return ExitCode::Ok;
    }
    if (_syncPal->snapshot(ReplicaSide::Local)->isOrphan(nodeId)) {
        // This can happen if the propagation of template exclusions has been unexpectedly interrupted.
        // This special handling should be removed once the app keeps track on such interruptions.
        return ExitCode::Ok;
    }

    LOGW_SYNCPAL_DEBUG(_logger, L"Item " << Path2WStr(absolutePath)
                                         << L" still exists on local replica. Snapshot not up to date, restarting sync.");

    sentry::Handler::captureMessage(sentry::Level::Warning, "ComputeFSOperationWorker::checkIfOkToDelete",
                                    "Unwanted local delete operation averted");

    setExitCause(ExitCause::InvalidSnapshot);
    return {ExitCode::DataError, ExitCause::InvalidSnapshot}; // We need to rebuild the local snapshot from scratch.
}

void ComputeFSOperationWorker::deleteChildOpRecursively(const std::shared_ptr<const Snapshot> remoteSnapshot,
                                                        const NodeId &remoteNodeId, NodeIdSet &tmpTooBigList) {
    NodeIdSet childrenIds;
    remoteSnapshot->getChildrenIds(remoteNodeId, childrenIds);

    for (auto &childId: childrenIds) {
        if (remoteSnapshot->type(childId) == NodeType::Directory) {
            deleteChildOpRecursively(remoteSnapshot, childId, tmpTooBigList);
        }
        (void) _syncPal->_remoteOperationSet->removeOp(remoteNodeId, OperationType::Create);
        tmpTooBigList.erase(childId);
    }
}

void ComputeFSOperationWorker::deleteLocalDescendantCreateOps(const NodeId &localNodeId) {
    const auto localSnapshot = _syncPal->snapshot(ReplicaSide::Local);
    const auto descendantIds = localSnapshot->getDescendantIds(localNodeId);

    for (const auto &descendantId: descendantIds) _syncPal->removeLocalOperation(descendantId, OperationType::Create);
}

void ComputeFSOperationWorker::updateUnsyncedList() {
    sentry::pTraces::scoped::UpdateUnsyncedList perfMonitor(syncDbId());
    SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), SyncNodeType::UndecidedList, _remoteUnsyncedList);
    NodeIdSet tmp;
    SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), SyncNodeType::BlackList, tmp);
    _remoteUnsyncedList.merge(tmp);
    SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), SyncNodeType::TmpRemoteBlacklist, _remoteTmpUnsyncedList);
    _remoteUnsyncedList.merge(_remoteTmpUnsyncedList);

    SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), SyncNodeType::TmpLocalBlacklist,
                                         _localTmpUnsyncedList); // Only tmp list on local side
}

bool ComputeFSOperationWorker::addFolderToDelete(const SyncPath &path) {
    SyncPath normalizedPath;
    if (!Utility::normalizedSyncPath(path, normalizedPath)) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in Utility::normalizedSyncPath: " << Utility::formatSyncPath(path));
        return false;
    }

    // Remove sub dirs
    auto dirPathToDeleteSetIt = _dirPathToDeleteSet.begin();
    while (dirPathToDeleteSetIt != _dirPathToDeleteSet.end()) {
        if (CommonUtility::isSubDir(normalizedPath, *dirPathToDeleteSetIt)) {
            dirPathToDeleteSetIt = _dirPathToDeleteSet.erase(dirPathToDeleteSetIt);
        } else {
            dirPathToDeleteSetIt++;
        }
    }

    // Insert dir
    _dirPathToDeleteSet.insert(normalizedPath);

    return true;
}

bool ComputeFSOperationWorker::checkIfPathIsInDeletedFolder(const SyncPath &path, bool &isInDeletedFolder) {
    isInDeletedFolder = false;

    SyncPath normalizedPath;
    if (!Utility::normalizedSyncPath(path, normalizedPath)) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in Utility::normalizedSyncPath: " << Utility::formatSyncPath(path));
        return false;
    }

    for (auto dirPathToDeleteSetIt = _dirPathToDeleteSet.begin(); dirPathToDeleteSetIt != _dirPathToDeleteSet.end();
         ++dirPathToDeleteSetIt) {
        if (CommonUtility::isSubDir(*dirPathToDeleteSetIt, normalizedPath)) {
            isInDeletedFolder = true;
            break;
        }
    }

    return true;
}

bool ComputeFSOperationWorker::hasChangedSinceLastSeen(const NodeIds &nodeIds) const {
    // Check if the item has change since the last sync
    for (const auto side: std::array<ReplicaSide, 2>{ReplicaSide::Local, ReplicaSide::Remote}) {
        const auto lastItemChangeSnapshotRevision = _syncPal->snapshot(side)->lastChangeRevision(nodeIds.nodeId(side));
        const auto lastSyncedSnapshotRevision =
                side == ReplicaSide::Local ? _lastLocalSnapshotSyncedRevision : _lastRemoteSnapshotSyncedRevision;
        if (lastItemChangeSnapshotRevision == 0 || lastItemChangeSnapshotRevision > lastSyncedSnapshotRevision) {
            return true;
        }
    }
    return false;
}

void ComputeFSOperationWorker::notifyIgnoredItem(const NodeId &nodeId, const SyncPath &relativePath, const NodeType nodeType) {
    if (!relativePath.root_name().empty()) {
        // Display the error only once per broken path.
        // We used root name here because the path is a relative path here. Therefor, root name should always be empty.
        // Only broken path (ex: a directory named "E:S") will have a non empty root name.
        const auto [_, inserted] = _ignoredDirectoryNames.insert(relativePath.root_name());
        if (inserted) {
            LOGW_SYNCPAL_INFO(_logger,
                              L"Item (or one of its descendants) has been ignored: " << Utility::formatSyncPath(relativePath));
            const Error err(_syncPal->syncDbId(), "", nodeId, nodeType, relativePath, ConflictType::None,
                            InconsistencyType::ReservedName);
            _syncPal->addError(err);
        }
    }
}

ExitInfo ComputeFSOperationWorker::blacklistItem(const SyncPath &relativeLocalPath) {
    // Blacklist item
    if (ExitInfo exitInfo = _syncPal->handleAccessDeniedItem(relativeLocalPath); !exitInfo) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in SyncPal::handleAccessDeniedItem: " << Utility::formatSyncPath(relativeLocalPath));
        return exitInfo;
    }

    // Update unsynced list cache
    updateUnsyncedList();
    return ExitCode::Ok;
}

} // namespace KDC
