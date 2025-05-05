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

#include "syncdb.h"
#include "syncdbcache.h"
namespace KDC {

bool SyncDbCache::isChacheUpToDate() const {
    if (_cachedRevision != _syncDb.revision()) {
        LOG_INFO(Log::instance()->getLogger(), "SyncDbCache: cache is not up to date, cached revision="
                                                       << _cachedRevision << ", current syncDb revision=" << _syncDb.revision());
        return false;
    }
    return true;
}

DbNodeId SyncDbCache::getDbNodeIdFromNodeId(ReplicaSide side, const NodeId& nodeId, bool& found) {
    found = false;
    const auto& nodeIdToDbNodeIdMap = side == ReplicaSide::Local ? _localNodeIdToDbNodeIdMap : _remoteNodeIdToDbNodeIdMap;
    const auto dbNodeIdIt = nodeIdToDbNodeIdMap.find(nodeId);
    if (dbNodeIdIt == nodeIdToDbNodeIdMap.end()) {
        LOG_WARN(Log::instance()->getLogger(),
                 "SyncDbCache::getDbNodeIdFromNodeId: node " << nodeId << " not found in nodeIdsCache");
        return -1;
    }
    found = true;
    return dbNodeIdIt->second;
}

const DbNode& SyncDbCache::getDbNodeFromDbNodeId(const DbNodeId& dbNodeId, bool& found) {
    found = false;
    const auto dbNodeIt = _dbNodesCache.find(dbNodeId);
    if (dbNodeIt == _dbNodesCache.end()) {
        LOG_ERROR(Log::instance()->getLogger(),
                  "SyncDbCache::getDbNodeFromDbNodeId: dbNodeId " << dbNodeId << " not found in dbNodesCache");
        return DbNode();
    }
    found = true;
    return dbNodeIt->second;
}

void SyncDbCache::clear() {
    const std::scoped_lock lock(_mutex);
    _dbNodesCache.clear();
    _dbNodesParentToChildrenMap.clear();
    _localNodeIdToDbNodeIdMap.clear();
    _remoteNodeIdToDbNodeIdMap.clear();
    _dbNodesPathCache.clear();
    _cachedRevision = 0;
}

bool SyncDbCache::reloadCacheIfNeeded() {
    if (isChacheUpToDate()) return true;
    clear();
    bool found = false;
    std::unordered_set<DbNode, DbNode::hashFunction> dbNodes;
    if (!_syncDb.dbNodes(dbNodes, _cachedRevision, found)) {
        LOG_ERROR(Log::instance()->getLogger(), " SyncDbCache::reloadCacheIfNeeded: Error getting dbNodes from SyncDb");
        return false;
    }

    for (const auto& dbNode: dbNodes) {
        (void) _dbNodesCache.try_emplace(dbNode.nodeId(), dbNode);
        if (dbNode.parentNodeId()) {
            (void) _dbNodesParentToChildrenMap[dbNode.parentNodeId().value()].push_back(dbNode.nodeId());
        }
        if (dbNode.hasLocalNodeId()) {
            (void) _localNodeIdToDbNodeIdMap.try_emplace(dbNode.nodeIdLocal().value(), dbNode.nodeId());
        }
        if (dbNode.hasRemoteNodeId()) {
            (void) _remoteNodeIdToDbNodeIdMap.try_emplace(dbNode.nodeIdRemote().value(), dbNode.nodeId());
        }
    }
    LOG_INFO(Log::instance()->getLogger(), "SyncDbCache updated, cached revision=" << _syncDb.revision());
    return true;
}

bool SyncDbCache::parent(ReplicaSide side, const NodeId& nodeId, NodeId& parentNodeId, bool& found) {
    const std::scoped_lock lock(_mutex);
    LOG_IF_FAIL(Log::instance()->getLogger(), _cachedRevision != 0);
    if (_cachedRevision == 0) return false;

    found = false;

    DbNodeId dbNodeId = getDbNodeIdFromNodeId(side, nodeId, found);
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "SyncDbCache::parent: node " << nodeId << " not found in syncDbCache");
        return true;
    }

    const DbNode& dbNode = getDbNodeFromDbNodeId(dbNodeId, found);
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "SyncDbCache::parent: dbNodeId " << dbNodeId << " not found in syncDbCache");
        return true;
    }

    if (!dbNode.parentNodeId()) {
        found = false;
        return true;
    }

    const DbNode& parentDbNode = getDbNodeFromDbNodeId(dbNode.parentNodeId().value(), found);
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "SyncDbCache::parent: dbNodeId " << dbNodeId << " not found in syncDbCache");
        return true;
    }

    parentNodeId = parentDbNode.nodeId(side);
    found = !parentNodeId.empty();
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "SyncDbCache::parent: node " << nodeId << " has a parent with an empty nodeId");
    }
    return true;
}

bool SyncDbCache::correspondingNodeId(ReplicaSide side, const NodeId& nodeIdIn, NodeId& nodeIdOut, bool& found) {
    const std::scoped_lock lock(_mutex);
    LOG_IF_FAIL(Log::instance()->getLogger(), _cachedRevision != 0);
    if (_cachedRevision == 0) return false;

    found = false;
    DbNodeId dbNodeId = getDbNodeIdFromNodeId(side, nodeIdIn, found);
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(),
                 "SyncDbCache::correspondingNodeId: node " << nodeIdIn << " not found in syncDbCache");
        return true;
    }

    const DbNode& dbNode = getDbNodeFromDbNodeId(dbNodeId, found);
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(),
                 "SyncDbCache::correspondingNodeId: dbNodeId " << dbNodeId << " not found in syncDbCache");
        return true;
    }

    nodeIdOut = dbNode.nodeId(otherSide(side));
    found = !nodeIdOut.empty();
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(),
                 "SyncDbCache::correspondingNodeId: node " << nodeIdIn << " has a corresponding with an empty nodeId");
    }
    return true;
}

bool SyncDbCache::dbId(ReplicaSide side, const NodeId& nodeId, DbNodeId& dbNodeId, bool& found) {
    const std::scoped_lock lock(_mutex);
    LOG_IF_FAIL(Log::instance()->getLogger(), _cachedRevision != 0);
    if (_cachedRevision == 0) return false;

    found = false;

    dbNodeId = getDbNodeIdFromNodeId(side, nodeId, found);
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "SyncDbCache::dbId: node " << nodeId << " not found in syncDbCache");
    }
    return true;
}

bool SyncDbCache::node(DbNodeId dbNodeId, DbNode& dbNode, bool& found) {
    const std::scoped_lock lock(_mutex);
    LOG_IF_FAIL(Log::instance()->getLogger(), _cachedRevision != 0);
    if (_cachedRevision == 0) return false;

    found = false;
    dbNode = getDbNodeFromDbNodeId(dbNodeId, found);
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "SyncDbCache::node: dbNodeId " << dbNodeId << " not found in syncDbCache");
    }
    return true;
}

bool SyncDbCache::node(ReplicaSide side, const NodeId& nodeId, DbNode& dbNode, bool& found) {
    const std::scoped_lock lock(_mutex);
    LOG_IF_FAIL(Log::instance()->getLogger(), _cachedRevision != 0);
    if (_cachedRevision == 0) return false;

    found = false;
    DbNodeId dbNodeId = getDbNodeIdFromNodeId(side, nodeId, found);
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "SyncDbCache::node: node " << nodeId << " not found in syncDbCache");
        return true;
    }

    dbNode = getDbNodeFromDbNodeId(dbNodeId, found);
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "SyncDbCache::node: dbNodeId " << dbNodeId << " not found in syncDbCache");
    }
    return true;
}

bool SyncDbCache::ids(ReplicaSide side, std::vector<NodeId>& ids, bool& found) {
    const std::scoped_lock lock(_mutex);
    LOG_IF_FAIL(Log::instance()->getLogger(), _cachedRevision != 0);
    if (_cachedRevision == 0) return false;

    const std::unordered_map<NodeId, DbNodeId>& nodeIdsCache =
            side == ReplicaSide::Local ? _localNodeIdToDbNodeIdMap : _remoteNodeIdToDbNodeIdMap;

    for (const auto& [nodeId, _]: nodeIdsCache) {
        ids.push_back(nodeId);
    }
    found = !nodeIdsCache.empty();
    return true;
}

bool SyncDbCache::ids(ReplicaSide side, std::set<NodeId>& ids, bool& found) {
    const std::scoped_lock lock(_mutex);
    LOG_IF_FAIL(Log::instance()->getLogger(), _cachedRevision != 0);
    if (_cachedRevision == 0) return false;

    const std::unordered_map<NodeId, DbNodeId>& nodeIdsCache =
            side == ReplicaSide::Local ? _localNodeIdToDbNodeIdMap : _remoteNodeIdToDbNodeIdMap;

    for (const auto& [nodeId, _]: nodeIdsCache) {
        (void) ids.insert(nodeId);
    }
    found = nodeIdsCache.size() > 0;
    return true;
}

bool SyncDbCache::ids(std::unordered_set<NodeIds, NodeIds::hashNodeIdsFunction>& ids, bool& found) {
    const std::scoped_lock lock(_mutex);
    LOG_IF_FAIL(Log::instance()->getLogger(), _cachedRevision != 0);
    if (_cachedRevision == 0) return false;

    for (const auto& [dbNodeId, dbNode]: _dbNodesCache) {
        NodeIds nodeIds;
        nodeIds.dbNodeId = dbNodeId;
        nodeIds.localNodeId = dbNode.nodeIdLocal().value();
        nodeIds.remoteNodeId = dbNode.nodeIdRemote().value();
        (void) ids.insert(nodeIds);
    }
    found = ids.size() > 0;

    return true;
}

bool SyncDbCache::path(DbNodeId dbNodeId, SyncPath& localPath, SyncPath& remotePath, bool& found, bool recursiveCall) {
    const std::scoped_lock lock(_mutex);
    LOG_IF_FAIL(Log::instance()->getLogger(), _cachedRevision != 0);
    if (_cachedRevision == 0) return false;

    found = false;
    // Check if the path is already cached
    if (_dbNodesPathCache.contains(dbNodeId)) {
        localPath = _dbNodesPathCache.at(dbNodeId).first;
        remotePath = _dbNodesPathCache.at(dbNodeId).second;
        found = true;
        return true;
    }

    auto& DbNode = getDbNodeFromDbNodeId(dbNodeId, found);
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "SyncDbCache::path: dbNodeId " << dbNodeId << " not found in syncDbCache");
        return false; // Not found
    }
    if (!DbNode.parentNodeId()) {
        localPath = DbNode.nameLocal();
        remotePath = DbNode.nameRemote();
    } else {
        if (!path(DbNode.parentNodeId().value(), localPath, remotePath, found, true) || !found) return true;
        localPath /= DbNode.nameLocal();
        remotePath /= DbNode.nameRemote();
    }

    // Only cache the path of the parents to avoid too much memory usage
    if (recursiveCall) (void) _dbNodesPathCache.try_emplace(dbNodeId, localPath, remotePath);
    found = true;
    return true;
}

bool SyncDbCache::path(ReplicaSide side, const NodeId& nodeId, SyncPath& resPath, bool& found) {
    const std::scoped_lock lock(_mutex);
    LOG_IF_FAIL(Log::instance()->getLogger(), _cachedRevision != 0);
    if (_cachedRevision == 0) return false;

    found = false;
    DbNodeId dbNodeId = getDbNodeIdFromNodeId(side, nodeId, found);
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "SyncDbCache::path: node " << nodeId << " not found in syncDbCache");
        return true;
    }

    SyncPath localPath;
    SyncPath remotePath;
    if (!path(dbNodeId, localPath, remotePath, found)) {
        LOG_WARN(Log::instance()->getLogger(), "SyncDbCache::path: dbNodeId " << dbNodeId << " not found in syncDbCache");
        return false;
    }
    resPath = side == ReplicaSide::Local ? localPath : remotePath;
    return true;
}

bool SyncDbCache::id(ReplicaSide side, const SyncPath& path, std::optional<NodeId>& nodeId, bool& found) {
    const std::scoped_lock lock(_mutex);
    LOG_IF_FAIL(Log::instance()->getLogger(), _cachedRevision != 0);
    if (_cachedRevision == 0) return false;

    found = false;
    const std::vector<SyncName> itemNames = Utility::splitPath(path);
    DbNode tmpNode = _syncDb.rootNode();
    if (itemNames.empty()) {
        nodeId = tmpNode.nodeId(side);
        found = true;
        return true;
    }

    const auto dbNodesParentToChildrenMap = _dbNodesParentToChildrenMap.end();
    for (auto nameIt = itemNames.rbegin(); nameIt != itemNames.rend(); ++nameIt) {
        auto children = _dbNodesParentToChildrenMap.find(tmpNode.nodeId());
        if (children == dbNodesParentToChildrenMap) {
            LOG_WARN(Log::instance()->getLogger(), "SyncDbCache::id: node " << tmpNode.nodeId() << " not found in syncDbCache");
            return true;
        }
        const auto childIt = std::ranges::find_if(children->second, [this, &nameIt, &side](const DbNodeId& childId) {
            const DbNode& childNode = _dbNodesCache.at(childId);
            return childNode.name(side) == *nameIt;
        });
        if (childIt == children->second.end()) {
            LOG_WARN(Log::instance()->getLogger(),
                     "SyncDbCache::id: node " << SyncName2Str(*nameIt) << " not found in syncDbCache");
            return true;
        }
        tmpNode = getDbNodeFromDbNodeId(*childIt, found);
        if (!found) {
            LOG_WARN(Log::instance()->getLogger(), "SyncDbCache::id: dbNodeId " << *childIt << " not found in syncDbCache");
            return true;
        }
    }
    nodeId = tmpNode.nodeId(side);
    return true;
}

bool SyncDbCache::id(ReplicaSide side, DbNodeId dbNodeId, NodeId& nodeId, bool& found) {
    const std::scoped_lock lock(_mutex);
    LOG_IF_FAIL(Log::instance()->getLogger(), _cachedRevision != 0);
    if (_cachedRevision == 0) return false;

    found = false;
    const DbNode& dbNode = getDbNodeFromDbNodeId(dbNodeId, found);
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "SyncDbCache::id: dbNodeId " << dbNodeId << " not found in syncDbCache");
        return true;
    }
    nodeId = dbNode.nodeId(side);
    return true;
}

SyncDbRevision SyncDbCache::revision() const {
    const std::scoped_lock lock(_mutex);
    return _cachedRevision;
}
} // namespace KDC
