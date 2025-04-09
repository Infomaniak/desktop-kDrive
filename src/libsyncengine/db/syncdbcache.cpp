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

#include "syncdbcache.h"

namespace KDC {
SyncDbCache::SyncDbCache(std::shared_ptr<SyncDb> syncDb) : _syncDb(syncDb) {}

bool SyncDbCache::isChacheUpToDate() const {
    if (_cachedRevision != _syncDb->revision()) {
        LOG_INFO(Log::instance()->getLogger(), "SyncDbCache: cache is not up to date.");
        return false;
    }
    return true;
}

void SyncDbCache::clearCache() {
    _dbNodesCache.clear();
    _localNodeIdsCache.clear();
    _remoteNodeIdsCache.clear();
    _cachedRevision = 0;
}

bool SyncDbCache::reloadCacheIfNeeded() {
    if (isChacheUpToDate()) {
        return true;
    }
    clearCache();
    if (!_syncDb) {
        LOG_ERROR(Log::instance()->getLogger(), "SyncDbCache: SyncDb is null");
        return false;
    }

    bool found = false;
    std::unordered_set<DbNode, DbNode::hashFunction> foundDbNodes;
    if (!_syncDb->dbNodes(foundDbNodes, _cachedRevision, found)) {
        LOG_ERROR(Log::instance()->getLogger(), " SyncDbCache::reloadCacheIfNeeded: Error getting db nodes from SyncDb");
        return false;
    }

    for (const auto& dbNode: foundDbNodes) {
        (void) _dbNodesCache.try_emplace(dbNode.nodeId(), dbNode);
        if (dbNode.hasLocalNodeId()) {
            (void) _localNodeIdsCache.try_emplace(NodeId(dbNode.nodeIdLocal().value()), dbNode.nodeId());
        }
        if (dbNode.hasRemoteNodeId()) {
            (void) _remoteNodeIdsCache.try_emplace(NodeId(dbNode.nodeIdRemote().value()), dbNode.nodeId());
        }
    }
    LOG_DEBUG(Log::instance()->getLogger(), "SyncDbCache: cache updated, revision=" << _syncDb->revision());
}

bool SyncDbCache::parent(ReplicaSide side, const NodeId& nodeId, NodeId& parentNodeId, bool& found) {
    if (!reloadCacheIfNeeded()) {
        return false;
    }
    found = false;
    const std::unordered_map<NodeId, DbNodeId>& nodeIdsCache =
            side == ReplicaSide::Local ? _localNodeIdsCache : _remoteNodeIdsCache;
    const auto dbNodeIdIt = nodeIdsCache.find(nodeId);
    if (dbNodeIdIt == nodeIdsCache.end()) {
        return true;
    }

    const auto dbNodeIt = _dbNodesCache.find(dbNodeIdIt->second);
    if (dbNodeIt == _dbNodesCache.end()) {
        return true;
    }
    const auto& dbNode = dbNodeIt->second;

    if (!dbNode.parentNodeId()) {
        return true;
    }

    const auto parentDbNodeIt = _dbNodesCache.find(dbNode.parentNodeId().value());
    if (parentDbNodeIt == _dbNodesCache.end()) {
        return true;
    }

    parentNodeId = parentDbNodeIt->second.nodeId(side);
    found = true;
    return true;
}

bool SyncDbCache::correspondingNodeId(ReplicaSide side, const NodeId& nodeIdIn, NodeId& nodeIdOut, bool& found) {
    if (!reloadCacheIfNeeded()) {
        return false;
    }
    found = false;
    const std::unordered_map<NodeId, DbNodeId>& nodeIdsCache =
            side == ReplicaSide::Local ? _localNodeIdsCache : _remoteNodeIdsCache;
    const auto dbNodeIdIt = nodeIdsCache.find(nodeIdIn);
    if (dbNodeIdIt == nodeIdsCache.end()) {
        return true;
    }

    const auto dbNodeIt = _dbNodesCache.find(dbNodeIdIt->second);
    if (dbNodeIt == _dbNodesCache.end()) {
        return true;
    }
    found = true;
    nodeIdOut = dbNodeIt->second.nodeId(side);
    return true;
}

// Returns database ID for the ID nodeId of the snapshot from replica `side`
bool SyncDbCache::dbId(ReplicaSide side, const NodeId& nodeId, DbNodeId& dbNodeId, bool& found) {
    if (!reloadCacheIfNeeded()) {
        return false;
    }
    found = false;
    dbNodeId = 0;
    if (side == ReplicaSide::Unknown) {
        LOG_ERROR(Log::instance()->getLogger(), "Call to SyncDb::dbId with snapshot=='ReplicaSide::Unknown'");
        return false;
    }
    const std::unordered_map<NodeId, DbNodeId>& nodeIdsCache =
            side == ReplicaSide::Local ? _localNodeIdsCache : _remoteNodeIdsCache;

    const auto dbNodeIdIt = nodeIdsCache.find(nodeId);
    if (dbNodeIdIt == nodeIdsCache.end()) {
        return true;
    }
    const auto dbNodeIt = _dbNodesCache.find(dbNodeIdIt->second);
    if (dbNodeIt == _dbNodesCache.end()) {
        return true;
    }
    found = true;
    dbNodeId = dbNodeIt->second.nodeId();
    return true;
}

bool SyncDbCache::node(DbNodeId dbNodeId, DbNode& dbNode, bool& found) {
    if (!reloadCacheIfNeeded()) {
        return false;
    }
    found = false;
    const auto dbNodeIt = _dbNodesCache.find(dbNodeId);
    if (dbNodeIt == _dbNodesCache.end()) {
        return true;
    }
    found = true;
    dbNode = dbNodeIt->second;
    return true;
}
bool SyncDbCache::node(ReplicaSide side, const NodeId& nodeId, DbNode& dbNode, bool& found) {
    if (!reloadCacheIfNeeded()) {
        return false;
    }
    found = false;

    const std::unordered_map<NodeId, DbNodeId>& nodeIdsCache =
            side == ReplicaSide::Local ? _localNodeIdsCache : _remoteNodeIdsCache;
    const auto dbNodeIdIt = nodeIdsCache.find(nodeId);
    if (dbNodeIdIt == nodeIdsCache.end()) {
        return true;
    }
    const auto dbNodeIt = _dbNodesCache.find(dbNodeIdIt->second);
    if (dbNodeIt == _dbNodesCache.end()) {
        return true;
    }
    found = true;
    dbNode = dbNodeIt->second;
    return true;
}
bool SyncDbCache::ids(ReplicaSide side, std::vector<NodeId>& ids, bool& found) {
    if (!reloadCacheIfNeeded()) {
        return false;
    }
    const std::unordered_map<NodeId, DbNodeId>& nodeIdsCache =
            side == ReplicaSide::Local ? _localNodeIdsCache : _remoteNodeIdsCache;

    for (const auto& nodeIdIt: nodeIdsCache) {
        ids.push_back(nodeIdIt.first);
    }
    found = nodeIdsCache.size() > 0;
    return true;
}
bool SyncDbCache::ids(ReplicaSide side, std::set<NodeId>& ids, bool& found) {
    if (!reloadCacheIfNeeded()) {
        return false;
    }
    const std::unordered_map<NodeId, DbNodeId>& nodeIdsCache =
            side == ReplicaSide::Local ? _localNodeIdsCache : _remoteNodeIdsCache;

    for (const auto& [nodeId, _]: nodeIdsCache) {
        (void) ids.insert(nodeId);
    }
    found = nodeIdsCache.size() > 0;
    return true;
}
} // namespace KDC
