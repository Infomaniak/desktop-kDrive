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

#include "utility/types.h"
#include "abstractnetworkjob.h"

namespace KDC {

class DirectDownloadJob final : public AbstractNetworkJob {
    public:
        DirectDownloadJob(const SyncPath &destinationFile, const std::string &url);

    protected:
        std::string getUrl() override { return _url; }

    private:
        bool handleResponse(std::istream &inputStream) override;
        bool handleError(std::istream &is, const Poco::URI &uri) override;

        const SyncPath _destinationFile;
        const std::string _url;
};

} // namespace KDC
