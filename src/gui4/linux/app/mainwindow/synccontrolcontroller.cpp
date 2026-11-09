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

#include "app/mainwindow/synccontrolcontroller.h"

#include "app/cache/appcache.h"
#include "app/cache/mainselectionstore.h"
#include "app/cache/syncerrorsprojection.h"
#include "app/mainwindow/homestateresolver.h"
#include "app/mainwindow/networkstatusobserver.h"
#include "app/services/syncservice.h"

#include <QLoggingCategory>

#include <optional>
#include <utility>

namespace KDC {

namespace {

Q_LOGGING_CATEGORY(lcSyncControlController, "gui.v4.synccontrol", QtInfoMsg)

} // namespace

SyncControlController::SyncControlController(const AppCache &appCache, const MainSelectionStore &selectionStore,
                                             const SyncErrorsProjection &projection, SyncService &syncService,
                                             const NetworkStatusObserver &networkStatusObserver, QObject *const parent) :
    QObject(parent),
    _appCache(appCache),
    _selectionStore(selectionStore),
    _projection(projection),
    _syncService(syncService),
    _networkStatusObserver(networkStatusObserver) {
    (void) connect(&_appCache, &AppCache::syncsChanged, this, &SyncControlController::refresh);
    (void) connect(&_appCache, &AppCache::usersChanged, this, &SyncControlController::refresh);
    (void) connect(&_appCache, &AppCache::syncStatusChanged, this, &SyncControlController::refresh);
    (void) connect(&_projection, &SyncErrorsProjection::blockedSyncsChanged, this, &SyncControlController::refresh);
    (void) connect(&_projection, &SyncErrorsProjection::currentBlockingErrorChanged, this, &SyncControlController::refresh);
    (void) connect(&_syncService, &SyncService::syncActionPendingChanged, this, &SyncControlController::refresh);
    (void) connect(&_selectionStore, &MainSelectionStore::currentSyncDbIdChanged, this, &SyncControlController::refresh);
    (void) connect(&_selectionStore, &MainSelectionStore::currentSyncStatusChanged, this, &SyncControlController::refresh);
    (void) connect(&_networkStatusObserver, &NetworkStatusObserver::offlineChanged, this, &SyncControlController::refresh);

    refresh();
}

bool SyncControlController::available() const {
    const bool selectedActionAvailable =
            _snapshot.selectedState == SyncControlState::Pause || _snapshot.selectedState == SyncControlState::Resume;
    return selectedActionAvailable || !_snapshot.pauseTargets.empty() || !_snapshot.resumeTargets.empty();
}

void SyncControlController::toggleSelected() {
    const auto syncDbId = static_cast<SyncDbId>(_selectionStore.currentSyncDbId());
    if (syncDbId == 0) {
        qCWarning(lcSyncControlController) << "Sync control ignored: no synchronization is selected";
        return;
    }

    switch (computeSnapshot().selectedState) {
        case SyncControlState::Pause:
            _syncService.stopSync(syncDbId);
            return;
        case SyncControlState::Resume:
            _syncService.startSync(syncDbId);
            return;
        case SyncControlState::Disabled:
        case SyncControlState::Pending:
            qCDebug(lcSyncControlController) << "Sync control ignored in current state | syncDbId:" << syncDbId;
            return;
    }
}

void SyncControlController::pauseAll() {
    const auto targets = computeSnapshot().pauseTargets;
    if (targets.empty()) {
        qCDebug(lcSyncControlController) << "Pause all ignored: no synchronization can be paused";
        return;
    }

    qCInfo(lcSyncControlController) << "Pause all requested | count:" << targets.size();
    _syncService.stopSyncs(targets);
}

void SyncControlController::resumeAll() {
    const auto targets = computeSnapshot().resumeTargets;
    if (targets.empty()) {
        qCDebug(lcSyncControlController) << "Resume all ignored: no synchronization can be resumed";
        return;
    }

    qCInfo(lcSyncControlController) << "Resume all requested | count:" << targets.size();
    _syncService.startSyncs(targets);
}

/**
 * Resolves the control of the selected synchronization and the targets of the actions on every synchronization. Offline,
 * nothing is available.
 */
SyncControlController::Snapshot SyncControlController::computeSnapshot() const {
    Snapshot snapshot;
    const auto contexts = _appCache.syncContexts();
    snapshot.syncCount = contexts.size();

    if (_networkStatusObserver.offline()) {
        return snapshot;
    }

    for (const auto &context: contexts) {
        const SyncDbId syncDbId = context.syncInfo.dbId();
        switch (syncControlStateOf(syncDbId, _projection.isSyncBlocked(syncDbId))) {
            case SyncControlState::Pause:
                snapshot.pauseTargets.push_back(syncDbId);
                break;
            case SyncControlState::Resume:
                snapshot.resumeTargets.push_back(syncDbId);
                break;
            case SyncControlState::Disabled:
            case SyncControlState::Pending:
                break;
        }
    }

    // The selected synchronization follows the blocking error applied at once on a selection change.
    const auto selectedSyncDbId = static_cast<SyncDbId>(_selectionStore.currentSyncDbId());
    if (selectedSyncDbId != 0) {
        snapshot.selectedState = syncControlStateOf(selectedSyncDbId, _projection.currentBlockingError().has_value());
    }

    return snapshot;
}

SyncControlState SyncControlController::syncControlStateOf(const SyncDbId syncDbId, const bool blocked) const {
    const auto runtimeInfo = _appCache.syncRuntimeInfo(syncDbId);
    const auto runtimeStatus = runtimeInfo.has_value() ? std::make_optional(runtimeInfo->status) : std::nullopt;
    const bool actionPending = _syncService.isStartSyncPending(syncDbId) || _syncService.isStopSyncPending(syncDbId);
    return resolveSyncControlState(runtimeStatus, actionPending, blocked);
}

void SyncControlController::refresh() {
    Snapshot snapshot = computeSnapshot();
    if (snapshot == _snapshot) {
        return;
    }

    _snapshot = std::move(snapshot);
    emit changed();
}

} // namespace KDC
