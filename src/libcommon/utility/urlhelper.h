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

#include "utility/types.h"

namespace KDC {

class UrlHelper {
    public:
        static std::string infomaniakApiUrl(uint8_t version = 2);
        static std::string kDriveApiUrl(uint8_t version = 2);
        static std::string notifyApiUrl(uint8_t version = 2);
        static std::string loginApiUrl();
        //! Returns the address of the kDrive web application page of a folder of a drive. The web application opens
        //! an item within its parent folder: the failure window of the file opening flow addresses the parent folder
        //! of an item, from which the item can be located and downloaded manually.
        static std::string kDriveWebAppFolderUrl(DriveId driveId, const std::string &folderNodeId);

    private:
        static bool usePreProdUrl();
};

} // namespace KDC
