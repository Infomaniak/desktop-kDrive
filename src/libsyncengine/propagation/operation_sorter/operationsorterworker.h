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

#include "operationsorterfilter.h"
#include "syncpal/operationprocessor.h"
#include "syncpal/syncpal.h"
#include "reconciliation/syncoperation.h"

#include <list>

namespace KDC {

class OperationSorterWorker : public OperationProcessor {
    public:
        OperationSorterWorker(const std::shared_ptr<SyncPal> &syncPal, const std::string &name, const std::string &shortName);

        void execute() override;

        [[nodiscard]] bool hasOrderChanged() const { return _hasOrderChanged; }

    private:
        void sortOperations();

        /**
         * @brief delete before move, e.g. user deletes an object at path "x" and moves another object "a" to "x".
         */
        void fixDeleteBeforeMove();
        // Idea : Insert in set the names of the deleted file and the destination names of the moved file. If same name inserted
        // twice, check fixDeleteBeforeMove.
        void fixDeleteBeforeMoveOptimized();
        /**
         * @brief move before create, e.g. user moves an object "a" to "b" and creates another object at "a".
         */
        void fixMoveBeforeCreate();
        // Idea : Insert in set the names of the created file and the origin names of the moved file. If same name inserted
        // twice, check fixMoveBeforeCreate.
        void fixMoveBeforeCreateOptimized();
        /**
         * @brief move before delete, e.g. user moves object "X/y" outside of directory "X" and then deletes "X".
         */
        void fixMoveBeforeDelete();
        // Idea : keep all deleted file paths. Check for each move if the origin path is inside a deleted path.
        void fixMoveBeforeDeleteOptimized();
        /**
         * @brief create before move, e.g. user creates directory "X" and moves object "y" into "X".
         */
        void fixCreateBeforeMove();
        // Idea : ???
        void fixCreateBeforeMoveOptimized();
        /**
         * @brief delete before create, e.g. user deletes object "x" and then creates a new object at "x".
         */
        void fixDeleteBeforeCreate();
        // Idea :Insert in set the names of the created file and the names of the deleted file. If same name inserted
        // twice, check fixDeleteBeforeCreate.
        void fixDeleteBeforeCreateOptimized();
        /**
         * @brief move before move (occupation), e.g. user moves file "a" to "temp" and then moves file "b" to "a".
         */
        void fixMoveBeforeMoveOccupied();
        // Idea : Insert in set the origin and destination names of the moved file. If same name inserted
        // twice, check fixMoveBeforeMoveOccupied.
        void fixMoveBeforeMoveOccupiedOptimized();
        /**
         * @brief create before create, e.g. user creates directory "X" and then creates an object inside it.
         */
        void fixCreateBeforeCreate();
        /**
         * @brief edit before move, e.g. user moves an object "a" to "b" and then edit it.
         */
        void fixEditBeforeMove();
        // Idea : List just the files that have both EDIT and MOVE operations.
        void fixEditBeforeMoveOptimized();
        /**
         * @brief move before move (parent-child flip), e.g. user moves directory "A/B" to "C", then moves directory "A" to
         * "C/A" (parent-child relationships are now flipped).
         */
        void fixMoveBeforeMoveHierarchyFlip();
        // Idea : check just the files whose parent has a MOVE event too. WHAT ABOUT HIERARCHY FLIP WITH A GRAND PARENT????
        void fixMoveBeforeMoveHierarchyOptimized();

        std::optional<SyncOperationList> fixImpossibleFirstMoveOp();
        bool breakCycle(SyncOperationList &cycle, const SyncOpPtr &renameResolutionOp);
        /**
         * @brief Move `opFirst` immediately after `opSecond` in the sorted operation list.
         * @param opFirst The operation to be moved.
         * @param opSecond The operation that must be executed before `opFirst`.
         */
        void moveFirstAfterSecond(const SyncOpPtr &opFirst, const SyncOpPtr &opSecond);
        /**
         * @brief Insert `op` and `parentOp` inside a list of re-ordered nodes.
         * @param op1 A permuted operation.
         * @param op2 The associated permuted operation.
         */
        void addPairToReorderings(const SyncOpPtr &op1, const SyncOpPtr &op2);

        /**
         * @brief Check if a CREATE operation has a parent CREATE operation with higher index in the sorted operation list. Higher
         * index means positioned further in the list and, therefor, the parent operation would be processed after the child
         * operation.
         * @param opIdToIndexMap A map giving the correspondence between an operation ID and its position in the sorted operation
         * list.
         * @param op The child operation to be compared.
         * @param ancestorOpWithHighestDistance The ancestor operation, positioned further the op in the sorted list, with the
         * highest distance.
         * @param relativeDepth The distance between the child node and the ancestor node in a tree. For example, in the path
         * A/AA/AAA/AAAA, the distance between A and AA is 1, the distance between A and AAAA is 3.
         * @return
         */
        bool hasParentWithHigherIndex(const std::unordered_map<UniqueId, int32_t> &opIdToIndexMap, const SyncOpPtr &op,
                                      SyncOpPtr &ancestorOpWithHighestDistance, int32_t &relativeDepth) const;

        bool getIdFromDb(ReplicaSide side, const SyncPath &path, NodeId &id) const;

        std::list<std::pair<SyncOpPtr, SyncOpPtr>> _reorderings;
        bool _hasOrderChanged{false};
        OperationSorterFilter _filter;

        friend class TestOperationSorterWorker;
};
} // namespace KDC
