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

#include "userinfo.h"
#include "libcommon/utility/types.h"
#include "libcommon/utility/utility.h"

static const auto userInfoDbId = "dbId";
static const auto userInfoUserId = "userId";
static const auto userInfoName = "name";
static const auto userInfoEmail = "email";
static const auto userInfoAvatar = "avatar";
static const auto userInfoConnected = "connected";
static const auto userInfoIsStaff = "isStaff";

namespace KDC {

UserInfo::UserInfo(const int dbId, const int userId, const QString &name, const QString &email, const QImage &avatar,
                   const bool connected) :
    _dbId(dbId),
    _userId(userId),
    _name(name),
    _email(email),
    _avatar(avatar),
    _connected(connected) {}

UserInfo::UserInfo() {}

void UserInfo::toDynamicStruct(Poco::DynamicStruct &str) const {
    CommonUtility::writeValueToStruct(str, userInfoDbId, _dbId);
    CommonUtility::writeValueToStruct(str, userInfoUserId, _userId);
    CommonUtility::writeValueToStruct(str, userInfoName, CommonUtility::qStr2CommString(_name));
    CommonUtility::writeValueToStruct(str, userInfoEmail, CommonUtility::qStr2CommString(_email));

    QByteArray avatarQBA = QByteArray::fromRawData((const char *) _avatar.bits(), _avatar.sizeInBytes());
    CommBLOB avatarBLOB(avatarQBA.begin(), avatarQBA.end());
    CommonUtility::writeValueToStruct(str, userInfoAvatar, avatarBLOB);

    CommonUtility::writeValueToStruct(str, userInfoConnected, _connected);
    CommonUtility::writeValueToStruct(str, userInfoIsStaff, _isStaff);
}

void UserInfo::fromDynamicStruct(const Poco::DynamicStruct &str) {
    CommonUtility::readValueFromStruct(str, userInfoDbId, _dbId);
    CommonUtility::readValueFromStruct(str, userInfoUserId, _userId);

    CommString name;
    CommonUtility::readValueFromStruct(str, userInfoName, name);
    _name = CommonUtility::commString2QStr(name);

    CommString email;
    CommonUtility::readValueFromStruct(str, userInfoEmail, email);
    _email = CommonUtility::commString2QStr(email);

    CommonUtility::readValueFromStruct(str, userInfoConnected, _connected);
    CommonUtility::readValueFromStruct(str, userInfoIsStaff, _isStaff);
}

QDataStream &operator>>(QDataStream &in, UserInfo &userInfo) {
    in >> userInfo._dbId >> userInfo._userId >> userInfo._name >> userInfo._email >> userInfo._avatar >> userInfo._connected >>
            userInfo._isStaff;
    return in;
}

QDataStream &operator<<(QDataStream &out, const UserInfo &userInfo) {
    out << userInfo._dbId << userInfo._userId << userInfo._name << userInfo._email << userInfo._avatar << userInfo._connected
        << userInfo._isStaff;
    return out;
}

QDataStream &operator<<(QDataStream &out, const QList<UserInfo> &list) {
    const auto count = static_cast<int>(list.size());
    out << count;
    for (int i = 0; i < count; i++) {
        const UserInfo &userInfo = list[i];
        out << userInfo;
    }
    return out;
}

QDataStream &operator>>(QDataStream &in, QList<UserInfo> &list) {
    int count = 0;
    in >> count;
    for (int i = 0; i < count; i++) {
        UserInfo userInfo;
        in >> userInfo;
        list.push_back(userInfo);
    }
    return in;
}

} // namespace KDC
