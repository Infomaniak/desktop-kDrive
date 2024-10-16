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

#include "syncnodecache.h"
#include "db/syncdb.h"
#include "libcommonserver/log/log.h"

#include <log4cplus/loggingmacros.h>

namespace KDC {

std::shared_ptr<SyncNodeCache> SyncNodeCache::_instance = nullptr;

std::shared_ptr<SyncNodeCache> SyncNodeCache::instance() noexcept {
    if (_instance == nullptr) {
        try {
            _instance = std::shared_ptr<SyncNodeCache>(new SyncNodeCache());
        } catch (std::exception const &) {
            assert(false);
            return nullptr;
        }
    }

    return _instance;
}

SyncNodeCache::SyncNodeCache() {}

ExitCode SyncNodeCache::syncNodes(int syncDbId, SyncNodeType type, std::unordered_set<NodeId> &syncNodes) {
    const std::scoped_lock lock(_mutex);

    if (_syncNodesMap.find(syncDbId) == _syncNodesMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "Sync not found in syncNodes map for syncDbId=" << syncDbId);
        return ExitCode::DataError;
    }

    if (_syncNodesMap[syncDbId].find(type) == _syncNodesMap[syncDbId].end()) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Type not found in syncNodes map for syncDbId=" << syncDbId << " and type= " << type);
        return ExitCode::DataError;
    }

    syncNodes = _syncNodesMap[syncDbId][type];
    return ExitCode::Ok;
}

ExitCode SyncNodeCache::update(int syncDbId, SyncNodeType type, const std::unordered_set<NodeId> &syncNodes) {
    const std::scoped_lock lock(_mutex);

    if (_syncDbMap.find(syncDbId) == _syncDbMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "Sync not found in syncDb map for syncDbId=" << syncDbId);
        return ExitCode::DataError;
    }

    if (_syncNodesMap.find(syncDbId) == _syncNodesMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "Sync not found in syncNodes map for syncDbId=" << syncDbId);
        return ExitCode::DataError;
    }

    if (_syncNodesMap[syncDbId].find(type) == _syncNodesMap[syncDbId].end()) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Type not found in syncNodes map for syncDbId=" << syncDbId << " and type= " << type);
        return ExitCode::DataError;
    }

    _syncNodesMap[syncDbId][type] = syncNodes;

    // Update sync nodes set
    if (!_syncDbMap[syncDbId]->updateAllSyncNodes(type, syncNodes)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in SyncDb::updateAllSyncNodes");
        return ExitCode::DbError;
    }

    return ExitCode::Ok;
}

ExitCode SyncNodeCache::initCache(int syncDbId, std::shared_ptr<SyncDb> syncDb) {
    const std::scoped_lock lock(_mutex);

    _syncDbMap[syncDbId] = syncDb;

    // Load sync nodes for all sync node types
    for (int typeInt = toInt(SyncNodeType::BlackList); typeInt <= toInt(SyncNodeType::TmpLocalBlacklist); typeInt++) {
        SyncNodeType type = fromInt<SyncNodeType>(typeInt);
        std::unordered_set<NodeId> nodeIdSet;
        if (!syncDb->selectAllSyncNodes(type, nodeIdSet)) {
            LOG_WARN(Log::instance()->getLogger(), "Error in SyncDb::selectAllSyncNodes");
            return ExitCode::DbError;
        }
        _syncNodesMap[syncDbId][type] = nodeIdSet;
    }

    return ExitCode::Ok;
}

ExitCode SyncNodeCache::clearCache(int syncDbId) {
    const std::scoped_lock lock(_mutex);

    if (_syncDbMap.find(syncDbId) == _syncDbMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "Sync not found in syncDb map for syncDbId=" << syncDbId);
        return ExitCode::DataError;
    }

    if (_syncNodesMap.find(syncDbId) == _syncNodesMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "Sync not found in syncNodes map for syncDbId=" << syncDbId);
        return ExitCode::DataError;
    }

    _syncDbMap.erase(syncDbId);
    _syncNodesMap.erase(syncDbId);

    return ExitCode::Ok;
}

} // namespace KDC
