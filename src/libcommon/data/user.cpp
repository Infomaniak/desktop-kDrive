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

#include "user.h"

#include "libcommonserver/log/log.h"
#include "utility/utility.h"

#include <QBuffer>
#include <QImageWriter>

#include <log4cplus/loggingmacros.h>

static const auto userDbIdKey = "dbId";
static const auto userUserIdKey = "userId";
static const auto userNameKey = "name";
static const auto userFirstNameKey = "firstName";
static const auto userEmailKey = "email";
static const auto userAvatarKey = "avatar";
static const auto userAvatarUrlKey = "avatarUrl";
static const auto userConnectedKey = "isConnected";
static const auto userIsStaffKey = "isStaff";

namespace KDC {

User::User(const UserDbId dbId, const UserId userId, const std::string &keychainKey, const std::string &name,
           const std::string &firstName, const std::string &email, const std::string &avatarUrl,
           const std::shared_ptr<std::vector<char>> avatar, const bool toMigrate) :
    _dbId(dbId),
    _userId(userId),
    _keychainKey(keychainKey),
    _name(name),
    _firstName(firstName),
    _email(email),
    _avatarUrl(avatarUrl),
    _avatar(avatar),
    _toMigrate(toMigrate) {}

void User::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, userDbIdKey, _dbId);
    CommonUtility::writeValueToStruct(dstruct, userUserIdKey, _userId);
    CommonUtility::writeValueToStruct(dstruct, userNameKey, CommonUtility::str2CommString(_name));
    CommonUtility::writeValueToStruct(dstruct, userFirstNameKey, CommonUtility::str2CommString(firstName()));
    CommonUtility::writeValueToStruct(dstruct, userEmailKey, CommonUtility::str2CommString(_email));

    if (_avatar) {
        CommBLOB avatarBLOB(_avatar->begin(), _avatar->end());
        CommonUtility::writeValueToStruct(dstruct, userAvatarKey, avatarBLOB);
    }

    CommonUtility::writeValueToStruct(dstruct, userConnectedKey, _connected);
    CommonUtility::writeValueToStruct(dstruct, userIsStaffKey, _isStaff);
    CommonUtility::writeValueToStruct(dstruct, userAvatarUrlKey, CommonUtility::str2CommString(_avatarUrl));
}

void User::fromDynamicStruct(const Poco::DynamicStruct &dstruct) {
    CommonUtility::readValueFromStruct(dstruct, userDbIdKey, _dbId);
    CommonUtility::readValueFromStruct(dstruct, userUserIdKey, _userId);

    CommString name;
    CommonUtility::readValueFromStruct(dstruct, userNameKey, name);
    _name = CommonUtility::commString2Str(name);

    CommString firstName;
    CommonUtility::readValueFromStruct(dstruct, userFirstNameKey, firstName);
    _firstName = CommonUtility::commString2Str(firstName);

    CommString email;
    CommonUtility::readValueFromStruct(dstruct, userEmailKey, email);
    _email = CommonUtility::commString2Str(email);

    if (dstruct.contains(userAvatarKey)) {
        CommBLOB avatarBLOB;
        CommonUtility::readValueFromStruct(dstruct, userAvatarKey, avatarBLOB);
        _avatar = std::make_shared<std::vector<char>>(avatarBLOB.begin(), avatarBLOB.end());
    }

    CommonUtility::readValueFromStruct(dstruct, userConnectedKey, _connected);
    CommonUtility::readValueFromStruct(dstruct, userIsStaffKey, _isStaff);

    CommString avatarUrl;
    CommonUtility::readValueFromStruct(dstruct, userAvatarUrlKey, avatarUrl);
    _avatarUrl = CommonUtility::commString2Str(avatarUrl);
}

} // namespace KDC
