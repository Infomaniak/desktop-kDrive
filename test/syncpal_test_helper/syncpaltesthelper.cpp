/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "syncpaltesthelper.hpp"
#include "libcommon/utility/timerutility.h"

#include "syncpal/syncpal.h"
#include "update_detection/file_system_observer/filesystemobserverworker.h"

namespace KDC {

SyncpalTestHelper::SyncpalTestHelper(const std::shared_ptr<SyncPal> syncPal) :
    _syncPal(syncPal),
    _setInitialSituation(syncPal),
    _executeOperations(syncPal) {}

void SyncpalTestHelper::setUp() {
    _syncPal->start();
}

void SyncpalTestHelper::tearDown() {}

void SyncpalTestHelper::setSyncpal(const std::shared_ptr<SyncPal> syncPal) {
    _syncPal = syncPal;
    _setInitialSituation.setSyncpal(syncPal);
    _executeOperations.setSyncpal(syncPal);
}

bool SyncpalTestHelper::setInitialSituation(const Situation &localSituation, const Situation &remoteSituation) {
    if (!_syncPal) return false;

    try {
        // If remote is needed (optional depending on test)
        if (_setInitialSituation.remoteRootId().empty()) {
            _setInitialSituation.setRemoteDrive(_syncPal->driveDbId(), *_syncPal->syncDb()->rootNode().nodeIdRemote());
        }

        // NOTE: generateInitialSituation() populates BOTH local and remote structures from a
        // single JSON tree in one call (every item is inserted on both sides at once). Calling it
        // twice here - once per Situation - will double-insert (and likely throw on duplicate ids)
        // if localSituation and remoteSituation aren't disjoint. Worth confirming whether local/remote
        // are meant to diverge before relying on this; if they should always match, only one call is needed.
        _setInitialSituation.generateInitialSituation(localSituation);
        _setInitialSituation.generateInitialSituation(remoteSituation);
    } catch (const std::exception &) {
        return false;
    }

    return true;
}

bool SyncpalTestHelper::getSituation(const Situation &, const Situation &) {
    return true;
}

bool SyncpalTestHelper::executeSyncUntilEnd(const std::chrono::milliseconds minWaitTime) const {
    const auto timeOutDuration = std::chrono::minutes(2);
    const TimerUtility timeoutTimer;

    // Wait for end of sync (A sync is considered ended when it stay in Idle for more than 3s)
    bool ended = false;
    while (!ended) {
        if (timeoutTimer.elapsed<std::chrono::minutes>() >= timeOutDuration) return false;

        if (_syncPal->isIdle() && !_syncPal->_localFSObserverWorker->updating() &&
            !_syncPal->_remoteFSObserverWorker->updating()) {
            const TimerUtility idleTimer;
            while (_syncPal->isIdle() && idleTimer.elapsed<std::chrono::microseconds>() < minWaitTime) {
                if (timeoutTimer.elapsed<std::chrono::minutes>() >= timeOutDuration) return false;
                Utility::msleep(5);
            }
            ended = idleTimer.elapsed<std::chrono::milliseconds>() >= minWaitTime;
        }
        Utility::msleep(100);
    }

    return true;
}

bool SyncpalTestHelper::executeSyncUpToStep(int, int) {
    return true;
}

bool SyncpalTestHelper::pauseSync() {
    return true;
}

bool SyncpalTestHelper::stopSync() {
    return true;
}

bool SyncpalTestHelper::executeOperations(const ReplicaSide side, const Operations &operations) {
    if (!_syncPal) return false;

    try {
        _executeOperations.executeOperations(side, operations);
    } catch (const std::exception &) {
        return false;
    }

    return true;
}

} // namespace KDC
