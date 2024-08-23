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

class CopyToDirectoryJob : public AbstractTokenNetworkJob {
    public:
        CopyToDirectoryJob(int driveDbId, const NodeId &remoteFileId, const NodeId &remoteDestId, const SyncName &newName);

        inline const NodeId &nodeId() const { return _nodeId; }
        inline SyncTime modtime() const { return _modtime; }

    protected:
        virtual bool handleResponse(std::istream &is) override;

    private:
        virtual std::string getSpecificUrl() override;
        virtual void setQueryParameters(Poco::URI &, bool &) override {}
        virtual void setData(bool &canceled) override;

        NodeId _remoteFileId;
        NodeId _remoteDestId;
        SyncName _newName;

        NodeId _nodeId;
        SyncTime _modtime = 0;
};

}  // namespace KDC
