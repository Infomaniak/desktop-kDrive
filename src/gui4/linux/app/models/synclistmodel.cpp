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

#include "synclistmodel.h"

namespace KDC {

SyncListModel::SyncListModel(AppCache &appCache, QObject *const parent) :
    QAbstractListModel(parent),
    _appCache(appCache) {
    (void) connect(&_appCache, &AppCache::syncsChanged, this, &SyncListModel::resetModel);
    (void) connect(&_appCache, &AppCache::selectedSyncDbIdChanged, this, &SyncListModel::emitSelectionRoleChanged);
}

int SyncListModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(_appCache.syncs().size());
}

QVariant SyncListModel::data(const QModelIndex &index, const int role) const {
    if (!index.isValid()) {
        return {};
    }

    const int row = index.row();
    const auto &syncs = _appCache.syncs();
    if (row < 0 || row >= static_cast<int>(syncs.size())) {
        return {};
    }

    const auto &sync = syncs[static_cast<size_t>(row)];
    switch (role) {
        case DbIdRole:
            return static_cast<qint64>(sync.dbId());
        case DriveDbIdRole:
            return static_cast<qint64>(sync.driveDbId());
        case LocalPathRole:
            return sync.localPath();
        case TargetPathRole:
            return sync.targetPath();
        case TargetNodeIdRole:
            return sync.targetNodeId();
        case SupportVfsRole:
            return sync.supportVfs();
        case VirtualFileModeRole:
            return static_cast<qint32>(sync.virtualFileMode());
        case NavigationPaneClsidRole:
            return sync.navigationPaneClsid();
        case SelectedRole:
            return static_cast<qint64>(sync.dbId()) == _appCache.selectedSyncDbId();
        default:
            return {};
    }
}

QHash<int, QByteArray> SyncListModel::roleNames() const {
    static const QHash<int, QByteArray> roles{{DbIdRole, "dbId"},
                                              {DriveDbIdRole, "driveDbId"},
                                              {LocalPathRole, "localPath"},
                                              {TargetPathRole, "targetPath"},
                                              {TargetNodeIdRole, "targetNodeId"},
                                              {SupportVfsRole, "supportVfs"},
                                              {VirtualFileModeRole, "virtualFileMode"},
                                              {NavigationPaneClsidRole, "navigationPaneClsid"},
                                              {SelectedRole, "selected"}};
    return roles;
}

void SyncListModel::resetModel() {
    beginResetModel();
    endResetModel();
}

void SyncListModel::emitSelectionRoleChanged() {
    const auto size = _appCache.syncs().size();
    if (size == 0) {
        return;
    }

    const QModelIndex first = index(0, 0);
    const QModelIndex last = index(static_cast<int>(size) - 1, 0);
    emit dataChanged(first, last, {SelectedRole});
}

} // namespace KDC
