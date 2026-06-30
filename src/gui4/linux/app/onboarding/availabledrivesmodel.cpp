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

#include "availabledrivesmodel.h"

#include <QColor>

#include <algorithm>
#include <cstddef>

namespace KDC {

namespace {

const QColor defaultDriveColor{QStringLiteral("#0098FF")};

[[nodiscard]] QString normalizedAccountName(const AvailableDriveContext &context) {
    if (!context.availableDriveInfo.accountName().isEmpty()) {
        return context.availableDriveInfo.accountName();
    }

    if (context.accountInfo.has_value()) {
        return QString::fromStdString(context.accountInfo->name());
    }

    return {};
}

[[nodiscard]] bool driveContextLessThan(const AvailableDriveContext &lhs, const AvailableDriveContext &rhs) {
    if (const auto nameCompare =
                QString::compare(lhs.availableDriveInfo.name(), rhs.availableDriveInfo.name(), Qt::CaseInsensitive);
        nameCompare != 0) {
        return nameCompare < 0;
    }

    if (const auto accountCompare = QString::compare(normalizedAccountName(lhs), normalizedAccountName(rhs), Qt::CaseInsensitive);
        accountCompare != 0) {
        return accountCompare < 0;
    }

    if (lhs.availableDriveInfo.accountId() != rhs.availableDriveInfo.accountId()) {
        return lhs.availableDriveInfo.accountId() < rhs.availableDriveInfo.accountId();
    }

    return lhs.availableDriveInfo.driveId() < rhs.availableDriveInfo.driveId();
}

} // namespace

AvailableDrivesModel::AvailableDrivesModel(AppCache &cache, OnboardingState &onboardingState, UserService &userService,
                                           OnboardingFlowController &flowController, QObject *const parent) :
    QAbstractListModel(parent),
    _cache(cache),
    _onboardingState(onboardingState),
    _userService(userService),
    _flowController(flowController) {
    (void) connect(&_cache, &AppCache::usersChanged, this, &AvailableDrivesModel::rebuild);
    (void) connect(&_cache, &AppCache::availableDrivesChanged, this, [this](const UserDbId userDbId) {
        if (userDbId == selectedUserDbId()) {
            rebuild();
        }
    });
    (void) connect(&_cache, &AppCache::allAvailableDrivesChanged, this, &AvailableDrivesModel::rebuild);
    (void) connect(&_onboardingState, &OnboardingState::selectedUserDbIdChanged, this, [this] {
        setLoadFailed(false);
        rebuild();
        emit loadingChanged();
        emit emptyChanged();
    });
    (void) connect(&_onboardingState, &OnboardingState::selectedAvailableDrivesChanged, this, [this] {
        if (!_contexts.empty()) {
            emit dataChanged(index(0, 0), index(static_cast<qint32>(_contexts.size()) - 1, 0), {SelectedRole});
        }
        emitSelectionDependentChanges();
    });
    (void) connect(&_userService, &UserService::loadingChanged, this, [this] {
        emit loadingChanged();
        emit emptyChanged();
    });
    (void) connect(&_userService, &UserService::availableDrivesLoaded, this, [this](const UserDbId userDbId) {
        if (userDbId == selectedUserDbId()) {
            setLoadFailed(false);
        }
    });
    (void) connect(&_userService, &UserService::availableDrivesLoadFailed, this, [this](const UserDbId userDbId) {
        if (userDbId == selectedUserDbId()) {
            setLoadFailed(true);
            emit emptyChanged();
        }
    });

    rebuild();
}

int AvailableDrivesModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }

    return static_cast<int>(_contexts.size());
}

QVariant AvailableDrivesModel::data(const QModelIndex &index, const int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    const auto row = static_cast<qint32>(index.row());
    const auto &context = _contexts[static_cast<std::size_t>(row)];
    const auto key = keyAt(row);

    switch (role) {
        case UserDbIdRole:
            return QVariant::fromValue(static_cast<qint64>(key.userDbId));
        case AccountIdRole:
            return QVariant::fromValue(static_cast<qint64>(key.accountId));
        case DriveIdRole:
            return QVariant::fromValue(static_cast<qint64>(key.driveId));
        case NameRole:
        case Qt::DisplayRole:
            return context.availableDriveInfo.name();
        case AccountNameRole:
            return accountNameForContext(context);
        case ColorRole:
            return context.availableDriveInfo.color().isValid() ? context.availableDriveInfo.color() : defaultDriveColor;
        case SelectedRole:
            return context.alreadyConfigured || _onboardingState.isAvailableDriveSelected(key);
        case AlreadyConfiguredRole:
            return context.alreadyConfigured;
        case EnabledRole:
            return !context.alreadyConfigured;
        case TooltipRole:
            return context.alreadyConfigured ? tr("This kDrive is already configured.\nGo to your settings to modify it.")
                                             : QString();
        default:
            return {};
    }
}

QHash<int, QByteArray> AvailableDrivesModel::roleNames() const {
    return {
            {UserDbIdRole, "userDbId"},       {AccountIdRole, "accountId"},
            {DriveIdRole, "driveId"},         {NameRole, "name"},
            {AccountNameRole, "accountName"}, {ColorRole, "color"},
            {SelectedRole, "selected"},       {AlreadyConfiguredRole, "alreadyConfigured"},
            {EnabledRole, "enabled"},         {TooltipRole, "tooltip"},
    };
}

bool AvailableDrivesModel::loading() const {
    const auto userDbId = selectedUserDbId();
    return userDbId != 0 && _userService.isLoadAvailableDrivesPending(static_cast<qint64>(userDbId));
}

bool AvailableDrivesModel::empty() const {
    return !loading() && !_loadFailed && _contexts.empty();
}

qint32 AvailableDrivesModel::selectedCount() const {
    qint32 count = 0;
    for (qint32 row = 0; row < static_cast<qint32>(_contexts.size()); ++row) {
        if (!_contexts[static_cast<std::size_t>(row)].alreadyConfigured && isRowSelected(row)) {
            ++count;
        }
    }
    return count;
}

qint32 AvailableDrivesModel::configuredCount() const {
    return static_cast<qint32>(
            std::ranges::count_if(_contexts, [](const AvailableDriveContext &context) { return context.alreadyConfigured; }));
}

bool AvailableDrivesModel::canContinue() const {
    return selectedCount() > 0 || configuredCount() > 0;
}

bool AvailableDrivesModel::canOpenAdvancedSettings() const {
    return hasSelectedDrives();
}

QString AvailableDrivesModel::userName() const {
    if (const auto user = _cache.userDisplayInfo(selectedUserDbId())) {
        return user->name();
    }
    return {};
}

QString AvailableDrivesModel::userEmail() const {
    if (const auto user = _cache.userDisplayInfo(selectedUserDbId())) {
        return user->email();
    }
    return {};
}

QString AvailableDrivesModel::userAvatarSource() const {
    if (const auto user = _cache.userDisplayInfo(selectedUserDbId())) {
        if (!user->avatarSource().isEmpty()) {
            return user->avatarSource();
        }
        return user->avatarUrl();
    }
    return {};
}

void AvailableDrivesModel::reload() {
    const auto userDbId = selectedUserDbId();
    if (userDbId == 0 || loading()) {
        return;
    }

    setLoadFailed(false);
    _userService.loadAvailableDrives(static_cast<qint64>(userDbId));
}

void AvailableDrivesModel::toggleDrive(const qint32 row) {
    if (row < 0 || row >= static_cast<qint32>(_contexts.size())) {
        return;
    }

    if (_contexts[static_cast<std::size_t>(row)].alreadyConfigured) {
        return;
    }

    _onboardingState.toggleAvailableDrive(keyAt(row));
}

void AvailableDrivesModel::requestAdvancedSettings() {
    _flowController.requestAdvancedSettings();
}

void AvailableDrivesModel::continueOnboarding() {
    if (!canContinue()) {
        return;
    }

    _flowController.requestDriveSelectionContinue();
}

void AvailableDrivesModel::openDriveOffers() {
    _flowController.requestDriveOffers();
}

void AvailableDrivesModel::startForFree() {
    _flowController.requestFreeDriveOrder();
}

AvailableDriveKey AvailableDrivesModel::keyAt(const qint32 row) const {
    const auto &driveInfo = _contexts[static_cast<std::size_t>(row)].availableDriveInfo;
    return AvailableDriveKey{
            .userDbId = driveInfo.userDbId(),
            .accountId = driveInfo.accountId(),
            .driveId = driveInfo.driveId(),
    };
}

QString AvailableDrivesModel::accountNameForContext(const AvailableDriveContext &context) const {
    return normalizedAccountName(context);
}

bool AvailableDrivesModel::isRowSelected(const qint32 row) const {
    return _onboardingState.isAvailableDriveSelected(keyAt(row));
}

bool AvailableDrivesModel::hasSelectedDrives() const {
    return selectedCount() > 0;
}

UserDbId AvailableDrivesModel::selectedUserDbId() const {
    return _onboardingState.typedSelectedUserDbId();
}

void AvailableDrivesModel::rebuild() {
    const auto userDbId = selectedUserDbId();
    auto contexts = userDbId == 0 ? std::vector<AvailableDriveContext>{} : _cache.availableDriveContexts(userDbId);
    std::ranges::sort(contexts, driveContextLessThan);

    beginResetModel();
    _contexts = std::move(contexts);
    endResetModel();

    emit userChanged();
    emitListDependentChanges();
}

void AvailableDrivesModel::emitSelectionDependentChanges() {
    emit selectedCountChanged();
    emit canContinueChanged();
    emit canOpenAdvancedSettingsChanged();
}

void AvailableDrivesModel::emitListDependentChanges() {
    emit emptyChanged();
    emit configuredCountChanged();
    emitSelectionDependentChanges();
}

void AvailableDrivesModel::setLoadFailed(const bool loadFailed) {
    if (_loadFailed == loadFailed) {
        return;
    }

    _loadFailed = loadFailed;
    emit loadFailedChanged();
    emit emptyChanged();
}

} // namespace KDC
