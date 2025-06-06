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
#include "abstracttokennetworkjob.h"
#include "libcommonserver/vfs/vfs.h"

namespace KDC {

class MoveJob : public AbstractTokenNetworkJob {
    public:
        MoveJob(const std::shared_ptr<Vfs> &vfs, int driveDbId, const SyncPath &destFilepath, const NodeId &fileId,
                const NodeId &destDirId, const SyncName &name = Str(""));
        ~MoveJob() override;

        bool canRun() override;

    private:
        std::string getSpecificUrl() override;
        void setQueryParameters(Poco::URI &uri, bool &canceled) override;
        ExitInfo setData() override;

        SyncPath _destFilepath;
        std::string _fileId;
        std::string _destDirId;
        SyncName _name;
        const std::shared_ptr<Vfs> _vfs;
};

} // namespace KDC
