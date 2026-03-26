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

class GetFileInfoJobV3 : public AbstractTokenNetworkJob {
    public:
        GetFileInfoJobV3(UserDbId userDbId, DriveId driveId, const NodeId &nodeId);
        GetFileInfoJobV3(DriveDbId driveDbId, const NodeId &nodeId);

        inline const NodeId &nodeId() const { return _nodeId; }
        inline SyncTime creationTime() const { return _creationTime; }
        inline SyncTime modificationTime() const { return _modificationTime; }
        inline int32_t lastModifiedByUserId() const { return _lastModifiedByUserId; }
        inline int64_t size() const { return _size; }

    protected:
        ExitInfo handleResponse(std::istream &is) override;
        ExitInfo handleError(const std::string &replyBody, const Poco::URI &uri) override;

    private:
        std::string getSpecificUrl() override;
        void setQueryParameters(Poco::URI &) override;
        inline ExitInfo setData() override { return ExitCode::Ok; }

        NodeId _nodeId;
        std::string _name;
        SyncTime _creationTime{0};
        SyncTime _modificationTime{0};
        int32_t _lastModifiedByUserId{-1};
        int64_t _size{-1};
};

} // namespace KDC
