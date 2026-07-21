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

#include "app/appconstants.h"

#include <QColor>
#include <QCoreApplication>

#include <algorithm>
#include <cstddef>

namespace KDC {

namespace {

[[nodiscard]] QString normalizedAccountName(const AvailableDriveContext &context) {
    if (!context.availableDrive.accountName().empty()) {
        return QString::fromStdString(context.availableDrive.accountName());
    }

    if (context.accountInfo.has_value()) {
        return QString::fromStdString(context.accountInfo->name());
    }

    return {};
}

[[nodiscard]] bool driveContextLessThan(const AvailableDriveContext &lhs, const AvailableDriveContext &rhs) {
    if (const auto nameCompare = QString::compare(QString::fromStdString(lhs.availableDrive.name()),
                                                  QString::fromStdString(rhs.availableDrive.name()), Qt::CaseInsensitive);
        nameCompare != 0) {
        return nameCompare < 0;
    }

    if (const auto accountCompare = QString::compare(normalizedAccountName(lhs), normalizedAccountName(rhs), Qt::CaseInsensitive);
        accountCompare != 0) {
        return accountCompare < 0;
    }

    if (lhs.availableDrive.accountId() != rhs.availableDrive.accountId()) {
        return lhs.availableDrive.accountId() < rhs.availableDrive.accountId();
    }

    return lhs.availableDrive.driveId() < rhs.availableDrive.driveId();
}

} // namespace

AvailableDrivesModel::AvailableDrivesModel(const AppCache &cache, OnboardingState &onboardingState, QObject *const parent) :
    QAbstractListModel(parent),
    _cache(cache),
    _onboardingState(onboardingState) {
    (void) connect(&_cache, &AppCache::usersChanged, this, &AvailableDrivesModel::rebuild);
    (void) connect(&_cache, &AppCache::availableDrivesChanged, this, [this](const UserDbId userDbId) {
        if (userDbId == selectedUserDbId()) {
            rebuild();
        }
    });
    (void) connect(&_cache, &AppCache::allAvailableDrivesChanged, this, &AvailableDrivesModel::rebuild);
    (void) connect(&_onboardingState, &OnboardingState::selectedUserDbIdChanged, this, &AvailableDrivesModel::rebuild);
    (void) connect(&_onboardingState, &OnboardingState::selectedAvailableDrivesChanged, this, [this] {
        if (!_contexts.empty()) {
            emit dataChanged(index(0, 0), index(static_cast<qint32>(_contexts.size()) - 1, 0), {SelectedRole});
        }
        emit selectedCountChanged();
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

    switch (role) {
        case NameRole:
        case Qt::DisplayRole:
            return QString::fromStdString(context.availableDrive.name());
        case AccountNameRole:
            return accountNameForContext(context);
        case ColorRole:
            if (const QColor color{QString::fromStdString(context.availableDrive.color())}; color.isValid()) {
                return color;
            }
            return AppConstants::Drive::defaultColor();
        case SelectedRole:
            return context.alreadyConfigured || isRowSelected(row);
        case AlreadyConfiguredRole:
            return context.alreadyConfigured;
        case EnabledRole:
            return !context.alreadyConfigured;
        case TooltipRole:
            return context.alreadyConfigured ? qtTrId("onboardingAlreadySyncedDriveTooltip") : QString();
        default:
            return {};
    }
}

QHash<int, QByteArray> AvailableDrivesModel::roleNames() const {
    return {
            {NameRole, "name"},
            {AccountNameRole, "accountName"},
            {ColorRole, "color"},
            {SelectedRole, "selected"},
            {AlreadyConfiguredRole, "alreadyConfigured"},
            {EnabledRole, "enabled"},
            {TooltipRole, "tooltip"},
    };
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

void AvailableDrivesModel::toggleDrive(const qint32 row) {
    if (row < 0 || row >= static_cast<qint32>(_contexts.size())) {
        return;
    }

    if (_contexts[static_cast<std::size_t>(row)].alreadyConfigured) {
        return;
    }

    _onboardingState.toggleAvailableDrive(keyAt(row));
}

AvailableDriveKey AvailableDrivesModel::keyAt(const qint32 row) const {
    const auto &driveInfo = _contexts[static_cast<std::size_t>(row)].availableDrive;
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

UserDbId AvailableDrivesModel::selectedUserDbId() const {
    return _onboardingState.typedSelectedUserDbId();
}

void AvailableDrivesModel::rebuild() {
    const auto userDbId = selectedUserDbId();
    auto contexts = userDbId == 0 ? std::vector<AvailableDriveContext>{} : _cache.availableDriveContexts(userDbId);
    (void) std::ranges::sort(contexts, driveContextLessThan);

    beginResetModel();
    _contexts = std::move(contexts);
    endResetModel();

    emit selectedCountChanged();
    emit configuredCountChanged();
}

} // namespace KDC
