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

#include "abstracttokennetworkjob.h"

namespace KDC {

class GetFileInfoJob : public AbstractTokenNetworkJob {
    public:
        GetFileInfoJob(int userDbId, int driveId, const NodeId &nodeId);
        GetFileInfoJob(int driveDbId, const NodeId &nodeId);

        inline const NodeId &nodeId() const { return _nodeId; }
        inline const NodeId &parentNodeId() const { return _parentNodeId; }
        inline SyncTime creationTime() const { return _creationTime; }
        inline SyncTime modtime() const { return _modtime; }
        inline bool isLink() const { return _isLink; }
        inline int64_t size() const { return _size; }

        inline void setWithPath(bool val) { _withPath = val; }
        inline SyncPath path() const { return _path; }

    protected:
        virtual bool handleResponse(std::istream &is) override;
        virtual bool handleError(std::istream &is, const Poco::URI &uri) override;

    private:
        virtual std::string getSpecificUrl() override;
        virtual void setQueryParameters(Poco::URI &, bool &) override;
        virtual void setData(bool &canceled) override { canceled = false; }

        NodeId _nodeId;
        NodeId _parentNodeId;
        std::string _name;
        SyncTime _creationTime{0};
        SyncTime _modtime{0};
        bool _isLink{false};
        bool _withPath{false};
        int64_t _size{-1};
        SyncPath _path;
};

} // namespace KDC
