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
#include "app/settings/fileexclusioncontroller.h"
#include "app/settings/generalsettingscontroller.h"
#include "app/settings/networksettingscontroller.h"
#include "app/settings/settingssyncactivationcontroller.h"

#include <QObject>

namespace KDC {

/** Process-long composition facade exposed to the independent Settings window. */
class SettingsWindowController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(GeneralSettingsController *general READ generalController CONSTANT)
        Q_PROPERTY(AdvancedSettingsController *advanced READ advancedController CONSTANT)
        Q_PROPERTY(FileExclusionController *fileExclusions READ fileExclusionController CONSTANT)
        Q_PROPERTY(NetworkSettingsController *network READ networkController CONSTANT)
        Q_PROPERTY(SettingsSyncActivationController *syncActivation READ syncActivationController CONSTANT)

    public:
        SettingsWindowController(GeneralSettingsController &general, AdvancedSettingsController &advanced,
                                 FileExclusionController &fileExclusions, NetworkSettingsController &network,
                                 SettingsSyncActivationController &syncActivation, QObject *parent = nullptr);

        [[nodiscard]] GeneralSettingsController *generalController() { return &_generalController; }
        [[nodiscard]] AdvancedSettingsController *advancedController() { return &_advancedController; }
        [[nodiscard]] FileExclusionController *fileExclusionController() { return &_fileExclusionController; }
        [[nodiscard]] NetworkSettingsController *networkController() { return &_networkController; }
        [[nodiscard]] SettingsSyncActivationController *syncActivationController() { return &_syncActivationController; }

        Q_INVOKABLE void requestOpen() { emit openRequested(); }
        Q_INVOKABLE void requestAccountConnection() { emit accountConnectionRequested(); }

        void refreshUpdates() const;

    signals:
        void openRequested();
        void accountConnectionRequested();

    private:
        GeneralSettingsController &_generalController;
        AdvancedSettingsController &_advancedController;
        FileExclusionController &_fileExclusionController;
        NetworkSettingsController &_networkController;
        SettingsSyncActivationController &_syncActivationController;
};

} // namespace KDC
