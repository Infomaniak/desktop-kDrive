/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

GetAllFilesInDirectoryJob::GetAllFilesInDirectoryJob(const UserDbId userDbId, const DriveId driveId, NodeId fileId,
                                                     const TranslationMode translationMode /* = TranslationMode::V2ToV3 */) :
    FileListJob(userDbId, driveId, std::move(fileId), translationMode) {}

GetAllFilesInDirectoryJob::GetAllFilesInDirectoryJob(const DriveDbId driveDbId, NodeId fileId,
                                                     const TranslationMode translationMode /* = TranslationMode::V2ToV3 */) :
    FileListJob(driveDbId, std::move(fileId), translationMode) {}


void GetAllFilesInDirectoryJob::abort() {
    LOG_DEBUG(_logger, "Aborting exhaustive file list request " << jobId());
    SyncJob::abort();
}

ExitInfo GetAllFilesInDirectoryJob::runJob() {
    _remoteNodeInfoList.clear();
    bool hasMore = false;
    std::string cursor;
    do {
        std::shared_ptr<GetFilesInDirectoryJob> fileListJob = nullptr;
        try {
            fileListJob =
                    _driveDbId ? std::make_shared<GetFilesInDirectoryJob>(_driveDbId, _fileId, cursor, _translationMode)
                               : std::make_shared<GetFilesInDirectoryJob>(_userDbId, _driveId, _fileId, cursor, _translationMode);
        } catch (const std::bad_alloc &e) {
            LOG_WARN(Log::instance()->getLogger(), getConstructorFailureLogMessage(e));

            return AbstractTokenNetworkJob::exception2ExitCode(e);
        }

        fileListJob->setListingConf(_listingConf);

        if (const auto exitInfo = fileListJob->runSynchronously(); !exitInfo) {
            LOG_WARN(Log::instance()->getLogger(), getRunSynchronouslyFailureLogMessage(exitInfo));

            return exitInfo;
        }

        // Concatenate partial listings
        const auto &nodeInfoList = fileListJob->v3RemoteNodeInfoList();
        _remoteNodeInfoList.reserve(_remoteNodeInfoList.size() + nodeInfoList.size());
        (void) _remoteNodeInfoList.insert(_remoteNodeInfoList.end(), nodeInfoList.begin(), nodeInfoList.end());

        hasMore = fileListJob->hasMore();
        cursor = fileListJob->cursor();
    } while (hasMore);

    return ExitCode::Ok;
}

} // namespace KDC
