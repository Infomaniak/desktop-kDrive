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

static const auto accountDbIdKey = "dbId";
static const auto accountIdKey = "id";
static const auto accountUserDbIdKey = "userDbId";
static const auto accountNameKey = "name";

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
    CommonUtility::writeValueToStruct(dstruct, accountDbIdKey, _dbId);
    CommonUtility::writeValueToStruct(dstruct, accountIdKey, _accountId);
    CommonUtility::writeValueToStruct(dstruct, accountUserDbIdKey, _userDbId);
    CommonUtility::writeValueToStruct(dstruct, accountNameKey, CommonUtility::str2CommString(_name));
}

void Account::fromDynamicStruct(const Poco::DynamicStruct &dstruct) {
    CommonUtility::readValueFromStruct(dstruct, accountDbIdKey, _dbId);
    CommonUtility::readValueFromStruct(dstruct, accountIdKey, _accountId);
    CommonUtility::readValueFromStruct(dstruct, accountUserDbIdKey, _userDbId);

    CommString name;
    CommonUtility::readValueFromStruct(dstruct, accountNameKey, name);
    _name = CommonUtility::commString2Str(name);
}

} // namespace KDC
