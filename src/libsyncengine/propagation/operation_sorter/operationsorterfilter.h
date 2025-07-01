// Infomaniak kDrive - Desktop
// Copyright (C) 2023-2025 Infomaniak Network SA
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include "libsyncengine/reconciliation/syncoperation.h"

namespace KDC {
using NameToOpMap = std::unordered_map<SyncName, SyncOpPtr, SyncNameHashFunction, std::equal_to<>>;
using NodeIdToOpListMap = std::unordered_map<DbNodeId, std::list<SyncOpPtr>>;
using SyncPathToSyncOpMap = std::unordered_map<SyncPath, SyncOpPtr, PathHashFunction>;

class OperationSorterFilter {
    public:
        explicit OperationSorterFilter(const std::unordered_map<UniqueId, SyncOpPtr> &ops);

        void filterOperations();
        void clear();

        [[nodiscard]] const std::list<std::pair<SyncOpPtr, SyncOpPtr>> &fixDeleteBeforeMoveCandidates() const {
            return _fixDeleteBeforeMoveCandidates;
        }
        [[nodiscard]] const std::list<std::pair<SyncOpPtr, SyncOpPtr>> &fixMoveBeforeCreateCandidates() const {
            return _fixMoveBeforeCreateCandidates;
        }
        [[nodiscard]] const std::list<std::pair<SyncOpPtr, SyncOpPtr>> &fixMoveBeforeDeleteCandidates() const {
            return _fixMoveBeforeDeleteCandidates;
        }
        [[nodiscard]] const std::list<std::pair<SyncOpPtr, SyncOpPtr>> &fixCreateBeforeMoveCandidates() const {
            return _fixCreateBeforeMoveCandidates;
        }
        [[nodiscard]] const std::list<std::pair<SyncOpPtr, SyncOpPtr>> &fixDeleteBeforeCreateCandidates() const {
            return _fixDeleteBeforeCreateCandidates;
        }
        [[nodiscard]] const std::list<std::pair<SyncOpPtr, SyncOpPtr>> &fixMoveBeforeMoveOccupiedCandidates() const {
            return _fixMoveBeforeMoveOccupiedCandidates;
        }
        [[nodiscard]] const NodeIdToOpListMap &fixEditBeforeMoveCandidates() const { return _fixEditBeforeMoveCandidates; }
        [[nodiscard]] const std::list<std::pair<SyncOpPtr, SyncOpPtr>> &fixMoveBeforeMoveHierarchyFlipCandidates() const {
            return _fixMoveBeforeMoveHierarchyFlipCandidates;
        }

    private:
        const std::unordered_map<UniqueId, SyncOpPtr> &_ops;

        /**
         * @brief Insert in a set the names of the deleted items and the destination names of the moved items. If the same name is
         * inserted twice, fixDeleteBeforeMove will be checked for the corresponding operations.
         * @param op The SyncOperation to be checked.
         * @param deleteBeforeMoveCandidates The map storing the names of affected items.
         */
        void filterDeleteBeforeMoveCandidates(const SyncOpPtr &op, NameToOpMap &deleteBeforeMoveCandidates);
        /**
         * @brief Insert in a set the names of the created items and the origin names of the moved items. If the same name is
         * inserted twice, fixMoveBeforeCreate will be checked for the corresponding operations.
         * @param op The SyncOperation to be checked.
         * @param moveBeforeCreateCandidates The map storing the names of affected items.
         */
        void filterMoveBeforeCreateCandidates(const SyncOpPtr &op, NameToOpMap &moveBeforeCreateCandidates);
        /**
         * @brief For each move operation, check if the origin path is inside a deleted path.
         * @param op The SyncOperation to be checked.
         * @param deletedDirectoryPaths The map storing all the deleted directory paths.
         * @param moveOriginPaths The map storing all move operation origin paths.
         */
        void filterMoveBeforeDeleteCandidates(const SyncOpPtr &op, SyncPathToSyncOpMap &deletedDirectoryPaths,
                                              SyncPathToSyncOpMap &moveOriginPaths);
        /**
         * @brief For each move operation, check if the destination path is inside a created path.
         * @param op The SyncOperation to be checked.
         * @param createdDirectoryPaths The map storing the all created directory paths.
         * @param moveDestinationPaths The map storing all move operation destination paths.
         */
        void filterCreateBeforeMoveCandidates(const SyncOpPtr &op, SyncPathToSyncOpMap &createdDirectoryPaths,
                                              SyncPathToSyncOpMap &moveDestinationPaths);
        /**
         * @brief Insert in a set the names of the deleted items and the names of the created items. If the same name is
         * inserted twice, fixDeleteBeforeCreate will be checked for the corresponding operations.
         * @param op The SyncOperation to be checked.
         * @param deleteBeforeCreateCandidates The map storing the names of affected items.
         */
        void filterDeleteBeforeCreateCandidates(const SyncOpPtr &op, NameToOpMap &deleteBeforeCreateCandidates);
        /**
         * @brief Insert in a set the origin and destination names of the moved items. If the same name is inserted twice,
         * fixMoveBeforeMoveOccupied will be checked for the corresponding operations.
         * @param op The SyncOperation to be checked.
         * @param moveBeforeMoveOccupiedCandidates The map storing the names of affected items.
         */
        void filterMoveBeforeMoveOccupiedCandidates(const SyncOpPtr &op, NameToOpMap &moveOriginNames,
                                                    NameToOpMap &moveDestinationNames);
        /**
         * @brief Select only the operations that have both an EDIT and a MOVE operation.
         * @param op The SyncOperation to be checked.
         */
        void filterEditBeforeMoveCandidates(const SyncOpPtr &op);
        /**
         * @brief If the origin path of opA contains the origin path of opB and, the destination path of opB contains the
         * destination path of opA, then we are in a case of moveMoveHierarchyFlip.
         * @param op The SyncOperation to be checked.
         * @param moveBeforeMoveHierarchyFlipCandidates The list storing every move operations.
         */
        void filterMoveBeforeMoveHierarchyFlipCandidates(
                const SyncOpPtr &op, std::list<std::pair<SyncOpPtr, SyncPath>> &moveBeforeMoveHierarchyFlipCandidates);

        std::list<std::pair<SyncOpPtr, SyncOpPtr>> _fixDeleteBeforeMoveCandidates;
        std::list<std::pair<SyncOpPtr, SyncOpPtr>> _fixMoveBeforeCreateCandidates;
        std::list<std::pair<SyncOpPtr, SyncOpPtr>> _fixMoveBeforeDeleteCandidates;
        std::list<std::pair<SyncOpPtr, SyncOpPtr>> _fixCreateBeforeMoveCandidates;
        std::list<std::pair<SyncOpPtr, SyncOpPtr>> _fixDeleteBeforeCreateCandidates;
        std::list<std::pair<SyncOpPtr, SyncOpPtr>> _fixMoveBeforeMoveOccupiedCandidates;
        NodeIdToOpListMap _fixEditBeforeMoveCandidates;
        std::list<std::pair<SyncOpPtr, SyncOpPtr>> _fixMoveBeforeMoveHierarchyFlipCandidates;
};

} // namespace KDC
