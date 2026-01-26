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

#include "driveavailableinfo.h"
#include "utility/utility.h"

static const auto driveAvailableInfoDriveId = "driveId";
static const auto driveAvailableInfoUserId = "userId";
static const auto driveAvailableInfoAccountId = "accountId";
static const auto driveAvailableInfoAccountName = "accountName";
static const auto driveAvailableInfoName = "name";
static const auto driveAvailableInfoColor = "color";
static const auto driveAvailableInfoUserDbId = "userDbId";

namespace KDC {

DriveAvailableInfo::DriveAvailableInfo(int driveId, int userId, int accountId, const std::string &accountName,
                                       const std::string &name, const std::string &color) :
    _driveId(driveId),
    _userId(userId),
    _accountId(accountId),
    _accountName(accountName),
    _name(name),
    _color(color) {}


void DriveAvailableInfo::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, driveAvailableInfoDriveId, _driveId);
    CommonUtility::writeValueToStruct(dstruct, driveAvailableInfoUserId, _userId);
    CommonUtility::writeValueToStruct(dstruct, driveAvailableInfoAccountId, _accountId);
    CommonUtility::writeValueToStruct(dstruct, driveAvailableInfoAccountName, _accountName);
    CommonUtility::writeValueToStruct(dstruct, driveAvailableInfoName, _name);
    CommonUtility::writeValueToStruct(dstruct, driveAvailableInfoColor, _color);
    CommonUtility::writeValueToStruct(dstruct, driveAvailableInfoUserDbId, _userDbId);
}

void DriveAvailableInfo::fromDynamicStruct(const Poco::DynamicStruct &dstruct) {
    CommonUtility::readValueFromStruct(dstruct, driveAvailableInfoDriveId, _driveId);
    CommonUtility::readValueFromStruct(dstruct, driveAvailableInfoUserId, _userId);
    CommonUtility::readValueFromStruct(dstruct, driveAvailableInfoAccountId, _accountId);

    CommString str;
    CommonUtility::readValueFromStruct(dstruct, driveAvailableInfoAccountName, str);
    _accountName = CommonUtility::commString2Str(str);

    CommonUtility::readValueFromStruct(dstruct, driveAvailableInfoName, str);
    _name = CommonUtility::commString2Str(str);

    CommonUtility::readValueFromStruct(dstruct, driveAvailableInfoColor, str);
    _color = CommonUtility::commString2Str(str);

    CommonUtility::readValueFromStruct(dstruct, driveAvailableInfoUserDbId, _userDbId);
}

QDataStream &operator>>(QDataStream &in, DriveAvailableInfo &info) {
    QString accountName;
    QString driveName;
    QString driveColor;
    in >> info._driveId >> info._userId >> info._accountId >> accountName >> driveName >> driveColor >> info._userDbId;
    info.setAccountName(accountName.toStdString());
    info.setName(driveName.toStdString());
    info.setColor(driveColor.toStdString());
    return in;
}

QDataStream &operator<<(QDataStream &out, const DriveAvailableInfo &info) {
    out << info._driveId << info._userId << info._accountId << QString::fromStdString(info._accountName)
        << QString::fromStdString(info._name) << QColor(QString::fromStdString(info._color)) << info._userDbId;
    return out;
}

QDataStream &operator<<(QDataStream &out, const QList<DriveAvailableInfo> &list) {
    int count = static_cast<int>(list.size());
    out << count;
    for (int i = 0; i < count; i++) {
        const auto &info = list[i];
        out << info;
    }
    return out;
}

QDataStream &operator>>(QDataStream &in, QList<DriveAvailableInfo> &list) {
    int count = 0;
    in >> count;
    for (int i = 0; i < count; i++) {
        DriveAvailableInfo info;
        in >> info;
        list.push_back(info);
    }
    return in;
}

} // namespace KDC
