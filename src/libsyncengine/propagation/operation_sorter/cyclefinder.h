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

#include "reconciliation/syncoperation.h"

#include <list>

namespace KDC {

class CycleFinder {
    public:
        explicit CycleFinder(const std::list<std::pair<SyncOpPtr, SyncOpPtr>> &reorderings) :
            _reorderings(reorderings) {};

        void findCompleteCycle();
        [[nodiscard]] const SyncOperationList &completeCycle() const { return _completeCycle; }
        [[nodiscard]] bool hasCompleteCycle() const { return _cycleFound; }

    private:
        bool findNextItemInChain(const std::list<std::pair<SyncOpPtr, SyncOpPtr>> &list, const SyncOpPtr &startOp,
                                 std::list<std::pair<SyncOpPtr, SyncOpPtr>> &pairsInCycles, SyncOpPtr &nextOp);
        void removeLastItemFromChain(std::list<std::pair<SyncOpPtr, SyncOpPtr>> &list,
                                     std::list<std::pair<SyncOpPtr, SyncOpPtr>> &pairsInCycles, SyncOpPtr &nextOp);

        std::list<std::pair<SyncOpPtr, SyncOpPtr>> _reorderings;
        SyncOperationList _completeCycle;
        bool _cycleFound{false};
};

} // namespace KDC
