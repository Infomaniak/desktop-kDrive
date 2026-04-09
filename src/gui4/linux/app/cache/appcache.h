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

#include "libcommon/utility/types.h"
#include "libcommon/info/accountinfo.h"
#include "libcommon/info/driveavailableinfo.h"
#include "libcommon/info/driveinfo.h"
#include "libcommon/info/errorinfo.h"
#include "libcommon/info/syncinfo.h"
#include "libcommon/info/userinfo.h"

#include <QObject>

#include <vector>

namespace KDC {

/**
 * Central observable cache for Linux v4 UI-facing state.
 *
 * The cache stores typed snapshots and incremental updates coming from C++ services.
 * QML-facing services and list models should read from this cache instead of parsing
 * transport payloads directly.
 */
class AppCache : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool connected READ connected WRITE setConnected NOTIFY connectedChanged)
        Q_PROPERTY(qint64 selectedUserDbId READ selectedUserDbId WRITE setSelectedUserDbId NOTIFY selectedUserDbIdChanged)
        Q_PROPERTY(qint64 selectedDriveDbId READ selectedDriveDbId WRITE setSelectedDriveDbId NOTIFY selectedDriveDbIdChanged)
        Q_PROPERTY(qint64 selectedSyncDbId READ selectedSyncDbId WRITE setSelectedSyncDbId NOTIFY selectedSyncDbIdChanged)

    public:
        explicit AppCache(QObject *parent = nullptr);

        bool connected() const { return _connected; }
        qint64 selectedUserDbId() const;
        qint64 selectedDriveDbId() const;
        qint64 selectedSyncDbId() const;

        const std::vector<UserInfo> &users() const { return _users; }
        const std::vector<AccountInfo> &accounts() const { return _accounts; }
        const std::vector<DriveInfo> &drives() const { return _drives; }
        // Available drives for the currently selected user context.
        const std::vector<DriveAvailableInfo> &availableDrives() const { return _availableDrives; }
        const std::vector<SyncInfo> &syncs() const { return _syncs; }
        const std::vector<ErrorInfo> &errors() const { return _errors; }

        void setConnected(bool connected);
        void setSelectedUserDbId(qint64 userDbId);
        void setSelectedDriveDbId(qint64 driveDbId);
        void setSelectedSyncDbId(qint64 syncDbId);

        void clearAll();

        void replaceUsers(const std::vector<UserInfo> &users);
        void replaceAccounts(const std::vector<AccountInfo> &accounts);
        void replaceDrives(const std::vector<DriveInfo> &drives);
        // Replaces the current selected-user available drives snapshot.
        void replaceAvailableDrives(const std::vector<DriveAvailableInfo> &availableDrives);
        void replaceSyncs(const std::vector<SyncInfo> &syncs);
        void replaceErrors(const std::vector<ErrorInfo> &errors);

    public slots:
        void upsertUser(const UserInfo &info);
        void removeUser(UserDbId userDbId);

        void upsertAccount(const AccountInfo &info);
        void removeAccount(AccountDbId accountDbId);

        void upsertDrive(const DriveInfo &info);
        void removeDrive(DriveDbId driveDbId);

        void upsertSync(const SyncInfo &info);
        void removeSync(SyncDbId syncDbId);

        void upsertError(const ErrorInfo &info);
        void removeError(ErrorDbId errorDbId);

    signals:
        void connectedChanged();
        void selectedUserDbIdChanged();
        void selectedDriveDbIdChanged();
        void selectedSyncDbIdChanged();

        void usersChanged();
        void accountsChanged();
        void drivesChanged();
        void availableDrivesChanged();
        void syncsChanged();
        void errorsChanged();

    private:
        bool _connected{false};
        UserDbId _selectedUserDbId{0};
        DriveDbId _selectedDriveDbId{0};
        SyncDbId _selectedSyncDbId{0};

        std::vector<UserInfo> _users;
        std::vector<AccountInfo> _accounts;
        std::vector<DriveInfo> _drives;
        std::vector<DriveAvailableInfo> _availableDrives;
        std::vector<SyncInfo> _syncs;
        std::vector<ErrorInfo> _errors;
};

} // namespace KDC
