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

#include "tmpblacklistmanager.h"

#include "requests/syncnodecache.h"
#include "requests/parameterscache.h"

#include <unordered_set>

static const int oneHour = 3600;

namespace KDC {

TmpBlacklistManager::TmpBlacklistManager(std::shared_ptr<SyncPal> syncPal) : _syncPal(syncPal) {
    LOG_DEBUG(Log::instance()->getLogger(), "TmpBlacklistManager created");
}

TmpBlacklistManager::~TmpBlacklistManager() {
    LOG_DEBUG(Log::instance()->getLogger(), "TmpBlacklistManager destroyed");
}

int TmpBlacklistManager::getErrorCount(const NodeId &nodeId, ReplicaSide side) const noexcept {
    const auto &errors = side == ReplicaSide::Local ? _localErrors : _remoteErrors;
    const auto errorItem = errors.find(nodeId);

    return errorItem == errors.end() ? 0 : errorItem->second.count + 1;
}

void logMessage(const std::wstring &msg, const NodeId &id, const ReplicaSide side, const SyncPath &path = "") {
    LOGW_INFO(Log::instance()->getLogger(), msg.c_str()
                                                << L" - node ID='" << Utility::s2ws(id).c_str() << L"' - side='" << side
                                                << (path.empty() ? L"'" : (L"' - " + Utility::formatSyncPath(path)).c_str()));
}

void TmpBlacklistManager::increaseErrorCount(const NodeId &nodeId, NodeType type, const SyncPath &relativePath,
                                             ReplicaSide side) {
    auto &errors = side == ReplicaSide::Local ? _localErrors : _remoteErrors;

    auto errorItem = errors.find(nodeId);
    if (errorItem != errors.end()) {
        errorItem->second.count++;
        errorItem->second.lastErrorTime = std::chrono::steady_clock::now();
        logMessage(L"Error counter increased", nodeId, side, relativePath);

        if (errorItem->second.count >= 1) {  // We try only once. TODO: If we keep this logic, no need to keep _count
            insertInBlacklist(nodeId, side);

            SentryHandler::instance()->captureMessage(SentryLevel::Warning, "TmpBlacklistManager::increaseErrorCount",
                                                      "Blacklisting item temporarily to avoid infinite loop");
            Error err(_syncPal->syncDbId(), "", nodeId, type, relativePath, ConflictType::None, InconsistencyType::None,
                      CancelType::TmpBlacklisted);
            _syncPal->addError(err);
        }
    } else {
        TmpErrorInfo errorInfo;
        errorInfo.path = relativePath;
        errors.try_emplace(nodeId, errorInfo);
        logMessage(L"Item added in error list", nodeId, side, relativePath);
    }
}

void TmpBlacklistManager::blacklistItem(const NodeId &nodeId, const SyncPath &relativePath, ReplicaSide side) {
    auto &errors = side == ReplicaSide::Local ? _localErrors : _remoteErrors;
    if (auto errorItem = errors.find(nodeId); errorItem != errors.end()) {
        // Reset error timer
        errorItem->second.count++;
        errorItem->second.lastErrorTime = std::chrono::steady_clock::now();
    } else {
        TmpErrorInfo errorInfo;
        errorInfo.path = relativePath;
        errors.try_emplace(nodeId, errorInfo);
        logMessage(L"Item added in error list", nodeId, side, relativePath);
    }

    insertInBlacklist(nodeId, side);
}

void TmpBlacklistManager::refreshBlacklist() {
    auto now = std::chrono::steady_clock::now();
    for (int i = 0; i < 2; i++) {
        ReplicaSide side = i == 0 ? ReplicaSide::Local : ReplicaSide::Remote;
        auto &errors = side == ReplicaSide::Local ? _localErrors : _remoteErrors;

        auto errorIt = errors.begin();
        while (errorIt != errors.end()) {
            if (const std::chrono::duration<double> elapsed_seconds = now - errorIt->second.lastErrorTime;
                elapsed_seconds.count() > oneHour) {
                logMessage(L"Removing item from tmp blacklist", errorIt->first, side);

                SyncNodeType blacklistType =
                    side == ReplicaSide::Local ? SyncNodeType::TmpLocalBlacklist : SyncNodeType::TmpRemoteBlacklist;

                std::unordered_set<NodeId> tmp;
                SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), blacklistType, tmp);
                tmp.erase(errorIt->first);
                SyncNodeCache::instance()->update(_syncPal->syncDbId(), blacklistType, tmp);

                errorIt = errors.erase(errorIt);
                continue;
            }

            errorIt++;
        }
    }
}

void TmpBlacklistManager::removeItemFromTmpBlacklist(const NodeId &nodeId, ReplicaSide side) {
    SyncNodeType blacklistType = side == ReplicaSide::Local ? SyncNodeType::TmpLocalBlacklist : SyncNodeType::TmpRemoteBlacklist;

    std::unordered_set<NodeId> tmp;
    SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), blacklistType, tmp);
    if (tmp.find(nodeId) != tmp.end()) {
        tmp.erase(nodeId);
        SyncNodeCache::instance()->update(_syncPal->syncDbId(), blacklistType, tmp);
    }

    auto &errors = side == ReplicaSide::Local ? _localErrors : _remoteErrors;
    errors.erase(nodeId);
}

bool TmpBlacklistManager::isTmpBlacklisted(const SyncPath &path, ReplicaSide side) const {
    auto &errors = side == ReplicaSide::Local ? _localErrors : _remoteErrors;
    for (const auto &errorInfo : errors) {
        if (Utility::startsWith(path, errorInfo.second.path)) {
            return true;
        }
    }

    return false;
}

void TmpBlacklistManager::insertInBlacklist(const NodeId &nodeId, ReplicaSide side) {
    SyncNodeType blacklistType = side == ReplicaSide::Local ? SyncNodeType::TmpLocalBlacklist : SyncNodeType::TmpRemoteBlacklist;

    std::unordered_set<NodeId> tmp;
    SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), blacklistType, tmp);
    tmp.insert(nodeId);
    SyncNodeCache::instance()->update(_syncPal->syncDbId(), blacklistType, tmp);

    logMessage(L"Item added in tmp blacklist", nodeId, side);
    removeFromDB(nodeId, side);
}

void TmpBlacklistManager::removeFromDB(const NodeId &nodeId, const ReplicaSide side) {
    DbNodeId dbNodeId = -1;
    bool found = false;
    if (!_syncPal->_syncDb->dbId(side, nodeId, dbNodeId, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in SyncDb::dbId");
        return;
    }
    if (found) {
        // Delete old node
        if (!_syncPal->_syncDb->deleteNode(dbNodeId, found)) {
            LOG_WARN(Log::instance()->getLogger(), "Error in SyncDb::deleteNode");
            return;
        }

        LOG_INFO(Log::instance()->getLogger(), "Item " << dbNodeId << " removed from DB");
    }
}

}  // namespace KDC
