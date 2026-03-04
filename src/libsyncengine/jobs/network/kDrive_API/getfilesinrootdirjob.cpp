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
    FileListJob(userDbId, driveId) {}


GetFilesInRootDirJob::GetFilesInRootDirJob(const int driveDbId) :
    FileListJob(driveDbId) {}


void GetFilesInRootDirJob::abort() {
    LOG_DEBUG(_logger, "Aborting exhaustive root file list request " << jobId());
    SyncJob::abort();
}


ExitInfo GetFilesInRootDirJob::runJob() {
    // On the first iteration, we get the file list of the remote drive's root folder.
    // On the second iteration, we get the file list of the user private root folder.
    for (const bool translateV2ToV3: {false, true}) {
        std::shared_ptr<GetAllFilesInDirectoryJob> getRootListJob = nullptr;
        try {
            getRootListJob =
                    _driveDbId ? std::make_shared<GetAllFilesInDirectoryJob>(_driveDbId, NodeId{"1"}, translateV2ToV3)
                               : std::make_shared<GetAllFilesInDirectoryJob>(_userDbId, _driveId, NodeId{"1"}, translateV2ToV3);
        } catch (const std::exception &e) {
            LOG_WARN(Log::instance()->getLogger(), getConstructorFailureLogMessage(e));

            return AbstractTokenNetworkJob::exception2ExitCode(e);
        }

        getRootListJob->setListingConf(_listingConf);
        if (const auto exitInfo = getRootListJob->runSynchronously(); !exitInfo) {
            LOG_WARN(Log::instance()->getLogger(), getRunSynchronouslyFailureLogMessage(exitInfo));

            return exitInfo;
        }

        if (_nodeInfoList.empty())
            _nodeInfoList = getRootListJob->v3NodeInfoList();
        else {
            // Concatenate the two listings.
            _nodeInfoList.reserve(_nodeInfoList.size() + getRootListJob->v3NodeInfoList().size());
            _nodeInfoList.insert(_nodeInfoList.end(), getRootListJob->v3NodeInfoList().begin(),
                                 getRootListJob->v3NodeInfoList().end());
        }
    }

    return ExitCode::Ok;
}

} // namespace KDC
