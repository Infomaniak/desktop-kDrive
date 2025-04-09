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

#include "syncdb.h"

namespace KDC {

class SyncDbCache {
    public:
        SyncDbCache(std::shared_ptr<SyncDb> syncDb);
        bool reloadCacheIfNeeded();
        void clearCache();
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

    private:
        bool isChacheUpToDate() const;
        SyncDbRevision _cachedRevision = 0;
        std::shared_ptr<SyncDb> _syncDb;
        std::unordered_map<DbNodeId, DbNode> _dbNodesCache;
        std::unordered_map<NodeId, DbNodeId> _localNodeIdsCache;
        std::unordered_map<NodeId, DbNodeId> _remoteNodeIdsCache;
};

} // namespace KDC
