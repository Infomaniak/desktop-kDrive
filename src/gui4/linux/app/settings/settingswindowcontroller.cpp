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

#include "settingswindowcontroller.h"

#include <utility>

namespace KDC {

SettingsWindowController::SettingsWindowController(GeneralSettingsController &general, AdvancedSettingsController &advanced,
                                                   FileExclusionController &fileExclusions, NetworkSettingsController &network,
                                                   SettingsSyncActivationController &syncActivation,
                                                   DriveManagementController &driveManagement, QObject *const parent) :
    QObject(parent),
    _generalController(general),
    _advancedController(advanced),
    _fileExclusionController(fileExclusions),
    _networkController(network),
    _syncActivationController(syncActivation),
    _driveManagementController(driveManagement) {}

void SettingsWindowController::refreshUpdates() const {
    _generalController.refreshUpdates();
}

void SettingsWindowController::requestOpenUser(const UserDbId userDbId) {
    requestOpenTarget({.kind = TargetKind::User, .userDbId = userDbId});
}

void SettingsWindowController::requestOpenDrive(const DriveDbId driveDbId) {
    requestOpenTarget({.kind = TargetKind::Drive, .driveDbId = driveDbId});
}

void SettingsWindowController::requestOpenAdvancedSync(const DriveDbId driveDbId, const SyncDbId syncDbId) {
    requestOpenTarget({.kind = TargetKind::AdvancedSync, .driveDbId = driveDbId, .syncDbId = syncDbId});
}

void SettingsWindowController::requestOpenSyncManagement(const DriveDbId driveDbId, const SyncDbId syncDbId,
                                                         const bool advancedSync) {
    requestOpenTarget(
            {.kind = TargetKind::SyncManagement, .driveDbId = driveDbId, .syncDbId = syncDbId, .advancedSync = advancedSync});
}

void SettingsWindowController::takePendingTarget() {
    const auto target = std::exchange(_pendingTarget, std::nullopt);
    if (!target.has_value()) {
        return;
    }

    switch (target->kind) {
        case TargetKind::User:
            emit userShowRequested(target->userDbId);
            return;
        case TargetKind::Drive:
            emit driveShowRequested(target->driveDbId);
            return;
        case TargetKind::AdvancedSync:
            emit advancedSyncShowRequested(target->driveDbId, target->syncDbId);
            return;
        case TargetKind::SyncManagement:
            emit syncManagementShowRequested(target->driveDbId, target->syncDbId, target->advancedSync);
            return;
    }
}

// An existing window takes the target on targetPending(), before being shown, so that it animates the navigation only
// when it was already displayed. A window created by openRequested() takes the target itself.
void SettingsWindowController::requestOpenTarget(const Target &target) {
    _pendingTarget = target;
    emit targetPending();
    emit openRequested();
}

} // namespace KDC
