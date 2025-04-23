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

class OperationSorterFilter {
    public:
        explicit OperationSorterFilter(const std::unordered_map<UniqueId, SyncOpPtr> &ops);

        void filterOperations();

        [[nodiscard]] const std::list<std::pair<SyncOpPtr, SyncOpPtr>> &fixDeleteBeforeMoveCandidates() const {
            return _fixDeleteBeforeMoveCandidates;
        }
        [[nodiscard]] const std::list<std::pair<SyncOpPtr, SyncOpPtr>> &fixMoveBeforeCreateCandidates() const {
            return _fixMoveBeforeCreateCandidates;
        }
        [[nodiscard]] const std::list<std::pair<SyncOpPtr, SyncOpPtr>> &fixMoveBeforeDeleteCandidates() const {
            return _fixMoveBeforeDeleteCandidates;
        }
        [[nodiscard]] const std::unordered_map<NodeId, std::list<SyncOpPtr>, StringHashFunction, std::equal_to<>> &
        fixEditBeforeMoveCandidates() const {
            return _fixEditBeforeMoveCandidates;
        }

    private:
        const std::unordered_map<UniqueId, SyncOpPtr> &_ops;

        void filterFixDeleteBeforeMoveCandidates(
                const SyncOpPtr &op,
                std::unordered_map<SyncName, SyncOpPtr, StringHashFunction, std::equal_to<>> &deleteBeforeMove);
        void filterFixMoveBeforeCreateCandidates(
                const SyncOpPtr &op,
                std::unordered_map<SyncName, SyncOpPtr, StringHashFunction, std::equal_to<>> &moveBeforeCreate);

        void filterFixEditBeforeMoveCandidates(const SyncOpPtr &op);

        std::list<std::pair<SyncOpPtr, SyncOpPtr>> _fixDeleteBeforeMoveCandidates;
        std::list<std::pair<SyncOpPtr, SyncOpPtr>> _fixMoveBeforeCreateCandidates;
        std::list<std::pair<SyncOpPtr, SyncOpPtr>> _fixMoveBeforeDeleteCandidates;
        std::unordered_map<NodeId, std::list<SyncOpPtr>, StringHashFunction, std::equal_to<>> _fixEditBeforeMoveCandidates;
};

} // namespace KDC
