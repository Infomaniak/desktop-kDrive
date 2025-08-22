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

namespace KDC {

class ContinueFileListWithCursorJob final : public AbstractListingJob {
    public:
        ContinueFileListWithCursorJob(int driveDbId, const std::string &cursor, NodeSet blacklist = {});

    private:
        std::string getSpecificUrl() override;
        void setQueryParameters(Poco::URI &uri, bool &canceled) override;

        std::string _cursor;
};

} // namespace KDC
