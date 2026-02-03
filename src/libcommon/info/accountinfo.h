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

#include <QImage>
#include <QDataStream>
#include <QString>
#include <QList>

#include <Poco/Dynamic/Struct.h>

namespace KDC {

class AccountInfo {
    public:
        AccountInfo() = default;
        AccountInfo(int dbId, int userDbId);

        [[nodiscard]] int dbId() const { return _dbId; }
        void setDbId(const int dbId) { _dbId = dbId; }
        [[nodiscard]] int userDbId() const { return _userDbId; }
        void setUserDbId(const int userDbId) { _userDbId = userDbId; }
        [[nodiscard]] int id() const { return _id; }
        void setId(const int accountId) { _id = accountId; }
        [[nodiscard]] std::string name() const { return _name; }
        void setName(const std::string &name) { _name = name; }

        void toDynamicStruct(Poco::DynamicStruct &dstruct) const;
        void fromDynamicStruct(const Poco::DynamicStruct &dstruct);

        friend QDataStream &operator>>(QDataStream &in, AccountInfo &userInfo);
        friend QDataStream &operator<<(QDataStream &out, const AccountInfo &userInfo);

        friend QDataStream &operator>>(QDataStream &in, QList<AccountInfo> &list);
        friend QDataStream &operator<<(QDataStream &out, const QList<AccountInfo> &list);


    private:
        int _dbId{0};
        int _userDbId{0};
        int _id{-1};
        int _accountId{-1}; // Ignored by the old client-server communication
        std::string _name;
};

} // namespace KDC
