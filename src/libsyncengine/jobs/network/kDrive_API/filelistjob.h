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
#include "jobs/network/kDrive_API/listingconf.h"

#include "info/nodeinfo.h"

namespace KDC {

class FileListJob : public SyncJob {
    public:
        FileListJob(UserDbId userDbId, DriveDbId driveId, NodeId fileId = {},
                    TranslationMode translationMode = TranslationMode::None);
        explicit FileListJob(DriveDbId driveDbId, NodeId fileId = {}, TranslationMode translationMode = TranslationMode::None);

        void setListingConf(const ListingConf &listingConf) { _listingConf = listingConf; };

        // The return value of this accessor is only meaningful when all the responses of the
        // underlying file list requests have been handled.
        [[nodiscard]] NodeInfoList nodeInfoList() const { return v2NodeInfoList(); };

        // The node info list as returned by the backend API v3
        // The return value is only meaningful when all the responses of the
        // underlying file list requests have been handled.
        [[nodiscard]] const NodeInfoList &v3NodeInfoList() const { return _nodeInfoList; };

    protected:
        [[nodiscard]] std::string getConstructorFailureLogMessage(const std::exception &e) const;
        [[nodiscard]] std::string getRunSynchronouslyFailureLogMessage(const ExitInfo &exitInfo) const;

        UserDbId _userDbId{0};

        // The identifier of the drive in the local database
        DriveId _driveId{0};

        // The identifier of the drive in the local database
        DriveDbId _driveDbId{0};

        // The remote identifier of the folder whose file list is queried.
        NodeId _fileId;

        ListingConf _listingConf;

        // The deserialization of the request JSON result.
        NodeInfoList _nodeInfoList;

        // If `_translationMode` is `TranslationMode::V2ToV3`, the identifier of the directory, that is `_fileId`, will
        // be translated in every underlying file listing requests.
        TranslationMode _translationMode{TranslationMode::None};

    private:
        // The node info list as returned by the backend API v2
        [[nodiscard]] NodeInfoList v2NodeInfoList() const;

        [[nodiscard]] std::string createLogMessage(const std::string &coreMsg) const;

        [[nodiscard]] virtual std::string getConstructorFailureCoreMsg() const = 0;
        [[nodiscard]] virtual std::string getRunSynchronouslyFailureCoreMsg() const = 0;
};

} // namespace KDC
