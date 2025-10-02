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

namespace KDC {

class UploadSessionChunkJob : public AbstractUploadSessionJob {
    public:
        UploadSessionChunkJob(UploadSessionType uploadType, int driveDbId, const SyncPath &filepath,
                              const std::string &sessionToken, const std::string &chunkContent, uint64_t chunkNb,
                              uint64_t chunkSize, UniqueId sessionJobId);

        UploadSessionChunkJob(UploadSessionType uploadType, const SyncPath &filepath, const std::string &sessionToken,
                              const std::string &chunkContent, uint64_t chunkNb, uint64_t chunkSize, UniqueId sessionJobId);
        ~UploadSessionChunkJob() override;

        const std::string &chunkHash() const { return _chunkHash; }
        UniqueId sessionJobId() const { return _sessionJobId; }
        uint64_t chunkSize() const { return _chunkSize; }

    private:
        std::string getSpecificUrl() override;
        std::string getContentType() override;
        void setQueryParameters(Poco::URI &) override;
        ExitInfo setData() override { return ExitCode::Ok; }

        std::string _chunkHash;
        uint64_t _chunkNb = 0;
        uint64_t _chunkSize = 0;
        UniqueId _sessionJobId = 0;
};

} // namespace KDC
