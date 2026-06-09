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

#include "data/drive.h"

#include "libcommonserver/log/log.h"
#include "utility/utility.h"

#include <log4cplus/loggingmacros.h>

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

Drive::Drive(const DriveDbId dbId, const DriveId driveId, const AccountDbId accountDbId, const std::string &name,
             const int64_t size, const std::string &color, const bool notifications, const bool admin) :
    _dbId(dbId),
    _driveId(driveId),
    _accountDbId(accountDbId),
    _name(name),
    _size(size),
    _color(color),
    _notifications(notifications),
    _admin(admin) {}

void Drive::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, driveInfoDbId, dbId());
    CommonUtility::writeValueToStruct(dstruct, driveInfoId, driveId());
    CommonUtility::writeValueToStruct(dstruct, driveInfoAccountDbId, accountDbId());
    CommonUtility::writeValueToStruct(dstruct, driveInfoName, CommonUtility::str2CommString(name()));
    CommonUtility::writeValueToStruct(dstruct, driveInfoSize, size());
    CommonUtility::writeValueToStruct(dstruct, driveInfoColor, CommonUtility::str2CommString(color()));
    CommonUtility::writeValueToStruct(dstruct, driveInfoNotifications, notifications());
    CommonUtility::writeValueToStruct(dstruct, driveInfoAdmin, admin());
    CommonUtility::writeValueToStruct(dstruct, driveInfoMaintenance,
                                      maintenanceInfo().inMaintenance()); // TODO : send all maintenance info?
    CommonUtility::writeValueToStruct(dstruct, driveInfoLocked, locked());
    CommonUtility::writeValueToStruct(dstruct, driveInfoUsedSize, usedSize());
    CommonUtility::writeValueToStruct(dstruct, driveInfoAccessDenied, accessDenied());
    CommonUtility::writeValueToStruct(dstruct, driveInfoPackInfo, packInfo(), info2DynamicVar<PackInfo>);
}

void Drive::fromDynamicStruct(const Poco::DynamicStruct &dstruct) {
    DriveDbId driveDbId = 0;
    CommonUtility::readValueFromStruct(dstruct, driveInfoDbId, driveDbId);
    setDbId(driveDbId);

    DriveId driveId = 0;
    CommonUtility::readValueFromStruct(dstruct, driveInfoId, driveId);
    setDriveId(driveId);

    AccountDbId accountDbId = 0;
    CommonUtility::readValueFromStruct(dstruct, driveInfoAccountDbId, accountDbId);
    setAccountDbId(accountDbId);

    CommString name;
    CommonUtility::readValueFromStruct(dstruct, driveInfoName, name);
    setName(CommonUtility::commString2Str(name));

    int64_t size = 0;
    CommonUtility::readValueFromStruct(dstruct, driveInfoSize, size);
    setSize(size);

    CommString color;
    CommonUtility::readValueFromStruct(dstruct, driveInfoColor, color);
    setColor(CommonUtility::commString2Str(color));

    bool notifications = false;
    CommonUtility::readValueFromStruct(dstruct, driveInfoNotifications, notifications);
    setNotifications(notifications);

    bool admin = false;
    CommonUtility::readValueFromStruct(dstruct, driveInfoAdmin, admin);
    setAdmin(admin);

    bool inMaintenance = false;
    CommonUtility::readValueFromStruct(dstruct, driveInfoMaintenance, inMaintenance);
    auto newMaintenanceInfo = maintenanceInfo();
    newMaintenanceInfo.setInMaintenance(inMaintenance);
    setMaintenanceInfo(newMaintenanceInfo);

    CommonUtility::readValueFromStruct(dstruct, driveInfoLocked, _locked);
    CommonUtility::readValueFromStruct(dstruct, driveInfoUsedSize, _usedSize);
    CommonUtility::readValueFromStruct(dstruct, driveInfoAccessDenied, _accessDenied);
    CommonUtility::readValueFromStruct(dstruct, driveInfoPackInfo, _packInfo, dynamicVar2Struct<PackInfo>);
}

void operator>>(QDataStream &in, Drive &drive) {
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

    drive.setDbId(static_cast<DriveDbId>(dbId));
    drive.setDriveId(static_cast<DriveId>(id));
    drive.setAccountDbId(static_cast<AccountDbId>(accountDbId));
    drive.setName(name.toStdString());
    drive.setColor(color.name().toStdString());
    drive.setNotifications(notifications);
    drive.setAdmin(admin);
    MaintenanceInfo maintenanceInfo;
    maintenanceInfo.setInMaintenance(maintenance);
    drive.setMaintenanceInfo(maintenanceInfo);
    drive.setLocked(locked);
    drive.setAccessDenied(accessDenied);
}

QDataStream &operator<<(QDataStream &out, const Drive &drive) {
    out << static_cast<qint64>(drive.dbId()) << static_cast<qint64>(drive.driveId()) << static_cast<qint64>(drive.accountDbId())
        << QString::fromStdString(drive.name()) << QColor(QString::fromStdString(drive.color())) << drive.notifications()
        << drive.admin() << drive.maintenanceInfo().inMaintenance() << drive.locked() << drive.accessDenied();
    return out;
}

void operator>>(QDataStream &in, QList<Drive> &list) {
    uint64_t count = 0;
    in >> count;
    for (uint64_t i = 0; i < count; i++) {
        Drive info;
        in >> info;
        list.push_back(info);
    }
}

QDataStream &operator<<(QDataStream &out, const QList<Drive> &list) {
    auto count = static_cast<uint64_t>(list.size());
    out << count;
    for (uint64_t i = 0; i < count; i++) {
        out << list[static_cast<qsizetype>(i)];
    }
    return out;
}

} // namespace KDC
