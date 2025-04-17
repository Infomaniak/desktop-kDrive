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

#include "abstractuploadsession.h"
#include "utility/types.h"

namespace KDC {

class LogUploadSession : public AbstractUploadSession {
    public:
        explicit LogUploadSession(const SyncPath &filepath, uint64_t nbParallelThread = 1);

    protected:
        bool runJobInit() override;
        std::shared_ptr<UploadSessionStartJob> createStartJob() override;
        std::shared_ptr<UploadSessionChunkJob> createChunkJob(const std::string &chunkContent, uint64_t chunkNb,
                                                              std::streamsize actualChunkSize) override;
        std::shared_ptr<UploadSessionFinishJob> createFinishJob() override;
        std::shared_ptr<UploadSessionCancelJob> createCancelJob() override;

        bool handleStartJobResult(const std::shared_ptr<UploadSessionStartJob> &StartJob, std::string uploadToken) override;
        bool handleFinishJobResult(const std::shared_ptr<UploadSessionFinishJob> &finishJob) override;
        bool handleCancelJobResult(const std::shared_ptr<UploadSessionCancelJob> &cancelJob) override;
};

} // namespace KDC
