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
#include "utility/urlhelper.h"

#include <config.h>

namespace KDC {

class GetAppVersionJob : public AbstractNetworkJob {
    public:
        GetAppVersionJob(Platform platform, const std::string &appID);
        GetAppVersionJob(Platform platform, const std::string &appID, const std::vector<int> &userIdList);
        ~GetAppVersionJob() override = default;

        /**
         * @brief Return the adequate version info between Production or Production-Next.
         * @return `VersionChannel` enum value.
         */
        [[nodiscard]] VersionChannel prodVersionChannel() const { return _prodVersionChannel; }
        [[nodiscard]] const VersionInfo &versionInfo(const VersionChannel channel) { return _versionsInfo[channel]; }
        [[nodiscard]] const AllVersionsInfo &versionsInfo() const { return _versionsInfo; }

        std::string getUrl() override { return UrlHelper::infomaniakApiUrl(1, true) + getSpecificUrl(); }

        static std::string toStr(Platform platform);
        static std::string toStr(VersionChannel channel);

    protected:
        bool handleResponse(std::istream &is) override;

    private:
        std::string getSpecificUrl() override;
        std::string getContentType(bool &canceled) override;
        void setQueryParameters(Poco::URI &uri, bool &canceled) override;
        ExitInfo setData() override { return ExitCode::Ok; }
        bool handleError(std::istream &is, const Poco::URI &uri) override;

        [[nodiscard]] VersionChannel toDistributionChannel(const std::string &val) const;

        const Platform _platform{Platform::Unknown};
        const std::string _appId;
        const std::vector<int> _userIdList;
        VersionChannel _prodVersionChannel{VersionChannel::Unknown};
        AllVersionsInfo _versionsInfo;
};

} // namespace KDC
