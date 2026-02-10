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


#include "getfilesinrootdirjob.h"
#include "getallfilesindirectoryjob.h"

#include "jobs/network/apitranslator.h"

namespace KDC {

GetFilesInRootDirJob::GetFilesInRootDirJob(const int userDbId, const int driveId) :
    _userDbId{userDbId},
    _driveId{driveId} {
    assert(_userDbId > 0 && "Invalid user DB ID.");
    assert(_driveId > 0 && "Invalid drive ID.");
}

GetFilesInRootDirJob::GetFilesInRootDirJob(const int driveDbId) :
    _driveDbId{driveDbId} {
    assert(_driveDbId > 0 && "Invalid drive DB ID.");
}

void GetFilesInRootDirJob::abort() {
    LOG_DEBUG(_logger, "Aborting exhaustive root file list request " << jobId());
    SyncJob::abort();
}

ExitInfo GetFilesInRootDirJob::runJob() {
    GetAllFilesInDirectoryJob getRootDirListJob =
            _driveDbId ? GetAllFilesInDirectoryJob(_driveDbId, NodeId{"1"}, true, false)
                       : GetAllFilesInDirectoryJob(_userDbId, _driveId, NodeId{"1"}, true, false);

    getRootDirListJob.runSynchronously();

    GetAllFilesInDirectoryJob getPrivateDirListJob = _driveDbId ? GetAllFilesInDirectoryJob(_driveDbId, NodeId{"1"})
                                                                : GetAllFilesInDirectoryJob(_userDbId, _driveId, NodeId{"1"});
    getPrivateDirListJob.runSynchronously();

    // Concatenate the two listings
    _nodeInfoList = getRootDirListJob.nodeInfoList();
    _nodeInfoList.reserve(_nodeInfoList.size() + getPrivateDirListJob.nodeInfoList().size());
    _nodeInfoList.insert(_nodeInfoList.end(), getPrivateDirListJob.nodeInfoList().begin(),
                         getPrivateDirListJob.nodeInfoList().end());
}

} // namespace KDC
