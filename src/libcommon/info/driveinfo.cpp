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

#include "driveinfo.h"
#include "utility/utility.h"

static const auto driveInfoDbId = "dbId";
static const auto driveInfoId = "id";
static const auto driveInfoAccountDbId = "accountDbId";
static const auto driveInfoName = "name";
static const auto driveInfoSize = "size";
static const auto driveInfoColor = "color";
static const auto driveInfoNotifications = "notifications";
static const auto driveInfoAdmin = "admin";
static const auto driveInfoMaintenance = "maintenance";
static const auto driveInfoLocked = "locked";
static const auto driveInfoUsedSize = "usedSize";
static const auto driveInfoAccessDenied = "accessDenied";
static const auto driveInfoPackInfo = "packInfo";

namespace KDC {

void DriveInfo::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, driveInfoDbId, _dbId);
    CommonUtility::writeValueToStruct(dstruct, driveInfoId, _id);
    CommonUtility::writeValueToStruct(dstruct, driveInfoAccountDbId, _accountDbId);
    CommonUtility::writeValueToStruct(dstruct, driveInfoName, CommonUtility::qStr2CommString(_name));
    CommonUtility::writeValueToStruct(dstruct, driveInfoSize, _size);
    CommonUtility::writeValueToStruct(dstruct, driveInfoColor, CommonUtility::qStr2CommString(_color.name()));
    CommonUtility::writeValueToStruct(dstruct, driveInfoNotifications, _notifications);
    CommonUtility::writeValueToStruct(dstruct, driveInfoAdmin, _admin);
    CommonUtility::writeValueToStruct(dstruct, driveInfoMaintenance, _maintenance);
    CommonUtility::writeValueToStruct(dstruct, driveInfoLocked, _locked);
    CommonUtility::writeValueToStruct(dstruct, driveInfoUsedSize, _usedSize);
    CommonUtility::writeValueToStruct(dstruct, driveInfoAccessDenied, _accessDenied);
    CommonUtility::writeValueToStruct(dstruct, driveInfoPackInfo, _packInfo, info2DynamicVar<PackInfo>);
}

void DriveInfo::fromDynamicStruct(const Poco::DynamicStruct &dstruct) {
    CommonUtility::readValueFromStruct(dstruct, driveInfoDbId, _dbId);
    CommonUtility::readValueFromStruct(dstruct, driveInfoId, _id);
    CommonUtility::readValueFromStruct(dstruct, driveInfoAccountDbId, _accountDbId);

    CommString name;
    CommonUtility::readValueFromStruct(dstruct, driveInfoName, name);
    _name = CommonUtility::commString2QStr(name);

    CommonUtility::readValueFromStruct(dstruct, driveInfoSize, _size);

    CommString color;
    CommonUtility::readValueFromStruct(dstruct, driveInfoColor, color);
    _color = QColor(CommonUtility::commString2QStr(color));

    CommonUtility::readValueFromStruct(dstruct, driveInfoNotifications, _notifications);
    CommonUtility::readValueFromStruct(dstruct, driveInfoAdmin, _admin);
    CommonUtility::readValueFromStruct(dstruct, driveInfoMaintenance, _maintenance);
    CommonUtility::readValueFromStruct(dstruct, driveInfoLocked, _locked);
    CommonUtility::readValueFromStruct(dstruct, driveInfoUsedSize, _usedSize);
    CommonUtility::readValueFromStruct(dstruct, driveInfoAccessDenied, _accessDenied);
    CommonUtility::readValueFromStruct(dstruct, driveInfoPackInfo, _packInfo, dynamicVar2Struct<PackInfo>);
}

void operator>>(QDataStream &in, DriveInfo &info) {
    qint64 dbId{0};
    qint64 id{0};
    qint64 accountDbId{0};
    QString name;
    QColor color;
    bool notifications{false};
    bool admin{false};
    bool maintenance{false};
    bool locked{false};
    bool accessDenied{false};

    in >> dbId >> id >> accountDbId >> name >> color >> notifications >> admin >> maintenance >> locked >> accessDenied;

    info.setDbId(static_cast<DriveDbId>(dbId));
    info.setId(static_cast<DriveId>(id));
    info.setAccountDbId(static_cast<AccountDbId>(accountDbId));
    info.setName(name);
    info.setColor(color);
    info.setNotifications(notifications);
    info.setAdmin(admin);
    info.setMaintenance(maintenance);
    info.setLocked(locked);
    info.setAccessDenied(accessDenied);
}

QDataStream &operator<<(QDataStream &out, const DriveInfo &info) {
    out << static_cast<qint64>(info.dbId()) << static_cast<qint64>(info.id()) << static_cast<qint64>(info.accountDbId())
        << info.name() << info.color() << info.notifications() << info.admin() << info.maintenance() << info.locked()
        << info.accessDenied();
    return out;
}

void operator>>(QDataStream &in, QList<DriveInfo> &list) {
    int count = 0;
    in >> count;
    for (int i = 0; i < count; i++) {
        DriveInfo info;
        in >> info;
        list.push_back(info);
    }
}

QDataStream &operator<<(QDataStream &out, const QList<DriveInfo> &list) {
    int count = static_cast<int>(list.size());
    out << count;
    for (int i = 0; i < count; i++) {
        out << list[i];
    }
    return out;
}

} // namespace KDC
