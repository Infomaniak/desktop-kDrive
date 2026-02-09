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

#include "info/nodeinfo.h"

namespace KDC {

class GetAllFilesInDirectoryJob : public SyncJob {
    public:
        GetAllFilesInDirectoryJob(int userDbId, int driveId, NodeId fileId, bool dirOnly = false);
        explicit GetAllFilesInDirectoryJob(int driveDbId, NodeId fileId, bool dirOnly = false);

        void setWithPath(const bool val) { _withPath = val; }
        void setLimit(const int64_t limit) { _limit = limit; }

        // The return value of this accessor is only meaningful when all the responses of the
        // underlying file list requests have been handled.
        [[nodiscard]] const NodeInfoList &nodeInfoList() const { return _nodeInfoList; };

        void abort() override;

    private:
        ExitInfo runJob() override;

        using UserDbId = int32_t;
        UserDbId _userDbId{0};

        using DriveId = int32_t;
        // The identifier of the drive in the local database
        DriveId _driveId{0};

        using DriveDbId = int32_t;
        // The identifier of the drive in the local database
        DriveDbId _driveDbId{0};

        // The remote identifier of the folder whose file list is queried.
        NodeId _fileId;

        // If `_withPath` is `true`, the paths of the items will be returned.
        bool _withPath{false};

        // If `_dirOnly` is `true`, directories only will be listed in the result.
        bool _dirOnly{false};

        // The maximal number of items returned by the request.
        int64_t _limit{10};

        // The deserialization of the request JSON result.
        NodeInfoList _nodeInfoList;

        [[nodiscard]] std::string getConstructorFailureLogMessage(const std::exception &e);
        [[nodiscard]] std::string getRunSynchronouslyFailureLogMessage(const ExitInfo &exitInfo);
};

} // namespace KDC
