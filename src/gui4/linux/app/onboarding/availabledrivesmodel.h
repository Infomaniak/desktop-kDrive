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

#include "app/cache/appcache.h"
#include "app/cache/onboardingstate.h"
#include "app/onboarding/onboardingflowcontroller.h"
#include "app/services/userservice.h"

#include <QAbstractListModel>
#include <QLoggingCategory>
#include <QString>

#include <vector>

Q_DECLARE_LOGGING_CATEGORY(lcAvailableDrivesModel)

namespace KDC {

/**
 * QML adapter for the onboarding available-drive selection step.
 *
 * Role: expose selected-user available drives, disabled/configured state, and step-level UI actions to QML.
 * Non-role: create syncs or own durable onboarding selections; those stay in services and OnboardingState.
 */
class AvailableDrivesModel final : public QAbstractListModel {
        Q_OBJECT
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
        enum Role : uint16_t {
            UserDbIdRole = Qt::UserRole + 1,
            AccountIdRole,
            DriveIdRole,
            NameRole,
            AccountNameRole,
            ColorRole,
            SelectedRole,
            AlreadyConfiguredRole,
            EnabledRole,
            TooltipRole,
        };
        Q_ENUM(Role)

        explicit AvailableDrivesModel(AppCache &cache, OnboardingState &onboardingState, UserService &userService,
                                      OnboardingFlowController &flowController, QObject *parent = nullptr);

        [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

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
        Q_INVOKABLE void toggleDrive(qint32 row);
        Q_INVOKABLE void changeUser();
        Q_INVOKABLE void requestAdvancedSettings();
        Q_INVOKABLE void continueOnboarding();
        Q_INVOKABLE void openDriveOffers();
        Q_INVOKABLE void startForFree();

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
        [[nodiscard]] AvailableDriveKey keyAt(qint32 row) const;
        [[nodiscard]] QString accountNameForContext(const AvailableDriveContext &context) const;
        [[nodiscard]] bool isRowSelected(qint32 row) const;
        [[nodiscard]] bool hasSelectedDrives() const;
        [[nodiscard]] UserDbId selectedUserDbId() const;
        void rebuild();
        void emitSelectionDependentChanges();
        void emitListDependentChanges();
        void setLoadFailed(bool loadFailed);

        AppCache &_cache;
        OnboardingState &_onboardingState;
        UserService &_userService;
        OnboardingFlowController &_flowController;
        std::vector<AvailableDriveContext> _contexts;
        bool _loadFailed{false};
};

} // namespace KDC
