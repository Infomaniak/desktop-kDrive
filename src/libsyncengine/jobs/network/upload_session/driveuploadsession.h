/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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
#include "db/syncdb.h"

#include <log4cplus/logger.h>

namespace KDC {

class DriveUploadSession : public AbstractUploadSession {
    public:
        DriveUploadSession(int driveDbId, std::shared_ptr<SyncDb> syncDb, const SyncPath &filepath, const SyncName &filename,
                           const NodeId &remoteParentDirId, SyncTime modtime, bool liteSyncActivated,
                           uint64_t nbParalleleThread = 1);
        ~DriveUploadSession() override;

        inline const NodeId &nodeId() const { return _nodeId; }
        inline SyncTime modtime() const { return _modtimeOut; }

    protected:

        bool prepareChunkJob(const std::shared_ptr<UploadSessionChunkJob> &chunkJob) override;
        bool handleStartJobResult(const std::shared_ptr<UploadSessionStartJob> &StartJob, std::string uploadToken) override;
        bool handleFinishJobResult(const std::shared_ptr<UploadSessionFinishJob> &finishJob) override;
        bool handleCancelJobResult(const std::shared_ptr<UploadSessionCancelJob> &cancelJob) override;
        bool runJobInit() override;

        std::shared_ptr<UploadSessionStartJob> createStartJob() override;
        std::shared_ptr<UploadSessionChunkJob> createChunkJob(const std::string &chunckContent, uint64_t chunkNb,
                                                              std::streamsize actualChunkSize) override;
        std::shared_ptr<UploadSessionFinishJob> createFinishJob() override;
        std::shared_ptr<UploadSessionCancelJob> createCancelJob() override;

    private:
        int _driveDbId = 0;
        std::shared_ptr<SyncDb> _syncDb;

        NodeId _fileId;
        SyncTime _modtimeIn = 0;

        bool _liteSyncActivated = false;

        int64_t _uploadSessionTokenDbId = 0;
        NodeId _remoteParentDirId;

        NodeId _nodeId;
        SyncTime _modtimeOut = 0;
};
}  // namespace KDC
