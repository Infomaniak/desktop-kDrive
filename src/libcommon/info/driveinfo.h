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

namespace KDC {

class DriveInfo {
    public:
        DriveInfo();

        void setDbId(const int driveDbId) { _dbId = driveDbId; }
        int dbId() const { return _dbId; }
        void setId(const int driveId) { _id = driveId; }
        int id() const { return _id; }
        void setAccountDbId(const int accountDbId) { _accountDbId = accountDbId; }
        int accountDbId() const { return _accountDbId; }
        void setName(const QString &name) { _name = name; }
        const QString &name() const { return _name; }
        void setColor(const QColor &color) { _color = color; }
        const QColor &color() const { return _color; }
        bool notifications() const { return _notifications; }
        void setNotifications(const bool notifications) { _notifications = notifications; }
        bool admin() const { return _admin; }
        void setAdmin(const bool admin) { _admin = admin; }

        bool maintenance() const { return _maintenance; }
        void setMaintenance(const bool maintenance) { _maintenance = maintenance; }
        bool locked() const { return _locked; }
        void setLocked(const bool newLocked) { _locked = newLocked; }
        bool accessDenied() const { return _accessDenied; }
        void setAccessDenied(const bool accessDenied) { _accessDenied = accessDenied; }

        friend void operator>>(QDataStream &in, DriveInfo &info);
        friend QDataStream &operator<<(QDataStream &out, const DriveInfo &info);

        friend void operator>>(QDataStream &in, QList<DriveInfo> &list);
        friend QDataStream &operator<<(QDataStream &out, const QList<DriveInfo> &list);

    protected:
        int _dbId{0};
        int _id{0};
        int _accountDbId{0};
        QString _name;
        QColor _color;
        bool _notifications{false};
        bool _admin{false};

        // Non DB attributes
        bool _maintenance{false};
        bool _locked{false};
        bool _accessDenied{false};
};

} // namespace KDC
