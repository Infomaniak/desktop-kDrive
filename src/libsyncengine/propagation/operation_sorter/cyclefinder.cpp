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

#include "cyclefinder.h"

namespace KDC {

void CycleFinder::findCompleteCycle() {
    // auto list = _reorderings;
    auto list = _reorderings;
    for (auto it = list.begin(); it != list.end();) {
        const auto pair = *it;
        const auto startOp = pair.first;
        auto nextOp = pair.second;
        SyncOperationList currentCycle;
        currentCycle.pushOp(startOp);

        std::list<std::pair<SyncOpPtr, SyncOpPtr>> pairsInCycles;
        pairsInCycles.push_back(pair);

        // bool cycleFound = false;
        bool nextFound = true;
        while (nextFound) {
            nextFound = false;
            for (auto &otherPair: /*list*/ list) {
                if (pair == otherPair) continue;

                if (otherPair.first == nextOp) {
                    // Next item in chain found.
                    currentCycle.pushOp(nextOp);
                    nextOp = otherPair.second;
                    pairsInCycles.push_back(otherPair);
                    nextFound = true;
                }

                if (nextOp == startOp) {
                    // A cycle is complete!
                    // cycleFound = true;
                    pairsInCycles.push_back(otherPair);
                    // currentCycle.pushOp(otherPair.first);
                    _completeCycle = currentCycle;
                    _cycleFound = true;
                    return; // Return now. There might be other cycles, but since they will be solved one by one, we do not need
                    // to find them all.
                }
            }
        }

        it = list.erase(it);
    }
}

} // namespace KDC
