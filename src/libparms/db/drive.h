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

#pragma once

#include "libparms/parmslib.h"

#include <string>

#include <log4cplus/logger.h>

namespace KDC {

class PARMS_EXPORT Drive {
    public:
        Drive();
        Drive(int dbId, int driveId, int accountDbId, const std::string &name = std::string(), int64_t size = int64_t(),
              const std::string &color = std::string(), bool notifications = true, bool admin = true);

        inline void setDbId(int dbId) { _dbId = dbId; }
        inline int dbId() const { return _dbId; }
        inline void setDriveId(int driveId) { _driveId = driveId; }
        inline int driveId() const { return _driveId; }
        inline void setAccountDbId(int accountDbId) { _accountDbId = accountDbId; }
        inline int accountDbId() const { return _accountDbId; }
        inline void setName(const std::string &newDriveName) { _name = newDriveName; }
        inline const std::string &name() const { return _name; }
        inline int64_t size() const { return _size; }
        inline void setSize(int64_t newSize) { _size = newSize; }
        inline std::string color() const { return _color; }
        inline void setColor(std::string color) { _color = color; }
        inline bool notifications() const { return _notifications; }
        inline void setNotifications(bool newNotifications) { _notifications = newNotifications; }
        inline bool admin() const { return _admin; }
        inline void setAdmin(bool admin) { _admin = admin; }

        inline bool maintenance() const { return _maintenance; }
        inline void setMaintenance(bool newMaintenance) { _maintenance = newMaintenance; }
        inline bool notRenew() const { return _notRenew; }
        inline void setNotRenew(bool newNotRenew) { _notRenew = newNotRenew; }
        inline int64_t maintenanceFrom() const { return _maintenanceFrom; }
        inline void setMaintenanceFrom(int64_t newMaintenanceFrom) { _maintenanceFrom = newMaintenanceFrom; }
        inline bool locked() const { return _locked; }
        inline void setLocked(bool newLocked) { _locked = newLocked; }
        inline int64_t usedSize() { return _usedSize; }
        inline void setUsedSize(int64_t newUsedSize) { _usedSize = newUsedSize; }
        inline bool accessDenied() const { return _accessDenied; }
        inline void setAccessDenied(bool accessDenied) { _accessDenied = accessDenied; }

    private:
        log4cplus::Logger _logger;
        int _dbId;
        int _driveId;
        int _accountDbId;
        std::string _name;
        int64_t _size;
        std::string _color; // #RRGGBB format
        bool _notifications;
        bool _admin;

        // Non DB attributes
        bool _maintenance;
        bool _notRenew = false;
        int64_t _maintenanceFrom;
        bool _locked;
        int64_t _usedSize;
        bool _accessDenied;
};

} // namespace KDC
