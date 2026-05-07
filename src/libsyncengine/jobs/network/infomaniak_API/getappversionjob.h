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
#include "utility/urlhelper.h"

#include <config.h>

namespace KDC {

class GetAppVersionJob : public AbstractTokenNetworkJob {
    public:
        GetAppVersionJob(const DistributionChannel currentChannel, const std::string &appID);
        GetAppVersionJob(const DistributionChannel currentChannel, const std::string &appID,
                         const std::vector<UserId> &userIdList);
        ~GetAppVersionJob() override = default;

        [[nodiscard]] const VersionInfo &versionInfo() { return _versionsInfo; }

        [[nodiscard]] const std::string &minAppVersion() const { return _minAppVersion; }

    protected:
        ExitInfo handleResponse(std::istream &is) override;

    private:
        std::string getUrl() override { return UrlHelper::infomaniakApiUrl(_apiVersion, true) + getSpecificUrl(); }
        std::string getSpecificUrl() override;

        void setQueryParameters(Poco::URI &uri) override;
        ExitInfo handleError(const std::string &replyBody, const Poco::URI &uri) override;

        const DistributionChannel _currentChannel{DistributionChannel::Unknown};

        const std::string _appId;
        const std::vector<UserId> _userIdList;

        VersionInfo _versionsInfo;
        std::string _minAppVersion;
};

} // namespace KDC
