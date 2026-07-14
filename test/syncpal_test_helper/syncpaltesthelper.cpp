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

#include "syncpaltesthelper.h"
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

void SyncpalTestHelper::tearDown() {
    if (_syncPal) {
        _syncPal->stop(SyncPal::PauseCaller::Sync, SyncPal::DbBehaviorAfterStop::Remove);
    }
}

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

        // generateInitialSituation() creates the described items on both the real local filesystem and
        // the real remote drive in one call (every item is inserted on both sides at once, just like
        // ExecuteOperations does for operations). Calling it a second time with the same content would
        // create duplicate local/remote items. Local and remote are therefore expected to match here, so
        // only one call is needed. The SyncPal is then run so it discovers the generated items itself and
        // populates its own Db/update-trees/snapshots with real ids.
        if (!(localSituation == remoteSituation)) return false;
        _setInitialSituation.generateInitialSituation(localSituation);
    } catch (const SituationGeneratorException &) {
        return false;
    }

    return executeSyncUntilEnd();
}

bool SyncpalTestHelper::getSituation(const Situation &, const Situation &) const {
    return false;
}

bool SyncpalTestHelper::executeSyncUntilEnd(const std::chrono::milliseconds minWaitTime) const {
    const auto timeOutDuration = std::chrono::minutes(2);
    const TimerUtility timeoutTimer;

    // Wait for end of sync (A sync is considered ended when it stays in Idle for more than minWaitTime)
    TimerUtility idleTimer;
    bool wasIdle = false;
    while (true) {
        if (timeoutTimer.elapsed<std::chrono::minutes>() >= timeOutDuration) return false;

        if (const bool isIdleNow = _syncPal->isIdle() && !_syncPal->_localFSObserverWorker->updating() &&
                                   !_syncPal->_remoteFSObserverWorker->updating();
            !isIdleNow) {
            wasIdle = false;
        } else if (!wasIdle) {
            wasIdle = true;
            idleTimer.restart();
        } else if (idleTimer.elapsed<std::chrono::milliseconds>() >= minWaitTime) {
            return true;
        }

        Utility::msleep(100);
    }
}

bool SyncpalTestHelper::executeSyncUpToStep([[maybe_unused]] const int64_t targetStep,
                                            [[maybe_unused]] const int64_t timeout) const {
    return false;
}

bool SyncpalTestHelper::pauseSync() const {
    return false;
}

bool SyncpalTestHelper::stopSync() const {
    return false;
}

bool SyncpalTestHelper::executeOperations(const ReplicaSide side, const Operations &operations) {
    if (!_syncPal) return false;

    try {
        _executeOperations.executeOperations(side, operations);
    } catch (const OperationsParserException &) {
        return false;
    }

    return true;
}

} // namespace KDC
