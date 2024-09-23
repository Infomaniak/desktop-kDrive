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

#include "syncpal/sharedobject.h"
#include "fsoperation.h"
#include "libcommon/utility/types.h"

#include <mutex>
#include <unordered_set>
#include <unordered_map>

namespace KDC {

using FSOpPtr = std::shared_ptr<FSOperation>;

class FSOperationSet : public SharedObject {
    public:
        explicit FSOperationSet(ReplicaSide side) : _side(side) {}
        ~FSOperationSet();

        FSOperationSet(const FSOperationSet &other) :
            _ops(other._ops), _opsByType(other._opsByType), _opsByNodeId(other._opsByNodeId), _side(other._side) {}
        FSOperationSet &operator=(FSOperationSet &other);

        bool getOp(UniqueId id, FSOpPtr &opPtr) const;
        std::unordered_map<UniqueId, FSOpPtr> getAllOps() const;
        std::unordered_set<UniqueId> getOpsByType(const OperationType type) const;
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
        std::unordered_map<UniqueId, FSOpPtr> _ops;
        std::unordered_map<OperationType, std::unordered_set<UniqueId>> _opsByType;
        std::unordered_map<NodeId, std::unordered_set<UniqueId>> _opsByNodeId;
        ReplicaSide _side = ReplicaSide::Unknown;
        mutable std::recursive_mutex _mutex;
};

} // namespace KDC
