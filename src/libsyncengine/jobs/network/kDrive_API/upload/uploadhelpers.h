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

#pragma once

#include "jobs/network/kDrive_API/checkhashmatchjob.h"
#include "jobs/network/kDrive_API/postfilemodificationdatejob.h"

#include "libcommon/utility/types.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"

#include <log4cplus/logger.h>

#include <optional>

namespace KDC {

struct ApplyFileDatesResult {
        ExitInfo exitInfo = ExitCode::Ok;
        NodeId nodeId;
        SyncTime creationTime = 0;
        SyncTime modificationTime = 0;
        int64_t size = 0;
};

[[nodiscard]] inline ApplyFileDatesResult applyFileDatesForExistingFile(const log4cplus::Logger &logger,
                                                                        const DriveDbId driveDbId, const SyncPath &filePath,
                                                                        const NodeId &fileId, const SyncTime creationTimeIn,
                                                                        const SyncTime modificationTimeIn) {
    ApplyFileDatesResult result;

    PostFileModificationDateJob postJob(driveDbId, fileId, modificationTimeIn);
    if (const auto exitInfo = postJob.runSynchronously(); !exitInfo) {
        LOGW_DEBUG(logger, L"PostFileModificationDateJob failed: " << exitInfo);
        result.exitInfo = exitInfo;
        return result;
    }

    uint64_t fileSize = 0;
    if (auto ioError = IoError::Success; !IoHelper::getFileSize(filePath, fileSize, ioError)) {
        LOGW_WARN(logger, L"Error in IoHelper::getFileSize for " << Utility::formatIoError(filePath, ioError));
        result.exitInfo = ExitInfo(ExitCode::SystemError, ExitCause::Unknown);
        return result;
    } else if (ioError == IoError::NoSuchFileOrDirectory) {
        LOGW_WARN(logger, L"Unable to read file size for " << Utility::formatIoError(filePath, ioError));
        result.exitInfo = ExitInfo(ExitCode::SystemError, ExitCause::NotFound);
        return result;
    } else if (ioError == IoError::AccessDenied) {
        LOGW_WARN(logger, L"Unable to read file size for " << Utility::formatIoError(filePath, ioError));
        result.exitInfo = ExitInfo(ExitCode::SystemError, ExitCause::FileAccessError);
        return result;
    } else if (ioError != IoError::Success) {
        LOGW_WARN(logger, L"Unable to read file size for " << Utility::formatIoError(filePath, ioError));
        result.exitInfo = ExitInfo(ExitCode::SystemError, ExitCause::Unknown);
        return result;
    }

    result.nodeId = fileId;
    result.creationTime = creationTimeIn;
    result.modificationTime = postJob.lastModifiedAt();
    result.size = static_cast<int64_t>(fileSize);
    return result;
}

struct ResolveUploadNeedResult {
        ExitInfo exitInfo = ExitCode::Ok;
        bool shouldUpload = true;
        std::optional<ApplyFileDatesResult> applyFileDatesResult;
};

[[nodiscard]] inline ResolveUploadNeedResult resolveUploadNeed(const log4cplus::Logger &logger, const DriveDbId driveDbId,
                                                               const SyncPath &filePath, const NodeId &fileId,
                                                               const int64_t remoteSize, const SyncTime creationTimeIn,
                                                               const SyncTime modificationTimeIn) {
    ResolveUploadNeedResult result;
    if (remoteSize < 0) {
        LOGW_WARN(logger, L"resolveUploadNeed: remote size unknown for " << Utility::formatSyncPath(filePath)
                                                                         << L". Proceeding with upload.");
        return result;
    }

    CheckHashMatchJob hashJob(driveDbId, filePath, fileId, remoteSize);
    if (const auto exitInfo = hashJob.runSynchronously(); !exitInfo) {
        LOGW_DEBUG(logger, L"CheckHashMatchJob failed: " << exitInfo << L" Proceeding with upload.");
        result.exitInfo = exitInfo;
        return result;
    }

    result.shouldUpload = !hashJob.hashMatch();
    if (!result.shouldUpload) {
        LOGW_DEBUG(logger, L"Changing last modified date without uploading: hash match");
        result.applyFileDatesResult =
                applyFileDatesForExistingFile(logger, driveDbId, filePath, fileId, creationTimeIn, modificationTimeIn);
        if (!result.applyFileDatesResult->exitInfo) {
            LOGW_DEBUG(logger, L"applyFileDatesForExistingFile failed: " << result.applyFileDatesResult->exitInfo
                                                                         << L" Proceeding with upload.");
            result.shouldUpload = true;
            result.exitInfo = result.applyFileDatesResult->exitInfo;
            result.applyFileDatesResult.reset();
        }
    }

    return result;
}

} // namespace KDC
