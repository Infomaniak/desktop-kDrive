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

#include "server/comm/guijobs/abstractguijob.h"
#include "libcommon/info/driveinfo.h"

namespace KDC {

class NodeCreateMissingFoldersJob : public AbstractGuiJob {
    public:
        NodeCreateMissingFoldersJob(std::shared_ptr<CommManager> commManager, int requestId, const Poco::DynamicStruct &inParams,
                                    std::shared_ptr<AbstractCommChannel> channel);

    private:
        // Input parameters
        int32_t _userDbId = 0;
        int32_t _driveId = 0;
        SyncPath _relativePath;
        NodeId _parentNodeId;

        // Output parameters
        NodeId _nodeId;


        ExitInfo deserializeInputParms() override;
        ExitInfo serializeOutputParms() override;
        ExitInfo process() override;

        ExitInfo pauseDriveSyncs(std::vector<SyncDbId> &pausedSyncs);
        ExitInfo createMissingFolders(NodeId &firstCreatedNodeId);
        ExitInfo blacklistNodeOnAllDriveSyncs(const NodeId &nodeId);
        void resumeSyncs(const std::vector<SyncDbId> &pausedSyncs);

        friend class TestGuiCommChannel;
};

} // namespace KDC
