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
        struct MaintenanceInfo {
                bool _maintenance{false};
                bool _notRenew{false};
                bool _asleep{false};
                bool _wakingUp{false};
                int64_t _maintenanceFrom{0};
        };

        Drive();
        Drive(int dbId, int driveId, int accountDbId, const std::string &name = std::string(), int64_t size = int64_t(),
              const std::string &color = std::string(), bool notifications = true, bool admin = true);

        void setDbId(int dbId) { _dbId = dbId; }
        int dbId() const { return _dbId; }
        void setDriveId(int driveId) { _driveId = driveId; }
        int driveId() const { return _driveId; }
        void setAccountDbId(int accountDbId) { _accountDbId = accountDbId; }
        int accountDbId() const { return _accountDbId; }
        void setName(const std::string &newDriveName) { _name = newDriveName; }
        const std::string &name() const { return _name; }
        int64_t size() const { return _size; }
        void setSize(int64_t newSize) { _size = newSize; }
        std::string color() const { return _color; }
        void setColor(std::string color) { _color = color; }
        bool notifications() const { return _notifications; }
        void setNotifications(bool newNotifications) { _notifications = newNotifications; }
        bool admin() const { return _admin; }
        void setAdmin(bool admin) { _admin = admin; }

        const MaintenanceInfo &maintenanceInfo() const { return _maintenanceInfo; }
        void setMaintenanceInfo(const MaintenanceInfo &info) { _maintenanceInfo = info; }
        bool locked() const { return _locked; }
        void setLocked(bool newLocked) { _locked = newLocked; }
        int64_t usedSize() { return _usedSize; }
        void setUsedSize(int64_t newUsedSize) { _usedSize = newUsedSize; }
        bool accessDenied() const { return _accessDenied; }
        void setAccessDenied(bool accessDenied) { _accessDenied = accessDenied; }

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
        MaintenanceInfo _maintenanceInfo;
        bool _locked;
        int64_t _usedSize;
        bool _accessDenied;
};

} // namespace KDC
