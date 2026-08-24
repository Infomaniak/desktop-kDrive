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

#include "libcommon/utility/types.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"

#include <log4cplus/logger.h>

namespace KDC {

struct ResolveUploadNeedResult {
        ExitInfo exitInfo = ExitCode::Ok;
        bool shouldUpload = true;
};

template<typename ApplyFileDates>
[[nodiscard]] ResolveUploadNeedResult resolveUploadNeed(const log4cplus::Logger &logger, const DriveDbId driveDbId,
                                                        const SyncPath &filePath, const NodeId &fileId, const int64_t remoteSize,
                                                        ApplyFileDates &&applyFileDates) {
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
        if (const auto exitInfo = std::forward<ApplyFileDates>(applyFileDates)(); !exitInfo) {
            LOGW_DEBUG(logger, L"applyFileDates failed: " << exitInfo << L" Proceeding with upload.");
            result.shouldUpload = true;
            result.exitInfo = exitInfo;
        }
    }

    return result;
}

} // namespace KDC
