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

#include "tmpblacklistmanager.h"

#include "requests/syncnodecache.h"

#include <unordered_set>

static const int oneHour = 3600;

namespace KDC {

TmpBlacklistManager::TmpBlacklistManager(std::shared_ptr<SyncPal> syncPal) : _syncPal(syncPal) {
    LOG_SYNCPAL_DEBUG(Log::instance()->getLogger(), "TmpBlacklistManager created");
}

TmpBlacklistManager::~TmpBlacklistManager() {
    LOG_SYNCPAL_DEBUG(Log::instance()->getLogger(), "TmpBlacklistManager destroyed");
}

int TmpBlacklistManager::getErrorCount(const NodeId &nodeId, ReplicaSide side) const noexcept {
    const auto &errors = side == ReplicaSide::Local ? _localErrors : _remoteErrors;
    const auto errorItem = errors.find(nodeId);

    return errorItem == errors.end() ? 0 : errorItem->second.count + 1;
}

void TmpBlacklistManager::logMessage(const std::wstring &msg, const NodeId &id, const ReplicaSide side,
                                     const SyncPath &path) const {
    LOGW_SYNCPAL_INFO(Log::instance()->getLogger(),
                      msg.c_str() << L" - node ID='" << Utility::s2ws(id).c_str() << L"' - side='" << side
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

        if (errorItem->second.count >= 1) { // We try only once. TODO: If we keep this logic, no need to keep _count
            insertInBlacklist(nodeId, side);

            sentry::Handler::captureMessage(sentry::Level::Warning, "TmpBlacklistManager::increaseErrorCount",
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

    std::list<NodeId> toBeRemoved;
    for (const auto &errorInfo: errors) {
        if (Utility::isDescendantOrEqual(errorInfo.second.path, relativePath) && errorInfo.first != nodeId) {
            toBeRemoved.push_back(errorInfo.first);
        }
    }

    for (const auto &id: toBeRemoved) {
        removeItemFromTmpBlacklist(id, side);
    }
}

void TmpBlacklistManager::refreshBlacklist() {
    using namespace std::chrono;
    const auto now = steady_clock::now();
    for (const auto side: std::array<ReplicaSide, 2>{ReplicaSide::Local, ReplicaSide::Remote}) {
        auto &errors = side == ReplicaSide::Local ? _localErrors : _remoteErrors;
        auto errorIt = errors.begin();
        while (errorIt != errors.end()) {
            if (const duration<double> elapsed_seconds = now - errorIt->second.lastErrorTime; elapsed_seconds.count() > oneHour) {
                logMessage(L"Removing item from tmp blacklist", errorIt->first, side);

                const auto blacklistType_ = blackListType(side);

                std::unordered_set<NodeId> tmp;
                SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), blacklistType_, tmp);
                tmp.erase(errorIt->first);
                SyncNodeCache::instance()->update(_syncPal->syncDbId(), blacklistType_, tmp);

                errorIt = errors.erase(errorIt);
                continue;
            }

            errorIt++;
        }
    }
}

void TmpBlacklistManager::removeItemFromTmpBlacklist(const NodeId &nodeId, ReplicaSide side) {
    const SyncNodeType blacklistType_ = blackListType(side);

    std::unordered_set<NodeId> tmp;
    SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), blacklistType_, tmp);
    if (tmp.contains(nodeId)) {
        tmp.erase(nodeId);
        SyncNodeCache::instance()->update(_syncPal->syncDbId(), blacklistType_, tmp);
    }

    auto &errors = side == ReplicaSide::Local ? _localErrors : _remoteErrors;
    errors.erase(nodeId);
}

void TmpBlacklistManager::removeItemFromTmpBlacklist(const SyncPath &relativePath) {
    NodeId remotedId;
    NodeId localId;

    // Find the node id of the item to be removed
    for (const auto &[nodeId, tmpInfo]: _localErrors) {
        if (Utility::isDescendantOrEqual(tmpInfo.path, relativePath)) {
            localId = nodeId;
            break;
        }
    }

    for (const auto &[nodeId, tmpInfo]: _remoteErrors) {
        if (Utility::isDescendantOrEqual(tmpInfo.path, relativePath)) {
            remotedId = nodeId;
            break;
        }
    }

    if (!localId.empty()) {
        removeItemFromTmpBlacklist(localId, ReplicaSide::Local);
    }

    if (!remotedId.empty()) {
        removeItemFromTmpBlacklist(remotedId, ReplicaSide::Remote);
    }
}

bool TmpBlacklistManager::isTmpBlacklisted(const SyncPath &path, ReplicaSide side) const {
    auto &errors = side == ReplicaSide::Local ? _localErrors : _remoteErrors;
    for (const auto &errorInfo: errors) {
        if (Utility::isDescendantOrEqual(path, errorInfo.second.path)) return true;
    }

    return false;
}

bool TmpBlacklistManager::isTmpBlacklisted(const NodeId &nodeId, ReplicaSide side) const {
    auto &errors = side == ReplicaSide::Local ? _localErrors : _remoteErrors;
    return (errors.contains(nodeId));
}

void TmpBlacklistManager::insertInBlacklist(const NodeId &nodeId, ReplicaSide side) {
    const auto blacklistType_ = blackListType(side);

    std::unordered_set<NodeId> tmp;
    SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), blacklistType_, tmp);
    tmp.insert(nodeId);
    SyncNodeCache::instance()->update(_syncPal->syncDbId(), blacklistType_, tmp);

    logMessage(L"Item added in tmp blacklist", nodeId, side);
    removeFromDB(nodeId, side);
}

void TmpBlacklistManager::removeFromDB(const NodeId &nodeId, const ReplicaSide side) {
    DbNodeId dbNodeId = -1;
    bool found = false;
    if (!_syncPal->syncDb()->dbId(side, nodeId, dbNodeId, found)) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Error in SyncDb::dbId");
        return;
    }
    if (found) {
        // Delete old node
        if (!_syncPal->syncDb()->deleteNode(dbNodeId, found)) {
            LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Error in SyncDb::deleteNode");
            return;
        }

        LOG_SYNCPAL_INFO(Log::instance()->getLogger(), "Item " << dbNodeId << " removed from DB");
    }
}

} // namespace KDC
