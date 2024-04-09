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

#pragma once

#include "utility/types.h"
#include "syncpal.h"
#include <unordered_map>

namespace KDC {

class TmpBlacklistManager {
    public:
        struct TmpErrorInfo {
                int _count = 0;
                std::chrono::time_point<std::chrono::steady_clock> _lastErrorTime = std::chrono::steady_clock::now();
                SyncPath _path;
        };

        TmpBlacklistManager(std::shared_ptr<SyncPal> syncPal);
        ~TmpBlacklistManager();

        void increaseErrorCount(const NodeId &nodeId, NodeType type, const SyncPath &relativePath, const ReplicaSide side);
        void blacklistItem(const NodeId &nodeId, const SyncPath &relativePath, const ReplicaSide side);
        void refreshBlacklist();
        void removeItemFromTmpBlacklist(const NodeId &nodeId, const ReplicaSide side);
        bool isTmpBlacklisted(const ReplicaSide side, const SyncPath &path);

    private:
        void insertInBlacklist(const NodeId &nodeId, const ReplicaSide side);
        void removeFromDB(const NodeId &nodeId, const ReplicaSide side);

        std::unordered_map<NodeId, TmpErrorInfo> _localErrors;
        std::unordered_map<NodeId, TmpErrorInfo> _remoteErrors;
        std::shared_ptr<SyncPal> _syncPal;
};

}  // namespace KDC
