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

#include "syncpal/operationprocessor.h"
#include "syncpal/syncpal.h"
#include "reconciliation/syncoperation.h"

#include <list>
#include <map>

namespace KDC {

class OperationSorterWorker : public OperationProcessor {
    public:
        OperationSorterWorker(std::shared_ptr<SyncPal> syncPal, const std::string name, const std::string &shortName);

        void execute() override;

        inline bool hasOrderChanged() const { return _hasOrderChanged; }

    private:
        SyncOperationList _unsortedList;

        std::list<std::pair<SyncOpPtr, SyncOpPtr>> _reorderings;
        bool _hasOrderChanged;

        ExitCode sortOperations();
        void fixDeleteBeforeMove();
        void fixMoveBeforeCreate();
        void fixMoveBeforeDelete();
        void fixCreateBeforeMove();
        void fixDeleteBeforeCreate();
        void fixMoveBeforeMoveOccupied();
        void fixCreateBeforeCreate();
        void fixEditBeforeMove();
        void fixMoveBeforeMoveHierarchyFlip();
        std::optional<SyncOperationList> fixImpossibleFirstMoveOp();
        std::list<SyncOperationList> findCompleteCycles();
        bool breakCycle(SyncOperationList &cycle, SyncOpPtr renameResolutionOp);
        void moveFirstAfterSecond(SyncOpPtr opFirst, SyncOpPtr opSecond);
        void addPairToReoderings(SyncOpPtr op, std::shared_ptr<SyncOperation> opOnFirstDepends);

        bool hasParentWithHigherIndex(const std::unordered_map<UniqueId, int> &indexMap, const SyncOpPtr op,
                                      const int createOpIndex, SyncOpPtr &ancestorOp, int &depth);

        friend class TestOperationSorterWorker;
};
}  // namespace KDC
