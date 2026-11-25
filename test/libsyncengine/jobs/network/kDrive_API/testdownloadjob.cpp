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

#include "testdownloadjob.h"

#include "jobs/network/kDrive_API/downloadjob.h"
#include "jobs/network/abstracttokennetworkjob.h"
#include "libcommonserver/io/cachedirectory.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/keychainmanager/keychainmanager.h"
#include "libcommonserver/utility/utility.h"
#include "keychainmanager/apitoken.h"
#include "libparms/db/parmsdb.h"
#include "mocks/mockkeychainstorage.h"
#include "mocks/libcommonserver/db/mockdb.h"
#include "requests/parameterscache.h"
#include "test_utility/testhelpers.h"
#include "version.h"

#include <filesystem>
#include <fstream>

using namespace CppUnit;

namespace KDC {

void TestDownloadJob::setUp() {
    TestBase::start();

    // Init in-memory keychain (testing mode, no real OS keychain used)
    (void) KeyChainManager::instance(std::make_shared<MockKeyChainStorage>());

    ApiToken apiToken;
    apiToken.setAccessToken("dummy_access_token");
    const std::string keychainKey("test_download_job_key");
    (void) KeyChainManager::instance()->writeData(keychainKey, apiToken.reconstructJsonString());

    // Init ParmsDb
    (void) ParmsDb::instance(_localTempDir.path() / MockDb::makeDbMockFileName(), KDRIVE_VERSION_STRING, true, true);

    // Insert the minimal User / Account / Drive so DownloadJob construction succeeds
    User user(1, 12345, keychainKey);
    (void) ParmsDb::instance()->insertUser(user);

    Account account(1, 67890, user.dbId(), "test_account");
    (void) ParmsDb::instance()->insertAccount(account);

    Drive drive(1, 99999, account.dbId(), "test_drive", 0, std::string());
    (void) ParmsDb::instance()->insertDrive(drive);
}

void TestDownloadJob::tearDown() {
    AbstractTokenNetworkJob::clearCache();
    ParmsDb::instance()->close();
    ParmsDb::reset();
    ParametersCache::reset();
    TestBase::stop();
}

void TestDownloadJob::testRunJobDestinationThroughLink() {
    const SyncPath syncDirPath = _localTempDir.path();

    // A directory link located inside the sync directory points to a directory located outside of it.
    const SyncPath outsideDirPath = _localTempDir.path().parent_path() / "testDownloadJob_outside";
    std::error_code ec;
    (void) std::filesystem::remove_all(outsideDirPath, ec); // Cleanup a possible leftover from a previously interrupted run
    CPPUNIT_ASSERT(std::filesystem::create_directory(outsideDirPath, ec) && !ec);
    const SyncPath victimFilePath = outsideDirPath / "victim.txt";
    { std::ofstream ofs(victimFilePath); }

    IoError ioError = IoError::Unknown;
    const SyncPath linkPath = syncDirPath / "dir_link";
    CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::createSymlink(outsideDirPath, linkPath, true, ioError));

    // An EDIT whose destination is reached through the link.
    const DownloadJob::FileDownloadInfo fileDownloadInfo{1, "deadbeef", linkPath / "victim.txt", 100, 0, 0, false};
    const auto cacheDirectory = std::make_shared<CacheDirectory>(syncDirPath);
    const auto job = std::make_shared<DownloadJob>(/*vfs=*/nullptr, cacheDirectory, fileDownloadInfo,
                                                   DownloadJob::DateTimePolicy::ApplyDateTime);

    // The job must be rejected before any hashing, dates or virtual file metadata update, and content replacement.
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::SystemError, ExitCause::PathContainsSymlink), job->runSynchronously());

    // The item located outside of the sync directory is left unmodified.
    CPPUNIT_ASSERT(std::filesystem::exists(victimFilePath));

    CPPUNIT_ASSERT(std::filesystem::remove_all(outsideDirPath, ec) > 0 && !ec);
}

} // namespace KDC
