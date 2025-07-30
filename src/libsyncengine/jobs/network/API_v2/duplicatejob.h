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

#include "abstracttokennetworkjob.h"
#include "libcommonserver/vfs/vfs.h"

namespace KDC {

class DuplicateJob : public AbstractTokenNetworkJob {
    public:
        DuplicateJob(const std::shared_ptr<Vfs> &vfs, int driveDbId, const NodeId &remoteFileId,
                     const SyncPath &absoluteFinalPath);
        ~DuplicateJob() override;

        inline const NodeId &nodeId() const { return _nodeId; }
        inline SyncTime modtime() const { return _modtime; }

    protected:
        bool handleResponse(std::istream &is) override;

    private:
        std::string getSpecificUrl() override;
        void setQueryParameters(Poco::URI &, bool &) override { /* No query parameters */ }
        ExitInfo setData() override;

        NodeId _remoteFileId;
        SyncPath _absoluteFinalPath;

        NodeId _nodeId;
        SyncTime _modtime = 0;
        const std::shared_ptr<Vfs> _vfs;
};

} // namespace KDC
