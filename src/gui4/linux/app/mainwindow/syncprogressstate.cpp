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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "syncprogressstate.h"

#include "app/cache/mainselectionstore.h"
#include "libcommon/utility/cstypes.h"

#include <algorithm>
#include <limits>
#include <optional>

namespace KDC {

namespace {

// `qsTrId` and QML integers are 32-bit.
qint32 toQmlItemCount(const int64_t count) {
    return static_cast<qint32>(std::clamp<int64_t>(count, 0, std::numeric_limits<qint32>::max()));
}

// The step of a `Starting` synchronization is ignored: its status is set optimistically by the client, so its step and counters
// still describe the previous run.
SyncProgressState::Phase resolveSyncProgressPhase(const std::optional<SyncStatus> status, const SyncStep step,
                                                  const int64_t totalFiles) {
    using enum SyncProgressState::Phase;

    if (status == SyncStatus::Starting) {
        return Exploring;
    }
    if (status != SyncStatus::Running) {
        return None;
    }

    switch (step) {
        // Scoped to the switch: `None` means `SyncStep::None` here, and `SyncProgressState::Phase::None` above.
        using enum SyncStep;
        case None:
        case Idle:
        case UpdateDetection1:
            return Exploring;
        case Propagation2:
            // The server has not counted the items yet.
            return totalFiles > 0 ? Propagating : Analysing;
        case UpdateDetection2:
        case Reconciliation1:
        case Reconciliation2:
        case Reconciliation3:
        case Reconciliation4:
        case Propagation1:
        case Done:
        case EnumEnd:
            return Analysing;
    }
    return Analysing;
}

} // namespace

SyncProgressState::SyncProgressState(const MainSelectionStore &mainSelectionStore, QObject *const parent) :
    QObject(parent),
    _mainSelectionStore(mainSelectionStore) {
    (void) connect(&_mainSelectionStore, &MainSelectionStore::currentSyncRuntimeInfoChanged, this, &SyncProgressState::refresh);
    refresh();
}

void SyncProgressState::refresh() {
    const auto runtimeInfo = _mainSelectionStore.currentSyncRuntimeInfo();

    const Phase nextPhase = runtimeInfo.has_value()
                                    ? resolveSyncProgressPhase(runtimeInfo->status, runtimeInfo->step, runtimeInfo->totalFiles)
                                    : Phase::None;

    // Counters only mean something while items are being propagated.
    const bool hasCounters = nextPhase == Phase::Propagating;
    const qint32 nextCompletedItems = hasCounters ? toQmlItemCount(runtimeInfo->currentFile) : 0;
    const qint32 nextTotalItems = hasCounters ? toQmlItemCount(runtimeInfo->totalFiles) : 0;

    if (_phase == nextPhase && _completedItems == nextCompletedItems && _totalItems == nextTotalItems) {
        return;
    }

    _phase = nextPhase;
    _completedItems = nextCompletedItems;
    _totalItems = nextTotalItems;
    emit progressChanged();
}

} // namespace KDC
