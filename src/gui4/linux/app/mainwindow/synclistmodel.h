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

#include "app/cache/mainselectionstore.h"

#include <QAbstractListModel>

#include <vector>

namespace KDC {

/**
 * QML list adapter for configured synchronizations.
 *
 * Rows are projections of AppCache sync contexts. MainSelectionStore remains the selection authority; this model only
 * exposes the selected row and role for presentation.
 */
class SyncListModel final : public QAbstractListModel {
        Q_OBJECT
        Q_PROPERTY(qint32 selectedRow READ selectedRow NOTIFY selectedRowChanged)

    public:
        enum Role {
            SyncDbIdRole = Qt::UserRole + 1,
            DriveNameRole,
            DriveColorRole,
            ErrorCountRole,
            SelectedRole,
        };
        Q_ENUM(Role)

        explicit SyncListModel(const AppCache &cache, MainSelectionStore &selectionStore, QObject *parent = nullptr);

        [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
        [[nodiscard]] qint32 selectedRow() const;

    signals:
        void selectedRowChanged();

    private:
        void rebuild();
        void handleSelectionChanged();
        [[nodiscard]] qint32 rowForSyncDbId(SyncDbId syncDbId) const;

        const AppCache &_cache;
        MainSelectionStore &_selectionStore;
        std::vector<SyncContext> _contexts;
        SyncDbId _selectedSyncDbId{0};
};

} // namespace KDC
