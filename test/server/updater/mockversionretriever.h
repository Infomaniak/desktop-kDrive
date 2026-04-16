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

#include "testincludes.h"
#include "utility/types.h"
#include "server/updater/versionretriever.h"
#include "mockgetappversionjob.h"

namespace KDC {

class MockVersionRetriever : public VersionRetriever {
    public:
        void setUpdateShouldBeAvailable(const bool val) { _updateShouldBeAvailable = val; }
        void setBigMinAppVersion(const bool val) { _bigMinAppVersion = val; }
        void setBigMinOsVersion(const bool bigMinOsVersion) { _bigMinOsVersion = bigMinOsVersion; }
        void setAllVersionInfo(const AllVersionsInfo &versionInfo) { _versionsInfo = versionInfo; }
        void setVersionReceived(const bool isVersionReceived) { _isVersionReceived = isVersionReceived; }
        void setChecksumForAllChannels(const std::string &checksum) {
            for (auto &[channel, info]: _versionsInfo) {
                info.checksum = checksum;
            }
        }

    private:
        ExitCode generateGetAppVersionJob(const DistributionChannel channel, std::shared_ptr<AbstractNetworkJob> &job) override {
            static const std::string appUid = "1234567890";
            auto mockJob = std::make_shared<MockGetAppVersionJob>(channel, appUid, _updateShouldBeAvailable);
            mockJob->setBigMinAppVersion(_bigMinAppVersion);
            mockJob->setBigMinOsVersion(_bigMinOsVersion);
            job = mockJob;
            return ExitCode::Ok;
        }

        bool _updateShouldBeAvailable{false};
        bool _bigMinAppVersion{false};
        bool _bigMinOsVersion{false};
};
} // namespace KDC
