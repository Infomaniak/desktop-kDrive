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

#include "driveinfo.h"

namespace KDC {

DriveInfo::DriveInfo(int dbId, int accountDbId, const QString &name, const QColor &color) :
    _dbId(dbId), _accountDbId(accountDbId), _name(name), _color(color) {}

DriveInfo::DriveInfo() : _dbId(0), _accountDbId(0) {}

QDataStream &operator>>(QDataStream &in, DriveInfo &info) {
    in >> info._dbId >> info._accountDbId >> info._name >> info._color >> info._notifications >> info._maintenance >>
            info._locked >> info._accessDenied;
    return in;
}

QDataStream &operator<<(QDataStream &out, const DriveInfo &info) {
    out << info._dbId << info._accountDbId << info._name << info._color << info._notifications << info._maintenance
        << info._locked << info._accessDenied;
    return out;
}

QDataStream &operator<<(QDataStream &out, const QList<DriveInfo> &list) {
    int count = static_cast<int>(list.size());
    out << count;
    for (int i = 0; i < count; i++) {
        DriveInfo info = list[i];
        out << info;
    }
    return out;
}

QDataStream &operator>>(QDataStream &in, QList<DriveInfo> &list) {
    int count = 0;
    in >> count;
    for (int i = 0; i < count; i++) {
        DriveInfo *info = new DriveInfo();
        in >> *info;
        list.push_back(*info);
    }
    return in;
}

} // namespace KDC
