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
#include "update_detection/file_system_observer/snapshot/snapshotitem.h"

namespace KDC {

class CsvFullFileListWithCursorJob : public AbstractTokenNetworkJob {
    public:
        enum CsvIndex {
            CsvIndexId = 0,
            CsvIndexParentId,
            CsvIndexName,
            CsvIndexType,
            CsvIndexSize,
            CsvIndexCreatedAt,
            CsvIndexModtime,
            CsvIndexCanWrite,
            CsvIndexIsLink,
            CsvIndexEnd
        };

    public:
        CsvFullFileListWithCursorJob(int driveDbId, const NodeId &dirId, std::unordered_set<NodeId> blacklist = {},
                                     bool zip = true);

        /**
         * @brief getItem
         * @param item : item extracted from line of the CSV file
         * @param error : blocking error, stop the process
         * @param ignore : parsing issue, non blocking, just ignore the item
         * @return if return == true, continue parsing
         */
        bool getItem(SnapshotItem &item, bool &error, bool &ignore);
        std::string getCursor();

    private:
        virtual std::string getSpecificUrl() override;
        virtual void setQueryParameters(Poco::URI &uri, bool &) override;
        inline virtual void setData(bool &canceled) override { canceled = false; }

        virtual bool handleResponse(std::istream &is) override;

        bool updateSnapshotItem(const std::string &str, const CsvIndex index, SnapshotItem &item);

        NodeId _dirId;
        std::unordered_set<NodeId> _blacklist;
        bool _zip = true;
        bool _ignoreFirstLine = true;

        std::stringstream _ss;
};

}  // namespace KDC
