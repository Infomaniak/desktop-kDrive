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

#include "jobs/network/abstracttokennetworkjob.h"

#include "libparms/db/drive.h"

namespace KDC {

class GetInfoDriveJob : public AbstractTokenNetworkJob {
    public:
        GetInfoDriveJob(int userDbId, int driveId);
        GetInfoDriveJob(int driveDbId);


        [[nodiscard]] const std::string &name() const { return _name; }
        [[nodiscard]] int64_t size() const { return _size; }
        [[nodiscard]] bool isAdmin() const { return _isAdmin; }
        [[nodiscard]] int accountId() const { return _accountId; }
        [[nodiscard]] const std::string &colorHex() const { return _colorHex; }
        [[nodiscard]] bool isInMaintenance() const { return _isInMaintenance; }
        [[nodiscard]] int64_t maintenanceFrom() const { return _maintenanceFrom; }
        [[nodiscard]] bool isLocked() const { return _isLocked; }
        [[nodiscard]] int64_t usedSize() const { return _usedSize; }
        [[nodiscard]] const Drive::PackInfo &packInfo() const { return _packInfo; }

    protected:
        ExitInfo handleError(const std::string &replyBody, const Poco::URI &uri) override;
        ExitInfo handleJsonResponse(const std::string &replyBody) override;

    private:
        inline ExitInfo setData() override { return ExitCode::Ok; }
        void setQueryParameters(Poco::URI &uri) override;

        std::string _name;
        int64_t _size{0};
        bool _isAdmin{false};
        int _accountId{0};
        std::string _colorHex;
        bool _isInMaintenance{false};
        int64_t _maintenanceFrom{0};
        bool _isLocked{false};
        int64_t _usedSize{0};
        Drive::PackInfo _packInfo;
};

} // namespace KDC
