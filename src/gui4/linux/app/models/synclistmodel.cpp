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

#include "utility/types.h"

namespace KDC {

SyncListModel::SyncListModel(AppCache &appCache, MainSelectionStore &mainSelectionStore, QObject *const parent) :
    QAbstractListModel(parent),
    _appCache(appCache),
    _mainSelectionStore(mainSelectionStore),
    _contexts(_mainSelectionStore.syncContexts()) {
    (void) connect(&_appCache, &AppCache::usersChanged, this, &SyncListModel::resetModel);
    (void) connect(&_appCache, &AppCache::accountsChanged, this, &SyncListModel::resetModel);
    (void) connect(&_appCache, &AppCache::drivesChanged, this, &SyncListModel::resetModel);
    (void) connect(&_appCache, &AppCache::syncsChanged, this, &SyncListModel::resetModel);
    (void) connect(&_appCache, &AppCache::syncErrorsChanged, this, &SyncListModel::resetModel);
    (void) connect(&_mainSelectionStore, &MainSelectionStore::currentSyncDbIdChanged, this,
                   &SyncListModel::emitSelectionRoleChanged);
}

qint32 SyncListModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<qint32>(_contexts.size());
}

QVariant SyncListModel::data(const QModelIndex &index, const qint32 role) const {
    const auto *const context = contextForIndex(index);
    if (context == nullptr) {
        return {};
    }

    switch (role) {
        case DbIdRole:
            return static_cast<qint64>(context->sync.dbId());
        case DriveDbIdRole:
            return static_cast<qint64>(context->drive.dbId());
        case DriveIdRole:
            return static_cast<qint64>(context->drive.id());
        case DriveNameRole:
            return context->drive.name();
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
        case LocalPathRole:
            return context->sync.localPath();
        case TargetPathRole:
            return context->sync.targetPath();
        case TargetNodeIdRole:
            return context->sync.targetNodeId();
        case SupportVfsRole:
            return context->sync.supportVfs();
        case VirtualFileModeRole:
            return toInt(context->sync.virtualFileMode());
        case NavigationPaneClsidRole:
            return context->sync.navigationPaneClsid();
        case ErrorCountRole:
            return static_cast<qint64>(context->errors.size());
        case HasErrorRole:
            return context->latestError.has_value();
        case LatestErrorDbIdRole:
            return context->latestError ? QVariant::fromValue(static_cast<qint64>(context->latestError->dbId())) : QVariant{};
        case LatestErrorTimeRole:
            return context->latestError ? QVariant::fromValue(context->latestError->getTime()) : QVariant{};
        case LatestErrorLevelRole:
            return context->latestError ? QVariant::fromValue(toInt(context->latestError->level())) : QVariant{};
        case LatestErrorExitCodeRole:
            return context->latestError ? QVariant::fromValue(toInt(context->latestError->exitCode())) : QVariant{};
        case LatestErrorPathRole:
            return context->latestError ? context->latestError->path() : QVariant{};
        case SelectedRole:
            return context->sync.dbId() == static_cast<SyncDbId>(_mainSelectionStore.currentSyncDbId());
        default:
            return {};
    }
}

QHash<qint32, QByteArray> SyncListModel::roleNames() const {
    static const QHash<qint32, QByteArray> roles{{DbIdRole, "dbId"},
                                                 {DriveDbIdRole, "driveDbId"},
                                                 {DriveIdRole, "driveId"},
                                                 {DriveNameRole, "driveName"},
                                                 {AccountDbIdRole, "accountDbId"},
                                                 {AccountIdRole, "accountId"},
                                                 {AccountNameRole, "accountName"},
                                                 {UserDbIdRole, "userDbId"},
                                                 {UserIdRole, "userId"},
                                                 {UserNameRole, "userName"},
                                                 {UserEmailRole, "userEmail"},
                                                 {LocalPathRole, "localPath"},
                                                 {TargetPathRole, "targetPath"},
                                                 {TargetNodeIdRole, "targetNodeId"},
                                                 {SupportVfsRole, "supportVfs"},
                                                 {VirtualFileModeRole, "virtualFileMode"},
                                                 {NavigationPaneClsidRole, "navigationPaneClsid"},
                                                 {ErrorCountRole, "errorCount"},
                                                 {HasErrorRole, "hasError"},
                                                 {LatestErrorDbIdRole, "latestErrorDbId"},
                                                 {LatestErrorTimeRole, "latestErrorTime"},
                                                 {LatestErrorLevelRole, "latestErrorLevel"},
                                                 {LatestErrorExitCodeRole, "latestErrorExitCode"},
                                                 {LatestErrorPathRole, "latestErrorPath"},
                                                 {SelectedRole, "selected"}};
    return roles;
}

void SyncListModel::selectSync(const qint64 syncDbId) {
    _mainSelectionStore.selectSync(syncDbId);
}

void SyncListModel::resetModel() {
    beginResetModel();
    _contexts = _mainSelectionStore.syncContexts();
    endResetModel();
}

void SyncListModel::emitSelectionRoleChanged() {
    if (_contexts.empty()) {
        return;
    }

    emit dataChanged(index(0, 0), index(static_cast<qint32>(_contexts.size()) - 1, 0), {SelectedRole});
}

const SyncContext *SyncListModel::contextForIndex(const QModelIndex &index) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<qint32>(_contexts.size())) {
        return nullptr;
    }

    return &_contexts[static_cast<size_t>(index.row())];
}

} // namespace KDC
