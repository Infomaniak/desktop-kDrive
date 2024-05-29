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

#pragma once

#include "syncpal/isyncworker.h"
#include "db/syncdb.h"
#include "syncpal/syncpal.h"

namespace KDC {

class ComputeFSOperationWorker : public ISyncWorker {
    public:
        ComputeFSOperationWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName);
        /**
         * Constructor used for testing only
         * @param testSyncDb
         * @param testLocalSnapshot
         * @param testRemoteSnapshot
         * @param name
         * @param shortName
         */
        ComputeFSOperationWorker(const std::shared_ptr<SyncDb> testSyncDb, const std::shared_ptr<Snapshot> testLocalSnapshot
                                 , const std::shared_ptr<Snapshot> testRemoteSnapshot
                                 , const std::string &name, const std::string &shortName);

        const std::unordered_map<NodeId, SyncPath> getFileSizeMismatchMap() const { return _fileSizeMismatchMap; }

    protected:
        virtual void execute() override;

    private:
        ExitCode exploreDbTree(std::unordered_set<NodeId> &localIdsSet, std::unordered_set<NodeId> &remoteIdsSet);
        ExitCode exploreSnapshotTree(ReplicaSide side, const std::unordered_set<NodeId> &idsSet);
        ExitCode checkFileIntegrity(const DbNode &dbNode);

        bool isExcludedFromSync(const std::shared_ptr<Snapshot> snapshot, const ReplicaSide side, const NodeId &nodeId,
                                const SyncPath &path, NodeType type, int64_t size);
        bool isInUnsyncedList(const NodeId &nodeId, const ReplicaSide side);  // Search parent in DB
        bool isInUnsyncedList(const std::shared_ptr<Snapshot> snapshot, const NodeId &nodeId, const ReplicaSide side,
                              bool tmpListOnly = false);  // Search parent in snapshot
        bool isWhitelisted(const std::shared_ptr<Snapshot> snapshot, const NodeId &nodeId);
        bool isTooBig(const std::shared_ptr<Snapshot> remoteSnapshot, const NodeId &remoteNodeId, int64_t size);
        bool isPathTooLong(const SyncPath &path, const NodeId &nodeId, NodeType type);

        ExitCode checkIfOkToDelete(ReplicaSide side, const SyncPath &relativePath, const NodeId &nodeId);

        void deleteChildOpRecursively(const std::shared_ptr<Snapshot> remoteSnapshot, const NodeId &remoteNodeId,
                                      std::unordered_set<NodeId> &tmpTooBigList);

        void updateUnsyncedList();

        void logOperationGeneration(const ReplicaSide side, const FSOpPtr fsOp);

        const std::shared_ptr<SyncDb> _syncDb;
        const std::shared_ptr<Snapshot> _localSnapshot;
        const std::shared_ptr<Snapshot> _remoteSnapshot;
        Sync _sync;

        std::unordered_set<NodeId> _remoteUnsyncedList;
        std::unordered_set<NodeId> _remoteTmpUnsyncedList;
        std::unordered_set<NodeId> _localTmpUnsyncedList;

        std::unordered_set<SyncPath, hashPathFunction> _dirPathToDeleteSet;

        std::unordered_map<NodeId, SyncPath> _fileSizeMismatchMap;

        void addFolderToDelete(const SyncPath &path);
        bool pathInDeletedFolder(const SyncPath &path);

        friend class TestComputeFSOperationWorker;
};

}  // namespace KDC
