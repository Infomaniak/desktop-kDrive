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

#include "driveavailableinfo.h"

namespace KDC {

DriveAvailableInfo::DriveAvailableInfo(int driveId, int userId, int accountId, const QString &name, const QColor &color) :
    _driveId(driveId),
    _userId(userId),
    _accountId(accountId),
    _name(name),
    _color(color),
    _userDbId(0) {}

DriveAvailableInfo::DriveAvailableInfo() :
    _driveId(0),
    _userId(0),
    _accountId(0),
    _name(QString()),
    _color(QString()),
    _userDbId(0) {}

QDataStream &operator>>(QDataStream &in, DriveAvailableInfo &info) {
    in >> info._driveId >> info._userId >> info._accountId >> info._name >> info._color >> info._userDbId;
    return in;
}

QDataStream &operator<<(QDataStream &out, const DriveAvailableInfo &info) {
    out << info._driveId << info._userId << info._accountId << info._name << info._color << info._userDbId;
    return out;
}

QDataStream &operator<<(QDataStream &out, const QList<DriveAvailableInfo> &list) {
    int count = static_cast<int>(list.size());
    out << count;
    for (int i = 0; i < count; i++) {
        DriveAvailableInfo info = list[i];
        out << info;
    }
    return out;
}

QDataStream &operator>>(QDataStream &in, QList<DriveAvailableInfo> &list) {
    int count = 0;
    in >> count;
    for (int i = 0; i < count; i++) {
        DriveAvailableInfo info;
        in >> info;
        list.push_back(info);
    }
    return in;
}

} // namespace KDC
