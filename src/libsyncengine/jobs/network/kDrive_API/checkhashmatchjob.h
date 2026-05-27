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

#include "jobs/network/abstracttokennetworkjob.h"

namespace KDC {

class CheckHashMatchJob : public AbstractTokenNetworkJob {
    public:
        CheckHashMatchJob(DriveDbId driveDbId, const SyncPath &filePath, const NodeId &nodeId,
                          const int64_t remoteSize);
        CheckHashMatchJob(DriveDbId driveDbId, const SyncPath &filePath, const NodeId &nodeId, const int64_t localSize,
                          const int64_t remoteSize);

        [[nodiscard]] const NodeId &nodeId() const { return _nodeId; }
        [[nodiscard]] bool shouldDownload() const { return _shouldDownload; }

    protected:
        ExitInfo handleResponse(std::istream &is) override;

    private:
        std::string getSpecificUrl() override;
        ExitInfo getFileSize(const SyncPath &path, int64_t &size);
        ExitInfo runJob() noexcept override;

        SyncPath _filePath;

        NodeId _nodeId;
        std::string _remoteHash;
        std::string _localHash;

        int64_t _localSize = 0;
        int64_t _remoteSize = 0;

        bool _shouldDownload = true;
};

} // namespace KDC
