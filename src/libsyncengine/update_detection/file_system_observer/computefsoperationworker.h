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

#pragma once

#include "syncpal/isyncworker.h"
#include "db/syncdb.h"
#include "db/syncdbreadonlycache.h"
#include "syncpal/syncpal.h"

namespace KDC {

class ComputeFSOperationWorker : public ISyncWorker {
    public:
        ComputeFSOperationWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName);
        /**
         * Constructor used for testing only
         * @param testSyncDb
         * @param name
         * @param shortName
         */
        ComputeFSOperationWorker(SyncDbReadOnlyCache &testSyncDbReadOnlyCache, const std::string &name,
                                 const std::string &shortName);

        const std::unordered_map<NodeId, SyncPath> getFileSizeMismatchMap() const { return _fileSizeMismatchMap; }

    protected:
        void execute() override;

    private:
        using NodeIdSet = NodeSet;
        using DbNodeIdSet = std::unordered_set<DbNodeId>;
        using NodeIdsSet = std::unordered_set<NodeIds, NodeIds::HashFunction>;
        SnapshotRevision _lastLocalSnapshotSyncedRevision = 0;
        SnapshotRevision _lastRemoteSnapshotSyncedRevision = 0;


        // Detect changes based on the database records: delete, move and edit operations
        ExitCode inferChangesFromDb(NodeIdSet &localIdsSet, NodeIdSet &remoteIdsSet);
        ExitCode inferChangesFromDb(const NodeType nodeType, NodeIdSet &localIdsSet, NodeIdSet &remoteIdsSet,
                                    NodeIdsSet &remainingNodesIds); // Restrict change detection to a node type.
        ExitCode inferChangeFromDbNode(const ReplicaSide side, const DbNode &dbNode, const SyncPath &localDbPath,
                                       const SyncPath &remoteDbPath); // Detect change for a single node on a specific side.

        // Detect changes based on the snapshot records: create operations
        ExitCode exploreSnapshotTree(ReplicaSide side, const NodeSet &idsSet);

        ExitCode checkFileIntegrity(const DbNode &dbNode);

        bool isExcludedFromSync(const std::shared_ptr<const Snapshot> snapshot, const ReplicaSide side, const NodeId &nodeId,
                                const SyncPath &path, NodeType type, int64_t size);
        /**
         * Check if the item, or any ancestor, appears in any blacklist. Also checks for each corresponding node in other snapshot
         * if it appears in any blacklist. Parents are retrieved from DB.
         * @param nodeId The ID of the item to evaluate.
         * @param side The replica side corresponding to the provided ID.
         * @return `true` if the item is blacklisted in any blacklist.
         */
        bool isInUnsyncedListParentSearchInDb(const NodeId &nodeId, ReplicaSide side) const;
        /**
         * Check if the item, or any ancestor, appears in any blacklist. Also checks for each corresponding node in other snapshot
         * if it appears in any blacklist. Parents are retrieved from snapshot.
         * @param snapshot The snapshot that contains `nodeId`
         * @param nodeId The ID of the item to evaluate.
         * @param side The replica side corresponding to the provided ID.
         * @return `true` if the item is blacklisted in any blacklist.
         */
        bool isInUnsyncedListParentSearchInSnapshot(std::shared_ptr<const Snapshot> snapshot, const NodeId &nodeId,
                                                    ReplicaSide side) const; // Search parent in snapshot
        bool isWhitelisted(const std::shared_ptr<const Snapshot> snapshot, const NodeId &nodeId) const;
        bool isTooBig(const std::shared_ptr<const Snapshot> remoteSnapshot, const NodeId &remoteNodeId, int64_t size);
        bool isPathTooLong(const SyncPath &path, const NodeId &nodeId, NodeType type) const;
#if defined(KD_LINUX)
        void isReusedNodeId(const NodeId &localNodeId, const DbNode &dbNode, const std::shared_ptr<const Snapshot> &snapshot,
                            bool &isReused) const;
#endif
        ExitInfo checkIfOkToDelete(ReplicaSide side, const SyncPath &relativePath, const NodeId &nodeId, bool &isExcluded);

        void deleteChildOpRecursively(const std::shared_ptr<const Snapshot> remoteSnapshot, const NodeId &remoteNodeId,
                                      NodeSet &tmpTooBigList);
        void deleteLocalDescendantOps(const NodeId &localNodeId);

        void updateUnsyncedList();
        ExitCode updateSyncNode(SyncNodeType syncNodeType);
        ExitCode updateSyncNode();
        void logOperationGeneration(const ReplicaSide side, const FSOpPtr fsOp);
        void notifyIgnoredItem(const NodeId &nodeId, const SyncPath &relativePath, NodeType nodeType);
        ExitInfo blacklistItem(const SyncPath &relativeLocalPath);

        // The remote propagation of the creation of local items that reuse identifiers of deleted items is postponed to the next
        // synchronization. So is the propagation of the creation of the descendants of those local items.
        void postponeCreateOperationsOnReusedIds();

        SyncDbReadOnlyCache &_syncDbReadOnlyCache;
        Sync _sync;

        NodeIdSet _remoteUnsyncedList;
        NodeIdSet _remoteTmpUnsyncedList;
        NodeIdSet _localTmpUnsyncedList;
        NodeIdSet _localReusedIds;

        std::unordered_set<SyncPath, PathHashFunction> _dirPathToDeleteSet;
        std::unordered_map<NodeId, SyncPath> _fileSizeMismatchMap; // File size mismatch checks are only enabled when env var:
                                                                   // KDRIVE_ENABLE_FILE_SIZE_MISMATCH_DETECTION is set
        SyncNameSet _ignoredDirectoryNames;

        uint16_t _timeDifferenceThresholdForEdit{0};

        bool addFolderToDelete(const SyncPath &path);
        bool checkIfPathIsInDeletedFolder(const SyncPath &path, bool &isInDeletedFolder);
        bool hasChangedSinceLastSeen(const NodeIds &nodeIds) const;
        friend class TestComputeFSOperationWorker;
};

} // namespace KDC
