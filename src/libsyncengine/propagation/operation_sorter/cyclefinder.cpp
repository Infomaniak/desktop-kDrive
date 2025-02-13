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

void logCycle(SyncOperationList &currentCycle) {
    SyncName str;
    for (const auto &opId: currentCycle.opSortedList()) {
        const auto op = currentCycle.getOp(opId);
        str += op->affectedNode()->name();
        str += " ";
    }
    LOGW_INFO(Log::instance()->getLogger(), L"Chain is now: " << Utility::formatSyncName(str));
}

void CycleFinder::findCompleteCycle() {
    for (const auto &pair: _reorderings) {
        const auto startOp = pair.first;
        auto nextOp = pair.second;

        SyncOperationList currentCycle;
        (void) currentCycle.pushOp(startOp);
        LOGW_INFO(Log::instance()->getLogger(), L"Start op is " << Utility::formatSyncName(startOp->affectedNode()->name()));
        logCycle(currentCycle);

        std::list<std::pair<SyncOpPtr, SyncOpPtr>> pairsInCycles;
        pairsInCycles.push_back(pair);

        std::list<std::pair<SyncOpPtr, SyncOpPtr>> list = _reorderings;
        (void) list.remove(pair);

        while (!list.empty()) {
            bool nextFound = false;
            for (const auto &otherPair: list) {
                if (otherPair.first == nextOp) {
                    // Next item in chain found.
                    if (!currentCycle.pushOp(nextOp)) {
                        LOGW_INFO(Log::instance()->getLogger(), L"Op " << Utility::formatSyncName(nextOp->affectedNode()->name())
                                                                       << L" is already in the current chain");
                        break;
                    }
                    LOGW_INFO(Log::instance()->getLogger(),
                              L"Adding pair " << Utility::formatSyncName(otherPair.first->affectedNode()->name()) << L" / "
                                              << Utility::formatSyncName(otherPair.second->affectedNode()->name())
                                              << L" to cycle");
                    logCycle(currentCycle);

                    nextOp = otherPair.second;
                    pairsInCycles.push_back(otherPair);
                    nextFound = true;

                    if (nextOp == startOp) {
                        // A cycle has been found!
                        pairsInCycles.push_back(otherPair);
                        LOG_INFO(Log::instance()->getLogger(), "Cycle found!");
                        logCycle(currentCycle);

                        _completeCycle = currentCycle;
                        _cycleFound = true;
                        // Return now. There might be other cycles, but since they will be solved one by one, we do not need to
                        // find them all.
                        return;
                    }
                }
            }

            if (!nextFound) {
                // We reach the end of a chain without finding a cycle. Remove last item in chain to explore other potential
                // branches.
                LOGW_INFO(Log::instance()->getLogger(),
                          L"Removing pair " << Utility::formatSyncName(pairsInCycles.back().first->affectedNode()->name())
                                            << L" / "
                                            << Utility::formatSyncName(pairsInCycles.back().second->affectedNode()->name()));
                (void) list.remove(pairsInCycles.back());
                pairsInCycles.pop_back();
                if (list.empty() || pairsInCycles.empty()) break;

                currentCycle.popOp();
                logCycle(currentCycle);

                nextOp = pairsInCycles.back().second;
            }
        }
    }
}

} // namespace KDC
