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
        DriveInfo(int dbId, int accountDbId, const QString &name, const QColor &color);
        DriveInfo();

        inline void setDbId(int driveDbId) { _dbId = driveDbId; }
        inline int dbId() const { return _dbId; }
        inline void setAccountDbId(int accountDbId) { _accountDbId = accountDbId; }
        inline int accountDbId() const { return _accountDbId; }
        inline void setName(const QString &name) { _name = name; }
        inline const QString &name() const { return _name; }
        inline void setColor(const QColor &color) { _color = color; }
        inline const QColor &color() const { return _color; }
        inline bool notifications() const { return _notifications; }
        inline void setNotifications(bool notifications) { _notifications = notifications; }
        inline bool admin() const { return _admin; }
        inline void setAdmin(bool admin) { _admin = admin; }

        inline bool maintenance() const { return _maintenance; }
        inline void setMaintenance(bool maintenance) { _maintenance = maintenance; }
        inline bool locked() const { return _locked; }
        inline void setLocked(bool newLocked) { _locked = newLocked; }
        inline bool accessDenied() const { return _accessDenied; }
        inline void setAccessDenied(bool accessDenied) { _accessDenied = accessDenied; }

        friend QDataStream &operator>>(QDataStream &in, DriveInfo &info);
        friend QDataStream &operator<<(QDataStream &out, const DriveInfo &info);

        friend QDataStream &operator>>(QDataStream &in, QList<DriveInfo> &list);
        friend QDataStream &operator<<(QDataStream &out, const QList<DriveInfo> &list);

    protected:
        int _dbId;
        int _accountDbId;
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
