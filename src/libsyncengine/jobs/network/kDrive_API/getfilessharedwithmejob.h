/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2026 Infomaniak Network SA
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

#include "info/nodeinfo.h"

namespace KDC {

class GetFilesSharedWithMeJob : public AbstractTokenNetworkJob {
    public:
        /// @brief List the files that the user shared on the drive.
        /// @param driveDbId The database ID of the drive.
        /// @param cursorInput The cursor of the previously fetched page, empty for a first page.
        /// @param limit The maximal number of items returned by the request.
        explicit GetFilesSharedWithMeJob(UserDbId userDbId, DriveId driveId, Cursor cursorInput = {}, int64_t limit = 10);

        // The node info list as returned by the backend API v3
        [[nodiscard]] const RemoteNodeInfoList &v3RemoteNodeInfoList() const { return _remoteNodeInfoList; }

        [[nodiscard]] const Cursor &cursor() const { return _cursorOutput; }
        [[nodiscard]] bool hasMore() const { return _hasMore; }
        [[nodiscard]] bool sharedIsEmpty() const { return _remoteNodeInfoList.empty(); }

    private:
        friend class TestGetFilesSharedWithMeJob;

        std::string getSpecificUrl() override;
        void setQueryParameters(Poco::URI &uri) override;
        ExitInfo handleResponse(std::istream &is) override;

        // Fill the `_remoteNodeInfoList` data structure with the deserialization
        // of the JSON result's `data` field.
        // Note: items with empty normalized name are ignored and not added to the list.
        ExitInfo deserializeDataArray();

        // The maximal number of items returned by the request.
        // Note: `limit` should be at least 5 since otherwise the backend returns an HTTP 422 error.
        int64_t _limit;

        // If `_hasMore` is `true` a subsequent request is required.
        bool _hasMore{false};

        // The starting point of the listing to be performed by the backend.
        Cursor _cursorInput;

        // Used for subsequent requests when `_hasMore` is `true`
        Cursor _cursorOutput;

        // The deserialization of the request JSON result.
        RemoteNodeInfoList _remoteNodeInfoList;
};

} // namespace KDC
