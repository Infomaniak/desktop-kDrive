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
#include "libcommon/utility/types.h"

namespace KDC {

class SyncPal;

// This job is used to quickly resolve a HardlinkNotSupportedError. Such errors occur on Windows when a file is hardlinked into
// another sync root (e.g. OneDrive) as the sync engine does not support hardlinks shared between two sync roots.
// The job removes the node from the sync database, saves a copy of the file into the rescue folder if the local file is not in
// sync with the database, and hard removes all the links of the file located under the sync root. The next synchronization will
// see the remote file as a new item and will download it again as a standard file.
class ErrorQuickResolveHardlinkJob : public AbstractGuiJob {
    public:
        ErrorQuickResolveHardlinkJob(std::shared_ptr<CommManager> commManager, int32_t requestId,
                                     const Poco::DynamicStruct &inParams, std::shared_ptr<AbstractCommChannel> channel);

    private:
        // Input parameters
        SyncDbId _syncDbId = 0;
        ErrorDbId _errorDbId = 0;
        NodeId _nodeId;
        SyncPath _relativeLocalPath;

        ExitInfo deserializeInputParms() override;
        ExitInfo serializeOutputParms() override { return ExitCode::Ok; }
        ExitInfo process() override;

        ExitInfo quickResolve(const std::shared_ptr<SyncPal> &syncPal);

        friend class TestGuiCommChannel;
        friend class TestErrorQuickResolveHardlinkJob;
};

} // namespace KDC
