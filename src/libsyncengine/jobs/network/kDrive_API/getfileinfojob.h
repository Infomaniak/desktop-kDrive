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

#include "jobs/network/abstracttokennetworkjob.h"

namespace KDC {

class GetFileInfoJob : public AbstractTokenNetworkJob {
    public:
        GetFileInfoJob(UserDbId userDbId, DriveId driveId, const RemoteNodeId &nodeId);
        GetFileInfoJob(DriveDbId driveDbId, const NodeId &nodeId);

        const RemoteNodeId &nodeId() const { return _nodeId; }
        const RemoteNodeId &parentNodeId() const { return _parentNodeId; }
        SyncTime creationTime() const { return _creationTime; }
        SyncTime modificationTime() const { return _modificationTime; }
        int32_t lastModifiedByUserId() const { return _lastModifiedByUserId; }
        bool isLink() const { return _isLink; }
        int64_t size() const { return _size; }

        void setWithPath(const bool val) { _withPath = val; }
        SyncPath path() const { return _path; }

    protected:
        ExitInfo handleResponse(std::istream &is) override;
        ExitInfo handleError(const std::string &replyBody, const Poco::URI &uri) override;

    private:
        std::string getSpecificUrl() override;
        void setQueryParameters(Poco::URI &) override;
        inline ExitInfo setData() override { return ExitCode::Ok; }

        RemoteNodeId _nodeId;
        RemoteNodeId _parentNodeId;
        std::string _name;
        SyncTime _creationTime{0};
        SyncTime _modificationTime{0};
        int32_t _lastModifiedByUserId{-1};
        bool _isLink{false};
        bool _withPath{false};
        int64_t _size{-1};
        SyncPath _path;
};

} // namespace KDC
