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

#include "accountinfo.h"
#include "utility/utility.h"

static const auto accountInfoDbId = "dbId";
static const auto accountInfoUserDbId = "userDbId";
static const auto accountInfoAccountId = "accountId";
static const auto accountInfoName = "name";

namespace KDC {

AccountInfo::AccountInfo(int dbId, int userDbId) :
    _dbId(dbId),
    _userDbId(userDbId) {}

void AccountInfo::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, accountInfoDbId, _dbId);
    CommonUtility::writeValueToStruct(dstruct, accountInfoUserDbId, _userDbId);
    CommonUtility::writeValueToStruct(dstruct, accountInfoAccountId, _accountId);
    CommonUtility::writeValueToStruct(dstruct, accountInfoName, CommonUtility::qStr2CommString(_name));
}

void AccountInfo::fromDynamicStruct(const Poco::DynamicStruct &dstruct) {
    CommonUtility::readValueFromStruct(dstruct, accountInfoDbId, _dbId);
    CommonUtility::readValueFromStruct(dstruct, accountInfoUserDbId, _userDbId);
    CommonUtility::readValueFromStruct(dstruct, accountInfoAccountId, _accountId);

    CommString name;
    CommonUtility::readValueFromStruct(dstruct, accountInfoName, name);
    _name = CommonUtility::commString2QStr(name);
}

QDataStream &operator>>(QDataStream &in, AccountInfo &accountInfo) {
    in >> accountInfo._dbId >> accountInfo._userDbId >> accountInfo._name;
    return in;
}

QDataStream &operator<<(QDataStream &out, const AccountInfo &accountInfo) {
    out << accountInfo._dbId << accountInfo._userDbId << accountInfo._name;
    return out;
}

QDataStream &operator<<(QDataStream &out, const QList<AccountInfo> &list) {
    int count = static_cast<int>(list.size());
    out << count;
    for (int i = 0; i < count; i++) {
        const AccountInfo &accountInfo = list[i];
        out << accountInfo;
    }
    return out;
}

QDataStream &operator>>(QDataStream &in, QList<AccountInfo> &list) {
    int count = 0;
    in >> count;
    for (int i = 0; i < count; i++) {
        AccountInfo accountInfo;
        in >> accountInfo;
        list.push_back(accountInfo);
    }
    return in;
}

} // namespace KDC
