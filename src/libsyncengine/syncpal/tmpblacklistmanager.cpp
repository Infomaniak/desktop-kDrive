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

static constexpr int oneHour = 3600;

namespace KDC {

TmpBlacklistManager::TmpBlacklistManager(std::shared_ptr<SyncPal> syncPal) :
    _syncPal(syncPal) {
    LOG_SYNCPAL_DEBUG(Log::instance()->getLogger(), "TmpBlacklistManager created");
}

TmpBlacklistManager::~TmpBlacklistManager() {
    LOG_SYNCPAL_DEBUG(Log::instance()->getLogger(), "TmpBlacklistManager destroyed");
}

void TmpBlacklistManager::logMessage(const std::wstring &msg, const NodeId &id, const ReplicaSide side,
                                     const SyncPath &path) const {
    LOGW_SYNCPAL_INFO(Log::instance()->getLogger(), msg << L" - node ID='" << CommonUtility::s2ws(id) << L"' - side='" << side
                                                        << (path.empty() ? L"'" : (L"' - " + Utility::formatSyncPath(path))));
}

void TmpBlacklistManager::increaseErrorCount(const NodeId &nodeId, const NodeType type, const SyncPath &relativePath,
                                             const ReplicaSide side, ExitInfo exitInfo /*= ExitInfo()*/) {
    auto &errors = side == ReplicaSide::Local ? _localErrors : _remoteErrors;

    if (const auto errorItem = errors.find(nodeId); errorItem != errors.end()) {
        errorItem->second.lastErrorTime = std::chrono::steady_clock::now();
        insertInBlacklist(nodeId, side);

        sentry::Handler::captureMessage(sentry::Level::Warning, "TmpBlacklistManager::increaseErrorCount",
                                        "Blacklisting item temporarily to avoid infinite loop");
        const Error err(_syncPal->syncDbId(), "", nodeId, type, relativePath, ConflictType::None, InconsistencyType::None,
                        exitInfo ? CancelType::TmpBlacklisted : CancelType::None, "", exitInfo.code(), exitInfo.cause());
        _syncPal->addError(err);
    } else {
        TmpErrorInfo errorInfo;
        errorInfo.path = relativePath;
        (void) errors.try_emplace(nodeId, errorInfo);
        logMessage(L"First time we try to blacklist this item, it is not blacklisted yet", nodeId, side, relativePath);
    }
}

void TmpBlacklistManager::blacklistItem(const NodeId &nodeId, const SyncPath &relativePath, const ReplicaSide side) {
    auto &errors = side == ReplicaSide::Local ? _localErrors : _remoteErrors;
    if (const auto &errorItem = errors.find(nodeId); errorItem != errors.end()) {
        // Reset error timer
        errorItem->second.lastErrorTime = std::chrono::steady_clock::now();
    } else {
        TmpErrorInfo errorInfo;
        errorInfo.path = relativePath;
        (void) errors.try_emplace(nodeId, errorInfo);
        logMessage(L"Item added in error list", nodeId, side, relativePath);
    }

    insertInBlacklist(nodeId, side);

    std::list<NodeId> toBeRemoved;
    for (const auto &[id, errorInfo]: errors) {
        if (CommonUtility::isDescendantOrEqual(errorInfo.path, relativePath) && id != nodeId) {
            toBeRemoved.push_back(id);
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

                NodeSet tmp;
                SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), blacklistType_, tmp);
                tmp.erase(errorIt->first);
                SyncNodeCache::instance()->update(_syncPal->syncDbId(), blacklistType_, tmp);

                errorIt = errors.erase(errorIt);
                continue;
            }

            ++errorIt;
        }
    }
}

void TmpBlacklistManager::removeItemFromTmpBlacklist(const SyncPath &relativePath) {
    NodeId remotedId;
    NodeId localId;

    // Find the node id of the item to be removed
    for (const auto &[nodeId, tmpInfo]: _localErrors) {
        if (CommonUtility::isDescendantOrEqual(tmpInfo.path, relativePath)) {
            localId = nodeId;
            break;
        }
    }

    for (const auto &[nodeId, tmpInfo]: _remoteErrors) {
        if (CommonUtility::isDescendantOrEqual(tmpInfo.path, relativePath)) {
            remotedId = nodeId;
            break;
        }
    }

    if (!localId.empty()) {
        eraseSingleItemFromBlacklist(localId, ReplicaSide::Local);
    }

    if (!remotedId.empty()) {
        eraseSingleItemFromBlacklist(remotedId, ReplicaSide::Remote);
    }
}

void TmpBlacklistManager::removeItemFromTmpBlacklist(const NodeId &nodeId, const ReplicaSide side) {
    if (const auto &errors = side == ReplicaSide::Local ? _localErrors : _remoteErrors; errors.contains(nodeId)) {
        removeItemFromTmpBlacklist(errors.at(nodeId).path);
    }
}

bool TmpBlacklistManager::isTmpBlacklisted(const SyncPath &path, const ReplicaSide side) const {
    for (auto &errors = side == ReplicaSide::Local ? _localErrors : _remoteErrors; const auto &errorInfo: errors) {
        if (CommonUtility::isDescendantOrEqual(path, errorInfo.second.path)) return true;
    }

    return false;
}

void TmpBlacklistManager::insertInBlacklist(const NodeId &nodeId, const ReplicaSide side) const {
    const auto blacklistType_ = blackListType(side);

    NodeSet tmp;
    SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), blacklistType_, tmp);
    tmp.insert(nodeId);
    SyncNodeCache::instance()->update(_syncPal->syncDbId(), blacklistType_, tmp);

    logMessage(L"Item added in tmp blacklist", nodeId, side);
}

void TmpBlacklistManager::eraseSingleItemFromBlacklist(const NodeId &nodeId, const ReplicaSide side) {
    const SyncNodeType blacklistType_ = blackListType(side);

    NodeSet tmp;
    SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), blacklistType_, tmp);
    if (tmp.contains(nodeId)) {
        tmp.erase(nodeId);
        SyncNodeCache::instance()->update(_syncPal->syncDbId(), blacklistType_, tmp);
    }

    auto &errors = side == ReplicaSide::Local ? _localErrors : _remoteErrors;
    errors.erase(nodeId);
    logMessage(L"Item removed from tmp blacklist", nodeId, side);
}

} // namespace KDC
