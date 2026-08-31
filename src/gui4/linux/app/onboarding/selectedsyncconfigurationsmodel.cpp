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

#include "selectedsyncconfigurationsmodel.h"

namespace KDC {

SelectedSyncConfigurationsModel::SelectedSyncConfigurationsModel(QObject *const parent) :
    QAbstractListModel(parent) {}

int SelectedSyncConfigurationsModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(_rows.size());
}

QVariant SelectedSyncConfigurationsModel::data(const QModelIndex &index, const int role) const {
    if (!index.isValid() || index.row() < 0 || static_cast<std::size_t>(index.row()) >= _rows.size()) return {};
    const auto &row = _rows[static_cast<std::size_t>(index.row())];
    switch (role) {
        case DriveNameRole:
        case Qt::DisplayRole:
            return row.driveName;
        case DriveColorRole:
            return row.driveColor;
        case LocalPathRole:
            return row.localPath;
        case CustomFolderRole:
            return row.customFolder;
        case CustomSelectionRole:
            return row.customSelection;
        default:
            return {};
    }
}

QHash<int, QByteArray> SelectedSyncConfigurationsModel::roleNames() const {
    return {{DriveNameRole, "driveName"},
            {DriveColorRole, "driveColor"},
            {LocalPathRole, "localPath"},
            {CustomFolderRole, "customFolder"},
            {CustomSelectionRole, "customSelection"}};
}

void SelectedSyncConfigurationsModel::setRows(std::vector<SelectedSyncConfigurationRow> rows) {
    beginResetModel();
    _rows = std::move(rows);
    endResetModel();
}

} // namespace KDC
