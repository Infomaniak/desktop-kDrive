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

namespace KDC {

class GetInfoUserJob final : public AbstractTokenNetworkJob {
    public:
        explicit GetInfoUserJob(int userDbId);

        [[nodiscard]] const std::string &name() const { return _name; }
        [[nodiscard]] const std::string &email() const { return _email; }
        [[nodiscard]] const std::string &avatarUrl() const { return _avatarUrl; }
        [[nodiscard]] bool isStaff() const { return _isStaff; }

    protected:
        bool handleJsonResponse(std::istream &is) override;

    private:
        std::string getSpecificUrl() override;
        void setQueryParameters(Poco::URI &, bool &canceled) override { canceled = false; }
        ExitInfo setData() override { return ExitCode::Ok; }

        std::string _name;
        std::string _email;
        std::string _avatarUrl;
        bool _isStaff{false};
};

} // namespace KDC
