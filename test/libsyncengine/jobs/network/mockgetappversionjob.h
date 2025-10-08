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
            GetAppVersionJob::handleResponse(is);
            return ExitCode::Ok;
        }

    private:
        std::string generateJsonReply(const std::string &tag, uint64_t buildVersion) {
            Poco::JSON::Object versionObj;
            versionObj.set("tag", tag);
            versionObj.set("tag_updated_at", "2020-06-04 15:06:37");
            versionObj.set("version_changelog", "test");
            versionObj.set("type", "production");
            versionObj.set("build_version", buildVersion);
            versionObj.set("build_min_os_version", "XXXX");
            versionObj.set("download_link", "test");

            Poco::JSON::Array publishedVersionsArray;
            for (const auto channel:
                 {VersionChannel::Prod, VersionChannel::Next, VersionChannel::Beta, VersionChannel::Internal}) {
                Poco::JSON::Object tmpObj;
                tmpObj.set("tag", tag);
                tmpObj.set("tag_updated_at", "2020-06-04 15:06:37");
                tmpObj.set("version_changelog", "test");
                tmpObj.set("type", GetAppVersionJob::toStr(channel));
                tmpObj.set("build_version", buildVersion);
                tmpObj.set("build_min_os_version", "XXXX");
                tmpObj.set("download_link", "test");
                publishedVersionsArray.add(tmpObj);
            }

            Poco::JSON::Object applicationObj;
            applicationObj.set("id", "27");
            applicationObj.set("name", "com.infomaniak.drive");
            applicationObj.set("platform", "mac-os");
            applicationObj.set("store", "kStore");
            applicationObj.set("api_id", "com.infomaniak.drive");
            applicationObj.set("min_version", "3.6.2");
            applicationObj.set("next_version_rate", "0");
            applicationObj.set("published_versions", publishedVersionsArray);

            Poco::JSON::Object dataObj;
            dataObj.set("application_id", "27");
            dataObj.set("prod_version", "production");
            dataObj.set("version", versionObj);
            dataObj.set("application", applicationObj);

            Poco::JSON::Object mainObj;
            mainObj.set("result", "success");
            mainObj.set("data", dataObj);

            std::ostringstream out;
            mainObj.stringify(out);
            return out.str();
        }

        bool _updateShouldBeAvailable{false};
};
} // namespace KDC
