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

#pragma once

#include "app/cache/appcache.h"
#include "app/cache/onboardingstate.h"
#include "app/onboarding/availabledrivesmodel.h"
#include "app/onboarding/onboardingflowcontroller.h"
#include "app/services/userservice.h"

#include <QAbstractItemModel>
#include <QObject>
#include <QString>

namespace KDC {

/**
 * QML-facing controller for the onboarding drive-selection screen.
 *
 * It owns presentation state and delegates durable selections, backend work, and navigation to their respective
 * collaborators.
 */
class DriveSelectionController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QAbstractItemModel *drivesModel READ drivesModel CONSTANT)
        Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
        Q_PROPERTY(bool empty READ empty NOTIFY emptyChanged)
        Q_PROPERTY(bool loadFailed READ loadFailed NOTIFY loadFailedChanged)
        Q_PROPERTY(qint32 selectedCount READ selectedCount NOTIFY selectedCountChanged)
        Q_PROPERTY(qint32 configuredCount READ configuredCount NOTIFY configuredCountChanged)
        Q_PROPERTY(bool canContinue READ canContinue NOTIFY canContinueChanged)
        Q_PROPERTY(bool canOpenAdvancedSettings READ canOpenAdvancedSettings NOTIFY canOpenAdvancedSettingsChanged)
        Q_PROPERTY(QString userName READ userName NOTIFY userChanged)
        Q_PROPERTY(QString userEmail READ userEmail NOTIFY userChanged)
        Q_PROPERTY(QString userAvatarSource READ userAvatarSource NOTIFY userChanged)

    public:
        explicit DriveSelectionController(const AppCache &cache, OnboardingState &onboardingState, UserService &userService,
                                          OnboardingFlowController &flowController, QObject *parent = nullptr);

        [[nodiscard]] QAbstractItemModel *drivesModel() { return &_drivesModel; }
        [[nodiscard]] AvailableDrivesModel *availableDrivesModel() { return &_drivesModel; }
        [[nodiscard]] bool loading() const;
        [[nodiscard]] bool empty() const;
        [[nodiscard]] bool loadFailed() const { return _loadFailed; }
        [[nodiscard]] qint32 selectedCount() const;
        [[nodiscard]] qint32 configuredCount() const;
        [[nodiscard]] bool canContinue() const;
        [[nodiscard]] bool canOpenAdvancedSettings() const;
        [[nodiscard]] QString userName() const;
        [[nodiscard]] QString userEmail() const;
        [[nodiscard]] QString userAvatarSource() const;

        Q_INVOKABLE void reload();
        Q_INVOKABLE void requestAdvancedSettings() const;
        Q_INVOKABLE void continueOnboarding() const;
        Q_INVOKABLE void openDriveOffers() const;
        Q_INVOKABLE void startForFree() const;

    signals:
        void loadingChanged();
        void emptyChanged();
        void loadFailedChanged();
        void selectedCountChanged();
        void configuredCountChanged();
        void canContinueChanged();
        void canOpenAdvancedSettingsChanged();
        void userChanged();

    private:
        [[nodiscard]] UserDbId selectedUserDbId() const;
        void setLoadFailed(bool loadFailed);

        const AppCache &_cache;
        OnboardingState &_onboardingState;
        UserService &_userService;
        OnboardingFlowController &_flowController;
        AvailableDrivesModel _drivesModel;
        bool _loadFailed{false};
};

} // namespace KDC
