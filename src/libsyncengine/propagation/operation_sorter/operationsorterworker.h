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
        struct CreatePair {
                CreatePair(SyncOpPtr op_, const int32_t opNodeDepth_, SyncOpPtr ancestorOp_) :
                    op(op_),
                    opNodeDepth(opNodeDepth_),
                    ancestorOp(ancestorOp_) {}
                SyncOpPtr op{nullptr};
                int32_t opNodeDepth{};
                SyncOpPtr ancestorOp{nullptr};
        };
        class CreatePairDepthCmp {
            public:
                bool operator()(const CreatePair &a, const CreatePair &b) const {
                    if (a.opNodeDepth == b.opNodeDepth) {
                        // If depths are equal, put op to move with greatest ID first to preserve initial ordering.
                        return a.op->id() < b.op->id();
                    }
                    return a.opNodeDepth < b.opNodeDepth;
                }
        };

        using CreatePairQueue = std::priority_queue<CreatePair, std::vector<CreatePair>, CreatePairDepthCmp>;

        void sortOperations();

        /**
         * @brief delete before move, e.g. user deletes an object at path "x" and moves another object "a" to "x".
         */
        void fixDeleteBeforeMove();
        /**
         * @brief move before create, e.g. user moves an object "a" to "b" and creates another object at "a".
         */
        void fixMoveBeforeCreate();
        /**
         * @brief move before delete, e.g. user moves object "X/y" outside of directory "X" and then deletes "X".
         */
        void fixMoveBeforeDelete();
        /**
         * @brief create before move, e.g. user creates directory "X" and moves object "y" into "X".
         */
        void fixCreateBeforeMove();
        /**
         * @brief delete before create, e.g. user deletes object "x" and then creates a new object at "x".
         */
        void fixDeleteBeforeCreate();
        /**
         * @brief move before move (occupation), e.g. user moves file "a" to "temp" and then moves file "b" to "a".
         */
        void fixMoveBeforeMoveOccupied();
        /**
         * @brief create before create, e.g. user creates directory "X" and then creates an object inside it.
         */
        void fixCreateBeforeCreate();
        /**
         * @brief edit before move, e.g. user moves an object "a" to "b" and then edit it.
         */
        void fixEditBeforeMove();
        /**
         * @brief move before move (parent-child flip), e.g. user moves directory "A/B" to "C", then moves directory "A" to
         * "C/A" (parent-child relationships are now flipped).
         */
        void fixMoveBeforeMoveHierarchyFlip();

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
         * highest distance.
         * @param depth The depth of op's affected node.
         * @return The operation on an ancestor of op's node with the highest index in the operation list, if it exists. Returns
         * nullptr otherwise.
         */
        SyncOpPtr getAncestorOpWithHighestIndex(const std::unordered_map<UniqueId, int32_t> &opIdToIndexMap, const SyncOpPtr &op,
                                                int32_t &relativeDepth) const;

        bool getIdFromDb(ReplicaSide side, const SyncPath &path, NodeId &id) const;

        std::pair<SyncOpPtr, SyncOpPtr> extractOpsByType(OperationType type1, OperationType type2, SyncOpPtr op,
                                                         SyncOpPtr otherOp) const;

        std::list<std::pair<SyncOpPtr, SyncOpPtr>> _reorderings;
        bool _hasOrderChanged{false};
        OperationSorterFilter _filter;

        friend class TestOperationSorterWorker;
};
} // namespace KDC
