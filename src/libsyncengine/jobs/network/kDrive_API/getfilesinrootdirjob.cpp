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

std::string GetFilesInRootDirJob::getConstructorFailureLogMessage(const std::exception &e) const {
    constexpr auto logMessage = "Error in GetFilesInRootDirJob::GetFilesInRootDirJob for ";
    std::stringstream ss;

    if (_driveDbId) {
        ss << logMessage << " driveDbId=" << _driveDbId;
    } else {
        ss << logMessage << " userDbId=" << _userDbId << " driveId=" << _driveId;
    }

    ss << " nodeId=" << _fileId << " error=" << e.what();

    return ss.str();
}

std::string GetFilesInRootDirJob::getRunSynchronouslyFailureLogMessage(const ExitInfo &exitInfo) const {
    constexpr auto logMessage = "Error in GetFilesInDirectoryJob::runSynchronously for ";
    std::stringstream ss;

    if (_driveDbId) {
        ss << logMessage << " driveDbId=" << _driveDbId;
    } else {
        ss << logMessage << " userDbId=" << _userDbId << " driveId=" << _driveId;
    }

    ss << " nodeId=" << _fileId << " exitInfo:" << exitInfo;

    return ss.str();
}

ExitInfo GetFilesInRootDirJob::runJob() {
    std::shared_ptr<GetAllFilesInDirectoryJob> getRootDirListJob = nullptr;
    try {
        getRootDirListJob =
                _driveDbId ? std::make_shared<GetAllFilesInDirectoryJob>(_driveDbId, NodeId{"1"}, _listingConf.dirOnly, false)
                           : std::make_shared<GetAllFilesInDirectoryJob>(_userDbId, _driveId, NodeId{"1"}, _listingConf.dirOnly,
                                                                         false);
    } catch (const std::exception &e) {
        LOG_WARN(Log::instance()->getLogger(), getConstructorFailureLogMessage(e));

        return AbstractTokenNetworkJob::exception2ExitCode(e);
    }

    if (const auto exitInfo = getRootDirListJob->runSynchronously(); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), getRunSynchronouslyFailureLogMessage(exitInfo));

        return exitInfo;
    }

    std::shared_ptr<GetAllFilesInDirectoryJob> getPrivateDirListJob = nullptr;
    try {
        getPrivateDirListJob = _driveDbId ? std::make_shared<GetAllFilesInDirectoryJob>(_driveDbId, NodeId{"1"})
                                          : std::make_shared<GetAllFilesInDirectoryJob>(_userDbId, _driveId, NodeId{"1"});

    } catch (const std::exception &e) {
        LOG_WARN(Log::instance()->getLogger(), getConstructorFailureLogMessage(e));

        return AbstractTokenNetworkJob::exception2ExitCode(e);
    }

    if (const auto exitInfo = getPrivateDirListJob->runSynchronously(); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), getRunSynchronouslyFailureLogMessage(exitInfo));

        return exitInfo;
    }

    // Concatenate the two listings
    _nodeInfoList = getRootDirListJob->nodeInfoList();
    _nodeInfoList.reserve(_nodeInfoList.size() + getPrivateDirListJob->nodeInfoList().size());
    _nodeInfoList.insert(_nodeInfoList.end(), getPrivateDirListJob->nodeInfoList().begin(),
                         getPrivateDirListJob->nodeInfoList().end());

    return ExitCode::Ok;
}

} // namespace KDC
