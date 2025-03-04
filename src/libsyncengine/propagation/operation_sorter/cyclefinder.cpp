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

void logCycle(const SyncOperationList &currentCycle) {
    std::string str;
    for (const auto &opId: currentCycle.opSortedList()) {
        str += " ";
        str += std::to_string(opId);
    }
    LOG_INFO(Log::instance()->getLogger(), "Chain is now:" << str);
}

void CycleFinder::findCompleteCycle() {
    for (const auto &pair: _reorderings) {
        const auto startOp = pair.first;
        auto nextOp = pair.second;

        _completeCycle.clear();
        (void) _completeCycle.pushOp(startOp);
        LOG_INFO(Log::instance()->getLogger(), "Start op is " << startOp->id());
        logCycle(_completeCycle);

        std::list<std::pair<SyncOpPtr, SyncOpPtr>> pairsInCycles;
        pairsInCycles.push_back(pair);

        std::list<std::pair<SyncOpPtr, SyncOpPtr>> list = _reorderings;
        (void) list.remove(pair);

        while (!list.empty() && !pairsInCycles.empty() && !_cycleFound) {
            if (!findNextItemInChain(list, startOp, pairsInCycles, nextOp)) {
                // We reach the end of a chain without finding a cycle. Remove last item in chain to explore other potential
                // branches.
                removeLastItemFromChain(list, pairsInCycles, nextOp);
            }
        }
        if (_cycleFound) {
            // Return now. There might be other cycles, but since they will be solved one by one, we do not need to
            // find them all.
            break;
        }
    }

    if (!_cycleFound) {
        _completeCycle.clear();
    }
}

bool CycleFinder::findNextItemInChain(const std::list<std::pair<SyncOpPtr, SyncOpPtr>> &list, const SyncOpPtr &startOp,
                                      std::list<std::pair<SyncOpPtr, SyncOpPtr>> &pairsInCycles, SyncOpPtr &nextOp) {
    bool nextFound = false;
    for (const auto &otherPair: list) {
        if (otherPair.first == nextOp) {
            // Next item in chain found.
            if (!_completeCycle.pushOp(nextOp)) {
                LOG_INFO(Log::instance()->getLogger(), "Op " << nextOp->id() << " is already in the current chain");
                break;
            }
            LOG_INFO(Log::instance()->getLogger(),
                     "Adding pair " << otherPair.first->id() << " / " << otherPair.second->id() << " to cycle");
            logCycle(_completeCycle);

            nextOp = otherPair.second;
            pairsInCycles.push_back(otherPair);
            nextFound = true;

            if (nextOp == startOp) {
                // A cycle has been found!
                pairsInCycles.push_back(otherPair);
                LOG_INFO(Log::instance()->getLogger(), "Cycle found!");
                logCycle(_completeCycle);

                _cycleFound = true;
                return true;
            }
        }
    }

    return nextFound;
}

void CycleFinder::removeLastItemFromChain(std::list<std::pair<SyncOpPtr, SyncOpPtr>> &list,
                                          std::list<std::pair<SyncOpPtr, SyncOpPtr>> &pairsInCycles, SyncOpPtr &nextOp) {
    LOG_INFO(Log::instance()->getLogger(),
             "Removing pair " << pairsInCycles.back().first->id() << " / " << pairsInCycles.back().second->id());
    (void) list.remove(pairsInCycles.back());
    pairsInCycles.pop_back();
    _completeCycle.popOp();
    logCycle(_completeCycle);

    if (!pairsInCycles.empty()) {
        nextOp = pairsInCycles.back().second;
    }
}

} // namespace KDC
