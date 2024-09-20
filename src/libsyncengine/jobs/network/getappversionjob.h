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

#include "API_v2/abstracttokennetworkjob.h"

#include <config.h>

namespace KDC {
class GetAppVersionJob : public AbstractNetworkJob {
    public:
        GetAppVersionJob(Platform platform, const std::string &appID);

        inline const VersionInfo &getVersionInfo(DistributionChannel channel) { return _versionInfo[channel]; }

        std::string getUrl() override { return INFOMANIAK_API_URL + getSpecificUrl(); }

    protected:
        bool handleResponse(std::istream &is) override;

    private:
        std::string getSpecificUrl() override;
        std::string getContentType(bool &canceled) override;
        void setQueryParameters(Poco::URI &, bool &canceled) override { /* no query parameters */
        }
        void setData(bool &canceled) override { /* no body parameters */
        }
        bool handleError(std::istream &is, const Poco::URI &uri) override;

        [[nodiscard]] DistributionChannel toDistributionChannel(const std::string &val) const;

        const Platform _platform;
        const std::string _appId;

        std::unordered_map<DistributionChannel, VersionInfo> _versionInfo;
};
} // namespace KDC
