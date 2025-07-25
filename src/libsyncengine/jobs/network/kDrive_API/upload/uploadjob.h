// Infomaniak kDrive - Desktop
// Copyright (C) 2023-2025 Infomaniak Network SA
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include "jobs/network/abstracttokennetworkjob.h"

#include "libcommon/utility/types.h"
#include "libcommonserver/vfs/vfs.h"

/**
 * WARNING:
 * Do not use this class to upload a file larger than 1GB. Instead, create an upload session and attach chunks.
 */

namespace KDC {

class UploadJob : public AbstractTokenNetworkJob {
    public:
        // Using file name and parent ID, for file creation only.
        UploadJob(const std::shared_ptr<Vfs> &vfs, int driveDbId, const SyncPath &absoluteFilePath, const SyncName &filename,
                  const NodeId &remoteParentDirId, SyncTime creationTime, SyncTime modificationTime);
        // Using file ID, for file edition only.
        UploadJob(const std::shared_ptr<Vfs> &vfs, int driveDbId, const SyncPath &absoluteFilePath, const NodeId &fileId,
                  SyncTime modificationTime);
        ~UploadJob() override;

        const NodeId &nodeId() const { return _nodeIdOut; }
        SyncTime creationTime() const { return _creationTimeOut; }
        SyncTime modificationTime() const { return _modificationTimeOut; }
        int64_t size() const { return _sizeOut; }

    protected:
        bool canRun() override;
        bool handleResponse(std::istream &is) override;

    private:
        std::string getSpecificUrl() override;
        void setQueryParameters(Poco::URI &, bool &canceled) override;
        ExitInfo setData() override;
        std::string getContentType(bool &canceled) override;

        ExitInfo readFile();
        ExitInfo readLink();

        SyncPath _absoluteFilePath;
        SyncName _filename;
        NodeId _fileId;
        NodeId _remoteParentDirId;
        std::string _contentHash;
        SyncTime _creationTimeIn = 0;
        SyncTime _modificationTimeIn = 0;

        NodeId _nodeIdOut;
        SyncTime _creationTimeOut = 0;
        SyncTime _modificationTimeOut = 0;
        int64_t _sizeOut = 0;

        LinkType _linkType = LinkType::None;
        SyncPath _linkTarget;
        NodeType _targetType = NodeType::File;

        const std::shared_ptr<Vfs> _vfs;
};

} // namespace KDC
