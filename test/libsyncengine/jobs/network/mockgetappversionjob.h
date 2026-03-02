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

#include "testincludes.h"
#include "utility/types.h"
#include "jobs/network/infomaniak_API/getappversionjob.h"

namespace KDC {

static const std::string highTagValue = "99.99.99";
static constexpr uint64_t highBuildVersionValue = 21240604;
static const std::string mediumTagValue = "55.55.55";
static constexpr uint64_t mediumBuildVersionValue = 20240604;
static const std::string lowTagValue = "1.1.1";
static constexpr uint64_t lowBuildVersionValue = 20200604;
enum VersionValue {
    High,
    Medium,
    Low
};

inline const std::string &tag(const VersionValue versionNumber) {
    switch (versionNumber) {
        case High:
            return highTagValue;
        case Medium:
            return mediumTagValue;
        case Low:
            return lowTagValue;
    }
    return lowTagValue;
}

inline uint64_t buildVersion(const VersionValue versionNumber) {
    switch (versionNumber) {
        case High:
            return highBuildVersionValue;
        case Medium:
            return mediumBuildVersionValue;
        case Low:
            return lowBuildVersionValue;
    }
    return lowBuildVersionValue;
}

class MockGetAppVersionJob final : public GetAppVersionJob {
    public:
        MockGetAppVersionJob(const Platform platform, const std::string &appID, const bool updateShouldBeAvailable) :
            GetAppVersionJob(platform, appID),
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

    private:
        std::string generateJsonReply(const std::string &tag, const uint64_t buildVersion) const {
            Poco::JSON::Object versionObj;
            (void) versionObj.set("tag", tag);
            (void) versionObj.set("tag_updated_at", "2020-06-04 15:06:37");
            (void) versionObj.set("version_changelog", "test");
            (void) versionObj.set("type", "production");
            (void) versionObj.set("build_version", buildVersion);
            (void) versionObj.set("build_min_os_version", "1.1");
            (void) versionObj.set("download_link", "test");

            Poco::JSON::Array publishedVersionsArray;
            for (const auto channel:
                 {VersionChannel::Prod, VersionChannel::Next, VersionChannel::Beta, VersionChannel::Internal}) {
                Poco::JSON::Object tmpObj;
                (void) tmpObj.set("tag", tag);
                (void) tmpObj.set("tag_updated_at", "2020-06-04 15:06:37");
                (void) tmpObj.set("version_changelog", "test");
                (void) tmpObj.set("type", GetAppVersionJob::toStr(channel));
                (void) tmpObj.set("build_version", buildVersion);
                (void) tmpObj.set("build_min_os_version", "1.1");
                (void) tmpObj.set("download_link", "test");
                (void) publishedVersionsArray.add(tmpObj);
            }

            Poco::JSON::Object applicationObj;
            (void) applicationObj.set("id", "27");
            (void) applicationObj.set("name", "com.infomaniak.drive");
            (void) applicationObj.set("platform", "mac-os");
            (void) applicationObj.set("store", "kStore");
            (void) applicationObj.set("api_id", "com.infomaniak.drive");
            (void) applicationObj.set("min_version", _bigMinAppVersion ? "1111.2222.3333.0" : "3.6.2.0");
            (void) applicationObj.set("next_version_rate", "0");
            (void) applicationObj.set("published_versions", publishedVersionsArray);

            Poco::JSON::Object dataObj;
            (void) dataObj.set("application_id", "27");
            (void) dataObj.set("prod_version", "production");
            (void) dataObj.set("version", versionObj);
            (void) dataObj.set("application", applicationObj);

            Poco::JSON::Object mainObj;
            (void) mainObj.set("result", "success");
            (void) mainObj.set("data", dataObj);

            std::ostringstream out;
            mainObj.stringify(out);
            return out.str();
        }

        bool _updateShouldBeAvailable{false};
        bool _bigMinAppVersion{false};
};
} // namespace KDC
