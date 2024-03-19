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

void TmpBlacklistManager::increaseErrorCount(const NodeId &nodeId, NodeType type, const SyncPath &relativePath,
                                             ReplicaSide side) {
    auto &errors = side == ReplicaSideLocal ? _localErrors : _remoteErrors;

    auto errorItem = errors.find(nodeId);
    if (errorItem != errors.end()) {
        errorItem->second._count++;
        errorItem->second._lastErrorTime = std::chrono::steady_clock::now();
        LOGW_WARN(Log::instance()->getLogger(), L"Error counter increased for item with nodeId="
                                                    << Utility::s2ws(nodeId).c_str() << L" and path="
                                                    << Path2WStr(relativePath).c_str());

        if (errorItem->second._count >= 1) {  // We try only once. TODO: If we keep this logic, no need to keep _count
            insertInBlacklist(nodeId, side);

#ifdef NDEBUG
            sentry_capture_event(sentry_value_new_message_event(SENTRY_LEVEL_WARNING, "TmpBlacklistManager::increaseErrorCount",
                                                                "Blacklisting item temporarily to avoid infinite loop"));
#endif

            Error err(_syncPal->syncDbId(), "", nodeId, type, relativePath, ConflictTypeNone, InconsistencyTypeNone,
                      CancelTypeTmpBlacklisted);
            _syncPal->addError(err);
        }
    } else {
        TmpErrorInfo errorInfo;
        errorInfo._path = relativePath;
        errors.insert({nodeId, errorInfo});

        LOGW_DEBUG(Log::instance()->getLogger(), L"Item added in " << (side == ReplicaSideLocal ? L"local" : L"remote")
                                                                   << L" error list with nodeId=" << Utility::s2ws(nodeId).c_str()
                                                                   << L" and path=" << Path2WStr(errorInfo._path).c_str());
    }
}

void TmpBlacklistManager::blacklistItem(const NodeId &nodeId, const SyncPath &relativePath, ReplicaSide side) {
    auto &errors = side == ReplicaSideLocal ? _localErrors : _remoteErrors;

    auto errorItem = errors.find(nodeId);
    if (errorItem != errors.end()) {
        errorItem->second._count++;
        errorItem->second._lastErrorTime = std::chrono::steady_clock::now();
    } else {
        TmpErrorInfo errorInfo;
        errorInfo._path = relativePath;
        errors.insert({nodeId, errorInfo});

        LOGW_DEBUG(Log::instance()->getLogger(), L"Item added in " << (side == ReplicaSideLocal ? L"local" : L"remote")
                                                                   << L" error list with nodeId=" << Utility::s2ws(nodeId).c_str()
                                                                   << L" and path=" << Path2WStr(errorInfo._path).c_str());
    }

    insertInBlacklist(nodeId, side);
}

void TmpBlacklistManager::refreshBlacklist() {
    auto now = std::chrono::steady_clock::now();
    for (int i = 0; i < 2; i++) {
        ReplicaSide side = i == 0 ? ReplicaSideLocal : ReplicaSideRemote;
        auto &errors = side == ReplicaSideLocal ? _localErrors : _remoteErrors;

        auto errorIt = errors.begin();
        while (errorIt != errors.end()) {
            std::chrono::duration<double> elapsed_seconds = now - errorIt->second._lastErrorTime;
            if (elapsed_seconds.count() > oneHour) {
                if (ParametersCache::instance()->parameters().extendedLog()) {
                    LOG_DEBUG(Log::instance()->getLogger(), "Removing " << Utility::side2Str(side).c_str() << "  item "
                                                                        << errorIt->first.c_str() << " from tmp blacklist.");
                }

                SyncNodeType blaclistType =
                    side == ReplicaSideLocal ? SyncNodeTypeTmpLocalBlacklist : SyncNodeTypeTmpRemoteBlacklist;

                std::unordered_set<NodeId> tmp;
                SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), blaclistType, tmp);
                tmp.erase(errorIt->first);
                SyncNodeCache::instance()->update(_syncPal->syncDbId(), blaclistType, tmp);

                errorIt = errors.erase(errorIt);
                continue;
            }

            errorIt++;
        }
    }
}

void TmpBlacklistManager::removeItemFromTmpBlacklist(const NodeId &nodeId, ReplicaSide side) {
    SyncNodeType blacklistType = side == ReplicaSideLocal ? SyncNodeTypeTmpLocalBlacklist : SyncNodeTypeTmpRemoteBlacklist;

    std::unordered_set<NodeId> tmp;
    SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), blacklistType, tmp);
    if (tmp.find(nodeId) != tmp.end()) {
        tmp.erase(nodeId);
        SyncNodeCache::instance()->update(_syncPal->syncDbId(), blacklistType, tmp);
    }

    auto &errors = side == ReplicaSideLocal ? _localErrors : _remoteErrors;
    errors.erase(nodeId);
}

bool TmpBlacklistManager::isTmpBlacklisted(ReplicaSide side, const SyncPath &path) {
    auto &errors = side == ReplicaSideLocal ? _localErrors : _remoteErrors;

    for (const auto &errorInfo : errors) {
        if (Utility::startsWith(path, errorInfo.second._path)) {
            return true;
        }
    }

    return false;
}

void TmpBlacklistManager::insertInBlacklist(const NodeId &nodeId, ReplicaSide side) {
    SyncNodeType blacklistType = side == ReplicaSideLocal ? SyncNodeTypeTmpLocalBlacklist : SyncNodeTypeTmpRemoteBlacklist;

    std::unordered_set<NodeId> tmp;
    SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), blacklistType, tmp);
    tmp.insert(nodeId);
    SyncNodeCache::instance()->update(_syncPal->syncDbId(), blacklistType, tmp);

    LOGW_INFO(Log::instance()->getLogger(), L"Item added in " << (side == ReplicaSideLocal ? L"local" : L"remote")
                                                              << L" tmp blacklist with nodeId=" << Utility::s2ws(nodeId).c_str());
}

}  // namespace KDC
