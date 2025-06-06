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

#include "utility/types.h"
#include "syncpal.h"
#include <unordered_map>

namespace KDC {

class TmpBlacklistManager {
    public:
        struct TmpErrorInfo {
                std::chrono::time_point<std::chrono::steady_clock> lastErrorTime = std::chrono::steady_clock::now();
                SyncPath path;
        };

        explicit TmpBlacklistManager(std::shared_ptr<SyncPal> syncPal);
        ~TmpBlacklistManager();

        void increaseErrorCount(const NodeId &nodeId, NodeType type, const SyncPath &relativePath, ReplicaSide side,
                                ExitInfo exitInfo = ExitInfo());
        void blacklistItem(const NodeId &nodeId, const SyncPath &relativePath, ReplicaSide side);
        void refreshBlacklist();

        // Remove the item from local and remote blacklist
        void removeItemFromTmpBlacklist(const SyncPath &relativePath);
        void removeItemFromTmpBlacklist(const NodeId &nodeId, ReplicaSide side);
        bool isTmpBlacklisted(const SyncPath &path, ReplicaSide side) const;

    private:
        void insertInBlacklist(const NodeId &nodeId, ReplicaSide side) const;
        void eraseSingleItemFromBlacklist(const NodeId &nodeId, ReplicaSide side);
        int syncDbId() const noexcept {
            assert(_syncPal);
            return _syncPal->syncDbId();
        };
        void logMessage(const std::wstring &msg, const NodeId &id, const ReplicaSide side, const SyncPath &path = "") const;
        static SyncNodeType blackListType(const ReplicaSide side) {
            return side == ReplicaSide::Local ? SyncNodeType::TmpLocalBlacklist : SyncNodeType::TmpRemoteBlacklist;
        }

        std::unordered_map<NodeId, TmpErrorInfo> _localErrors;
        std::unordered_map<NodeId, TmpErrorInfo> _remoteErrors;
        std::shared_ptr<SyncPal> _syncPal;
};

} // namespace KDC
