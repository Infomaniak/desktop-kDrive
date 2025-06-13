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
#include "syncdbreadonlycache.h"
namespace KDC {

bool SyncDbReadOnlyCache::isCacheUpToDate() const {
    if (_cachedRevision != _syncDb.revision()) {
        LOG_INFO(Log::instance()->getLogger(), "SyncDbReadOnlyCache: cache is not up to date, cached revision="
                                                       << _cachedRevision << ", current syncDb revision=" << _syncDb.revision());
        return false;
    }
    return true;
}

DbNodeId SyncDbReadOnlyCache::getDbNodeIdFromNodeId(const ReplicaSide side, const NodeId &nodeId, bool &found) {
    found = false;
    const auto &nodeIdToDbNodeIdMap = side == ReplicaSide::Local ? _localNodeIdToDbNodeIdMap : _remoteNodeIdToDbNodeIdMap;
    const auto dbNodeIdIt = nodeIdToDbNodeIdMap.find(nodeId);
    if (dbNodeIdIt == nodeIdToDbNodeIdMap.end()) {
        return -1;
    }
    found = true;
    return dbNodeIdIt->second;
}

std::optional<const std::reference_wrapper<DbNode>> SyncDbReadOnlyCache::getDbNodeFromDbNodeId(const DbNodeId &dbNodeId) {
    const auto dbNodeIt = _dbNodesCache.find(dbNodeId);
    if (dbNodeIt == _dbNodesCache.end()) {
        return std::nullopt;
    }
    return std::ref(dbNodeIt->second);
}

void SyncDbReadOnlyCache::clear() {
    const std::scoped_lock lock(_mutex);
    _dbNodesCache.clear();
    _dbNodesParentToChildrenMap.clear();
    _localNodeIdToDbNodeIdMap.clear();
    _remoteNodeIdToDbNodeIdMap.clear();
    _dbNodesPathCache.clear();
    _cachedRevision = 0;
}

bool SyncDbReadOnlyCache::reloadIfNeeded() {
    if (isCacheUpToDate()) return true;
    clear();
    bool found = false;
    std::unordered_set<DbNode, DbNode::HashFunction> dbNodes;
    if (!_syncDb.dbNodes(dbNodes, _cachedRevision, found)) {
        LOG_ERROR(Log::instance()->getLogger(), "SyncDbReadOnlyCache::reloadIfNeeded: Error getting dbNodes from SyncDb");
        return false;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "SyncDbReadOnlyCache::reloadIfNeeded: SyncDb is empty.");
        clear(); // Reset cachedRevision.
        return false;
    }

    for (const auto &dbNode: dbNodes) {
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
    LOG_INFO(Log::instance()->getLogger(), "SyncDbReadOnlyCache updated, cached revision=" << _syncDb.revision());
    return true;
}

bool SyncDbReadOnlyCache::parent(ReplicaSide side, const NodeId &nodeId, NodeId &parentNodeId, bool &found) {
    const std::scoped_lock lock(_mutex);
    LOG_IF_FAIL(Log::instance()->getLogger(), _cachedRevision != 0);
    if (_cachedRevision == 0) return _syncDb.parent(side, nodeId, parentNodeId, found); // Fallback to a call in db.
    if (side == ReplicaSide::Unknown) return false;

    found = false;
    DbNodeId dbNodeId = getDbNodeIdFromNodeId(side, nodeId, found);
    if (!found) {
        return true;
    }

    auto dbNode = getDbNodeFromDbNodeId(dbNodeId);
    if (!dbNode) {
        LOG_WARN(Log::instance()->getLogger(),
                 "SyncDbReadOnlyCache::parent: dbNodeId " << dbNodeId << " not found in syncDbReadOnlyCache");
        found = false;
        return true;
    }

    if (!dbNode->get().parentNodeId()) {
        found = false;
        return true;
    }

    auto parentDbNode = getDbNodeFromDbNodeId(dbNode->get().parentNodeId().value());
    if (!parentDbNode) {
        found = false;
        return true;
    }

    parentNodeId = parentDbNode->get().nodeId(side);
    found = !parentNodeId.empty();
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(),
                 "SyncDbReadOnlyCache::parent: nodeId " << nodeId << " has a parent with an empty nodeId");
    }
    return true;
}

bool SyncDbReadOnlyCache::correspondingNodeId(ReplicaSide side, const NodeId &nodeIdIn, NodeId &nodeIdOut, bool &found) {
    const std::scoped_lock lock(_mutex);
    LOG_IF_FAIL(Log::instance()->getLogger(), _cachedRevision != 0);
    if (_cachedRevision == 0) return _syncDb.correspondingNodeId(side, nodeIdIn, nodeIdOut, found); // Fallback to a call in db.
    if (side == ReplicaSide::Unknown) return false;
    found = false;
    DbNodeId dbNodeId = getDbNodeIdFromNodeId(side, nodeIdIn, found);
    if (!found) {
        return true;
    }

    auto dbNode = getDbNodeFromDbNodeId(dbNodeId);
    if (!dbNode) {
        found = false;
        LOG_WARN(Log::instance()->getLogger(),
                 "SyncDbReadOnlyCache::correspondingNodeId: dbNodeId " << dbNodeId << " not found in syncDbReadOnlyCache");
        return true;
    }

    nodeIdOut = dbNode->get().nodeId(otherSide(side));
    found = !nodeIdOut.empty();
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "SyncDbReadOnlyCache::correspondingNodeId: nodeIdIn "
                                                       << nodeIdIn << " has a corresponding node with an empty nodeId");
    }
    return true;
}

bool SyncDbReadOnlyCache::dbId(ReplicaSide side, const NodeId &nodeId, DbNodeId &dbNodeId, bool &found) {
    const std::scoped_lock lock(_mutex);
    LOG_IF_FAIL(Log::instance()->getLogger(), _cachedRevision != 0);
    if (_cachedRevision == 0) return _syncDb.dbId(side, nodeId, dbNodeId, found); // Fallback to a call in db.
    if (side == ReplicaSide::Unknown) return false;

    found = false;
    dbNodeId = getDbNodeIdFromNodeId(side, nodeId, found);
    return true;
}

bool SyncDbReadOnlyCache::node(DbNodeId dbNodeId, DbNode &dbNode, bool &found) {
    const std::scoped_lock lock(_mutex);
    LOG_IF_FAIL(Log::instance()->getLogger(), _cachedRevision != 0);
    if (_cachedRevision == 0) return _syncDb.node(dbNodeId, dbNode, found); // Fallback to a call in db.

    auto dbNodeOptional = getDbNodeFromDbNodeId(dbNodeId);
    found = dbNodeOptional.has_value();

    if (!found) return true;
    dbNode = *dbNodeOptional;
    return true;
}

bool SyncDbReadOnlyCache::node(ReplicaSide side, const NodeId &nodeId, DbNode &dbNode, bool &found) {
    const std::scoped_lock lock(_mutex);
    LOG_IF_FAIL(Log::instance()->getLogger(), _cachedRevision != 0);
    if (_cachedRevision == 0) return _syncDb.node(side, nodeId, dbNode, found); // Fallback to a call in db.
    if (side == ReplicaSide::Unknown) return false;

    found = false;
    DbNodeId dbNodeId = getDbNodeIdFromNodeId(side, nodeId, found);
    if (!found) {
        return true;
    }
    auto dbNodeOptional = getDbNodeFromDbNodeId(dbNodeId);
    if (dbNodeOptional) {
        dbNode = *dbNodeOptional;
        found = true;
    } else {
        found = false;
        LOG_WARN(Log::instance()->getLogger(),
                 "SyncDbReadOnlyCache::node: dbNodeId " << dbNodeId << " not found in syncDbReadOnlyCache");
    }
    return true;
}

bool SyncDbReadOnlyCache::ids(ReplicaSide side, std::vector<NodeId> &ids, bool &found) {
    const std::scoped_lock lock(_mutex);
    LOG_IF_FAIL(Log::instance()->getLogger(), _cachedRevision != 0)
    if (_cachedRevision == 0) return _syncDb.ids(side, ids, found); // Fallback to a call in db.
    if (side == ReplicaSide::Unknown) return false;

    const std::unordered_map<NodeId, DbNodeId> &nodeIdsCache =
            side == ReplicaSide::Local ? _localNodeIdToDbNodeIdMap : _remoteNodeIdToDbNodeIdMap;

    for (const auto &[nodeId, _]: nodeIdsCache) {
        ids.push_back(nodeId);
    }
    found = !nodeIdsCache.empty();
    return true;
}

bool SyncDbReadOnlyCache::ids(ReplicaSide side, NodeSet &ids, bool &found) {
    const std::scoped_lock lock(_mutex);
    LOG_IF_FAIL(Log::instance()->getLogger(), _cachedRevision != 0);
    if (_cachedRevision == 0) return _syncDb.ids(side, ids, found); // Fallback to a call in db.
    if (side == ReplicaSide::Unknown) return false;

    const std::unordered_map<NodeId, DbNodeId> &nodeIdsCache =
            side == ReplicaSide::Local ? _localNodeIdToDbNodeIdMap : _remoteNodeIdToDbNodeIdMap;

    for (const auto &[nodeId, _]: nodeIdsCache) {
        (void) ids.insert(nodeId);
    }
    found = !nodeIdsCache.empty();
    return true;
}

bool SyncDbReadOnlyCache::ids(std::unordered_set<NodeIds, NodeIds::HashFunction> &ids, bool &found) {
    const std::scoped_lock lock(_mutex);
    LOG_IF_FAIL(Log::instance()->getLogger(), _cachedRevision != 0);
    if (_cachedRevision == 0) return _syncDb.ids(ids, found); // Fallback to a call in db.

    for (const auto &[dbNodeId, dbNode]: _dbNodesCache) {
        NodeIds nodeIds;
        nodeIds.dbNodeId = dbNodeId;
        nodeIds.localNodeId = dbNode.nodeIdLocal().value();
        nodeIds.remoteNodeId = dbNode.nodeIdRemote().value();
        (void) ids.insert(nodeIds);
    }
    found = !ids.empty();

    return true;
}

bool SyncDbReadOnlyCache::path(DbNodeId dbNodeId, SyncPath &localPath, SyncPath &remotePath, bool &found, bool recursiveCall) {
    const std::scoped_lock lock(_mutex);
    LOG_IF_FAIL(Log::instance()->getLogger(), _cachedRevision != 0)
    if (_cachedRevision == 0) return _syncDb.path(dbNodeId, localPath, remotePath, found); // Fallback to a call in db.

    found = false;
    // Check if the path is already cached
    const auto cachedPathIt = _dbNodesPathCache.find(dbNodeId);
    if (cachedPathIt != _dbNodesPathCache.end()) {
        localPath = cachedPathIt->second.first;
        remotePath = cachedPathIt->second.second;
        found = true;
        return true;
    }

    auto dbNode = getDbNodeFromDbNodeId(dbNodeId);
    if (!dbNode) {
        return true;
    }
    if (!dbNode->get().parentNodeId()) {
        localPath = dbNode->get().nameLocal();
        remotePath = dbNode->get().nameRemote();
    } else {
        if (!path(dbNode->get().parentNodeId().value(), localPath, remotePath, found, true)) return false;
        if (!found) return true;
        localPath /= dbNode->get().nameLocal();
        remotePath /= dbNode->get().nameRemote();
    }

    // Only cache the path of the parents to avoid too much memory usage
    if (recursiveCall) (void) _dbNodesPathCache.try_emplace(dbNodeId, localPath, remotePath);
    found = true;
    return true;
}

bool SyncDbReadOnlyCache::path(ReplicaSide side, const NodeId &nodeId, SyncPath &resPath, bool &found) {
    const std::scoped_lock lock(_mutex);
    LOG_IF_FAIL(Log::instance()->getLogger(), _cachedRevision != 0);
    if (_cachedRevision == 0) return _syncDb.path(side, nodeId, resPath, found); // Fallback to a call in db.
    if (side == ReplicaSide::Unknown) return false;

    found = false;
    DbNodeId dbNodeId = getDbNodeIdFromNodeId(side, nodeId, found);
    if (!found) {
        return true;
    }

    SyncPath localPath;
    SyncPath remotePath;
    if (!path(dbNodeId, localPath, remotePath, found)) {
        LOG_WARN(Log::instance()->getLogger(), "SyncDbReadOnlyCache::path: dbNodeId " << dbNodeId << " failed");
        return false;
    }
    if (!found) {
        return true;
    }
    resPath = side == ReplicaSide::Local ? localPath : remotePath;
    return true;
}

bool SyncDbReadOnlyCache::id(ReplicaSide side, const SyncPath &path, std::optional<NodeId> &nodeId, bool &found) {
    const std::scoped_lock lock(_mutex);
    LOG_IF_FAIL(Log::instance()->getLogger(), _cachedRevision != 0);
    if (_cachedRevision == 0) return _syncDb.id(side, path, nodeId, found); // Fallback to a call in db.
    if (side == ReplicaSide::Unknown) return false;

    found = false;
    const auto &itemNames = CommonUtility::splitSyncPath(path);
    DbNode tmpNode = _syncDb.rootNode();
    if (itemNames.empty()) {
        nodeId = tmpNode.nodeId(side);
        found = true;
        return true;
    }

    const auto dbNodesParentToChildrenMapEnd = _dbNodesParentToChildrenMap.end();
    for (const auto &name: itemNames) {
        auto children = _dbNodesParentToChildrenMap.find(tmpNode.nodeId());
        if (children == dbNodesParentToChildrenMapEnd) {
            return true;
        }
        // Replace std::find_if by std::ranges::find_if once compiler version has been bumped for Linux release build.
        const auto childIt =
                std::find_if(children->second.begin(), children->second.end(), [this, &name, &side](const DbNodeId &childId) {
                    const DbNode &childNode = _dbNodesCache.at(childId);
                    return childNode.name(side) == name;
                });
        if (childIt == children->second.end()) {
            return true;
        }
        auto tmpNodeOptional = getDbNodeFromDbNodeId(*childIt);
        if (!tmpNodeOptional) {
            return true;
        }
        tmpNode = *tmpNodeOptional;
    }
    nodeId = tmpNode.nodeId(side);
    found = true;
    return true;
}

bool SyncDbReadOnlyCache::id(ReplicaSide side, DbNodeId dbNodeId, NodeId &nodeId, bool &found) {
    const std::scoped_lock lock(_mutex);
    LOG_IF_FAIL(Log::instance()->getLogger(), _cachedRevision != 0);
    if (_cachedRevision == 0) return _syncDb.id(side, dbNodeId, nodeId, found); // Fallback to a call in db.
    if (side == ReplicaSide::Unknown) return false;

    found = false;
    auto dbNodeOptional = getDbNodeFromDbNodeId(dbNodeId);
    if (!dbNodeOptional) {
        return true;
    }
    found = true;
    nodeId = dbNodeOptional->get().nodeId(side);
    return true;
}

DbNode SyncDbReadOnlyCache::rootNode() {
    return _syncDb.rootNode();
}

SyncDbRevision SyncDbReadOnlyCache::revision() const {
    const std::scoped_lock lock(_mutex);
    return _cachedRevision;
}
} // namespace KDC
