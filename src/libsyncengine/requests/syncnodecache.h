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

#include "syncenginelib.h"
#include "db/syncnode.h"
#include "db/syncdb.h"
#include "libcommon/utility/types.h"

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <mutex>

namespace KDC {

class SYNCENGINE_EXPORT SyncNodeCache {
    public:
        static std::shared_ptr<SyncNodeCache> instance();

        SyncNodeCache(SyncNodeCache const &) = delete;
        void operator=(SyncNodeCache const &) = delete;

        ExitCode syncNodes(int syncDbId, SyncNodeType type, std::unordered_set<NodeId> &syncNodes);
        ExitCode update(int syncDbId, SyncNodeType type, const std::unordered_set<NodeId> &syncNodes);
        ExitCode initCache(int syncDbId, std::shared_ptr<SyncDb> syncDb);
        ExitCode clearCache(int syncDbId);

    private:
        static std::shared_ptr<SyncNodeCache> _instance;
        std::unordered_map<int, std::shared_ptr<SyncDb>> _syncDbMap;
        std::unordered_map<int, std::unordered_map<SyncNodeType, std::unordered_set<NodeId>>> _syncNodesMap;

        std::mutex _mutex;

        SyncNodeCache();
};

} // namespace KDC
