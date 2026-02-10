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

#include "libsyncengine/jobs/syncjob.h"
#include "libsyncengine/jobs/network/kDrive_API/filelistjob.h"

#include "info/nodeinfo.h"

namespace KDC {

class GetAllFilesInDirectoryJob : public FileListJob {
    public:
        GetAllFilesInDirectoryJob(int userDbId, int driveId, NodeId fileId, bool translateV2ToV3 = false);
        explicit GetAllFilesInDirectoryJob(int driveDbId, NodeId fileId, bool translateV2ToV3 = false);

        void abort() override;

    private:
        ExitInfo runJob() override;

        [[nodiscard]] std::string getConstructorFailureCoreMsg() const override {
            return "Error in GetFilesInDirectoryJob::GetFilesInDirectoryJob for ";
        };
        [[nodiscard]] std::string getRunSynchronouslyFailureCoreMsg() const override {
            return "Error in GetFilesInDirectoryJob::runSynchronously for ";
        }
};

} // namespace KDC
