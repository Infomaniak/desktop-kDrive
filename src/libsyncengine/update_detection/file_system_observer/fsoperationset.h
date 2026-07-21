/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "syncpal/sharedobject.h"
#include "fsoperation.h"
#include "libcommon/utility/types.h"
#include "libcommon/utility/utility.h"

#include <mutex>
#include <unordered_set>
#include <unordered_map>

namespace KDC {

using FSOpPtr = std::shared_ptr<FSOperation>;
using OpMap = std::unordered_map<UniqueId, FSOpPtr>;

// Functor for ordering operations by path depth
// NB: The ops map passed as a parameter must not be modified
class CmpOp {
    public:
        explicit CmpOp(const OpMap &ops) :
            _ops(ops) {}

        bool operator()(const UniqueId id1, const UniqueId id2) const {
            const auto opIt1 = _ops.get().find(id1);
            SyncPath path1;
            if (opIt1 != _ops.get().end()) {
                path1 = opIt1->second->path();
            }

            const auto opIt2 = _ops.get().find(id2);
            SyncPath path2;
            if (opIt2 != _ops.get().end()) {
                path2 = opIt2->second->path();
            }

            const auto pathDepth1 = CommonUtility::pathDepth(path1);
            const auto pathDepth2 = CommonUtility::pathDepth(path2);

            return pathDepth1 == pathDepth2 ? id1 < id2 : pathDepth1 < pathDepth2;
        }

    private:
        std::reference_wrapper<const OpMap> _ops;
};

// Set of operations ordered by path depth
using OpSet = std::set<UniqueId, CmpOp>;

class FSOperationSet : public SharedObject {
    public:
        explicit FSOperationSet(ReplicaSide side) :
            _side(side) {}
        ~FSOperationSet();

        FSOperationSet(const FSOperationSet &other) :
            _ops(other._ops),
            _opsByType(other._opsByType),
            _opsByNodeId(other._opsByNodeId),
            _side(other._side) {}
        FSOperationSet &operator=(FSOperationSet &other);

        bool getOp(UniqueId id, FSOpPtr &opPtr) const;
        OpMap getAllOps() const;
        OpSet getOpsByType(const OperationType type) const;
        std::unordered_set<UniqueId> getOpsByNodeId(const NodeId &nodeId) const;

        uint64_t nbOps() const;
        uint64_t nbOpsByType(const OperationType type) const;

        void clear();
        void insertOp(FSOpPtr opPtr);
        bool removeOp(UniqueId id);
        bool removeOp(const NodeId &nodeId, const OperationType opType);

        bool findOp(const NodeId &nodeId, const OperationType opType, FSOpPtr &res) const;
        ReplicaSide side() const;

    private:
        OpMap _ops;
        std::unordered_map<OperationType, std::unordered_set<UniqueId>> _opsByType;
        std::unordered_map<NodeId, std::unordered_set<UniqueId>> _opsByNodeId;
        ReplicaSide _side = ReplicaSide::Unknown;
        mutable std::recursive_mutex _mutex;
};

} // namespace KDC
