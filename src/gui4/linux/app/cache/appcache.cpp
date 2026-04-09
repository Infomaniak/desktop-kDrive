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

#include "appcache.h"

#include <algorithm>

namespace {
template<typename T, typename IdType, typename Getter>
void upsertById(std::vector<T> &list, const T &value, IdType id, Getter getter) {
    const auto it = std::find_if(list.begin(), list.end(), [id, getter](const T &existing) { return getter(existing) == id; });
    if (it == list.end()) {
        list.push_back(value);
        return;
    }
    *it = value;
}

template<typename T, typename IdType, typename Getter>
bool removeById(std::vector<T> &list, IdType id, Getter getter) {
    const auto initialSize = list.size();
    std::erase_if(list, [id, getter](const T &value) { return getter(value) == id; });
    return list.size() != initialSize;
}
} // namespace

namespace KDC {

AppCache::AppCache(QObject *parent) :
    QObject(parent) {}

qint64 AppCache::selectedUserDbId() const {
    return static_cast<qint64>(_selectedUserDbId);
}

qint64 AppCache::selectedDriveDbId() const {
    return static_cast<qint64>(_selectedDriveDbId);
}

qint64 AppCache::selectedSyncDbId() const {
    return static_cast<qint64>(_selectedSyncDbId);
}

void AppCache::setConnected(const bool connected) {
    if (_connected == connected) {
        return;
    }
    _connected = connected;
    emit connectedChanged();
}

void AppCache::setSelectedUserDbId(const qint64 userDbId) {
    const auto typedUserDbId = static_cast<UserDbId>(userDbId);
    if (_selectedUserDbId == typedUserDbId) {
        return;
    }
    _selectedUserDbId = typedUserDbId;
    emit selectedUserDbIdChanged();
}

void AppCache::setSelectedDriveDbId(const qint64 driveDbId) {
    const auto typedDriveDbId = static_cast<DriveDbId>(driveDbId);
    if (_selectedDriveDbId == typedDriveDbId) {
        return;
    }
    _selectedDriveDbId = typedDriveDbId;
    emit selectedDriveDbIdChanged();
}

void AppCache::setSelectedSyncDbId(const qint64 syncDbId) {
    const auto typedSyncDbId = static_cast<SyncDbId>(syncDbId);
    if (_selectedSyncDbId == typedSyncDbId) {
        return;
    }
    _selectedSyncDbId = typedSyncDbId;
    emit selectedSyncDbIdChanged();
}

void AppCache::clearAll() {
    // Connection state is intentionally preserved: cache content and transport connectivity are orthogonal.
    const bool userSelectionChanged = _selectedUserDbId != 0;
    const bool driveSelectionChanged = _selectedDriveDbId != 0;
    const bool syncSelectionChanged = _selectedSyncDbId != 0;

    _users.clear();
    _accounts.clear();
    _drives.clear();
    _availableDrives.clear();
    _syncs.clear();
    _errors.clear();
    _selectedUserDbId = 0;
    _selectedDriveDbId = 0;
    _selectedSyncDbId = 0;

    emit usersChanged();
    emit accountsChanged();
    emit drivesChanged();
    emit availableDrivesChanged();
    emit syncsChanged();
    emit errorsChanged();
    if (userSelectionChanged) {
        emit selectedUserDbIdChanged();
    }
    if (driveSelectionChanged) {
        emit selectedDriveDbIdChanged();
    }
    if (syncSelectionChanged) {
        emit selectedSyncDbIdChanged();
    }
}

void AppCache::replaceUsers(const std::vector<UserInfo> &users) {
    _users = users;
    emit usersChanged();
}

void AppCache::replaceAccounts(const std::vector<AccountInfo> &accounts) {
    _accounts = accounts;
    emit accountsChanged();
}

void AppCache::replaceDrives(const std::vector<DriveInfo> &drives) {
    _drives = drives;
    emit drivesChanged();
}

void AppCache::replaceAvailableDrives(const std::vector<DriveAvailableInfo> &availableDrives) {
    _availableDrives = availableDrives;
    emit availableDrivesChanged();
}

void AppCache::replaceSyncs(const std::vector<SyncInfo> &syncs) {
    _syncs = syncs;
    emit syncsChanged();
}

void AppCache::replaceErrors(const std::vector<ErrorInfo> &errors) {
    _errors = errors;
    emit errorsChanged();
}

void AppCache::upsertUser(const UserInfo &info) {
    upsertById(_users, info, info.dbId(), [](const UserInfo &user) { return user.dbId(); });
    emit usersChanged();
}

void AppCache::removeUser(const UserDbId userDbId) {
    if (!removeById(_users, userDbId, [](const UserInfo &user) { return user.dbId(); })) {
        return;
    }
    if (_selectedUserDbId == userDbId) {
        _selectedUserDbId = 0;
        emit selectedUserDbIdChanged();
    }
    emit usersChanged();
}

void AppCache::upsertAccount(const AccountInfo &info) {
    upsertById(_accounts, info, info.dbId(), [](const AccountInfo &account) { return account.dbId(); });
    emit accountsChanged();
}

void AppCache::removeAccount(const AccountDbId accountDbId) {
    if (!removeById(_accounts, accountDbId, [](const AccountInfo &account) { return account.dbId(); })) {
        return;
    }
    emit accountsChanged();
}

void AppCache::upsertDrive(const DriveInfo &info) {
    upsertById(_drives, info, info.dbId(), [](const DriveInfo &drive) { return drive.dbId(); });
    emit drivesChanged();
}

void AppCache::removeDrive(const DriveDbId driveDbId) {
    if (!removeById(_drives, driveDbId, [](const DriveInfo &drive) { return drive.dbId(); })) {
        return;
    }
    if (_selectedDriveDbId == driveDbId) {
        _selectedDriveDbId = 0;
        emit selectedDriveDbIdChanged();
    }
    emit drivesChanged();
}

void AppCache::upsertSync(const SyncInfo &info) {
    upsertById(_syncs, info, info.dbId(), [](const SyncInfo &sync) { return sync.dbId(); });
    emit syncsChanged();
}

void AppCache::removeSync(const SyncDbId syncDbId) {
    if (!removeById(_syncs, syncDbId, [](const SyncInfo &sync) { return sync.dbId(); })) {
        return;
    }
    if (_selectedSyncDbId == syncDbId) {
        _selectedSyncDbId = 0;
        emit selectedSyncDbIdChanged();
    }
    emit syncsChanged();
}

void AppCache::upsertError(const ErrorInfo &info) {
    upsertById(_errors, info, info.dbId(), [](const ErrorInfo &error) { return error.dbId(); });
    emit errorsChanged();
}

void AppCache::removeError(const ErrorDbId errorDbId) {
    if (!removeById(_errors, errorDbId, [](const ErrorInfo &error) { return error.dbId(); })) {
        return;
    }
    emit errorsChanged();
}

} // namespace KDC
