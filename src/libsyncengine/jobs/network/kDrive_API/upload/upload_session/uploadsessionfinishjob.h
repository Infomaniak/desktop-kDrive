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
#include "libcommonserver/vfs/vfs.h"

namespace KDC {

class UploadSessionFinishJob : public AbstractUploadSessionJob {
    public:
        UploadSessionFinishJob(const std::shared_ptr<Vfs> &vfs, UploadSessionType uploadType, int driveDbId,
                               const SyncPath &absoluteFilePath, const std::string &sessionToken,
                               const std::string &totalChunkHash, uint64_t totalChunks, SyncTime creationTime,
                               SyncTime modificationTime);

        UploadSessionFinishJob(UploadSessionType uploadType, const SyncPath &absoluteFilePath, const std::string &sessionToken,
                               const std::string &totalChunkHash, uint64_t totalChunks, SyncTime creationTime,
                               SyncTime modificationTime);

        ~UploadSessionFinishJob() override;

        const NodeId &nodeId() const { return _nodeId; }
        SyncTime creationTime() const { return _creationTimeOut; }
        SyncTime modificationTime() const { return _modificationTimeOut; }
        int64_t size() const { return _sizeOut; }

    protected:
        bool handleResponse(std::istream &is) override;

    private:
        std::string getSpecificUrl() override;
        void setQueryParameters(Poco::URI &, bool &) override { /*No query parameters*/
        }
        inline ExitInfo setData() override;

        std::string _totalChunkHash;
        uint64_t _totalChunks = 0;
        const SyncTime _creationTimeIn = 0;
        const SyncTime _modificationTimeIn = 0;

        NodeId _nodeId;
        SyncTime _creationTimeOut = 0;
        SyncTime _modificationTimeOut = 0;
        int64_t _sizeOut = 0;
        const std::shared_ptr<Vfs> _vfs;
};

} // namespace KDC
