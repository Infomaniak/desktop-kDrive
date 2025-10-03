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

class GetRootFileListJob : public AbstractTokenNetworkJob {
    public:
        GetRootFileListJob(int userDbId, int driveId, uint64_t page = 1, bool dirOnly = false, uint64_t nbItemsPerPage = 1000);
        explicit GetRootFileListJob(int driveDbId, uint64_t page = 1, bool dirOnly = false, uint64_t nbItemsPerPage = 1000);

        void setWithPath(const bool val) { _withPath = val; }

        [[nodiscard]] uint64_t totalPages() const { return _totalPages; }

    private:
        std::string getSpecificUrl() override;
        void setQueryParameters(Poco::URI &uri) override;
        ExitInfo setData() override { return ExitCode::Ok; }

        ExitInfo handleResponse(std::istream &is) override;

        uint64_t _page{0};
        bool _dirOnly{false};
        uint64_t _nbItemsPerPage{0};
        bool _withPath{false};

        uint64_t _totalPages{0};
};

} // namespace KDC
