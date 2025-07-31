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

#include "abstractlistingjob.h"
#include "snapshotitemhandler.h"

namespace KDC {

class CsvFullFileListWithCursorJob final : public AbstractListingJob {
    public:
        CsvFullFileListWithCursorJob(int driveDbId, const NodeId &dirId, const NodeSet &blacklist = {}, bool zip = true);

        /**
         * @brief getItem
         * @param item : item extracted from line of the CSV file
         * @param error : blocking error, stop the process
         * @param ignore : parsing issue, non-blocking, just ignore the item
         * @param eof : whether the end of file has been reached or not
         * @return if return == true, continue parsing
         */
        bool getItem(SnapshotItem &item, bool &error, bool &ignore, bool &eof);
        std::string getCursor();

    private:
        std::string getSpecificUrl() override;
        void setSpecificQueryParameters(Poco::URI &uri) override;
        ExitInfo setData() override { return ExitCode::Ok; }

        bool handleResponse(std::istream &is) override;

        NodeId _dirId;
        bool _zip = true;

        std::stringstream _ss;
        SnapshotItemHandler _snapshotItemHandler;
};

} // namespace KDC
