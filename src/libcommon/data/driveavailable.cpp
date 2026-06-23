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

#include "data/driveavailable.h"

#include "utility/utility.h"

static const auto driveAvailableDriveId = "driveId";
static const auto driveAvailableUserId = "userId";
static const auto driveAvailableAccountId = "accountId";
static const auto driveAvailableAccountName = "accountName";
static const auto driveAvailableName = "name";
static const auto driveAvailableColor = "color";
static const auto driveAvailableUserDbId = "userDbId";

namespace KDC {

DriveAvailable::DriveAvailable(const DriveId driveId, const UserId userId, const AccountId accountId,
                               const std::string &accountName, const std::string &name, const std::string &color) :
    _driveId(driveId),
    _userId(userId),
    _accountId(accountId),
    _accountName(accountName),
    _name(name),
    _color(color) {}

void DriveAvailable::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, driveAvailableDriveId, driveId());
    CommonUtility::writeValueToStruct(dstruct, driveAvailableUserId, userId());
    CommonUtility::writeValueToStruct(dstruct, driveAvailableAccountId, accountId());
    CommonUtility::writeValueToStruct(dstruct, driveAvailableAccountName, CommonUtility::str2CommString(accountName()));
    CommonUtility::writeValueToStruct(dstruct, driveAvailableName, CommonUtility::str2CommString(name()));
    CommonUtility::writeValueToStruct(dstruct, driveAvailableColor, CommonUtility::str2CommString(color()));
    CommonUtility::writeValueToStruct(dstruct, driveAvailableUserDbId, userDbId());
}

void DriveAvailable::fromDynamicStruct(const Poco::DynamicStruct &dstruct) {
    CommonUtility::readValueFromStruct(dstruct, driveAvailableDriveId, _driveId);
    CommonUtility::readValueFromStruct(dstruct, driveAvailableUserId, _userId);
    CommonUtility::readValueFromStruct(dstruct, driveAvailableAccountId, _accountId);

    CommString accountName;
    if (dstruct.contains(driveAvailableAccountName)) {
        CommonUtility::readValueFromStruct(dstruct, driveAvailableAccountName, accountName);
        _accountName = CommonUtility::commString2Str(accountName);
    }

    CommString name;
    if (dstruct.contains(driveAvailableName)) {
        CommonUtility::readValueFromStruct(dstruct, driveAvailableName, name);
        _name = CommonUtility::commString2Str(name);
    }

    CommString color;
    if (dstruct.contains(driveAvailableColor)) {
        CommonUtility::readValueFromStruct(dstruct, driveAvailableColor, color);
        _color = CommonUtility::commString2Str(color);
    }

    if (dstruct.contains(driveAvailableUserDbId)) {
        CommonUtility::readValueFromStruct(dstruct, driveAvailableUserDbId, _userDbId);
    }
}

} // namespace KDC
