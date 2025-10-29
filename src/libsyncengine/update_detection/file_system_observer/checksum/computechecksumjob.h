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

#include "jobs/syncjob.h"
#include "update_detection/file_system_observer/snapshot/livesnapshot.h"

#include <log4cplus/logger.h>

namespace KDC {

class ComputeChecksumJob : public SyncJob {
    public:
        ComputeChecksumJob(const NodeId &nodeId, const SyncPath &filepath, std::shared_ptr<LiveSnapshot> localSnapshot);

    protected:
        ExitInfo runJob() override;

    private:
        log4cplus::Logger _logger;

        NodeId _nodeId;
        SyncPath _filePath;
        std::shared_ptr<LiveSnapshot> _localSnapshot;
};

} // namespace KDC
