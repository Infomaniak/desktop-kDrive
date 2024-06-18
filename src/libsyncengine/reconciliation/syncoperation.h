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
#include "reconciliation/conflict_finder/conflict.h"
#include "update_detection/update_detector/node.h"
#include "utility/types.h"

#include <list>

namespace KDC {

class SyncOperation {
    public:
        SyncOperation();

        inline OperationType type() const { return _type; }
        inline void setType(OperationType newType) { _type = newType; }
        inline const std::shared_ptr<Node> &affectedNode() const { return _affectedNode; }
        inline void setAffectedNode(const std::shared_ptr<Node> &node) { _affectedNode = node; }
        inline const std::shared_ptr<Node> &correspondingNode() const { return _correspondingNode; }
        inline void setCorrespondingNode(const std::shared_ptr<Node> &node) { _correspondingNode = node; }
        inline ReplicaSide targetSide() const { return _targetSide; }
        inline void setTargetSide(ReplicaSide newSide) { _targetSide = newSide; }
        inline bool omit() const { return _omit; }
        inline void setOmit(bool newOmit) { _omit = newOmit; }
        inline const SyncName &newName() const { return _newName; }
        inline void setNewName(const SyncName &newNewName) { _newName = newNewName; }
        inline const std::shared_ptr<Node> &newParentNode() const { return _newParentNode; }
        inline void setNewParentNode(const std::shared_ptr<Node> &newParentNode) { _newParentNode = newParentNode; }
        inline bool hasConflict() { return _conflict.type() != ConflictType::None; }
        inline const Conflict &conflict() const { return _conflict; }
        inline void setConflict(const Conflict &newConflict) { _conflict = newConflict; }

        bool operator==(const SyncOperation &other) const;
        inline bool operator!=(const SyncOperation &other) const { return !(*this == other); }

        inline UniqueId id() const { return _id; }
        inline UniqueId parentId() const { return _parentId; }
        inline void setParentId(UniqueId newParentId) { _parentId = newParentId; }
        inline bool hasParentOp() const { return _parentId > -1; }

    private:
        OperationType _type = OperationType::None;
        std::shared_ptr<Node> _affectedNode = nullptr;
        std::shared_ptr<Node> _correspondingNode = nullptr;  // The node on which we will apply the operation
        ReplicaSide _targetSide = ReplicaSide::Unknown;        // The side on which we will apply the operation
        bool _omit = false;                                  // If true, apply change only in DB
        SyncName _newName;  // New name on the replica on which we will apply the operation. Only for create and move operation
        std::shared_ptr<Node> _newParentNode =
            nullptr;  // New parent on the replica on which we will apply the operation. Only for move operation
        Conflict _conflict;

        UniqueId _id = -1;
        UniqueId _parentId =
            -1;  // ID of that parent operation i.e. the operation that must be completed before starting this one

        static UniqueId _nextId;
};

typedef std::shared_ptr<SyncOperation> SyncOpPtr;

class SyncOperationList : public SharedObject {
    public:
        SyncOperationList() {}
        SyncOperationList(const SyncOperationList &other)
            : _allOps(other._allOps),
              _opSortedList(other._opSortedList),
              _opListByType(other._opListByType),
              _node2op(other._node2op) {}
        ~SyncOperationList();

        void setOpList(const std::list<SyncOpPtr> &opList);

        SyncOpPtr getOp(UniqueId id);
        inline const std::list<UniqueId> &opSortedList() { return _opSortedList; }
        inline const std::unordered_set<UniqueId> &opListIdByType(OperationType type) { return _opListByType[type]; }
        inline const std::list<UniqueId> &getOpIdsFromNodeId(const NodeId &nodeId) { return _node2op[nodeId]; }

        void pushOp(SyncOpPtr op);
        void insertOp(std::list<UniqueId>::const_iterator pos, SyncOpPtr op);
        void deleteOp(std::list<UniqueId>::const_iterator it);
        inline int64_t size() const { return _allOps.size(); }
        inline int isEmpty() const { return _allOps.size() == 0; }
        void clear();
        void operator=(SyncOperationList const &other);

        void getMapIndexToOp(std::unordered_map<UniqueId, int> &map, OperationType typeFilter = OperationType::None);

    private:
        std::unordered_map<UniqueId, SyncOpPtr> _allOps;
        std::list<UniqueId> _opSortedList;
        std::unordered_map<OperationType, std::unordered_set<UniqueId>> _opListByType;
        std::unordered_map<NodeId, std::list<UniqueId>> _node2op;

        friend class TestOperationSorterWorker;
        friend class TestOperationGeneratorWorker;
};

}  // namespace KDC
