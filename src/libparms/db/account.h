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
#include "utility/types.h"

namespace KDC {

class Account {
    public:
        Account() = default;
        Account(int dbId, int accountId, int userDbId, const std::string &name);

        [[nodiscard]] int dbId() const { return _dbId; }
        void setDbId(int dbId) { _dbId = dbId; }
        [[nodiscard]] int accountId() const { return _accountId; }
        void setAccountId(int accountId) { _accountId = accountId; }
        [[nodiscard]] int userDbId() const { return _userDbId; }
        void setUserDbId(int userDbId) { _userDbId = userDbId; }
        [[nodiscard]] std::string name() const { return _name; }
        void setName(const std::string &name) { _name = name; }

    private:
        int _dbId{0};
        int _accountId{0};
        int _userDbId{0};
        std::string _name;
};

} // namespace KDC
