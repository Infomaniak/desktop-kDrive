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

#include "getrootfilelistjob.h"
#include "libcommon/utility/types.h"

namespace KDC {

class GetFileListJob : public GetRootFileListJob {
    public:
        GetFileListJob(int userDbId, int driveId, const NodeId &fileId, uint64_t page = 1, bool dirOnly = false,
                       uint64_t nbItemsPerPage = 1000);
        GetFileListJob(int driveDbId, const NodeId &fileId, uint64_t page = 1, bool dirOnly = false,
                       uint64_t nbItemsPerPage = 1000);

    private:
        std::string getSpecificUrl() override;
        ExitInfo setData() override { return ExitCode::Ok; }

        std::string _fileId;
};

} // namespace KDC
