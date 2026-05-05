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
#include "app/cache/mainselectionstore.h"

#include <QAbstractListModel>

#include <vector>

namespace KDC {

/**
 * QML-facing drive context list model for settings and drive-level navigation.
 *
 * Role: expose AppCache::driveContexts() and derive current-drive state from MainSelectionStore.
 * Non-role: own drive data, store independent drive selection, or perform protocol requests.
 */
class DriveListModel : public QAbstractListModel {
        Q_OBJECT

    public:
        enum Roles : int {
            DbIdRole = Qt::UserRole + 1,
            IdRole,
            AccountDbIdRole,
            AccountIdRole,
            AccountNameRole,
            UserDbIdRole,
            UserIdRole,
            UserNameRole,
            UserEmailRole,
            NameRole,
            SizeRole,
            UsedSizeRole,
            ColorRole,
            NotificationsRole,
            AdminRole,
            MaintenanceRole,
            LockedRole,
            AccessDeniedRole,
            PackIdRole,
            PackNameRole,
            PackDisplayNameRole,
            PackIsFreeRole,
            SyncCountRole,
            CurrentRole
        };
        Q_ENUM(Roles)

        explicit DriveListModel(AppCache &appCache, MainSelectionStore &mainSelectionStore, QObject *parent = nullptr);

        [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        Q_INVOKABLE void selectFirstSyncForDrive(qint64 driveDbId);

    private:
        void resetModel();
        void emitCurrentRoleChanged();
        [[nodiscard]] const DriveContext *contextForIndex(const QModelIndex &index) const;
        [[nodiscard]] DriveDbId currentDriveDbId() const;

        AppCache &_appCache;
        MainSelectionStore &_mainSelectionStore;
        std::vector<DriveContext> _contexts;
};

} // namespace KDC
