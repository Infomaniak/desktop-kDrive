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
 * QML-facing sync context list model for the Linux v4 main flow.
 *
 * Role: expose AppCache::syncContexts() with current-sync selection state from MainSelectionStore.
 * Non-role: own cache data, mutate selection rules, or perform protocol requests.
 */
class SyncListModel : public QAbstractListModel {
        Q_OBJECT

    public:
        enum Roles : qint32 {
            DbIdRole = Qt::UserRole + 1,
            DriveDbIdRole,
            DriveIdRole,
            DriveNameRole,
            AccountDbIdRole,
            AccountIdRole,
            AccountNameRole,
            UserDbIdRole,
            UserIdRole,
            UserNameRole,
            UserEmailRole,
            LocalPathRole,
            TargetPathRole,
            TargetNodeIdRole,
            SupportVfsRole,
            VirtualFileModeRole,
            NavigationPaneClsidRole,
            ErrorCountRole,
            HasErrorRole,
            LatestErrorDbIdRole,
            LatestErrorTimeRole,
            LatestErrorLevelRole,
            LatestErrorExitCodeRole,
            LatestErrorPathRole,
            SelectedRole
        };
        Q_ENUM(Roles)

        explicit SyncListModel(AppCache &appCache, MainSelectionStore &mainSelectionStore, QObject *parent = nullptr);

        [[nodiscard]] qint32 rowCount(const QModelIndex &parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex &index, qint32 role = Qt::DisplayRole) const override;
        [[nodiscard]] QHash<qint32, QByteArray> roleNames() const override;

        Q_INVOKABLE void selectSync(qint64 syncDbId);

    private:
        void resetModel();
        void emitSelectionRoleChanged();
        [[nodiscard]] const SyncContext *contextForIndex(const QModelIndex &index) const;

        AppCache &_appCache;
        MainSelectionStore &_mainSelectionStore;
        std::vector<SyncContext> _contexts;
};

} // namespace KDC
