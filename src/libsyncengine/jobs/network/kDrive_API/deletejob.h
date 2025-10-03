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

#include "jobs/network/abstracttokennetworkjob.h"
#include "jobs/network/networkjobsparams.h"

namespace KDC {

class DeleteJob : public AbstractTokenNetworkJob {
    public:
        DeleteJob(int driveDbId, const NodeId &remoteItemId, const NodeId &localItemId, const SyncPath &absoluteLocalFilepath,
                  NodeType nodeType);
        DeleteJob(int driveDbId, const NodeId &remoteItemId); // To be used in tests only.
        virtual bool canRun() override;

    private:
        virtual std::string getSpecificUrl() override;
        inline virtual ExitInfo setData() override { return ExitCode::Ok; }

        const NodeId _remoteItemId;
        const NodeId _localItemId;
        SyncPath _absoluteLocalFilepath;
        NodeType _nodeType;
};

} // namespace KDC
