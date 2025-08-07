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
#include "db/syncdb.h"

namespace KDC {

class DriveUploadSession : public AbstractUploadSession {
    public:
        // Using file name and parent ID, for file creation only.
        DriveUploadSession(const std::shared_ptr<Vfs> &vfs, int driveDbId, std::shared_ptr<SyncDb> syncDb,
                           const SyncPath &filepath, const SyncName &filename, const NodeId &remoteParentDirId,
                           SyncTime creationTime, SyncTime modificationTime, bool liteSyncActivated, uint64_t nbParallelThread);
        // Using file ID, for file edition only.
        DriveUploadSession(const std::shared_ptr<Vfs> &vfs, int driveDbId, std::shared_ptr<SyncDb> syncDb,
                           const SyncPath &filepath, const NodeId &fileId, SyncTime modificationTime, bool liteSyncActivated,
                           uint64_t nbParallelThread);
        ~DriveUploadSession() override;

        const NodeId &nodeId() const { return _nodeId; }
        SyncTime creationTime() const { return _creationTimeOut; }
        SyncTime modificationTime() const { return _modificationTimeOut; }
        int64_t size() const { return _sizeOut; }

    protected:
        bool handleStartJobResult(const std::shared_ptr<UploadSessionStartJob> &startJob,
                                  const std::string &uploadToken) override;
        bool handleFinishJobResult(const std::shared_ptr<UploadSessionFinishJob> &finishJob) override;
        bool handleCancelJobResult(const std::shared_ptr<UploadSessionCancelJob> &cancelJob) override;
        bool runJobInit() override;

        std::shared_ptr<UploadSessionStartJob> createStartJob() override;
        std::shared_ptr<UploadSessionChunkJob> createChunkJob(const std::string &chunkContent, uint64_t chunkNb,
                                                              std::streamsize actualChunkSize) override;
        std::shared_ptr<UploadSessionFinishJob> createFinishJob() override;
        std::shared_ptr<UploadSessionCancelJob> createCancelJob() override;

    private:
        int _driveDbId = 0;
        std::shared_ptr<SyncDb> _syncDb;

        NodeId _fileId;
        SyncTime _creationTimeIn = 0;
        const SyncTime _modificationTimeIn = 0;

        int64_t _uploadSessionTokenDbId = 0;
        NodeId _remoteParentDirId;

        NodeId _nodeId;
        SyncTime _creationTimeOut = 0;
        SyncTime _modificationTimeOut = 0;
        int64_t _sizeOut = 0;
        const std::shared_ptr<Vfs> _vfs;
};
} // namespace KDC
