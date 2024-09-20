
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

#include "testupdatemanager.h"

#include "db/parmsdb.h"
#include "jobs/jobmanager.h"
#include "requests/parameterscache.h"
#include "jobs/network/getappversionjob.h"
#include "keychainmanager/keychainmanager.h"
#include "server/updater_v2/updatemanager.h"
#include "test_utility/testhelpers.h"

#include <Poco/JSON/Parser.h>
#include <regex>

namespace KDC {

static const std::string bigVersionJsonUpdateStr =
        R"({"result":"success","data":{"application_id":27,"has_prod_next":false,"version":{"tag":"99.99.99","tag_updated_at":"2124-06-04 15:06:37","version_changelog":"test","type":"production","build_version":"21240604","build_min_os_version":"21240604","download_link":"test","data":["[]"]},"application":{"id":27,"name":"com.infomaniak.drive","platform":"mac-os","store":"kStore","api_id":"com.infomaniak.drive","min_version":"99.99.99","next_version_rate":0,"published_versions":[{"tag":"99.99.99","tag_updated_at":"2124-06-04 15:06:37","version_changelog":"test","type":"production","build_version":"21240604","build_min_os_version":"21240604","download_link":"test","data":["[]"]},{"tag":"99.99.99","tag_updated_at":"2124-06-04 15:06:12","version_changelog":"test","type":"beta","build_version":"21240604","build_min_os_version":"21240604","download_link":"test","data":["[]"]},{"tag":"99.99.99","tag_updated_at":"2124-06-04 15:05:44","version_changelog":"test","type":"internal","build_version":"21240604","build_min_os_version":"21240604","download_link":"test","data":["[]"]},{"tag":"99.99.99","tag_updated_at":"2124-06-04 15:03:29","version_changelog":"test","type":"production-next","build_version":"21240604","build_min_os_version":"21240604","download_link":"test","data":["[]"]}]}}})";
static const std::string smallVersionJsonUpdateStr =
        R"({"result":"success","data":{"application_id":27,"has_prod_next":false,"version":{"tag":"1.1.1","tag_updated_at":"2020-06-04 15:06:37","version_changelog":"test","type":"production","build_version":"20200604","build_min_os_version":"20200604","download_link":"test","data":["[]"]},"application":{"id":27,"name":"com.infomaniak.drive","platform":"mac-os","store":"kStore","api_id":"com.infomaniak.drive","min_version":"1.1.1","next_version_rate":0,"published_versions":[{"tag":"1.1.1","tag_updated_at":"2020-06-04 15:06:37","version_changelog":"test","type":"production","build_version":"20200604","build_min_os_version":"20200604","download_link":"test","data":["[]"]},{"tag":"1.1.1","tag_updated_at":"2020-06-04 15:06:12","version_changelog":"test","type":"beta","build_version":"20200604","build_min_os_version":"20200604","download_link":"test","data":["[]"]},{"tag":"1.1.1","tag_updated_at":"2020-06-04 15:05:44","version_changelog":"test","type":"internal","build_version":"20200604","build_min_os_version":"20200604","download_link":"test","data":["[]"]},{"tag":"1.1.1","tag_updated_at":"2020-06-04 15:03:29","version_changelog":"test","type":"production-next","build_version":"20200604","build_min_os_version":"20200604","download_link":"test","data":["[]"]}]}}})";

void TestUpdateManager::setUp() {
    // Create parmsDb
    bool alreadyExists = false;
    const std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExists, true);
    ParmsDb::instance(parmsDbPath, "3.4.0", true, true);
    ParametersCache::instance()->parameters().setExtendedLog(true);
}

class TestGetAppVersionJob final : public GetAppVersionJob {
    public:
        TestGetAppVersionJob(const Platform platform, const std::string &appID, const bool updateShouldBeAvailable) :
            GetAppVersionJob(platform, appID), _updateShoudBeAvailable(updateShouldBeAvailable) {}

        void runJob() noexcept override {
            const std::istringstream iss(_updateShoudBeAvailable ? bigVersionJsonUpdateStr : smallVersionJsonUpdateStr);
            std::istream is(iss.rdbuf());
            GetAppVersionJob::handleResponse(is);
        }

    private:
        bool _updateShoudBeAvailable{false};
};


class UpdateManagerTest final : public UpdateManager {
    public:
        void setUpdateShoudBeAvailable(const bool val) { _updateShoudBeAvailable = val; }

    private:
        ExitCode getAppVersionJob(std::shared_ptr<AbstractNetworkJob> &job) override {
            static const std::string appUid = "1234567890";
            job = std::make_shared<TestGetAppVersionJob>(CommonUtility::platform(), appUid, _updateShoudBeAvailable);
            return ExitCode::Ok;
        }

        bool _updateShoudBeAvailable{false};
};

void TestUpdateManager::testCheckUpdateAvailable() {
    // Version is higher than current version
    {
        UpdateManagerTest testObj;
        UniqueId jobId = 0;
        testObj.setUpdateShoudBeAvailable(true);
        testObj.checkUpdateAvailable(&jobId);
        while (!JobManager::instance()->isJobFinished(jobId)) Utility::msleep(10);
        CPPUNIT_ASSERT_EQUAL(UpdateStateV2::Available, testObj.state());
    }

    // Version is lower than current version
    {
        UpdateManagerTest testObj;
        UniqueId jobId = 0;
        testObj.setUpdateShoudBeAvailable(false);
        testObj.checkUpdateAvailable(&jobId);
        while (!JobManager::instance()->isJobFinished(jobId)) Utility::msleep(10);
        CPPUNIT_ASSERT_EQUAL(UpdateStateV2::UpToDate, testObj.state());
    }
}

void TestUpdateManager::testCurrentVersion() {
    const std::string test = CommonUtility::currentVersion();
#ifdef NDEBUG
    CPPUNIT_ASSERT(std::regex_match(test, std::regex(R"(\d{1,2}[.]\d{1,2}[.]\d{1,2}[.]\d{8}$)")));
#else
    CPPUNIT_ASSERT(std::regex_match(test, std::regex(R"(\d{1,2}[.]\d{1,2}[.]\d{1,2}[.]0$)")));
#endif
}

} // namespace KDC
