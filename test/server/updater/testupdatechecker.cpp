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

#include "testupdatechecker.h"

#include "jobs/jobmanager.h"
#include "jobs/network/getappversionjob.h"
#include "libcommon/utility/utility.h"
#include "requests/parameterscache.h"
#include "server/updater/updatechecker.h"
#include "utility/utility.h"

namespace KDC {

static const std::string highTagValue = "99.99.99";
static constexpr uint64_t highBuildVersionValue = 21240604;
static const std::string mediumTagValue = "55.55.55";
static constexpr uint64_t mediumBuildVersionValue = 20240604;
static const std::string lowTagValue = "1.1.1";
static constexpr uint64_t lowBuildVersionValue = 20200604;

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
         {DistributionChannel::Prod, DistributionChannel::Next, DistributionChannel::Beta, DistributionChannel::Internal}) {
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

class GetAppVersionJobTest final : public GetAppVersionJob {
    public:
        GetAppVersionJobTest(const Platform platform, const std::string &appID, const bool updateShouldBeAvailable) :
            GetAppVersionJob(platform, appID), _updateShouldBeAvailable(updateShouldBeAvailable) {}

        void runJob() noexcept override {
            const auto str = _updateShouldBeAvailable ? generateJsonReply(highTagValue, highBuildVersionValue)
                                                      : generateJsonReply(lowTagValue, lowBuildVersionValue);
            const std::istringstream iss(str);
            std::istream is(iss.rdbuf());
            GetAppVersionJob::handleResponse(is);
            _exitCode = ExitCode::Ok;
        }

    private:
        bool _updateShouldBeAvailable{false};
};

class UpdateCheckerTest final : public UpdateChecker {
    public:
        void setUpdateShoudBeAvailable(const bool val) { _updateShouldBeAvailable = val; }

    private:
        ExitCode generateGetAppVersionJob(std::shared_ptr<AbstractNetworkJob> &job) override {
            static const std::string appUid = "1234567890";
            job = std::make_shared<GetAppVersionJobTest>(CommonUtility::platform(), appUid, _updateShouldBeAvailable);
            return ExitCode::Ok;
        }

        bool _updateShouldBeAvailable{false};
};

void TestUpdateChecker::setUp() {
    ParametersCache::instance(true);
}

void TestUpdateChecker::testCheckUpdateAvailable() {
    // Version is higher than current version
    {
        UpdateCheckerTest testObj;
        UniqueId jobId = 0;
        testObj.setUpdateShoudBeAvailable(true);
        testObj.checkUpdateAvailability(&jobId);
        while (!JobManager::instance()->isJobFinished(jobId)) Utility::msleep(10);
        CPPUNIT_ASSERT(testObj.versionInfo(DistributionChannel::Beta).isValid());
    }

    // Version is lower than current version
    {
        UpdateCheckerTest testObj;
        UniqueId jobId = 0;
        testObj.setUpdateShoudBeAvailable(false);
        testObj.checkUpdateAvailability(&jobId);
        while (!JobManager::instance()->isJobFinished(jobId)) Utility::msleep(10);
        CPPUNIT_ASSERT(testObj.versionInfo(DistributionChannel::Beta).isValid());
    }
}

enum VersionValue { High, Medium, Low };

const std::string &tag(const VersionValue versionNumber) {
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

uint64_t buildVersion(const VersionValue versionNumber) {
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

VersionInfo getVersionInfo(const DistributionChannel channel, const VersionValue versionNumber) {
    VersionInfo versionInfo;
    versionInfo.channel = channel;
    versionInfo.tag = tag(versionNumber);
    versionInfo.buildVersion = buildVersion(versionNumber);
    versionInfo.downloadUrl = "test";
    return versionInfo;
}

void TestUpdateChecker::testVersionInfo() {
    UpdateChecker testObj;
    testObj._prodVersionChannel = DistributionChannel::Prod;

    auto testFunc = [&testObj](const VersionValue expectedValue, const DistributionChannel expectedChannel,
                               const DistributionChannel selectedChannel, const std::vector<VersionValue> &versionsNumber) {
        testObj._versionsInfo.clear();
        testObj._versionsInfo.try_emplace(DistributionChannel::Prod,
                                          getVersionInfo(DistributionChannel::Prod, versionsNumber[0]));
        testObj._versionsInfo.try_emplace(DistributionChannel::Beta,
                                          getVersionInfo(DistributionChannel::Beta, versionsNumber[1]));
        testObj._versionsInfo.try_emplace(DistributionChannel::Internal,
                                          getVersionInfo(DistributionChannel::Internal, versionsNumber[2]));
        CPPUNIT_ASSERT_EQUAL(expectedChannel, testObj.versionInfo(selectedChannel).channel);
        CPPUNIT_ASSERT_EQUAL(tag(expectedValue), testObj.versionInfo(selectedChannel).tag);
        CPPUNIT_ASSERT_EQUAL(buildVersion(expectedValue), testObj.versionInfo(selectedChannel).buildVersion);
    };

    // selected version: Prod
    /// versions values: Prod > Beta > Internal
    testFunc(High, DistributionChannel::Prod, DistributionChannel::Prod, {High, Medium, Low});
    /// versions values: Internal > Beta > Prod
    testFunc(Low, DistributionChannel::Prod, DistributionChannel::Prod, {Low, Medium, High});
    /// versions values: Prod == Beta == Internal
    testFunc(Medium, DistributionChannel::Prod, DistributionChannel::Prod, {Medium, Medium, Medium});

    // selected version: Beta
    /// versions values: Prod > Beta > Internal
    testFunc(High, DistributionChannel::Prod, DistributionChannel::Beta, {High, Medium, Low});
    /// versions values: Internal > Beta > Prod
    testFunc(Medium, DistributionChannel::Beta, DistributionChannel::Beta, {Low, Medium, High});
    /// versions values: Prod == Beta == Internal
    testFunc(Medium, DistributionChannel::Prod, DistributionChannel::Beta, {Medium, Medium, Medium});

    // selected version: Internal
    /// versions values: Prod > Beta > Internal
    testFunc(High, DistributionChannel::Prod, DistributionChannel::Internal, {High, Medium, Low});
    /// versions values: Internal > Beta > Prod
    testFunc(High, DistributionChannel::Internal, DistributionChannel::Internal, {Low, Medium, High});
    /// versions values: Beta > Prod > Internal
    testFunc(High, DistributionChannel::Beta, DistributionChannel::Internal, {Medium, High, Low});
    /// versions values: Prod > Internal > Beta
    testFunc(High, DistributionChannel::Prod, DistributionChannel::Internal, {High, Low, Medium});
    /// versions values: Beta > Internal > Prod
    testFunc(High, DistributionChannel::Beta, DistributionChannel::Internal, {Low, High, Medium});
    /// versions values: Prod == Beta == Internal
    testFunc(Medium, DistributionChannel::Prod, DistributionChannel::Internal, {Medium, Medium, Medium});
    /// versions values:  Beta == Prod > Internal
    testFunc(High, DistributionChannel::Prod, DistributionChannel::Internal, {High, High, Low});
    /// versions values: Beta == Internal > Prod
    testFunc(Medium, DistributionChannel::Beta, DistributionChannel::Internal, {Low, Medium, Medium});
    /// versions values: Prod == Internal > Beta
    testFunc(Medium, DistributionChannel::Prod, DistributionChannel::Internal, {Medium, Low, Medium});
}

} // namespace KDC
