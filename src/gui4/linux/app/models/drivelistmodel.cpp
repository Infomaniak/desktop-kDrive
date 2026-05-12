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

#include <algorithm>

namespace KDC {

DriveListModel::DriveListModel(AppCache &appCache, MainSelectionStore &mainSelectionStore, QObject *const parent) :
    QAbstractListModel(parent),
    _appCache(appCache),
    _mainSelectionStore(mainSelectionStore),
    _contexts(_appCache.driveContexts()) {
    (void) connect(&_appCache, &AppCache::usersChanged, this, &DriveListModel::resetModel);
    (void) connect(&_appCache, &AppCache::accountsChanged, this, &DriveListModel::resetModel);
    (void) connect(&_appCache, &AppCache::drivesChanged, this, &DriveListModel::resetModel);
    (void) connect(&_appCache, &AppCache::syncsChanged, this, &DriveListModel::resetModel);
    (void) connect(&_mainSelectionStore, &MainSelectionStore::currentSyncContextChanged, this,
                   &DriveListModel::emitCurrentRoleChanged);
}

qint32 DriveListModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<qint32>(_contexts.size());
}

QVariant DriveListModel::data(const QModelIndex &index, const qint32 role) const {
    const auto *const context = contextForIndex(index);
    if (context == nullptr) {
        return {};
    }

    switch (role) {
        case DbIdRole:
            return static_cast<qint64>(context->drive.dbId());
        case IdRole:
            return static_cast<qint64>(context->drive.id());
        case AccountDbIdRole:
            return static_cast<qint64>(context->account.dbId());
        case AccountIdRole:
            return static_cast<qint64>(context->account.id());
        case AccountNameRole:
            return QString::fromStdString(context->account.name());
        case UserDbIdRole:
            return static_cast<qint64>(context->user.dbId());
        case UserIdRole:
            return static_cast<qint64>(context->user.userId());
        case UserNameRole:
            return context->user.name();
        case UserEmailRole:
            return context->user.email();
        case NameRole:
            return context->drive.name();
        case SizeRole:
            return static_cast<qint64>(context->drive.size());
        case UsedSizeRole:
            return static_cast<qint64>(context->drive.usedSize());
        case ColorRole:
            return context->drive.color();
        case NotificationsRole:
            return context->drive.notifications();
        case AdminRole:
            return context->drive.admin();
        case MaintenanceRole:
            return context->drive.maintenance();
        case LockedRole:
            return context->drive.locked();
        case AccessDeniedRole:
            return context->drive.accessDenied();
        case PackIdRole:
            return static_cast<qint64>(context->drive.packInfo().id());
        case PackNameRole:
            return QString::fromStdString(context->drive.packInfo().name());
        case PackDisplayNameRole:
            return QString::fromStdString(context->drive.packInfo().displayName());
        case PackIsFreeRole:
            return context->drive.packInfo().isFree();
        case SyncCountRole:
            return static_cast<qint64>(context->syncs.size());
        case CurrentRole:
            return context->drive.dbId() == currentDriveDbId();
        default:
            return {};
    }
}

QHash<qint32, QByteArray> DriveListModel::roleNames() const {
    static const QHash<qint32, QByteArray> roles{{DbIdRole, "dbId"},
                                                 {IdRole, "id"},
                                                 {AccountDbIdRole, "accountDbId"},
                                                 {AccountIdRole, "accountId"},
                                                 {AccountNameRole, "accountName"},
                                                 {UserDbIdRole, "userDbId"},
                                                 {UserIdRole, "userId"},
                                                 {UserNameRole, "userName"},
                                                 {UserEmailRole, "userEmail"},
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
                                                 {SyncCountRole, "syncCount"},
                                                 {CurrentRole, "current"}};
    return roles;
}

void DriveListModel::selectFirstSyncForDrive(const qint64 driveDbId) {
    const auto typedDriveDbId = static_cast<DriveDbId>(driveDbId);
    const auto contextIt = std::ranges::find_if(
            _contexts, [typedDriveDbId](const DriveContext &context) { return context.drive.dbId() == typedDriveDbId; });
    if (contextIt == _contexts.end() || contextIt->syncs.empty()) {
        return;
    }

    _mainSelectionStore.selectSync(static_cast<qint64>(contextIt->syncs.front().dbId()));
}

void DriveListModel::resetModel() {
    beginResetModel();
    _contexts = _appCache.driveContexts();
    endResetModel();
}

void DriveListModel::emitCurrentRoleChanged() {
    if (_contexts.empty()) {
        return;
    }

    emit dataChanged(index(0, 0), index(static_cast<qint32>(_contexts.size()) - 1, 0), {CurrentRole});
}

const DriveContext *DriveListModel::contextForIndex(const QModelIndex &index) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<qint32>(_contexts.size())) {
        return nullptr;
    }

    return &_contexts[static_cast<size_t>(index.row())];
}

DriveDbId DriveListModel::currentDriveDbId() const {
    const auto context = _mainSelectionStore.currentSyncContext();
    return context ? context->drive.dbId() : 0;
}

} // namespace KDC
