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

#include "availabledrivelistmodel.h"

#include <algorithm>

namespace KDC {

namespace {
constexpr qint32 noVirtualFileMode = 0;
}

AvailableDriveListModel::AvailableDriveListModel(AppCache &appCache, OnboardingState &onboardingState, QObject *const parent) :
    QAbstractListModel(parent),
    _appCache(appCache),
    _onboardingState(onboardingState),
    _contexts(_appCache.availableDriveContexts(_onboardingState.typedSelectedUserDbId())) {
    (void) connect(&_appCache, &AppCache::usersChanged, this, &AvailableDriveListModel::resetModel);
    (void) connect(&_appCache, &AppCache::accountsChanged, this, &AvailableDriveListModel::resetModel);
    (void) connect(&_appCache, &AppCache::drivesChanged, this, &AvailableDriveListModel::resetModel);
    (void) connect(&_appCache, &AppCache::availableDrivesChanged, this, &AvailableDriveListModel::handleAvailableDrivesChanged);
    (void) connect(&_appCache, &AppCache::allAvailableDrivesChanged, this, &AvailableDriveListModel::resetModel);
    (void) connect(&_onboardingState, &OnboardingState::selectedUserDbIdChanged, this, &AvailableDriveListModel::resetModel);
    (void) connect(&_onboardingState, &OnboardingState::selectedAvailableDrivesChanged, this,
                   &AvailableDriveListModel::emitSelectionRolesChanged);
    (void) connect(&_onboardingState, &OnboardingState::pendingSyncConfigsChanged, this,
                   &AvailableDriveListModel::emitPendingConfigRolesChanged);
}

qint32 AvailableDriveListModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<qint32>(_contexts.size());
}

QVariant AvailableDriveListModel::data(const QModelIndex &index, const qint32 role) const {
    const auto *const context = contextForIndex(index);
    if (context == nullptr) {
        return {};
    }

    const auto key = keyForContext(*context);
    const auto pendingConfig = _onboardingState.pendingSyncConfig(key);
    switch (role) {
        case UserDbIdRole:
            return static_cast<qint64>(context->user.dbId());
        case UserIdRole:
            return static_cast<qint64>(context->user.userId());
        case UserNameRole:
            return context->user.name();
        case UserEmailRole:
            return context->user.email();
        case AccountIdRole:
            return static_cast<qint64>(context->availableDrive.accountId());
        case AccountNameRole:
            return context->account ? QString::fromStdString(context->account->name()) : context->availableDrive.accountName();
        case AccountDbIdRole:
            return context->account ? QVariant::fromValue(static_cast<qint64>(context->account->dbId())) : QVariant{};
        case HasConfiguredAccountRole:
            return context->account.has_value();
        case DriveIdRole:
            return static_cast<qint64>(context->availableDrive.driveId());
        case NameRole:
            return context->availableDrive.name();
        case ColorRole:
            return context->availableDrive.color();
        case AlreadyConfiguredRole:
            return context->alreadyConfigured;
        case ConfiguredDriveDbIdRole:
            return context->configuredDrive ? QVariant::fromValue(static_cast<qint64>(context->configuredDrive->dbId()))
                                            : QVariant{};
        case ConfiguredDriveNameRole:
            return context->configuredDrive ? context->configuredDrive->name() : QVariant{};
        case SelectedRole:
            return _onboardingState.isAvailableDriveSelected(key);
        case HasPendingConfigRole:
            return pendingConfig.has_value();
        case PendingLocalPathRole:
            return pendingConfig ? pendingConfig->localPath : QVariant{};
        case PendingTargetPathRole:
            return pendingConfig ? pendingConfig->targetPath : QVariant{};
        case PendingTargetNodeIdRole:
            return pendingConfig ? pendingConfig->targetNodeId : QVariant{};
        case PendingSupportVfsRole:
            return pendingConfig ? pendingConfig->supportVfs : QVariant{};
        case PendingVirtualFileModeRole:
            return pendingConfig ? static_cast<qint32>(pendingConfig->virtualFileMode) : noVirtualFileMode;
        default:
            return {};
    }
}

QHash<qint32, QByteArray> AvailableDriveListModel::roleNames() const {
    static const QHash<qint32, QByteArray> roles{{UserDbIdRole, "userDbId"},
                                                 {UserIdRole, "userId"},
                                                 {UserNameRole, "userName"},
                                                 {UserEmailRole, "userEmail"},
                                                 {AccountIdRole, "accountId"},
                                                 {AccountNameRole, "accountName"},
                                                 {AccountDbIdRole, "accountDbId"},
                                                 {HasConfiguredAccountRole, "hasConfiguredAccount"},
                                                 {DriveIdRole, "driveId"},
                                                 {NameRole, "name"},
                                                 {ColorRole, "color"},
                                                 {AlreadyConfiguredRole, "alreadyConfigured"},
                                                 {ConfiguredDriveDbIdRole, "configuredDriveDbId"},
                                                 {ConfiguredDriveNameRole, "configuredDriveName"},
                                                 {SelectedRole, "selected"},
                                                 {HasPendingConfigRole, "hasPendingConfig"},
                                                 {PendingLocalPathRole, "pendingLocalPath"},
                                                 {PendingTargetPathRole, "pendingTargetPath"},
                                                 {PendingTargetNodeIdRole, "pendingTargetNodeId"},
                                                 {PendingSupportVfsRole, "pendingSupportVfs"},
                                                 {PendingVirtualFileModeRole, "pendingVirtualFileMode"}};
    return roles;
}

void AvailableDriveListModel::selectUser(const qint64 userDbId) {
    _onboardingState.selectUser(userDbId);
}

void AvailableDriveListModel::clearSelectedUser() {
    _onboardingState.clearSelectedUser();
}

void AvailableDriveListModel::toggleAvailableDrive(const qint64 accountId, const qint64 driveId) {
    if (const auto key = keyForIds(accountId, driveId)) {
        _onboardingState.toggleAvailableDrive(*key);
    }
}

void AvailableDriveListModel::setPendingSyncConfig(const qint64 accountId, const qint64 driveId, const QString &localPath,
                                                   const QString &targetPath, const QString &targetNodeId, const bool supportVfs,
                                                   const qint32 virtualFileMode) {
    const auto key = keyForIds(accountId, driveId);
    if (!key) {
        return;
    }

    PendingSyncConfig config;
    config.localPath = localPath;
    config.targetPath = targetPath;
    config.targetNodeId = targetNodeId;
    config.supportVfs = supportVfs;
    config.virtualFileMode = static_cast<VirtualFileMode>(virtualFileMode);
    _onboardingState.setPendingSyncConfig(*key, config);
}

void AvailableDriveListModel::clearPendingSyncConfig(const qint64 accountId, const qint64 driveId) {
    if (const auto key = keyForIds(accountId, driveId)) {
        _onboardingState.clearPendingSyncConfig(*key);
    }
}

void AvailableDriveListModel::resetModel() {
    beginResetModel();
    _contexts = _appCache.availableDriveContexts(_onboardingState.typedSelectedUserDbId());
    endResetModel();
}

void AvailableDriveListModel::emitSelectionRolesChanged() {
    if (_contexts.empty()) {
        return;
    }

    emit dataChanged(index(0, 0), index(static_cast<qint32>(_contexts.size()) - 1, 0), {SelectedRole});
}

void AvailableDriveListModel::emitPendingConfigRolesChanged() {
    if (_contexts.empty()) {
        return;
    }

    emit dataChanged(index(0, 0), index(static_cast<qint32>(_contexts.size()) - 1, 0),
                     {HasPendingConfigRole, PendingLocalPathRole, PendingTargetPathRole, PendingTargetNodeIdRole,
                      PendingSupportVfsRole, PendingVirtualFileModeRole});
}

void AvailableDriveListModel::handleAvailableDrivesChanged(const UserDbId userDbId) {
    if (userDbId == _onboardingState.typedSelectedUserDbId()) {
        resetModel();
    }
}

const AvailableDriveContext *AvailableDriveListModel::contextForIndex(const QModelIndex &index) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<qint32>(_contexts.size())) {
        return nullptr;
    }

    return &_contexts[static_cast<size_t>(index.row())];
}

AvailableDriveKey AvailableDriveListModel::keyForContext(const AvailableDriveContext &context) const {
    return AvailableDriveKey{context.user.dbId(), context.availableDrive.accountId(), context.availableDrive.driveId()};
}

std::optional<AvailableDriveKey> AvailableDriveListModel::keyForIds(const qint64 accountId, const qint64 driveId) const {
    const auto typedAccountId = static_cast<AccountId>(accountId);
    const auto typedDriveId = static_cast<DriveId>(driveId);
    const auto contextIt = std::ranges::find_if(_contexts, [typedAccountId, typedDriveId](const AvailableDriveContext &context) {
        return context.availableDrive.accountId() == typedAccountId && context.availableDrive.driveId() == typedDriveId;
    });
    if (contextIt == _contexts.end()) {
        return std::nullopt;
    }

    return keyForContext(*contextIt);
}

} // namespace KDC
