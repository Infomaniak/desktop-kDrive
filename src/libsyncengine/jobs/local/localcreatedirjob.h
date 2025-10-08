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

namespace KDC {

class LocalCreateDirJob : public SyncJob {
    public:
        LocalCreateDirJob(const SyncPath &destFilepath);

        SyncPath destFilePath() const { return _destFilePath; }

        const NodeId &nodeId() const { return _nodeId; }
        SyncTime modtime() const { return _modtime; }
        SyncTime creationTime() const { return _creationTime; }

    protected:
        ExitInfo canRun() override;

    private:
        ExitInfo runJob() override;

        SyncPath _destFilePath;

        NodeId _nodeId;
        SyncTime _modtime = 0;
        SyncTime _creationTime = 0;
};

} // namespace KDC
