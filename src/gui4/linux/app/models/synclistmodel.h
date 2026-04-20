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

#include <QAbstractListModel>

namespace KDC {

/**
 * QML-facing list model for syncs stored in AppCache.
 *
 * This model currently uses coarse-grained cache notifications and performs
 * safe model resets on aggregate updates.
 */
class SyncListModel : public QAbstractListModel {
        Q_OBJECT

    public:
        enum Roles : int {
            DbIdRole = Qt::UserRole + 1,
            DriveDbIdRole,
            LocalPathRole,
            TargetPathRole,
            TargetNodeIdRole,
            SupportVfsRole,
            VirtualFileModeRole,
            NavigationPaneClsidRole,
            SelectedRole
        };
        Q_ENUM(Roles)

        explicit SyncListModel(AppCache &appCache, QObject *parent = nullptr);

        [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    private:
        void resetModel();
        void emitSelectionRoleChanged();

        AppCache &_appCache;
};

} // namespace KDC
