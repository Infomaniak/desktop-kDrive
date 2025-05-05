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
#include "libcommon/utility/types.h"
#include <set>
#include <unordered_map>
#include <list>
#include <mutex>

namespace KDC {
class SyncDb;
class SyncDbReadOnlyCache {
    public:
        explicit SyncDbReadOnlyCache(SyncDb &syncDb) :
            _syncDb(syncDb) {};
        bool reloadCacheIfNeeded();
        void clear();
        // Getters with replica IDs
        bool parent(ReplicaSide side, const NodeId &nodeId, NodeId &parentNodeid, bool &found);
        bool correspondingNodeId(ReplicaSide side, const NodeId &nodeIdIn, NodeId &nodeIdOut, bool &found);
        // Returns database ID for the ID nodeId of the snapshot from replica `side`
        bool dbId(ReplicaSide side, const NodeId &nodeId, DbNodeId &dbNodeId, bool &found);
        bool node(DbNodeId dbNodeId, DbNode &dbNode, bool &found);
        bool node(ReplicaSide side, const NodeId &nodeId, DbNode &dbNode, bool &found);
        // Returns the list of IDs contained in snapshot
        bool ids(ReplicaSide side, std::vector<NodeId> &ids, bool &found);
        bool ids(ReplicaSide side, std::set<NodeId> &ids, bool &found);
        bool ids(std::unordered_set<NodeIds, NodeIds::hashNodeIdsFunction> &ids, bool &found);

        bool path(DbNodeId dbNodeId, SyncPath &localPath, SyncPath &remotePath, bool &found, bool recursiveCall = false);
        bool path(ReplicaSide side, const NodeId &nodeId, SyncPath &path, bool &found);

        // Returns the id of the object from its path
        // path is relative to the root directory
        bool id(ReplicaSide side, const SyncPath &path, std::optional<NodeId> &nodeId, bool &found);

        bool id(ReplicaSide side, DbNodeId dbNodeId, NodeId &nodeId, bool &found);

        DbNode rootNode();
        SyncDbRevision revision() const;

    private:
        bool isChacheUpToDate() const;
        DbNodeId getDbNodeIdFromNodeId(ReplicaSide side, const NodeId &nodeId, bool &found);
        const DbNode &getDbNodeFromDbNodeId(const DbNodeId &dbNodeId, bool &found);
        SyncDbRevision _cachedRevision = 0;
        SyncDb &_syncDb;
        mutable std::recursive_mutex _mutex;
        std::unordered_map<DbNodeId, std::pair<SyncPath /*local*/, SyncPath /*remote*/>> _dbNodesPathCache;
        std::unordered_map<DbNodeId, DbNode> _dbNodesCache;
        std::unordered_map<DbNodeId /*parent*/, std::list<DbNodeId /*children*/>> _dbNodesParentToChildrenMap;
        std::unordered_map<NodeId, DbNodeId> _localNodeIdToDbNodeIdMap;
        std::unordered_map<NodeId, DbNodeId> _remoteNodeIdToDbNodeIdMap;
};

} // namespace KDC
