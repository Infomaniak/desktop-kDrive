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

#include <Poco/Dynamic/Struct.h>

#include <string>

namespace KDC {

class Account {
    public:
        Account() = default;
        Account(AccountDbId dbId, UserDbId userDbId);
        Account(AccountDbId dbId, AccountId accountId, UserDbId userDbId, const std::string &name);

        [[nodiscard]] AccountDbId dbId() const { return _dbId; }
        void setDbId(const AccountDbId dbId) { _dbId = dbId; }
        [[nodiscard]] AccountId accountId() const { return _accountId; }
        void setAccountId(const AccountId accountId) { _accountId = accountId; }
        [[nodiscard]] UserDbId userDbId() const { return _userDbId; }
        void setUserDbId(const UserDbId userDbId) { _userDbId = userDbId; }
        [[nodiscard]] const std::string &name() const { return _name; }
        void setName(const std::string &name) { _name = name; }

        void toDynamicStruct(Poco::DynamicStruct &dstruct) const;
        void fromDynamicStruct(const Poco::DynamicStruct &dstruct);

        /// TODO : to be removed once we moved to the new GUI ///
        friend void operator>>(QDataStream &in, Account &account) {
            qint64 dbId{0};
            qint64 userDbId{0};
            QString name;
            in >> dbId >> userDbId >> name;
            account.setDbId(static_cast<AccountDbId>(dbId));
            account.setUserDbId(static_cast<UserDbId>(userDbId));
            account.setName(name.toStdString());
        }
        friend QDataStream &operator<<(QDataStream &out, const Account &account) {
            out << static_cast<qint64>(account.dbId()) << static_cast<qint64>(account.userDbId())
                << QString::fromStdString(account.name());
            return out;
        }

        friend void operator>>(QDataStream &in, QList<Account> &list) {
            qint64 count = 0;
            in >> count;
            for (qint64 i = 0; i < count; i++) {
                Account account;
                in >> account;
                list.push_back(account);
            }
        }
        friend QDataStream &operator<<(QDataStream &out, const QList<Account> &list) {
            const auto count = static_cast<qint64>(list.size());
            out << count;
            for (qint64 i = 0; i < count; i++) {
                out << list[static_cast<qsizetype>(i)];
            }
            return out;
        }
        /////////////////////////////////////////////////////////

        bool operator==(const Account &other) const = default;

    private:
        AccountDbId _dbId{0};
        AccountId _accountId{0};
        UserDbId _userDbId{0};
        std::string _name;
};

using AccountList = std::vector<Account>;

} // namespace KDC
