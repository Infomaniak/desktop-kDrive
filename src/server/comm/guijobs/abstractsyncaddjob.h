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
#include "libcommon/info/syncinfo.h"

namespace KDC {

class AbstractSyncAddJob : public AbstractGuiJob {
    public:
        AbstractSyncAddJob(std::shared_ptr<CommManager> commManager, int requestId, const Poco::DynamicStruct &inParams,
                           std::shared_ptr<AbstractCommChannel> channel);

    protected:
        ExitInfo deserializeInputParms() override;
        ExitInfo serializeOutputParms() override;
        ExitInfo process() override { return ExitCode::Ok; }

        ExitInfo process(const SyncInfo &syncInfo);

        auto localFolderPath() const { return _localFolderPath; }
        auto serverFolderPath() const { return _serverFolderPath; }
        auto serverFolderNodeId() const { return _serverFolderNodeId; }
        auto liteSync() const { return _liteSync; }
        auto blackList() const { return _blackList; }

        auto &syncInfo() { return _syncInfo; }

    private:
        // Input parameters
        SyncPath _localFolderPath;
        SyncPath _serverFolderPath;
        NodeId _serverFolderNodeId;
        bool _liteSync = false;
        std::vector<NodeId> _blackList;

        // Output parameters
        SyncInfo _syncInfo;
};

} // namespace KDC
