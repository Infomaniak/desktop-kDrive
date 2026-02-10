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

#include "jobs/network/kDrive_API/getallfilesindirectoryjob.h"
#include "jobs/network/kDrive_API/getfilesindirectoryjob.h"

namespace KDC {

GetAllFilesInDirectoryJob::GetAllFilesInDirectoryJob(const int userDbId, const DriveId driveId, NodeId fileId,
                                                     const bool dirOnly /*= false*/, const bool translateV2ToV3) :
    _userDbId{userDbId},
    _driveId{driveId},
    _fileId(std::move(fileId)),
    _translateV2ToV3(translateV2ToV3),
    _dirOnly(dirOnly) {
    assert(_userDbId > 0 && "Invalid user DB ID.");
    assert(_driveId > 0 && "Invalid drive ID.");
}

GetAllFilesInDirectoryJob::GetAllFilesInDirectoryJob(const int driveDbId, NodeId fileId, const bool dirOnly /*= false*/,
                                                     const bool translateV2ToV3 /*= false */) :
    _driveDbId{driveDbId},
    _fileId(std::move(fileId)),
    _translateV2ToV3(translateV2ToV3),
    _dirOnly(dirOnly) {
    assert(_driveDbId > 0 && "Invalid drive DB ID.");
}

void GetAllFilesInDirectoryJob::abort() {
    LOG_DEBUG(_logger, "Aborting exhaustive file list request " << jobId());
    SyncJob::abort();
}

std::string GetAllFilesInDirectoryJob::getConstructorFailureLogMessage(const std::exception &e) const {
    constexpr auto logMessage = "Error in GetFilesInDirectoryJob::GetFilesInDirectoryJob for ";
    std::stringstream ss;

    if (_driveDbId) {
        ss << logMessage << " driveDbId=" << _driveDbId;
    } else {
        ss << logMessage << " userDbId=" << _userDbId << " driveId=" << _driveId;
    }

    ss << " nodeId=" << _fileId << " error=" << e.what();

    return ss.str();
}

std::string GetAllFilesInDirectoryJob::getRunSynchronouslyFailureLogMessage(const ExitInfo &exitInfo) const {
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

ExitInfo GetAllFilesInDirectoryJob::runJob() {
    bool hasMore = false;
    std::string cursor;
    do {
        std::shared_ptr<GetFilesInDirectoryJob> fileListJob = nullptr;
        try {
            fileListJob =
                    _driveDbId ? std::make_shared<GetFilesInDirectoryJob>(_driveDbId, _fileId, cursor, _dirOnly, _translateV2ToV3)
                               : std::make_shared<GetFilesInDirectoryJob>(_userDbId, _driveId, _fileId, cursor, _dirOnly,
                                                                          _translateV2ToV3);
        } catch (const std::exception &e) {
            LOG_WARN(Log::instance()->getLogger(), getConstructorFailureLogMessage(e));

            return AbstractTokenNetworkJob::exception2ExitCode(e);
        }

        fileListJob->setLimit(_limit);
        fileListJob->setWithPath(_withPath);

        if (const auto exitInfo = fileListJob->runSynchronously(); !exitInfo) {
            LOG_WARN(Log::instance()->getLogger(), getRunSynchronouslyFailureLogMessage(exitInfo));

            return exitInfo;
        }

        // Concatenate partial listings
        const auto &nodeInfoList = fileListJob->nodeInfoList();
        _nodeInfoList.reserve(_nodeInfoList.size() + _nodeInfoList.size());
        _nodeInfoList.insert(_nodeInfoList.end(), nodeInfoList.begin(), nodeInfoList.end());

        hasMore = fileListJob->hasMore();
        cursor = fileListJob->cursor();
    } while (hasMore);

    return ExitCode::Ok;
}

} // namespace KDC
