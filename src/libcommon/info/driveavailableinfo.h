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

#include <QDataStream>
#include <QString>
#include <QList>
#include <QColor>

#include <Poco/Dynamic/Struct.h>

namespace KDC {

class DriveAvailableInfo {
    public:
        DriveAvailableInfo() = default;
        DriveAvailableInfo(DriveId driveId, UserId userId, AccountId accountId, const QString &accountName, const QString &name,
                           const QString &color);

        [[nodiscard]] DriveId driveId() const { return _driveId; }
        void setDriveId(const DriveId driveId) { _driveId = driveId; }
        [[nodiscard]] UserId userId() const { return _userId; }
        void setUserId(const UserId userId) { _userId = userId; }
        [[nodiscard]] AccountId accountId() const { return _accountId; }
        void setAccountId(const AccountId accountId) { _accountId = accountId; }
        [[nodiscard]] const QString &accountName() const { return _accountName; }
        void setAccountName(const QString &accountName) { _accountName = accountName; }
        [[nodiscard]] const QString &name() const { return _name; }
        void setName(const QString &name) { _name = name; }
        [[nodiscard]] const QColor &color() const { return _color; }
        void setColor(const QColor &color) { _color = color; }
        [[nodiscard]] UserDbId userDbId() const { return _userDbId; }
        void setUserDbId(const UserDbId userDbId) { _userDbId = userDbId; }

        void toDynamicStruct(Poco::DynamicStruct &dstruct) const;
        void fromDynamicStruct(const Poco::DynamicStruct &dstruct);

        friend QDataStream &operator>>(QDataStream &in, DriveAvailableInfo &info);
        friend QDataStream &operator<<(QDataStream &out, const DriveAvailableInfo &info);

        friend QDataStream &operator>>(QDataStream &in, QList<DriveAvailableInfo> &list);
        friend QDataStream &operator<<(QDataStream &out, const QList<DriveAvailableInfo> &list);

    private:
        DriveId _driveId{0};
        UserId _userId{0};
        AccountId _accountId{0};
        QString _accountName;
        QString _name;
        QColor _color;

        UserDbId _userDbId{0};
};

} // namespace KDC
