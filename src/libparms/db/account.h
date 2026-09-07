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

namespace KDC {

class Account {
    public:
        Account() = default;
        Account(AccountDbId dbId, AccountId accountId, UserDbId userDbId, const std::string &name);

        [[nodiscard]] AccountDbId dbId() const { return _dbId; }
        void setDbId(const AccountDbId dbId) { _dbId = dbId; }
        [[nodiscard]] AccountId accountId() const { return _accountId; }
        void setAccountId(const AccountId accountId) { _accountId = accountId; }
        [[nodiscard]] UserDbId userDbId() const { return _userDbId; }
        void setUserDbId(const UserDbId userDbId) { _userDbId = userDbId; }
        [[nodiscard]] std::string name() const { return _name; }
        void setName(const std::string &name) { _name = name; }

    private:
        AccountDbId _dbId{0};
        AccountId _accountId{0};
        UserDbId _userDbId{0};
        std::string _name;
};

} // namespace KDC
