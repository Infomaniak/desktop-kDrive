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

#include "maintenanceinfo.h"

#include "utility/utility.h"

namespace KDC {

static const auto isInMaintenance = "inMaintenance";
static const auto maintenanceNotRenew = "notRenew";
static const auto maintenanceAsleep = "asleep";
static const auto maintenanceWakingUp = "wakingUp";
static const auto inMaintenanceFrom = "from";

MaintenanceInfo::MaintenanceInfo(const bool inMaintenance, const bool notRenew, const bool asleep, const bool wakingUp,
                                 const int64_t maintenanceFrom) :
    _inMaintenance(inMaintenance),
    _notRenew(notRenew),
    _asleep(asleep),
    _wakingUp(wakingUp),
    _maintenanceFrom(maintenanceFrom) {}

void MaintenanceInfo::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, isInMaintenance, _inMaintenance);
    CommonUtility::writeValueToStruct(dstruct, maintenanceNotRenew, _notRenew);
    CommonUtility::writeValueToStruct(dstruct, maintenanceAsleep, _asleep);
    CommonUtility::writeValueToStruct(dstruct, maintenanceWakingUp, _wakingUp);
    CommonUtility::writeValueToStruct(dstruct, inMaintenanceFrom, _maintenanceFrom);
}

void MaintenanceInfo::fromDynamicStruct(const Poco::DynamicStruct &dstruct) {
    CommonUtility::readValueFromStruct(dstruct, isInMaintenance, _inMaintenance);
    CommonUtility::readValueFromStruct(dstruct, maintenanceNotRenew, _notRenew);
    CommonUtility::readValueFromStruct(dstruct, maintenanceAsleep, _asleep);
    CommonUtility::readValueFromStruct(dstruct, maintenanceWakingUp, _wakingUp);
    CommonUtility::readValueFromStruct(dstruct, inMaintenanceFrom, _maintenanceFrom);
}


} // namespace KDC
