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

#include "userinfo.h"
#include "utility/types.h"
#include "utility/utility.h"

#include <QImageWriter>
#include <QBuffer>

static const auto userInfoDbId = "dbId";
static const auto userInfoUserId = "userId";
static const auto userInfoName = "name";
static const auto userInfoFirstName = "firstName";
static const auto userInfoEmail = "email";
static const auto userInfoAvatar = "avatar";
static const auto userInfoAvatarUrl = "avatarUrl";
static const auto userInfoConnected = "isConnected";
static const auto userInfoIsStaff = "isStaff";

namespace KDC {

UserInfo::UserInfo(const UserDbId dbId, const UserId userId, const QString &name, const QString &firstName, const QString &email, const QImage &avatar,
                   const bool connected) :
    _dbId(dbId),
    _userId(userId),
    _name(name),
    _firstName(firstName),
    _email(email),
    _avatar(avatar),
    _connected(connected) {}

void UserInfo::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, userInfoDbId, _dbId);
    CommonUtility::writeValueToStruct(dstruct, userInfoUserId, _userId);
    CommonUtility::writeValueToStruct(dstruct, userInfoName, CommonUtility::qStr2CommString(_name));
    CommonUtility::writeValueToStruct(dstruct, userInfoFirstName, CommonUtility::qStr2CommString(_firstName));
    CommonUtility::writeValueToStruct(dstruct, userInfoEmail, CommonUtility::qStr2CommString(_email));

    QByteArray avatarQBA;
    QBuffer buffer(&avatarQBA);
    if (buffer.open(QIODevice::WriteOnly) && _avatar.save(&buffer, "PNG")) {
        buffer.close();
        CommBLOB avatarBLOB(avatarQBA.begin(), avatarQBA.end());
        CommonUtility::writeValueToStruct(dstruct, userInfoAvatar, avatarBLOB);
    }

    CommonUtility::writeValueToStruct(dstruct, userInfoConnected, _connected);
    CommonUtility::writeValueToStruct(dstruct, userInfoIsStaff, _isStaff);
    CommonUtility::writeValueToStruct(dstruct, userInfoAvatarUrl, CommonUtility::qStr2CommString(_avatarUrl));
}

void UserInfo::fromDynamicStruct(const Poco::DynamicStruct &dstruct) {
    CommonUtility::readValueFromStruct(dstruct, userInfoDbId, _dbId);
    CommonUtility::readValueFromStruct(dstruct, userInfoUserId, _userId);

    CommString name;
    CommonUtility::readValueFromStruct(dstruct, userInfoName, name);
    _name = CommonUtility::commString2QStr(name);

    CommString firstName;
    CommonUtility::readValueFromStruct(dstruct, userInfoFirstName, firstName);
    _firstName = CommonUtility::commString2QStr(firstName);

    CommString email;
    CommonUtility::readValueFromStruct(dstruct, userInfoEmail, email);
    _email = CommonUtility::commString2QStr(email);

    CommBLOB avatarBLOB;
    CommonUtility::readValueFromStruct(dstruct, userInfoAvatar, avatarBLOB);
    QByteArray avatarQBA;
    (void) std::copy(avatarBLOB.begin(), avatarBLOB.end(), std::back_inserter(avatarQBA));
    (void) _avatar.loadFromData(avatarQBA);

    CommonUtility::readValueFromStruct(dstruct, userInfoConnected, _connected);
    CommonUtility::readValueFromStruct(dstruct, userInfoIsStaff, _isStaff);

    CommString avatarUrl;
    CommonUtility::readValueFromStruct(dstruct, userInfoAvatarUrl, avatarUrl);
    _avatarUrl = CommonUtility::commString2QStr(avatarUrl);
}

QDataStream &operator>>(QDataStream &in, UserInfo &userInfo) {
    qint64 userDbId = 0;
    qint64 userId = 0;
    in >> userDbId >> userId >> userInfo._name >> userInfo._firstName >> userInfo._email >> userInfo._avatar >>
            userInfo._connected >> userInfo._isStaff >> userInfo._avatarUrl;
    userInfo.setDbId(static_cast<UserDbId>(userDbId));
    userInfo.setUserId(static_cast<UserId>(userId));
    return in;
}

QDataStream &operator<<(QDataStream &out, const UserInfo &userInfo) {
    out << static_cast<qint64>(userInfo._dbId) << static_cast<qint64>(userInfo._userId) << userInfo._name << userInfo._firstName
        << userInfo._email << userInfo._avatar << userInfo._connected << userInfo._isStaff << userInfo._avatarUrl;
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
