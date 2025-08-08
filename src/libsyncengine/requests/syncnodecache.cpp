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

#include "syncnodecache.h"
#include "db/syncdb.h"
#include "libcommonserver/log/log.h"

#include <log4cplus/loggingmacros.h>

namespace KDC {

std::shared_ptr<SyncNodeCache> SyncNodeCache::_instance = nullptr;

std::shared_ptr<SyncNodeCache> SyncNodeCache::instance() {
    if (_instance == nullptr) {
        try {
            _instance = std::shared_ptr<SyncNodeCache>(new SyncNodeCache());
        } catch (std::exception const &) {
            return nullptr;
        }
    }

    return _instance;
}

SyncNodeCache::SyncNodeCache() {}

ExitCode SyncNodeCache::checkIfSyncExists(const int syncDbId) const noexcept {
    if (!_syncDbMap.contains(syncDbId)) {
        LOG_WARN(Log::instance()->getLogger(), "Sync not found in syncDb map for syncDbId=" << syncDbId);
        return ExitCode::DataError;
    }

    if (!_syncNodesMap.contains(syncDbId)) {
        LOG_WARN(Log::instance()->getLogger(), "Sync not found in syncNodes map for syncDbId=" << syncDbId);
        return ExitCode::DataError;
    }

    return ExitCode::Ok;
}


ExitCode SyncNodeCache::checkIfTypeExists(const int syncDbId, const SyncNodeType type) const {
    assert(_syncNodesMap.contains(syncDbId) && "Sync not found in SyncNodeCache::checkIfTypeExists.");

    if (!_syncNodesMap.at(syncDbId).contains(type)) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Type not found in syncNodes map for syncDbId=" << syncDbId << " and type= " << type);
        return ExitCode::DataError;
    }

    return ExitCode::Ok;
}

ExitCode SyncNodeCache::syncNodes(const int syncDbId, const SyncNodeType type, NodeSet &syncNodes) {
    const std::scoped_lock lock(_mutex);

    if (auto exitCode = checkIfSyncExists(syncDbId); exitCode != ExitCode::Ok) return exitCode;
    if (auto exitCode = checkIfTypeExists(syncDbId, type); exitCode != ExitCode::Ok) return exitCode;

    syncNodes = _syncNodesMap[syncDbId][type];

    return ExitCode::Ok;
}

bool SyncNodeCache::contains(const int syncDbId, const SyncNodeType type, const NodeId &nodeId) const noexcept {
    if (auto exitCode = checkIfSyncExists(syncDbId); exitCode != ExitCode::Ok) return false;
    if (auto exitCode = checkIfTypeExists(syncDbId, type); exitCode != ExitCode::Ok) return false;

    return _syncNodesMap.at(syncDbId).at(type).contains(nodeId);
}
bool SyncNodeCache::contains(const int syncDbId, const NodeId &nodeId) const noexcept {
    if (auto exitCode = checkIfSyncExists(syncDbId); exitCode != ExitCode::Ok) return false;

    for (auto typeInt = toInt(SyncNodeType::BlackList); typeInt <= toInt(SyncNodeType::TmpLocalBlacklist); ++typeInt) {
        const auto type = fromInt<SyncNodeType>(typeInt);
        if (_syncNodesMap.at(syncDbId).at(type).contains(nodeId)) return true;
    }

    return false;
}

ExitInfo SyncNodeCache::deleteSyncNode(const int syncDbId, const NodeId &nodeId) {
    const std::scoped_lock lock(_mutex);

    if (auto exitCode = checkIfSyncExists(syncDbId); exitCode != ExitCode::Ok) return exitCode;

    // Remove `nodeId` from cache.
    for (auto &[type, nodeSet]: _syncNodesMap[syncDbId]) {
        if (nodeSet.contains(nodeId)) {
            (void) nodeSet.erase(nodeId);
            break;
        }
    }

    // Remove `nodeId` from SyncDb.
    bool found = false;
    if (!_syncDbMap[syncDbId]->deleteSyncNode(nodeId, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in SyncDb::deleteSyncNode");
        return ExitCode::DbError;
    }

    ExitCause exitCause = ExitCause::Unknown;
    if (!found) exitCause = ExitCause::NotFound;

    return {ExitCode::Ok, exitCause};
}

ExitCode SyncNodeCache::update(const int syncDbId, const SyncNodeType type, const NodeSet &syncNodes) {
    const std::scoped_lock lock(_mutex);

    if (auto exitCode = checkIfSyncExists(syncDbId); exitCode != ExitCode::Ok) return exitCode;
    if (auto exitCode = checkIfTypeExists(syncDbId, type); exitCode != ExitCode::Ok) return exitCode;

    _syncNodesMap[syncDbId][type] = syncNodes;

    // Update sync nodes set
    if (!_syncDbMap[syncDbId]->updateAllSyncNodes(type, syncNodes)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in SyncDb::updateAllSyncNodes");
        return ExitCode::DbError;
    }

    return ExitCode::Ok;
}

ExitCode SyncNodeCache::initCache(const int syncDbId, std::shared_ptr<SyncDb> syncDb) {
    const std::scoped_lock lock(_mutex);

    _syncDbMap[syncDbId] = syncDb;

    // Load sync nodes for all sync node types
    for (auto typeInt = toInt(SyncNodeType::BlackList); typeInt <= toInt(SyncNodeType::TmpLocalBlacklist); ++typeInt) {
        const auto type = fromInt<SyncNodeType>(typeInt);
        NodeSet nodeIdSet;
        if (!syncDb->selectAllSyncNodes(type, nodeIdSet)) {
            LOG_WARN(Log::instance()->getLogger(), "Error in SyncDb::selectAllSyncNodes");
            return ExitCode::DbError;
        }
        _syncNodesMap[syncDbId][type] = nodeIdSet;
    }

    return ExitCode::Ok;
}

ExitCode SyncNodeCache::clear(const int syncDbId) {
    const std::scoped_lock lock(_mutex);

    if (auto exitCode = checkIfSyncExists(syncDbId); exitCode != ExitCode::Ok) return exitCode;

    _syncDbMap.erase(syncDbId);
    _syncNodesMap.erase(syncDbId);

    return ExitCode::Ok;
}

} // namespace KDC
