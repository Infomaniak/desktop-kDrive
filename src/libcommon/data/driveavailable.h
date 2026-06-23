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

#include "utility/types.h"

#include <QDataStream>
#include <QList>
#include <QColor>

#include <Poco/Dynamic/Struct.h>

#include <string>

namespace KDC {

class DriveAvailable {
    public:
        DriveAvailable() = default;
        DriveAvailable(DriveId driveId, UserId userId, AccountId accountId, const std::string &accountName,
                       const std::string &name, const std::string &color);

        [[nodiscard]] DriveId driveId() const { return _driveId; }
        void setDriveId(const DriveId driveId) { _driveId = driveId; }
        [[nodiscard]] UserId userId() const { return _userId; }
        void setUserId(const UserId userId) { _userId = userId; }
        [[nodiscard]] AccountId accountId() const { return _accountId; }
        void setAccountId(const AccountId accountId) { _accountId = accountId; }
        [[nodiscard]] const std::string &accountName() const { return _accountName; }
        void setAccountName(const std::string &accountName) { _accountName = accountName; }
        [[nodiscard]] const std::string &name() const { return _name; }
        void setName(const std::string &name) { _name = name; }
        [[nodiscard]] const std::string &color() const { return _color; }
        void setColor(const std::string &color) { _color = color; }
        [[nodiscard]] UserDbId userDbId() const { return _userDbId; }
        void setUserDbId(const UserDbId userDbId) { _userDbId = userDbId; }

        void toDynamicStruct(Poco::DynamicStruct &dstruct) const;
        void fromDynamicStruct(const Poco::DynamicStruct &dstruct);

        /// TODO : to be removed once we moved to the new GUI ///
        friend void operator>>(QDataStream &in, DriveAvailable &info) {
            qint64 driveId{0};
            qint64 userId{0};
            qint64 accountId{0};
            QString accountName;
            QString name;
            QColor color;
            qint64 userDbId{0};

            in >> driveId >> userId >> accountId >> accountName >> name >> color >> userDbId;

            info.setDriveId(static_cast<DriveId>(driveId));
            info.setUserId(static_cast<UserId>(userId));
            info.setAccountId(static_cast<AccountId>(accountId));
            info.setAccountName(accountName.toStdString());
            info.setName(name.toStdString());
            info.setColor(color.name().toStdString());
            info.setUserDbId(static_cast<UserDbId>(userDbId));
        }
        friend QDataStream &operator<<(QDataStream &out, const DriveAvailable &info) {
            out << static_cast<qint64>(info.driveId()) << static_cast<qint64>(info.userId())
                << static_cast<qint64>(info.accountId()) << QString::fromStdString(info.accountName())
                << QString::fromStdString(info.name()) << QColor(QString::fromStdString(info.color()))
                << static_cast<qint64>(info.userDbId());
            return out;
        }

        friend void operator>>(QDataStream &in, QList<DriveAvailable> &list) {
            qint64 count = 0;
            in >> count;
            for (qint64 i = 0; i < count; i++) {
                DriveAvailable info;
                in >> info;
                list.push_back(info);
            }
        }
        friend QDataStream &operator<<(QDataStream &out, const QList<DriveAvailable> &list) {
            const auto count = static_cast<qint64>(list.size());
            out << count;
            for (qint64 i = 0; i < count; i++) {
                out << list[static_cast<qsizetype>(i)];
            }
            return out;
        }
        /////////////////////////////////////////////////////////

        bool operator==(const DriveAvailable &other) const = default;

    private:
        DriveId _driveId{0};
        UserId _userId{0};
        AccountId _accountId{0};
        std::string _accountName;
        std::string _name;
        std::string _color; // #RRGGBB format

        UserDbId _userDbId{0};
};

using DriveAvailableList = std::vector<DriveAvailable>;

} // namespace KDC
