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

#pragma once

#include "app/settings/advancedsettingscontroller.h"
#include "app/settings/drivemanagementcontroller.h"
#include "app/settings/fileexclusioncontroller.h"
#include "app/settings/generalsettingscontroller.h"
#include "app/settings/networksettingscontroller.h"
#include "app/settings/settingssyncactivationcontroller.h"
#include "libcommon/utility/types.h"

#include <QObject>

#include <cstdint>
#include <optional>

namespace KDC {

/**
 * Process-long composition facade exposed to the independent Settings window.
 *
 * TODO Pass these controllers to SettingsWindow as initial properties, like SettingsUserService, and keep only the
 * window requests here.
 */
class SettingsWindowController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(GeneralSettingsController *general READ generalController CONSTANT)
        Q_PROPERTY(AdvancedSettingsController *advanced READ advancedController CONSTANT)
        Q_PROPERTY(FileExclusionController *fileExclusions READ fileExclusionController CONSTANT)
        Q_PROPERTY(NetworkSettingsController *network READ networkController CONSTANT)
        Q_PROPERTY(SettingsSyncActivationController *syncActivation READ syncActivationController CONSTANT)
        Q_PROPERTY(DriveManagementController *driveManagement READ driveManagementController CONSTANT)

    public:
        SettingsWindowController(GeneralSettingsController &general, AdvancedSettingsController &advanced,
                                 FileExclusionController &fileExclusions, NetworkSettingsController &network,
                                 SettingsSyncActivationController &syncActivation, DriveManagementController &driveManagement,
                                 QObject *parent = nullptr);

        [[nodiscard]] GeneralSettingsController *generalController() { return &_generalController; }
        [[nodiscard]] AdvancedSettingsController *advancedController() { return &_advancedController; }
        [[nodiscard]] FileExclusionController *fileExclusionController() { return &_fileExclusionController; }
        [[nodiscard]] NetworkSettingsController *networkController() { return &_networkController; }
        [[nodiscard]] SettingsSyncActivationController *syncActivationController() { return &_syncActivationController; }
        [[nodiscard]] DriveManagementController *driveManagementController() { return &_driveManagementController; }

        Q_INVOKABLE void requestOpen() { emit openRequested(); }
        Q_INVOKABLE void requestAccountConnection() { emit accountConnectionRequested(); }
        Q_INVOKABLE void requestQuitConfirmation() { emit quitConfirmationRequested(); }
        // Opens Settings on the file exclusions page (Advanced), for the errors page.
        void requestOpenFileExclusions() {
            emit openRequested();
            emit fileExclusionsRequested();
        }
        // Opens Settings on the update button, for an update notification.
        void requestOpenUpdate() {
            emit openRequested();
            emit updateRequested();
        }
        // Opens Settings on the synchronizations, where a new synchronization is created, for the errors page.
        void requestOpenAccounts() {
            emit openRequested();
            emit accountsRequested();
        }

        /**
         * Open the Settings window on the card of a user, on the management page of a drive, on one advanced
         * synchronization of a drive, or on the "Manage synchronization" page of a synchronization. The target stays pending
         * until the window takes it, because the window may not exist yet when the request is made.
         */
        void requestOpenUser(UserDbId userDbId);
        void requestOpenDrive(DriveDbId driveDbId);
        void requestOpenAdvancedSync(DriveDbId driveDbId, SyncDbId syncDbId);
        void requestOpenSyncManagement(DriveDbId driveDbId, SyncDbId syncDbId, bool advancedSync);

        /**
         * Emits the show signal of the pending target, if any, and clears it.
         */
        Q_INVOKABLE void takePendingTarget();

        void refreshUpdates() const;

    signals:
        void openRequested();
        void accountConnectionRequested();
        void fileExclusionsRequested();
        void updateRequested();
        void accountsRequested();
        void targetPending();
        // Ids are qint64 at the QML boundary.
        void userShowRequested(qint64 userDbId);
        void driveShowRequested(qint64 driveDbId);
        void advancedSyncShowRequested(qint64 driveDbId, qint64 syncDbId);
        void syncManagementShowRequested(qint64 driveDbId, qint64 syncDbId, bool advancedSync);
        void quitConfirmationRequested();

    private:
        GeneralSettingsController &_generalController;
        AdvancedSettingsController &_advancedController;
        FileExclusionController &_fileExclusionController;
        NetworkSettingsController &_networkController;
        SettingsSyncActivationController &_syncActivationController;
        DriveManagementController &_driveManagementController;
        enum class TargetKind : uint8_t {
            User,
            Drive,
            AdvancedSync,
            SyncManagement,
        };

        struct Target {
                TargetKind kind{TargetKind::User};
                UserDbId userDbId{0};
                DriveDbId driveDbId{0};
                SyncDbId syncDbId{0};
                bool advancedSync{false};
        };

        std::optional<Target> _pendingTarget;

        void requestOpenTarget(const Target &target);
};

} // namespace KDC
