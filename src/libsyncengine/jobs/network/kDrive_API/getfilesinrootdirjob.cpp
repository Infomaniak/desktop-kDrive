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

#include "jobs/network/jobexceptions.h"
#include "jobs/network/kDrive_API/apitranslator.h"

namespace KDC {

GetFilesInRootDirJob::GetFilesInRootDirJob(const UserDbId userDbId, const DriveId driveId) :
    FileListJob(userDbId, driveId) {}


GetFilesInRootDirJob::GetFilesInRootDirJob(const DriveDbId driveDbId) :
    FileListJob(driveDbId) {}


void GetFilesInRootDirJob::abort() {
    LOG_DEBUG(_logger, "Aborting exhaustive root file list request " << jobId());
    SyncJob::abort();
}


ExitInfo GetFilesInRootDirJob::runJob() {
    // On the first iteration, we get the file list of the remote drive's root folder.
    // On the second iteration, we get the file list of the user private root folder.
    for (const auto translateV2ToV3: {TranslationMode::None, TranslationMode::V2ToV3}) {
        std::shared_ptr<GetAllFilesInDirectoryJob> getRootListJob = nullptr;
        try {
            getRootListJob = _driveDbId ? std::make_shared<GetAllFilesInDirectoryJob>(
                                                  _driveDbId, ApiTranslator::v2RootFolderRemoteId(), translateV2ToV3)
                                        : std::make_shared<GetAllFilesInDirectoryJob>(
                                                  _userDbId, _driveId, ApiTranslator::v2RootFolderRemoteId(), translateV2ToV3);
        } catch (const std::bad_alloc &badAllocationException) {
            return exception2ExitCode(badAllocationException);
        } catch (const JobException &e) {
            LOG_WARN(_logger, getConstructorFailureLogMessage(e));

            return exception2ExitCode(e);
        }

        getRootListJob->setListingConf(_listingConf);
        if (const auto exitInfo = getRootListJob->runSynchronously(); !exitInfo) {
            LOG_WARN(_logger, getRunSynchronouslyFailureLogMessage(exitInfo));

            return exitInfo;
        }

        if (_remoteNodeInfoList.empty())
            _remoteNodeInfoList = getRootListJob->v3RemoteNodeInfoList();
        else {
            // Concatenate the two listings.
            _remoteNodeInfoList.reserve(_remoteNodeInfoList.size() + getRootListJob->v3RemoteNodeInfoList().size());
            _remoteNodeInfoList.insert(_remoteNodeInfoList.end(), getRootListJob->v3RemoteNodeInfoList().begin(),
                                       getRootListJob->v3RemoteNodeInfoList().end());
        }
    }

    return ExitCode::Ok;
}

std::string getFileListConstructorErrorMsg(FileListJob *job, const UserDbId userDbId, const DriveId driveId,
                                           const JobException &e) {
    const auto coreMsg = dynamic_cast<GetFilesInRootDirJob *>(job) ? "GetFilesInRootDirJob::GetFilesInRootDirJob"
                                                                   : " GetAllFilesInDirectoryJob::GetAllFilesInDirectoryJob";
    std::stringstream ss;
    ss << "Error in " << coreMsg << " for userDbId=" << userDbId << " driveId=" << driveId << " error=" << e.what();

    return ss.str();
}

std::string getFileListExecErrorMsg(FileListJob *job, const UserDbId userDbId, const DriveId driveId, const ExitInfo &exitInfo) {
    const auto coreMsg = dynamic_cast<GetFilesInRootDirJob *>(job) ? "GetFilesInRootDirJob::runSynchronously"
                                                                   : " GetAllFilesInDirectoryJob::runSynchronously";
    std::stringstream ss;
    ss << "Error in " << coreMsg << " for userDbId=" << userDbId << " driveId=" << driveId << " ExitInfo:" << exitInfo;

    return ss.str();
}

std::string getFileListConstructorErrorMsg(FileListJob *job, const DriveDbId driveDbId, const RemoteNodeId &nodeId,
                                           const JobException &e) {
    const std::string coreMsg = dynamic_cast<GetFilesInRootDirJob *>(job)
                                        ? "GetFilesInRootDirJob::GetFilesInRootDirJob"
                                        : " GetAllFilesInDirectoryJob::GetAllFilesInDirectoryJob";
    std::stringstream ss;
    ss << "Error in " << coreMsg << " for driveDbId=" << driveDbId << " nodeId=" << nodeId << " error=" << e.what();

    return ss.str();
}

std::string getFileListExecErrorMsg(FileListJob *job, const DriveDbId driveDbId, const RemoteNodeId &nodeId,
                                    const ExitInfo &exitInfo) {
    const std::string coreMsg = dynamic_cast<GetFilesInRootDirJob *>(job) ? "GetFilesInRootDirJob::runSynchronously"
                                                                          : " GetAllFilesInDirectoryJob::runSynchronously";
    std::stringstream ss;
    ss << "Error in " << coreMsg << " for driveDbId=" << driveDbId << " nodeId=" << nodeId << " ExitInfo:" << exitInfo;

    return ss.str();
}

} // namespace KDC
