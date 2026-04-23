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

#include "drivelistmodel.h"

namespace KDC {

DriveListModel::DriveListModel(AppCache &appCache, QObject *const parent) :
    QAbstractListModel(parent),
    _appCache(appCache) {
    (void) connect(&_appCache, &AppCache::drivesChanged, this, &DriveListModel::resetModel);
    (void) connect(&_appCache, &AppCache::selectedDriveDbIdChanged, this, &DriveListModel::emitSelectionRoleChanged);
}

int DriveListModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(_appCache.drives().size());
}

QVariant DriveListModel::data(const QModelIndex &index, const int role) const {
    if (!index.isValid()) {
        return {};
    }

    const int row = index.row();
    const auto &drives = _appCache.drives();
    if (row < 0 || row >= static_cast<int>(drives.size())) {
        return {};
    }

    const auto &drive = drives[static_cast<size_t>(row)];
    switch (role) {
        case DbIdRole:
            return static_cast<qint64>(drive.dbId());
        case IdRole:
            return static_cast<qint64>(drive.id());
        case AccountDbIdRole:
            return static_cast<qint64>(drive.accountDbId());
        case NameRole:
            return drive.name();
        case SizeRole:
            return static_cast<qint64>(drive.size());
        case UsedSizeRole:
            return static_cast<qint64>(drive.usedSize());
        case ColorRole:
            return drive.color();
        case NotificationsRole:
            return drive.notifications();
        case AdminRole:
            return drive.admin();
        case MaintenanceRole:
            return drive.maintenance();
        case LockedRole:
            return drive.locked();
        case AccessDeniedRole:
            return drive.accessDenied();
        case PackIdRole:
            return static_cast<qint64>(drive.packInfo().id());
        case PackNameRole:
            return QString::fromStdString(drive.packInfo().name());
        case PackDisplayNameRole:
            return QString::fromStdString(drive.packInfo().displayName());
        case PackIsFreeRole:
            return drive.packInfo().isFree();
        case SelectedRole:
            return static_cast<qint64>(drive.dbId()) == _appCache.selectedDriveDbId();
        default:
            return {};
    }
}

QHash<int, QByteArray> DriveListModel::roleNames() const {
    static const QHash<int, QByteArray> roles{{DbIdRole, "dbId"},
                                              {IdRole, "id"},
                                              {AccountDbIdRole, "accountDbId"},
                                              {NameRole, "name"},
                                              {SizeRole, "size"},
                                              {UsedSizeRole, "usedSize"},
                                              {ColorRole, "color"},
                                              {NotificationsRole, "notifications"},
                                              {AdminRole, "admin"},
                                              {MaintenanceRole, "maintenance"},
                                              {LockedRole, "locked"},
                                              {AccessDeniedRole, "accessDenied"},
                                              {PackIdRole, "packId"},
                                              {PackNameRole, "packName"},
                                              {PackDisplayNameRole, "packDisplayName"},
                                              {PackIsFreeRole, "packIsFree"},
                                              {SelectedRole, "selected"}};
    return roles;
}

void DriveListModel::resetModel() {
    beginResetModel();
    endResetModel();
}

void DriveListModel::emitSelectionRoleChanged() {
    const auto size = _appCache.drives().size();
    if (size == 0) {
        return;
    }

    const QModelIndex first = index(0, 0);
    const QModelIndex last = index(static_cast<int>(size) - 1, 0);
    emit dataChanged(first, last, {SelectedRole});
}

} // namespace KDC
