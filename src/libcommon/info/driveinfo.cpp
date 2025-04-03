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

#include "driveinfo.h"

namespace KDC {

DriveInfo::DriveInfo() {}

void operator>>(QDataStream &in, DriveInfo &info) {
    int dbId{0};
    int id{0};
    int accountDbId{0};
    QString name;
    QColor color;
    bool notifications{false};
    bool admin{false};
    bool maintenance{false};
    bool locked{false};
    bool accessDenied{false};

    in >> dbId >> id >> accountDbId >> name >> color >> notifications >> admin >> maintenance >> locked >> accessDenied;

    info.setDbId(dbId);
    info.setId(id);
    info.setAccountDbId(accountDbId);
    info.setName(name);
    info.setColor(color);
    info.setNotifications(notifications);
    info.setAdmin(admin);
    info.setMaintenance(maintenance);
    info.setLocked(locked);
    info.setAccessDenied(accessDenied);
}

QDataStream &operator<<(QDataStream &out, const DriveInfo &info) {
    out << info.dbId() << info.id() << info.accountDbId() << info.name() << info.color() << info.notifications() << info.admin()
        << info.maintenance() << info.locked() << info.accessDenied();
    return out;
}

void operator>>(QDataStream &in, QList<DriveInfo> &list) {
    int count = 0;
    in >> count;
    for (int i = 0; i < count; i++) {
        DriveInfo info;
        in >> info;
        list.push_back(info);
    }
}

QDataStream &operator<<(QDataStream &out, const QList<DriveInfo> &list) {
    int count = static_cast<int>(list.size());
    out << count;
    for (int i = 0; i < count; i++) {
        out << list[i];
    }
    return out;
}

} // namespace KDC
