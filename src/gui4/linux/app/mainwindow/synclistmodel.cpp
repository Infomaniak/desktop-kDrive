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

#include "app/mainwindow/synclistmodel.h"

#include <QColor>

#include <cstddef>

namespace KDC {

namespace {

const QColor defaultDriveColor{QStringLiteral("#0098FF")};

} // namespace

SyncListModel::SyncListModel(const AppCache &cache, MainSelectionStore &selectionStore, QObject *const parent) :
    QAbstractListModel(parent),
    _cache(cache),
    _selectionStore(selectionStore) {
    (void) connect(&_cache, &AppCache::usersChanged, this, &SyncListModel::rebuild);
    (void) connect(&_cache, &AppCache::accountsChanged, this, &SyncListModel::rebuild);
    (void) connect(&_cache, &AppCache::drivesChanged, this, &SyncListModel::rebuild);
    (void) connect(&_cache, &AppCache::syncsChanged, this, &SyncListModel::rebuild);
    (void) connect(&_cache, &AppCache::syncErrorsChanged, this, &SyncListModel::rebuild);
    (void) connect(&_selectionStore, &MainSelectionStore::currentSyncDbIdChanged, this, &SyncListModel::handleSelectionChanged);

    rebuild();
}

int SyncListModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(_contexts.size());
}

QVariant SyncListModel::data(const QModelIndex &index, const int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    const auto &context = _contexts[static_cast<std::size_t>(index.row())];
    switch (role) {
        case SyncDbIdRole:
            return static_cast<qint64>(context.syncInfo.dbId());
        case DriveNameRole:
        case Qt::DisplayRole:
            return QString::fromStdString(context.drive.name());
        case DriveColorRole: {
            const QColor color{QString::fromStdString(context.drive.color())};
            return color.isValid() ? color : defaultDriveColor;
        }
        case ErrorCountRole:
            return static_cast<qint32>(context.errorInfoList.size());
        case SelectedRole:
            return context.syncInfo.dbId() == _selectedSyncDbId;
        default:
            return {};
    }
}

QHash<int, QByteArray> SyncListModel::roleNames() const {
    return {
            {SyncDbIdRole, "syncDbId"},     {DriveNameRole, "driveName"}, {DriveColorRole, "driveColor"},
            {ErrorCountRole, "errorCount"}, {SelectedRole, "isSelected"},
    };
}

qint32 SyncListModel::selectedRow() const {
    return rowForSyncDbId(_selectedSyncDbId);
}

void SyncListModel::rebuild() {
    const auto previousSelectedRow = selectedRow();

    beginResetModel();
    _contexts = _cache.syncContexts();
    _selectedSyncDbId = static_cast<SyncDbId>(_selectionStore.currentSyncDbId());
    endResetModel();

    if (selectedRow() != previousSelectedRow) {
        emit selectedRowChanged();
    }
}

void SyncListModel::handleSelectionChanged() {
    const auto previousSelectedRow = selectedRow();
    _selectedSyncDbId = static_cast<SyncDbId>(_selectionStore.currentSyncDbId());
    const auto currentSelectedRow = selectedRow();

    if (previousSelectedRow >= 0) {
        emit dataChanged(index(previousSelectedRow, 0), index(previousSelectedRow, 0), {SelectedRole});
    }
    if (currentSelectedRow >= 0 && currentSelectedRow != previousSelectedRow) {
        emit dataChanged(index(currentSelectedRow, 0), index(currentSelectedRow, 0), {SelectedRole});
    }
    if (currentSelectedRow != previousSelectedRow) {
        emit selectedRowChanged();
    }
}

qint32 SyncListModel::rowForSyncDbId(const SyncDbId syncDbId) const {
    if (syncDbId == 0) {
        return -1;
    }
    for (qint32 row = 0; row < static_cast<qint32>(_contexts.size()); ++row) {
        if (_contexts[static_cast<std::size_t>(row)].syncInfo.dbId() == syncDbId) {
            return row;
        }
    }
    return -1;
}

} // namespace KDC
