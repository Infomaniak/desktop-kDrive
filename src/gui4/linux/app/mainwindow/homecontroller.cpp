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

#include "homecontroller.h"

#include "app/cache/appcache.h"
#include "app/cache/mainselectionstore.h"
#include "app/mainwindow/homestateresolver.h"
#include "app/mainwindow/networkstatusobserver.h"
#include "app/navigation/approuter.h"
#include "app/services/syncservice.h"
#include "app/systraycontroller.h"

#include <QLoggingCategory>

#include <algorithm>

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcHomeController, "gui.v4.homecontroller", QtInfoMsg)
} // namespace

HomeController::HomeController(AppCache &appCache, MainSelectionStore &mainSelectionStore, SyncService &syncService,
                               AppRouter &appRouter, SystemTrayController &systemTrayController,
                               NetworkStatusObserver &networkStatusObserver, QObject *const parent) :
    QObject(parent),
    _appCache(appCache),
    _mainSelectionStore(mainSelectionStore),
    _syncService(syncService),
    _appRouter(appRouter),
    _systemTrayController(systemTrayController),
    _networkStatusObserver(networkStatusObserver) {
    (void) connect(&_mainSelectionStore, &MainSelectionStore::currentContextChanged, this, &HomeController::homeChanged);
    (void) connect(&_mainSelectionStore, &MainSelectionStore::currentSyncStatusChanged, this, &HomeController::homeChanged);
    (void) connect(&_appCache, &AppCache::usersChanged, this, &HomeController::homeChanged);
    (void) connect(&_appCache, &AppCache::syncsChanged, this, &HomeController::homeChanged);
    (void) connect(&_appCache, &AppCache::syncErrorsChanged, this, &HomeController::homeChanged);
    (void) connect(&_syncService, &SyncService::syncActionPendingChanged, this, [this](const qint64 syncDbId) {
        if (syncDbId == currentSyncDbId()) {
            emit homeChanged();
        }
    });
    (void) connect(&_systemTrayController, &SystemTrayController::trayModeActiveChanged, this, &HomeController::homeChanged);
    (void) connect(&_networkStatusObserver, &NetworkStatusObserver::offlineChanged, this, &HomeController::homeChanged);
}

HomeController::HomeStatus HomeController::status() const {
    const auto context = _mainSelectionStore.currentSyncContext();
    return resolveHomeStatus(context.has_value(), _networkStatusObserver.offline(), currentRuntimeStatus());
}

HomeController::PrimaryAction HomeController::primaryAction() const {
    switch (status()) {
        case HomeStatus::UpToDate:
            if (errorCount() > 0) {
                return PrimaryAction::ShowActivities;
            }
            return _systemTrayController.trayModeActive() ? PrimaryAction::HideWindow : PrimaryAction::None;
        case HomeStatus::Syncing:
            return PrimaryAction::ShowActivities;
        case HomeStatus::Paused:
            return syncControlState() == SyncControlState::Resume ? PrimaryAction::ResumeSync : PrimaryAction::None;
        case HomeStatus::SetupRequired:
            return hasConnectedUser() ? PrimaryAction::ConfigureSync : PrimaryAction::SignIn;
        case HomeStatus::Loading:
        case HomeStatus::Offline:
            return PrimaryAction::None;
    }
    return PrimaryAction::None;
}

HomeController::SyncControlState HomeController::syncControlState() const {
    if (!hasCurrentDrive() || _networkStatusObserver.offline()) {
        return SyncControlState::Disabled;
    }
    if (syncActionPending()) {
        return SyncControlState::Pending;
    }

    const auto runtimeStatus = currentRuntimeStatus();
    if (!runtimeStatus.has_value()) {
        return SyncControlState::Disabled;
    }

    switch (*runtimeStatus) {
        case SyncStatus::Running:
        case SyncStatus::Idle:
            return SyncControlState::Pause;
        case SyncStatus::Paused:
        case SyncStatus::Stopped:
        case SyncStatus::Error:
            return SyncControlState::Resume;
        case SyncStatus::Starting:
        case SyncStatus::PauseAsked:
        case SyncStatus::StopAsked:
            return SyncControlState::Pending;
        case SyncStatus::Undefined:
        case SyncStatus::EnumEnd:
            return SyncControlState::Disabled;
    }
    return SyncControlState::Disabled;
}

QString HomeController::firstName() const {
    const auto context = _mainSelectionStore.currentSyncContext();
    return context.has_value() ? QString::fromStdString(context->userDisplayInfo.firstName()) : QString{};
}

QString HomeController::driveName() const {
    const auto context = _mainSelectionStore.currentSyncContext();
    return context.has_value() ? QString::fromStdString(context->drive.name()) : QString{};
}

QString HomeController::avatarSource() const {
    const auto context = _mainSelectionStore.currentSyncContext();
    return context.has_value() ? context->userDisplayInfo.avatarSource() : QString{};
}

int32_t HomeController::errorCount() const {
    const auto context = _mainSelectionStore.currentSyncContext();
    return context.has_value() ? static_cast<int32_t>(context->errors.size()) : 0;
}

bool HomeController::hasCurrentDrive() const {
    return _mainSelectionStore.currentSyncContext().has_value();
}

bool HomeController::hasConnectedUser() const {
    const auto users = _appCache.users();
    return std::ranges::any_of(users, [](const User &user) { return user.connected(); });
}

void HomeController::triggerPrimaryAction() {
    switch (primaryAction()) {
        case PrimaryAction::HideWindow:
            _systemTrayController.hideMainWindow();
            return;
        case PrimaryAction::ShowActivities:
            _appRouter.showActivities();
            return;
        case PrimaryAction::ResumeSync:
            _syncService.startSync(currentSyncDbId());
            return;
        case PrimaryAction::SignIn:
        case PrimaryAction::ConfigureSync:
            emit setupRequested();
            return;
        case PrimaryAction::None:
            return;
    }
}

void HomeController::toggleSync() {
    const auto syncDbId = currentSyncDbId();
    if (syncDbId == 0) {
        qCWarning(lcHomeController) << "Sync control ignored: no synchronization is selected";
        return;
    }

    switch (syncControlState()) {
        case SyncControlState::Pause:
            _syncService.stopSync(syncDbId);
            return;
        case SyncControlState::Resume:
            _syncService.startSync(syncDbId);
            return;
        case SyncControlState::Disabled:
        case SyncControlState::Pending:
            qCDebug(lcHomeController) << "Sync control ignored in current presentation state | syncDbId:" << syncDbId;
            return;
    }
}

std::optional<SyncStatus> HomeController::currentRuntimeStatus() const {
    const auto runtimeInfo = _mainSelectionStore.currentSyncRuntimeInfo();
    return runtimeInfo.has_value() ? std::make_optional(runtimeInfo->status) : std::nullopt;
}

qint64 HomeController::currentSyncDbId() const {
    return _mainSelectionStore.currentSyncDbId();
}

bool HomeController::syncActionPending() const {
    const auto syncDbId = currentSyncDbId();
    return syncDbId != 0 && (_syncService.isStartSyncPending(syncDbId) || _syncService.isStopSyncPending(syncDbId));
}

} // namespace KDC
