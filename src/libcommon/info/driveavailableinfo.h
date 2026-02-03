/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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
        DriveAvailableInfo(int driveId, int userId, int accountId, const QString &accountName, const QString &name,
                           const QString &color);

        [[nodiscard]] int driveId() const { return _driveId; }
        void setDriveId(int driveId) { _driveId = driveId; }
        [[nodiscard]] int userId() const { return _userId; }
        void setUserId(int userId) { _userId = userId; }
        [[nodiscard]] int accountId() const { return _accountId; }
        void setAccountId(int accountId) { _accountId = accountId; }
        [[nodiscard]] const QString &accountName() const { return _accountName; }
        void setAccountName(const QString &accountName) { _accountName = accountName; }
        [[nodiscard]] const QString &name() const { return _name; }
        void setName(const QString &name) { _name = name; }
        [[nodiscard]] const QColor &color() const { return _color; }
        void setColor(const QColor &color) { _color = color; }
        [[nodiscard]] int userDbId() const { return _userDbId; }
        void setUserDbId(int userDbId) { _userDbId = userDbId; }

        void toDynamicStruct(Poco::DynamicStruct &dstruct) const;
        void fromDynamicStruct(const Poco::DynamicStruct &dstruct);

        friend QDataStream &operator>>(QDataStream &in, DriveAvailableInfo &info);
        friend QDataStream &operator<<(QDataStream &out, const DriveAvailableInfo &info);

        friend QDataStream &operator>>(QDataStream &in, QList<DriveAvailableInfo> &list);
        friend QDataStream &operator<<(QDataStream &out, const QList<DriveAvailableInfo> &list);

    private:
        int _driveId{0};
        int _userId{0};
        int _accountId{0};
        QString _accountName;
        QString _name;
        QColor _color;

        int _userDbId{0};
};

} // namespace KDC
