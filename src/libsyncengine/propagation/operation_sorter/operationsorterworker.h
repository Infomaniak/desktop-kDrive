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

#include "syncpal/operationprocessor.h"
#include "syncpal/syncpal.h"
#include "reconciliation/syncoperation.h"

#include <list>
#include <map>

namespace KDC {

class OperationSorterWorker final : public OperationProcessor {
    public:
        OperationSorterWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName);

        void execute() override;

        [[nodiscard]] bool hasOrderChanged() const { return _hasOrderChanged; }

    private:
        SyncOperationList _unsortedList;

        std::list<std::pair<SyncOpPtr, SyncOpPtr>> _reorderings;
        bool _hasOrderChanged;

        ExitCode sortOperations();
        /**
         * @brief delete before move, e.g. user deletes an object at path “x” and moves another object “a” to “x”.
         */
        void fixDeleteBeforeMove();
        /**
         * @brief move before create, e.g. user moves an object “a“ to “b“ and creates another object at “a”.
         */
        void fixMoveBeforeCreate();
        /**
         * @brief move before delete, e.g. user moves object “X/y“ outside of directory “X“ (e.g. to “z“) and then deletes “X“.
         */
        void fixMoveBeforeDelete();
        /**
         * @brief create before move, e.g. user creates directory “X“ and moves object “y“ into “X“.
         */
        void fixCreateBeforeMove();
        /**
         * @brief delete before create, e.g. user deletes object “x“ and then creates a new object at “x“.
         */
        void fixDeleteBeforeCreate();
        /**
         * @brief move before move (occupation), e.g. user moves file “a“ to “temp“ and then moves file “b“ to “a“.
         */
        void fixMoveBeforeMoveOccupied();
        /**
         * @brief create before create, e.g. user creates directory “X“ and then creates an object inside it.
         */
        void fixCreateBeforeCreate();
        /**
         * @brief edit before move, e.g. user moves an object “a“ to “b“ and then edit it.
         */
        void fixEditBeforeMove();
        /**
         * @brief move before move (parent-child flip), e.g. user moves directory “A/B“ to “C“, then moves directory “A“ to
         * “C/A“ (parent-child relationships are now flipped).
         */
        void fixMoveBeforeMoveHierarchyFlip();

        std::optional<SyncOperationList> fixImpossibleFirstMoveOp();
        std::list<SyncOperationList> findCompleteCycles();
        bool breakCycle(SyncOperationList &cycle, SyncOpPtr renameResolutionOp);
        void moveFirstAfterSecond(SyncOpPtr opFirst, SyncOpPtr opSecond);
        void addPairToReorderings(SyncOpPtr op, std::shared_ptr<SyncOperation> opOnFirstDepends);

        bool hasParentWithHigherIndex(const std::unordered_map<UniqueId, int> &indexMap, SyncOpPtr op, int createOpIndex,
                                      SyncOpPtr &ancestorOp, int &depth);

        bool getIdFromDb(ReplicaSide side, const SyncPath &parentPath, NodeId &id) const;

        friend class TestOperationSorterWorker;
};
} // namespace KDC
