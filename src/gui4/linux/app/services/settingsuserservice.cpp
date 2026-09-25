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

#include "settingsuserservice.h"

#include "app/cache/appcache.h"
#include "app/services/userservice.h"
#include "app/settings/settingsusersmodel.h"

namespace KDC {

SettingsUserService::SettingsUserService(AppCache &appCache, UserService &userService, QObject *const parent) :
    QObject(parent),
    _appCache(appCache),
    _userService(userService),
    _usersModel(std::make_unique<SettingsUsersModel>(_appCache, *this)) {
    (void) connect(&_userService, &UserService::availableDrivesLoadingChanged, this, [this](const UserDbId userDbId) {
        if (_userService.isLoadAvailableDrivesPending(userDbId)) {
            (void) _failedAvailableDriveLoads.erase(userDbId);
        }
        emit userStateChanged(userDbId);
    });
    (void) connect(&_userService, &UserService::availableDrivesLoaded, this, [this](const UserDbId userDbId) {
        (void) _failedAvailableDriveLoads.erase(userDbId);
        emit userStateChanged(userDbId);
    });
    (void) connect(&_userService, &UserService::availableDrivesLoadFailed, this, [this](const UserDbId userDbId) {
        if (_appCache.user(userDbId).has_value()) {
            (void) _failedAvailableDriveLoads.insert(userDbId);
            emit userStateChanged(userDbId);
        }
    });
    (void) connect(&_appCache, &AppCache::usersChanged, this, &SettingsUserService::pruneMissingUsers);
}

SettingsUserService::~SettingsUserService() = default;

bool SettingsUserService::availableDrivesLoading(const UserDbId userDbId) const {
    return _userService.isLoadAvailableDrivesPending(userDbId) && _appCache.availableDrives(userDbId).empty();
}

bool SettingsUserService::availableDrivesFailed(const UserDbId userDbId) const {
    return _failedAvailableDriveLoads.contains(userDbId);
}

void SettingsUserService::refresh() const {
    for (const auto &user: _appCache.users()) {
        refreshAvailableDrives(user.dbId());
    }
}

void SettingsUserService::retryAvailableDrives(const qint64 userDbId) const {
    refreshAvailableDrives(static_cast<UserDbId>(userDbId));
}

void SettingsUserService::refreshAvailableDrives(const UserDbId userDbId) const {
    // Reuse an in-flight request, e.g. one started by onboarding: a new one would supersede it and duplicate the IPC call.
    if (!_appCache.user(userDbId).has_value() || _userService.isLoadAvailableDrivesPending(userDbId)) {
        return;
    }

    _userService.loadAvailableDrives(userDbId);
}

void SettingsUserService::pruneMissingUsers() {
    (void) std::erase_if(_failedAvailableDriveLoads,
                         [this](const UserDbId userDbId) { return !_appCache.user(userDbId).has_value(); });
}

} // namespace KDC
