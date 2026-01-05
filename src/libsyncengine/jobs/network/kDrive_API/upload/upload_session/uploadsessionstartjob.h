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

#include "abstractuploadsessionjob.h"
#include "utility/types.h"

namespace KDC {

class UploadSessionStartJob : public AbstractUploadSessionJob {
    public:
        // Using file name and parent ID, for create only
        UploadSessionStartJob(UploadSessionType uploadType, int driveDbId, const SyncName &filename, uint64_t size,
                              const NodeId &remoteParentDirId, uint64_t totalChunks);
        // Using file ID, for edit only
        UploadSessionStartJob(UploadSessionType uploadType, int driveDbId, const NodeId &fileId, uint64_t size,
                              uint64_t totalChunks);

        // Using file name for log upload
        UploadSessionStartJob(UploadSessionType uploadType, const SyncName &filename, uint64_t size, uint64_t totalChunks);

    private:
        std::string getSpecificUrl() override;
        inline ExitInfo setData() override;

        SyncName _filename;
        NodeId _fileId;
        uint64_t _totalSize = 0;
        NodeId _remoteParentDirId;
        uint64_t _totalChunks = 0;
        UploadSessionType _uploadType = UploadSessionType::Unknown;
};

} // namespace KDC
