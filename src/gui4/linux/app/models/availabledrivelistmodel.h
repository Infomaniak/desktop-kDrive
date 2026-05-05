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
#include "app/cache/cachetypes.h"
#include "app/cache/onboardingstate.h"

#include <QAbstractListModel>

#include <optional>
#include <vector>

namespace KDC {

/**
 * QML-facing available-drive list model for onboarding.
 *
 * Role: expose per-user available drive contexts and OnboardingState selection/pending config state.
 * Non-role: own available drives, store configured drives, or perform sync creation requests.
 */
class AvailableDriveListModel : public QAbstractListModel {
        Q_OBJECT

    public:
        enum Roles : int {
            UserDbIdRole = Qt::UserRole + 1,
            UserIdRole,
            UserNameRole,
            UserEmailRole,
            AccountIdRole,
            AccountNameRole,
            AccountDbIdRole,
            HasConfiguredAccountRole,
            DriveIdRole,
            NameRole,
            ColorRole,
            AlreadyConfiguredRole,
            ConfiguredDriveDbIdRole,
            ConfiguredDriveNameRole,
            SelectedRole,
            HasPendingConfigRole,
            PendingLocalPathRole,
            PendingTargetPathRole,
            PendingTargetNodeIdRole,
            PendingSupportVfsRole,
            PendingVirtualFileModeRole
        };
        Q_ENUM(Roles)

        explicit AvailableDriveListModel(AppCache &appCache, OnboardingState &onboardingState, QObject *parent = nullptr);

        [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        Q_INVOKABLE void selectUser(qint64 userDbId);
        Q_INVOKABLE void clearSelectedUser();
        Q_INVOKABLE void toggleAvailableDrive(qint64 accountId, qint64 driveId);
        Q_INVOKABLE void setPendingSyncConfig(qint64 accountId, qint64 driveId, const QString &localPath,
                                              const QString &targetPath, const QString &targetNodeId, bool supportVfs,
                                              qint32 virtualFileMode);
        Q_INVOKABLE void clearPendingSyncConfig(qint64 accountId, qint64 driveId);

    private:
        void resetModel();
        void emitSelectionRolesChanged();
        void emitPendingConfigRolesChanged();
        void handleAvailableDrivesChanged(UserDbId userDbId);
        [[nodiscard]] const AvailableDriveContext *contextForIndex(const QModelIndex &index) const;
        [[nodiscard]] AvailableDriveKey keyForContext(const AvailableDriveContext &context) const;
        [[nodiscard]] std::optional<AvailableDriveKey> keyForIds(qint64 accountId, qint64 driveId) const;

        AppCache &_appCache;
        OnboardingState &_onboardingState;
        std::vector<AvailableDriveContext> _contexts;
};

} // namespace KDC
