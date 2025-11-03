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

#include "server/comm/guijobs/syncaddabstractjob.h"
#include "libcommon/info/syncinfo.h"

namespace KDC {

class SyncAddJob : public SyncAddAbstractJob {
    public:
        SyncAddJob(std::shared_ptr<CommManager> commManager, int requestId, const Poco::DynamicStruct &inParams,
                   std::shared_ptr<AbstractCommChannel> channel);

    private:
        // Input parameters
        int _userDbId = 0;
        int _accountId = 0;
        int _driveId = 0;

        // Output parameters
        SyncInfo _syncInfo;

        ExitInfo deserializeInputParms() override;
        ExitInfo process() override;

        friend class TestGuiCommChannel;
};

} // namespace KDC
