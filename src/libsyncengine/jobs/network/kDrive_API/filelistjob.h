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

#include "libsyncengine/jobs/syncjob.h"
#include "jobs/network/kDrive_API/listingconf.h"

#include "info/nodeinfo.h"

namespace KDC {

class FileListJob : public SyncJob {
    public:
        /// @throw DbError
        FileListJob(UserDbId userDbId, DriveDbId driveId, NodeId fileId = {},
                    TranslationMode translationMode = TranslationMode::None);
        /// @throw DbError
        explicit FileListJob(DriveDbId driveDbId, NodeId fileId = {}, TranslationMode translationMode = TranslationMode::None);

        void setListingConf(const ListingConf &listingConf) { _listingConf = listingConf; };

        // The return value of this accessor is only meaningful when all the responses of the
        // underlying file list requests have been handled.
        [[nodiscard]] ExitInfo nodeInfoList(RemoteNodeInfoList &nodeInfoList) const {
            return v2RemoteNodeInfoList(nodeInfoList);
        };

        // The node info list as returned by the backend API v3.
        // The return value is only meaningful when all the responses of the
        // underlying file list requests have been handled.
        [[nodiscard]] const RemoteNodeInfoList &v3RemoteNodeInfoList() const { return _remoteNodeInfoList; };

    protected:
        [[nodiscard]] std::string getConstructorFailureLogMessage(const std::exception &e) const;
        [[nodiscard]] std::string getRunSynchronouslyFailureLogMessage(const ExitInfo &exitInfo) const;

        UserDbId _userDbId{0};

        // The identifier of the drive in the local database
        DriveId _driveId{0};

        // The identifier of the drive in the local database
        DriveDbId _driveDbId{0};

        // The remote identifier of the folder whose file list is queried.
        RemoteNodeId _remoteDirId;

        ListingConf _listingConf;

        // The deserialization of the request JSON result.
        RemoteNodeInfoList _remoteNodeInfoList;

        // If `_translationMode` is `TranslationMode::V2ToV3`, the identifier of the directory, that is `_fileId`, will
        // be translated in every underlying file listing requests.
        TranslationMode _translationMode{TranslationMode::None};

    private:
        // The node info list as returned by the backend API v2
        [[nodiscard]] ExitInfo v2RemoteNodeInfoList(RemoteNodeInfoList &nodeInfoList) const;

        [[nodiscard]] std::string createLogMessage(const std::string &coreMsg) const;

        [[nodiscard]] virtual std::string getConstructorFailureCoreMsg() const = 0;
        [[nodiscard]] virtual std::string getRunSynchronouslyFailureCoreMsg() const = 0;
};

} // namespace KDC
