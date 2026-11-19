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

#include "abstractguijob.h"

namespace KDC {

//! Signal sent to the client interface to inform it of the state of a file opening requested through a
//! "kdrive://open/..." deeplink, and to close the corresponding pop-up window once the request has been processed.
class SignalFileOpeningStatusJob : public AbstractGuiJob {
    public:
        explicit SignalFileOpeningStatusJob(UniqueId jobId, FileOpeningPhase phase, int8_t percent, const CommString &itemName,
                                            const CommString &itemPath, int64_t receivedBytes, int64_t totalBytes);
        //! Failure status: reports the failure of the request, with the failure phase (Failed or NotFound) and the
        //! kDrive web application address of the item when it can be downloaded manually. The client interface
        //! shows its failure window with it. The drive database id and the remote node id of the item are reported
        //! as well, so that the failure window can trigger the download of the item into the download folder of the
        //! user.
        explicit SignalFileOpeningStatusJob(UniqueId jobId, FileOpeningPhase failurePhase, const CommString &webUrl,
                                            DriveDbId driveDbId, const CommString &remoteNodeId);

    private:
        ExitInfo deserializeInputParms() override { return ExitCode::Ok; }
        ExitInfo serializeOutputParms() override;
        ExitInfo process() override { return ExitCode::Ok; }

        // Output parameters
        UniqueId _jobId{0};
        FileOpeningPhase _phase{FileOpeningPhase::Finished};
        int8_t _percent{-1};
        CommString _itemName;
        CommString _itemPath;
        int64_t _receivedBytes{-1};
        int64_t _totalBytes{-1};
        CommString _webUrl; //!< kDrive web application address of the item, set when the opening failed.
        DriveDbId _driveDbId{0}; //!< Local database id of the drive of the item, set when the opening failed.
        CommString _remoteNodeId; //!< Remote node id of the item, set when the opening failed.
};

} // namespace KDC
