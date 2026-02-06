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

class GetFilesInDirectoryJob : public AbstractTokenNetworkJob {
    public:
        GetFilesInDirectoryJob(int userDbId, int driveId, const NodeId &fileId, bool dirOnly = false);
        explicit GetFilesInDirectoryJob(int driveDbId, const NodeId &fileId, bool dirOnly = false);

        void setWithPath(const bool val) { _withPath = val; }


    private:
        std::string getSpecificUrl() override;
        void setQueryParameters(Poco::URI &uri) override;
        ExitInfo setData() override { return ExitCode::Ok; }

        ExitInfo handleResponse(std::istream &is) override;


        bool _dirOnly{false};
        bool _withPath{false};
};

} // namespace KDC
