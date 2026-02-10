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

#include "jobs/network/abstracttokennetworkjob.h"

#include "info/nodeinfo.h"

namespace KDC {

class GetFilesInDirectoryJob : public AbstractTokenNetworkJob {
    public:
        GetFilesInDirectoryJob(int userDbId, int driveId, NodeId fileId, std::string cursorInput = {}, bool dirOnly = false,
                               bool translateV2toV3 = false);
        explicit GetFilesInDirectoryJob(int driveDbId, NodeId fileId, std::string cursorInput = {}, bool dirOnly = false,
                                        bool translateV2toV3 = false);

        void setWithPath(const bool val) { _withPath = val; }
        void setLimit(const int64_t limit) { _limit = limit; }
        [[nodiscard]] const std::string &cursor() const { return _cursorOutput; }
        [[nodiscard]] bool hasMore() const { return _hasMore; }

        // The return value of this accessor is only meaningful if the response has been handled.
        [[nodiscard]] const NodeInfoList &nodeInfoList() const { return _nodeInfoList; };

    protected:
        ExitInfo handleResponse(std::istream &is) override;


    private:
        std::string getSpecificUrl() override;
        void setQueryParameters(Poco::URI &uri) override;
        ExitInfo setData() override { return ExitCode::Ok; }

        // Fill the `_nodeInfoList` data structure with the deserialization
        // of the JSON result's `data` field.
        ExitInfo deserializeDataArray();

        // The remote identifier of the folder whose file list is queried.
        NodeId _fileId;

        // If `_withPath` is `true`, the paths of the items will be returned.
        bool _withPath{false};

        // If `_hasMore` is `true` a subsequent request is required.
        bool _hasMore{false};

        // The starting point of the listing to be performed by the backend.
        std::string _cursorInput;

        // Used for subsequent requests when `_hasMore` is `true`
        std::string _cursorOutput;

        // If `_dirOnly` is `true`, directories only will be listed in the result.
        bool _dirOnly{false};

        // The maximal number of items returned by the request.
        int64_t _limit{10};

        // The deserialization of the request JSON result.
        NodeInfoList _nodeInfoList;
};

} // namespace KDC
