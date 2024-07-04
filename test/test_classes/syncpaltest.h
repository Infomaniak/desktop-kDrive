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

#include "syncpal/syncpal.h"

namespace KDC {

class SyncPalTest final : public SyncPal {
    public:
        SyncPalTest() = delete;
        SyncPalTest(const SyncPath &syncDbPath, const std::string &version, bool hasFullyCompleted)
            : SyncPal(syncDbPath, version, hasFullyCompleted) {}
        SyncPalTest(int syncDbId, const std::string &version) : SyncPal(syncDbId, version) {}

    private:
        // No implementation of the following methods in tests because `_tmpBlacklistManager` is not defined.
        void increaseErrorCount(const NodeId &nodeId, NodeType type, const SyncPath &relativePath, ReplicaSide side) override {}
        int getErrorCount(const NodeId &nodeId, ReplicaSide side) const noexcept override { return 0; }
        void blacklistTemporarily(const NodeId &nodeId, const SyncPath &relativePath, ReplicaSide side) override {}
        void refreshTmpBlacklist() override {}
        void removeItemFromTmpBlacklist(const NodeId &nodeId, ReplicaSide side) override {}
};

}  // namespace KDC
