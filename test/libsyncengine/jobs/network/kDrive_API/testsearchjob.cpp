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

#include "testsearchjob.h"

#include "version.h"
#include "jobs/network/kDrive_API/searchjob.h"
#include "jobs/network/abstracttokennetworkjob.h"
#include "requests/parameterscache.h"
#include "libcommonserver/utility/utility.h"

#include "libcommonserver/keychainmanager/keychainmanager.h"
#include "keychainmanager/apitoken.h"
#include "libparms/db/parmsdb.h"
#include "mocks/libcommonserver/db/mockdb.h"

#include <filesystem>
#include <sstream>

using namespace CppUnit;

namespace KDC {

namespace {

// Builds a minimal valid search API JSON response with a single file result
// whose remote path is pathValue (UTF-8 encoded).
std::string makeSearchResponseJson(const std::string &pathValue) {
    return R"({"cursor":"c1","has_more":false,"data":[{"id":"1","name":"doc.txt","type":"file","path":")" + pathValue +
           R"(","last_modified_at":1700000000,"size":100}]})";
}

} // anonymous namespace

void TestSearchJob::setUp() {
    TestBase::start();
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ Set Up TestSearchJob");

    // Init in-memory keychain (testing mode, no real OS keychain used)
    (void) KeyChainManager::instance(true);

    ApiToken apiToken;
    apiToken.setAccessToken("dummy_access_token");
    const std::string keychainKey("test_search_job_key");
    (void) KeyChainManager::instance()->writeToken(keychainKey, apiToken.reconstructJsonString());

    // Init ParmsDb
    (void) ParmsDb::instance(_localTempDir.path() / MockDb::makeDbMockFileName(), KDRIVE_VERSION_STRING, true, true);

    // Insert the minimal User / Account / Drive so SearchJob construction succeeds
    User user(1, 12345, keychainKey);
    (void) ParmsDb::instance()->insertUser(user);

    Account account(1, 67890, user.dbId(), "test_account");
    (void) ParmsDb::instance()->insertAccount(account);

    Drive drive(static_cast<int>(_driveDbId), 99999, account.dbId(), "test_drive", 0, std::string());
    (void) ParmsDb::instance()->insertDrive(drive);
}

void TestSearchJob::tearDown() {
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ Tear Down TestSearchJob");

    AbstractTokenNetworkJob::clearCache();
    ParmsDb::instance()->close();
    ParmsDb::reset();
    ParametersCache::reset();
    TestBase::stop();
}

void TestSearchJob::testHandleResponsePrivatePath() {
    // Paths returned by the API for "Private" files are prefixed with "/Private/".
    // handleResponse() should strip this prefix so SearchInfo::path() is relative.
    SearchJob job(_driveDbId, "doc");
    job._syncRootPath = _localTempDir.path();

    const std::string json = makeSearchResponseJson("/Private/testdir");
    std::istringstream is(json);
    const ExitInfo exitInfo = job.handleResponse(is);

    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), exitInfo);
    const auto results = job.searchResults();
    CPPUNIT_ASSERT_EQUAL(size_t{1}, results.size());
    CPPUNIT_ASSERT_EQUAL(SyncPath(Str("testdir")), results.front().path());
}

void TestSearchJob::testHandleResponseSharedPath() {
    // Paths returned by the API for "Shared" files are prefixed with "/Shared/".
    // handleResponse() should strip this prefix so SearchInfo::path() is relative.
    SearchJob job(_driveDbId, "doc");
    job._syncRootPath = _localTempDir.path();

    const std::string json = makeSearchResponseJson("/Shared/testdir");
    std::istringstream is(json);
    const ExitInfo exitInfo = job.handleResponse(is);

    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), exitInfo);
    const auto results = job.searchResults();
    CPPUNIT_ASSERT_EQUAL(size_t{1}, results.size());
    CPPUNIT_ASSERT_EQUAL(SyncPath(Str("testdir")), results.front().path());
}

void TestSearchJob::testHandleResponseLeadingSlash() {
    // When a path has no recognized prefix but begins with '/', handleResponse()
    // should strip the root component so the result is a relative path.
    SearchJob job(_driveDbId, "doc");
    job._syncRootPath = _localTempDir.path();

    const std::string json = makeSearchResponseJson("/testdir");
    std::istringstream is(json);
    const ExitInfo exitInfo = job.handleResponse(is);

    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), exitInfo);
    const auto results = job.searchResults();
    CPPUNIT_ASSERT_EQUAL(size_t{1}, results.size());
    CPPUNIT_ASSERT_EQUAL(SyncPath(Str("testdir")), results.front().path());
}

void TestSearchJob::testHandleResponseIsAvailableLocally() {
    // Create an actual directory under the sync root so that
    // SearchInfo::isAvailableLocally() returns true for it.
    const SyncPath existingDir = _localTempDir.path() / "existing_dir";
    std::error_code ec;
    std::filesystem::create_directory(existingDir, ec);
    CPPUNIT_ASSERT_MESSAGE("Failed to create test directory", !ec);

    {
        // A Private path that resolves to an existing local directory
        SearchJob job(_driveDbId, "doc");
        job._syncRootPath = _localTempDir.path();
        const std::string json = makeSearchResponseJson("/Private/existing_dir");
        std::istringstream is(json);
        CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), job.handleResponse(is));
        const auto results = job.searchResults();
        CPPUNIT_ASSERT_EQUAL(size_t{1}, results.size());
        CPPUNIT_ASSERT_EQUAL(true, results.front().isAvailableLocally());
    }

    {
        // A Private path that does NOT exist locally
        SearchJob job(_driveDbId, "doc");
        job._syncRootPath = _localTempDir.path();
        const std::string json = makeSearchResponseJson("/Private/missing_dir");
        std::istringstream is(json);
        CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), job.handleResponse(is));
        const auto results = job.searchResults();
        CPPUNIT_ASSERT_EQUAL(size_t{1}, results.size());
        CPPUNIT_ASSERT_EQUAL(false, results.front().isAvailableLocally());
    }
}

} // namespace KDC
