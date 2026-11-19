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

//! Starts the download, into a local folder, of the item of a file opening request that failed: the identity of the
//! item is provided by the client interface, which received it in the failure signal of the request. The reply
//! carries the id of the started download, to be matched with the download state signals, and the path into which the
//! item is downloaded.
class FileOpeningDownloadJob : public AbstractGuiJob {
    public:
        explicit FileOpeningDownloadJob(std::shared_ptr<CommManager> commManager, int requestId,
                                        const Poco::DynamicStruct &inParams, std::shared_ptr<AbstractCommChannel> channel);

    private:
        ExitInfo deserializeInputParms() override;
        ExitInfo serializeOutputParms() override;
        ExitInfo process() override;

        // Input parameters
        DriveDbId _driveDbId{0};
        CommString _remoteNodeId;
        CommString _destinationFolderPath; //!< Empty to download the item into the download folder of the user.

        // Output parameters
        UniqueId _downloadId{0};
        CommString _finalPath;

        friend class TestGuiCommChannel;
};

} // namespace KDC
