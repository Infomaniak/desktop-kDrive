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

#include "libsyncengine/jobs/syncjob.h"
#include "jobs/network/kDrive_API/listingconf.h"

#include "info/nodeinfo.h"

namespace KDC {

class GetFilesInRootDirJob : public SyncJob {
    public:
        GetFilesInRootDirJob(int userDbId, int driveId);
        explicit GetFilesInRootDirJob(int driveDbId);

        void setListingConf(const ListingConf &listingConf) { _listingConf = listingConf; };

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

        ListingConf _listingConf;

        // The deserialization of the request JSON result.
        NodeInfoList _nodeInfoList;

        std::string getConstructorFailureLogMessage(const std::exception &e) const;
        std::string getRunSynchronouslyFailureLogMessage(const ExitInfo &exitInfo) const;
};

} // namespace KDC
