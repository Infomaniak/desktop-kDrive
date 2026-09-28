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

#pragma once

#include "app/settings/settingsusersmodel.h"
#include "libcommon/utility/types.h"

#include <QObject>

#include <memory>
#include <unordered_set>

namespace KDC {

class AppCache;
class UserService;

/**
 * Process-long service for the Settings user cards.
 *
 * Product cards are anchored on UserDbId even though a user may own several backend Account nodes. Durable users,
 * accounts, drives, and synchronizations remain owned by AppCache; this service owns only request presentation state.
 */
class SettingsUserService final : public QObject {
        Q_OBJECT
        Q_PROPERTY(SettingsUsersModel *usersModel READ usersModel CONSTANT)

    public:
        SettingsUserService(AppCache &appCache, UserService &userService, QObject *parent = nullptr);
        ~SettingsUserService() override;

        [[nodiscard]] SettingsUsersModel *usersModel() const { return _usersModel.get(); }
        [[nodiscard]] bool availableDrivesLoading(UserDbId userDbId) const;
        [[nodiscard]] bool availableDrivesFailed(UserDbId userDbId) const;

        Q_INVOKABLE void refresh() const;
        Q_INVOKABLE void retryAvailableDrives(qint64 userDbId) const;
        Q_INVOKABLE void disconnectUser(qint64 userDbId);

    signals:
        void userStateChanged(UserDbId userDbId);
        void disconnectSucceeded(qint64 userDbId);
        void disconnectFailed(qint64 userDbId);

    private:
        void refreshAvailableDrives(UserDbId userDbId) const;
        void pruneMissingUsers();

        AppCache &_appCache;
        UserService &_userService;
        std::unique_ptr<SettingsUsersModel> _usersModel;
        std::unordered_set<UserDbId> _failedAvailableDriveLoads;
};

} // namespace KDC
