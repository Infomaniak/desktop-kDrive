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

//! Report the state of the macOS authorizations required by Lite Sync.
/*!
  Those authorizations cannot be reliably checked from the Client process: reading the TCC database requires the reading process
  itself to have been granted the Full Disk Access authorization, which the Client process does not need. Only the Server process
  can answer.
*/
class UtilityCheckMacOsPermissionsJob : public AbstractGuiJob {
    public:
        UtilityCheckMacOsPermissionsJob(std::shared_ptr<CommManager> commManager, int requestId,
                                        const Poco::DynamicStruct &inParams, std::shared_ptr<AbstractCommChannel> channel);

    private:
        // Output parameters
        bool _fullDiskAccess{false}; //!< The Server process has been granted the Full Disk Access authorization.
        bool _liteSyncExtEnabled{false}; //!< The Lite Sync extension is installed and enabled.
        bool _liteSyncExtFullDiskAccess{false}; //!< The Lite Sync extension has been granted the Full Disk Access authorization.

        ExitInfo deserializeInputParms() override { return ExitCode::Ok; };
        ExitInfo serializeOutputParms() override;
        ExitInfo process() override;

        friend class TestGuiCommChannel;
};

} // namespace KDC
