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

namespace KDC {

class SyncOfflineFilesSizeJob : public AbstractGuiJob {
    public:
        SyncOfflineFilesSizeJob(std::shared_ptr<CommManager> commManager, int requestId, const Poco::DynamicStruct &inParams,
                      std::shared_ptr<AbstractCommChannel> channel);

    private:
        // Input parameters
        int _syncDbId = 0;

        // Output parameters
        uint64_t _size = 0;

        ExitInfo deserializeInputParms() override;
        ExitInfo serializeOutputParms() override;
        ExitInfo process() override;

        friend class TestGuiCommChannel;
};

} // namespace KDC
