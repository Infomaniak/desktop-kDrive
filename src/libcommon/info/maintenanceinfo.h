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

#pragma once

namespace KDC {

class MaintenanceInfo {
    public:
        MaintenanceInfo() = default;
        MaintenanceInfo(const bool inMaintenance, const bool notRenew, const bool asleep, const bool wakingUp,
                        const int64_t maintenanceFrom);

        [[nodiscard]] bool inMaintenance() const { return _inMaintenance; }
        void setInMaintenance(const bool maintenance) { _inMaintenance = maintenance; }
        [[nodiscard]] bool notRenew() const { return _notRenew; }
        void setNotRenew(const bool notRenew) { _notRenew = notRenew; }
        [[nodiscard]] bool asleep() const { return _asleep; }
        void setAsleep(const bool asleep) { _asleep = asleep; }
        [[nodiscard]] bool wakingUp() const { return _wakingUp; }
        void setWakingUp(const bool wakingUp) { _wakingUp = wakingUp; }
        [[nodiscard]] int64_t maintenanceFrom() const { return _maintenanceFrom; }
        void setMaintenanceFrom(const int64_t maintenanceFrom) { _maintenanceFrom = maintenanceFrom; }

        void toDynamicStruct(Poco::DynamicStruct &dstruct) const;
        void fromDynamicStruct(const Poco::DynamicStruct &dstruct);

        bool operator==(const MaintenanceInfo &other) const = default;

    private:
        bool _inMaintenance{false};
        bool _notRenew{false};
        bool _asleep{false};
        bool _wakingUp{false};
        int64_t _maintenanceFrom{0};
};

} // namespace KDC
