
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

#include "testabstractupdater.h"

#include "db/parmsdb.h"
#include "requests/parameterscache.h"
#include "jobs/network/getappversionjob.h"
#include "keychainmanager/keychainmanager.h"

#include <regex>

#include "server/updater_v2/abstractupdater.h"
#include "test_utility/testhelpers.h"

#include <Poco/JSON/Parser.h>

namespace KDC {

static const std::string bigVersionJsonUpdateStr =
        R"({"result":"success","data":{"application_id":27,"has_prod_next":false,"version":{"tag":"99.99.99","tag_updated_at":"2124-06-04 15:06:37","version_changelog":"test","type":"production","build_version":"21240604","build_min_os_version":"21240604","download_link":"test","data":["[]"]},"application":{"id":27,"name":"com.infomaniak.drive","platform":"mac-os","store":"kStore","api_id":"com.infomaniak.drive","min_version":"99.99.99","next_version_rate":0,"published_versions":[{"tag":"99.99.99","tag_updated_at":"2124-06-04 15:06:37","version_changelog":"test","type":"production","build_version":"21240604","build_min_os_version":"21240604","download_link":"test","data":["[]"]},{"tag":"99.99.99","tag_updated_at":"2124-06-04 15:06:12","version_changelog":"test","type":"beta","build_version":"21240604","build_min_os_version":"21240604","download_link":"test","data":["[]"]},{"tag":"99.99.99","tag_updated_at":"2124-06-04 15:05:44","version_changelog":"test","type":"internal","build_version":"21240604","build_min_os_version":"21240604","download_link":"test","data":["[]"]},{"tag":"99.99.99","tag_updated_at":"2124-06-04 15:03:29","version_changelog":"test","type":"production-next","build_version":"21240604","build_min_os_version":"21240604","download_link":"test","data":["[]"]}]}}})";
static const std::string smallVersionJsonUpdateStr =
        R"({"result":"success","data":{"application_id":27,"has_prod_next":false,"version":{"tag":"1.1.1","tag_updated_at":"2020-06-04 15:06:37","version_changelog":"test","type":"production","build_version":"20200604","build_min_os_version":"20200604","download_link":"test","data":["[]"]},"application":{"id":27,"name":"com.infomaniak.drive","platform":"mac-os","store":"kStore","api_id":"com.infomaniak.drive","min_version":"1.1.1","next_version_rate":0,"published_versions":[{"tag":"1.1.1","tag_updated_at":"2020-06-04 15:06:37","version_changelog":"test","type":"production","build_version":"20200604","build_min_os_version":"20200604","download_link":"test","data":["[]"]},{"tag":"1.1.1","tag_updated_at":"2020-06-04 15:06:12","version_changelog":"test","type":"beta","build_version":"20200604","build_min_os_version":"20200604","download_link":"test","data":["[]"]},{"tag":"1.1.1","tag_updated_at":"2020-06-04 15:05:44","version_changelog":"test","type":"internal","build_version":"20200604","build_min_os_version":"20200604","download_link":"test","data":["[]"]},{"tag":"1.1.1","tag_updated_at":"2020-06-04 15:03:29","version_changelog":"test","type":"production-next","build_version":"20200604","build_min_os_version":"20200604","download_link":"test","data":["[]"]}]}}})";

void TestAbstractUpdater::setUp() {
    LOG_DEBUG(Log::instance()->getLogger(), "$$$$$ Set Up");

    // Create parmsDb
    bool alreadyExists = false;
    std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExists, true);
    ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);
    ParametersCache::instance()->parameters().setExtendedLog(true);
}

void TestAbstractUpdater::tearDown() {}

class TestGetAppVersionJob final : public GetAppVersionJob {
    public:
        TestGetAppVersionJob(const Platform platform, const std::string &appID, const bool updateAvailable) :
            GetAppVersionJob(platform, appID), _updateAvailable(updateAvailable) {}

        void runJob() noexcept override {
            std::istringstream iss(_updateAvailable ? bigVersionJsonUpdateStr : smallVersionJsonUpdateStr);
            std::istream is(iss.rdbuf());
            GetAppVersionJob::handleResponse(is);
        }

    private:
        bool _updateAvailable{false};
};

void TestAbstractUpdater::testCheckUpdateAvailable() {
    const auto appUid = "1234567890";

    // Version is higher than current version
    {
        auto *testJob = new TestGetAppVersionJob(CommonUtility::platform(), appUid, true);
        AbstractUpdater::instance()->setGetAppVersionJob(testJob);
        bool updateAvailable = false;
        AbstractUpdater::instance()->checkUpdateAvailable(updateAvailable);
        CPPUNIT_ASSERT(updateAvailable);
        delete testJob;
    }

    // Version is lower than current version
    {
        auto *testJob = new TestGetAppVersionJob(CommonUtility::platform(), appUid, false);
        AbstractUpdater::instance()->setGetAppVersionJob(testJob);
        bool updateAvailable = false;
        AbstractUpdater::instance()->checkUpdateAvailable(updateAvailable);
        CPPUNIT_ASSERT(!updateAvailable);
        delete testJob;
    }
}

void TestAbstractUpdater::testCurrentVersion() {
    const std::string test = CommonUtility::currentVersion();
#ifdef NDEBUG
    CPPUNIT_ASSERT(std::regex_match(test, std::regex(R"(\d{1,2}[.]\d{1,2}[.]\d{1,2}[.]\d{8}$)")));
#else
    CPPUNIT_ASSERT(std::regex_match(test, std::regex(R"(\d{1,2}[.]\d{1,2}[.]\d{1,2}[.]0$)")));
#endif
}

} // namespace KDC
