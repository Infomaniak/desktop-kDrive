/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#include "abstracttokennetworkjob.h"
#include "jobs/network/networkjobsparams.h"

namespace KDC {

class DownloadJob : public AbstractTokenNetworkJob {
    public:
        DownloadJob(int driveDbId, const NodeId &remoteFileId, const SyncPath &localpath, int64_t expectedSize,
                    SyncTime creationTime, SyncTime modtime, bool isCreate);
        DownloadJob(int driveDbId, const NodeId &remoteFileId, const SyncPath &localpath, int64_t expectedSize);
        ~DownloadJob();

        inline const NodeId &localNodeId() const { return _localNodeId; }
        inline SyncTime modtime() const { return _modtimeIn; }

    private:
        virtual std::string getSpecificUrl() override;
        virtual void setQueryParameters(Poco::URI &, bool &) override {}
        virtual void setData(bool &canceled) override { canceled = false; }

        virtual bool canRun() override;
        virtual void runJob() noexcept override;
        virtual bool handleResponse(std::istream &is) override;

        bool createLink(const std::string &mimeType, const std::string &data);
        bool removeTmpFile(const SyncPath &path);
        bool moveTmpFile(const SyncPath &path, bool &restartSync);

        NodeId _remoteFileId;
        SyncPath _localpath;
        int64_t _expectedSize = -1;
        SyncTime _creationTime = 0;
        SyncTime _modtimeIn = 0;
        bool _isCreate = false;
        bool _ignoreDateTime = false;
        bool _responseHandlingCanceled = false;

        NodeId _localNodeId;
};

}  // namespace KDC
