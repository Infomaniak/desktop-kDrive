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

        [[nodiscard]] OperationType type() const { return _type; }
        void setType(const OperationType newType) { _type = newType; }
        [[nodiscard]] const std::shared_ptr<Node> &affectedNode() const { return _affectedNode; }
        void setAffectedNode(const std::shared_ptr<Node> &node) { _affectedNode = node; }
        [[nodiscard]] const std::shared_ptr<Node> &correspondingNode() const { return _correspondingNode; }
        void setCorrespondingNode(const std::shared_ptr<Node> &node) { _correspondingNode = node; }
        [[nodiscard]] ReplicaSide targetSide() const { return _targetSide; }
        void setTargetSide(ReplicaSide newSide) { _targetSide = newSide; }
        [[nodiscard]] bool omit() const { return _omit; }
        void setOmit(bool newOmit) { _omit = newOmit; }
        [[nodiscard]] const SyncName &newName() const { return _newName; }
        void setNewName(const SyncName &newNewName) { _newName = newNewName; }
        [[nodiscard]] const SyncPath &localCreationTargetPath() const {
            assert(_type == OperationType::Create && _targetSide == ReplicaSide::Local);
            assert(!_localCreationTargetPath.empty());
            return _localCreationTargetPath;
        }
        void setLocalCreationTargetPath(const SyncPath &localCreationTargetPath) {
            assert(_type == OperationType::Create && _targetSide == ReplicaSide::Local);
            _localCreationTargetPath = localCreationTargetPath;
        }
        [[nodiscard]] const std::shared_ptr<Node> &newParentNode() const { return _newParentNode; }
        void setNewParentNode(const std::shared_ptr<Node> &newParentNode) { _newParentNode = newParentNode; }
        [[nodiscard]] bool hasConflict() const { return _conflict.type() != ConflictType::None; }
        [[nodiscard]] const Conflict &conflict() const { return _conflict; }
        void setConflict(const Conflict &newConflict) { _conflict = newConflict; }

        [[nodiscard]] SyncName nodeName(ReplicaSide side) const;
        [[nodiscard]] SyncPath nodePath(ReplicaSide side) const;
        [[nodiscard]] NodeType nodeType() const noexcept;

        bool operator==(const SyncOperation &other) const;

        [[nodiscard]] UniqueId id() const { return _id; }
        [[nodiscard]] UniqueId parentId() const { return _parentId; }
        void setParentId(UniqueId newParentId) { _parentId = newParentId; }
        [[nodiscard]] bool hasParentOp() const { return _parentId > -1; }


        [[nodiscard]] bool isBreakingCycleOp() const { return _isBreakingCycleOp; }
        void setIsBreakingCycleOp(const bool isBreakingCycleOp) { _isBreakingCycleOp = isBreakingCycleOp; }

    private:
        OperationType _type = OperationType::None;
        std::shared_ptr<Node> _affectedNode = nullptr;
        std::shared_ptr<Node> _correspondingNode = nullptr; // The node on which we will apply the operation
        ReplicaSide _targetSide = ReplicaSide::Unknown; // The side on which we will apply the operation
        bool _omit = false; // If true, apply change only in DB
        SyncName _newName; // New name on the replica on which we will apply the operation. Only for create and move operation
        SyncPath _localCreationTargetPath; // Relative path of the item to create. Only for local create operation.
        std::shared_ptr<Node> _newParentNode =
                nullptr; // New parent on the replica on which we will apply the operation. Only for move operation
        Conflict _conflict;
        bool _isBreakingCycleOp{false};

        UniqueId _id = -1;
        UniqueId _parentId = -1; // ID of that parent operation i.e. the operation that must be completed before starting this one

        static UniqueId _nextId;
};

typedef std::shared_ptr<SyncOperation> SyncOpPtr;

class SyncOperationList : public SharedObject {
    public:
        SyncOperationList() {}
        SyncOperationList(const SyncOperationList &other) :
            _allOps(other._allOps), _opSortedList(other._opSortedList), _opListByType(other._opListByType),
            _node2op(other._node2op) {}
        ~SyncOperationList();

        void setOpList(const std::list<SyncOpPtr> &opList);

        SyncOpPtr getOp(UniqueId id);
        [[nodiscard]] const std::list<UniqueId> &opSortedList() const { return _opSortedList; }
        const std::unordered_set<UniqueId> &opListIdByType(const OperationType type) { return _opListByType[type]; }
        const std::list<UniqueId> &getOpIdsFromNodeId(const NodeId &nodeId) { return _node2op[nodeId]; }

        void pushOp(SyncOpPtr op);
        void insertOp(std::list<UniqueId>::const_iterator pos, SyncOpPtr op);
        void deleteOp(std::list<UniqueId>::const_iterator it);
        [[nodiscard]] size_t size() const { return _allOps.size(); }
        [[nodiscard]] int isEmpty() const { return _allOps.empty(); }
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

} // namespace KDC
