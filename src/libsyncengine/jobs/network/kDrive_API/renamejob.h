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

#include "libcommon/utility/types.h"
#include "jobs/network/abstracttokennetworkjob.h"
#include "libcommonserver/vfs/vfs.h"

namespace KDC {

class RenameJob : public AbstractTokenNetworkJob {
    public:
        RenameJob(const std::shared_ptr<Vfs> &vfs, int driveDbId, const NodeId &remoteFileId, const SyncPath &absoluteFinalPath);
        ~RenameJob();

    private:
        virtual std::string getSpecificUrl() override;
        virtual void setQueryParameters(Poco::URI &, bool &canceled) override { canceled = false; }
        virtual ExitInfo setData() override;

        std::string _remoteFileId;
        SyncPath _absoluteFinalPath;
        const std::shared_ptr<Vfs> _vfs;
};

} // namespace KDC
