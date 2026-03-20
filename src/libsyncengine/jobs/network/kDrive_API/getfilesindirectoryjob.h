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

#include "jobs/network/abstracttokennetworkjob.h"
#include "jobs/network/kDrive_API/listingconf.h"

#include "info/nodeinfo.h"

namespace KDC {

class GetFilesInDirectoryJob : public AbstractTokenNetworkJob {
    public:
        GetFilesInDirectoryJob(int userDbId, int driveId, NodeId fileId, std::string cursorInput = {},
                               bool translateV2toV3 = false);
        explicit GetFilesInDirectoryJob(int driveDbId, NodeId fileId, std::string cursorInput = {}, bool translateV2toV3 = false);

        void setListingConf(const ListingConf &listingConf) { _listingConf = listingConf; };

        [[nodiscard]] const std::string &cursor() const { return _cursorOutput; }
        [[nodiscard]] bool hasMore() const { return _hasMore; }

        [[nodiscard]] NodeInfoList nodeInfoList();

        // The node info list as returned by the backend API v3
        [[nodiscard]] const NodeInfoList &v3NodeInfoList() const { return _nodeInfoList; };

    protected:
        ExitInfo handleResponse(std::istream &is) override;


    private:
        std::string getSpecificUrl() override;
        void setQueryParameters(Poco::URI &uri) override;
        ExitInfo setData() override { return ExitCode::Ok; }

        // The node info list as returned by the backend API v2
        [[nodiscard]] NodeInfoList v2NodeInfoList();

        // Fill the `_nodeInfoList` data structure with the deserialization
        // of the JSON result's `data` field.
        ExitInfo deserializeDataArray();

        // The remote identifier of the folder whose file list is queried.
        NodeId _fileId;

        // If `_hasMore` is `true` a subsequent request is required.
        bool _hasMore{false};

        // The starting point of the listing to be performed by the backend.
        std::string _cursorInput;

        // Used for subsequent requests when `_hasMore` is `true`
        std::string _cursorOutput;

        // The deserialization of the request JSON result.
        NodeInfoList _nodeInfoList;

        ListingConf _listingConf;
};

} // namespace KDC
