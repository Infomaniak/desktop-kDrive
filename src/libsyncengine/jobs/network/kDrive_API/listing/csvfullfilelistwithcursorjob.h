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
#include "remotesnapshotitemhandler.h"

namespace KDC {

class CsvFullFileListWithCursorJob final : public AbstractListingJob {
    public:
        enum class Zip {
            On,
            Off
        };
        CsvFullFileListWithCursorJob(DriveDbId driveDbId, RemoteNodeId remoteDirId, const RemoteNodeIdSet &blacklist = {},
                                     Zip zip = Zip::On);

        /**
         * @brief getItem
         * @param item : item extracted from line of the CSV file
         * @param error : blocking error, stop the process
         * @param ignore : parsing issue, non-blocking, just ignore the item
         * @param eof : whether the end of file has been reached or not
         * @return if return == true, continue parsing
         */
        bool getItem(RemoteSnapshotItem &item, bool &error, bool &ignore, bool &eof);
        std::string getCursor();

    private:
        std::string getSpecificUrl() override;
        std::string contentType() override;
        std::string acceptHeader() override;
        void setQueryParameters(Poco::URI &uri) override;

        ExitInfo handleResponse(std::istream &is) override;

        NodeId _remoteDirId;
        Zip _zip = Zip::On;

        std::stringstream _ss;
        RemoteSnapshotItemHandler _snapshotItemHandler;
};

} // namespace KDC
