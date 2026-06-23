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

#include "data/account.h"

#include "libcommonserver/log/log.h"
#include "utility/utility.h"

#include <log4cplus/loggingmacros.h>

static const auto accountDbId = "dbId";
static const auto accountId = "accountId";
static const auto accountUserDbId = "userDbId";
static const auto accountName = "name";

namespace KDC {

Account::Account(const AccountDbId dbId, const UserDbId userDbId) :
    _dbId(dbId),
    _userDbId(userDbId) {}

Account::Account(const AccountDbId dbId, const AccountId accountId, const UserDbId userDbId, const std::string &name) :
    _dbId(dbId),
    _accountId(accountId),
    _userDbId(userDbId),
    _name(name) {}

void Account::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, accountDbId, _dbId);
    CommonUtility::writeValueToStruct(dstruct, accountId, _accountId);
    CommonUtility::writeValueToStruct(dstruct, accountUserDbId, _userDbId);
    CommonUtility::writeValueToStruct(dstruct, accountName, CommonUtility::str2CommString(_name));
}

void Account::fromDynamicStruct(const Poco::DynamicStruct &dstruct) {
    CommonUtility::readValueFromStruct(dstruct, accountDbId, _dbId);
    CommonUtility::readValueFromStruct(dstruct, accountId, _accountId);
    CommonUtility::readValueFromStruct(dstruct, accountUserDbId, _userDbId);

    CommString name;
    CommonUtility::readValueFromStruct(dstruct, accountName, name);
    _name = CommonUtility::commString2Str(name);
}

} // namespace KDC
