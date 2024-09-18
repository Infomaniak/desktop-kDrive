/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

namespace KDC {

class DriveAvailableInfo {
    public:
        DriveAvailableInfo(int driveId, int userId, int accountId, const QString &name, const QColor &color);
        DriveAvailableInfo();

        inline void setDriveId(int driveId) { _driveId = driveId; }
        inline int driveId() const { return _driveId; }
        inline void setUserId(int userId) { _userId = userId; }
        inline int userId() const { return _userId; }
        inline void setAccountId(int accountId) { _accountId = accountId; }
        inline int accountId() const { return _accountId; }
        inline void setColor(QColor color) { _color = color; }
        inline QColor color() const { return _color; }
        inline void setName(QString name) { _name = name; }
        inline QString name() const { return _name; }
        inline void setUserDbId(int dbId) { _userDbId = dbId; }
        inline int userDbId() const { return _userDbId; }

        friend QDataStream &operator>>(QDataStream &in, DriveAvailableInfo &info);
        friend QDataStream &operator<<(QDataStream &out, const DriveAvailableInfo &info);

        friend QDataStream &operator>>(QDataStream &in, QList<DriveAvailableInfo> &list);
        friend QDataStream &operator<<(QDataStream &out, const QList<DriveAvailableInfo> &list);

    private:
        int _driveId;
        int _userId;
        int _accountId;
        QString _name;
        QColor _color;

        int _userDbId;
};

} // namespace KDC
