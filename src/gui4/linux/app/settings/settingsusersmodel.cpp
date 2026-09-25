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

#include "settingsusersmodel.h"

#include "app/services/settingsuserservice.h"

#include <algorithm>
#include <cstdint>
#include <iterator>

using namespace Qt::StringLiterals;

namespace KDC {

SettingsUsersModel::SettingsUsersModel(const AppCache &cache, SettingsUserService &settingsUserService, QObject *const parent) :
    QAbstractListModel(parent),
    _cache(cache),
    _settingsUserService(settingsUserService) {
    (void) connect(&_cache, &AppCache::usersChanged, this, &SettingsUsersModel::rebuild);
    (void) connect(&_settingsUserService, &SettingsUserService::userStateChanged, this,
                   &SettingsUsersModel::handleUserStateChanged);
    rebuild();
}

int SettingsUsersModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(_entries.size());
}

QVariant SettingsUsersModel::data(const QModelIndex &index, const int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    const auto &entry = _entries[static_cast<std::size_t>(index.row())];
    switch (role) {
        case UserDbIdRole:
            return static_cast<qint64>(entry.userDbId);
        case NameRole:
        case Qt::DisplayRole:
            return entry.name;
        case EmailRole:
            return entry.email;
        case DisconnectLabelRole:
            return entry.disconnectLabel;
        case AvatarSourceRole:
            return entry.avatarSource;
        case DrivesModelRole:
            return QVariant::fromValue<UserDrivesModel *>(entry.drivesModel.get());
        case AvailableDrivesLoadingRole:
            return _settingsUserService.availableDrivesLoading(entry.userDbId);
        case AvailableDrivesFailedRole:
            return _settingsUserService.availableDrivesFailed(entry.userDbId);
        default:
            return {};
    }
}

QHash<int, QByteArray> SettingsUsersModel::roleNames() const {
    return {
            {UserDbIdRole, "userDbId"},
            {NameRole, "name"},
            {EmailRole, "email"},
            {DisconnectLabelRole, "disconnectLabel"},
            {AvatarSourceRole, "avatarSource"},
            {DrivesModelRole, "drivesModel"},
            {AvailableDrivesLoadingRole, "availableDrivesLoading"},
            {AvailableDrivesFailedRole, "availableDrivesFailed"},
    };
}

void SettingsUsersModel::rebuild() {
    const auto previousCount = count();
    auto users = _cache.users();
    (void) std::ranges::sort(users, [](const User &lhs, const User &rhs) {
        if (const auto nameComparison =
                    QString::compare(QString::fromStdString(lhs.name()), QString::fromStdString(rhs.name()), Qt::CaseInsensitive);
            nameComparison != 0) {
            return nameComparison < 0;
        }
        return lhs.dbId() < rhs.dbId();
    });

    std::vector<UserEntry> entries;
    entries.reserve(users.size());
    QHash<QString, uint32_t> nameCounts;
    for (const auto &user: users) {
        const auto displayInfo = _cache.userDisplayInfo(user.dbId());
        if (!displayInfo.has_value()) {
            continue;
        }
        entries.push_back(UserEntry{
                .userDbId = displayInfo->dbId(),
                .name = QString::fromStdString(displayInfo->name()),
                .email = QString::fromStdString(displayInfo->email()),
                .disconnectLabel = {}, // Set below once all names are known.
                .avatarSource = displayInfo->avatarSource().isEmpty() ? QString::fromStdString(displayInfo->avatarUrl())
                                                                      : displayInfo->avatarSource(),
                .drivesModel = std::make_unique<UserDrivesModel>(_cache, user.dbId(), this),
        });
        ++nameCounts[entries.back().name];
    }

    for (auto &entry: entries) {
        entry.disconnectLabel = nameCounts.value(entry.name) > 1 && !entry.email.isEmpty()
                                        ? u"%1 (%2)"_s.arg(entry.name, entry.email)
                                        : entry.name;
    }

    beginResetModel();
    _entries = std::move(entries);
    endResetModel();

    if (count() != previousCount) {
        emit countChanged();
    }
}

void SettingsUsersModel::handleUserStateChanged(const UserDbId userDbId) {
    const auto it = std::ranges::find_if(_entries, [userDbId](const UserEntry &entry) { return entry.userDbId == userDbId; });
    if (it == _entries.end()) {
        return;
    }

    const auto row = static_cast<int>(std::distance(_entries.begin(), it));
    emit dataChanged(index(row, 0), index(row, 0), {AvailableDrivesLoadingRole, AvailableDrivesFailedRole});
}

} // namespace KDC
