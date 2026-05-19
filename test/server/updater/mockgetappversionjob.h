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
#include "jobs/network/infomaniak_API/getappversionjob.h"

namespace KDC {

static const std::string highTagValue = "99.99.99";
static constexpr uint64_t highBuildVersionValue = 21240604;
static const std::string lowTagValue = "1.1.1";
static constexpr uint64_t lowBuildVersionValue = 20200604;
enum VersionValue {
    High,
    Low
};

inline const std::string &tag(const VersionValue versionNumber) {
    switch (versionNumber) {
        case High:
            return highTagValue;
        case Low:
            return lowTagValue;
    }
    return lowTagValue;
}

inline uint64_t buildVersion(const VersionValue versionNumber) {
    switch (versionNumber) {
        case High:
            return highBuildVersionValue;
        case Low:
            return lowBuildVersionValue;
    }
    return lowBuildVersionValue;
}

class MockGetAppVersionJob final : public GetAppVersionJob {
    public:
        MockGetAppVersionJob(const DistributionChannel currentChannel, const std::string &appID,
                             const bool updateShouldBeAvailable) :
            GetAppVersionJob(currentChannel, appID),
            _updateShouldBeAvailable(updateShouldBeAvailable) {}

        ExitInfo runJob() noexcept override {
            const auto str = _updateShouldBeAvailable ? generateJsonReply(highTagValue, highBuildVersionValue)
                                                      : generateJsonReply(lowTagValue, lowBuildVersionValue);
            const std::istringstream iss(str);
            std::istream is(iss.rdbuf());
            (void) GetAppVersionJob::handleResponse(is);
            return ExitCode::Ok;
        }

        void setBigMinAppVersion(const bool val) { _bigMinAppVersion = val; }
        void setBigMinOsVersion(const bool bigMinOsVersion) { _bigMinOsVersion = bigMinOsVersion; }

    private:
        std::string generateJsonReply(const std::string &tag, const uint64_t buildVersion) const {
            Poco::JSON::Object dataObj;
            (void) dataObj.set("tag", tag);
            (void) dataObj.set("build_version", buildVersion);
            (void) dataObj.set("build_min_os_version", _bigMinOsVersion ? "9999.8888.7777" : "1.1.1");
            (void) dataObj.set("download_link", "dummyLink");
            (void) dataObj.set("checksum", "dummyChecksum");
            (void) dataObj.set("min_version", _bigMinAppVersion ? "9999.8888.7777.6666" : "3.6.2.0");
            (void) dataObj.set("channel", toString(_currentChannel));

            Poco::JSON::Object mainObj;
            (void) mainObj.set("result", "success");
            (void) mainObj.set("data", dataObj);

            std::ostringstream out;
            mainObj.stringify(out);
            return out.str();
        }

        bool _updateShouldBeAvailable{false};
        bool _bigMinAppVersion{false};
        bool _bigMinOsVersion{false};
};
} // namespace KDC
