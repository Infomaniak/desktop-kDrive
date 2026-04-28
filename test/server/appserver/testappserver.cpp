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

#include "testappserver.h"

#include "comm/guijobmanager.h"
#include "utility/types.h"
#include "requests/parameterscache.h"
#include "libcommonserver/keychainmanager/keychainmanager.h"
#include "libcommon/utility/utility.h"
#include "libsyncengine/jobs/syncjobmanager.h"
#include "mocks/libcommonserver/db/mockdb.h"

#include "test_utility/testhelpers.h"

namespace KDC {

void TestAppServer::setUp() {
    TestBase::start();

    // Create parmsDb
    const SyncPath parmsDbPath = _localTempDir.path() / MockDb::makeDbMockFileName();
    (void) ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);
    ParametersCache::instance()->parameters().setExtendedLog(true);

    if (QCoreApplication::instance()) {
        _appPtr = dynamic_cast<MockAppServer *>(QCoreApplication::instance());
        return;
    }

    const testhelpers::TestVariables testVariables;

    // Insert api token into keystore
    ApiToken apiToken;
    apiToken.setAccessToken(testVariables.apiToken);

    const std::string keychainKey("123");
    (void) KeyChainManager::instance(true);
    (void) KeyChainManager::instance()->writeToken(keychainKey, apiToken.reconstructJsonString());

    // Insert user, account, drive & sync
    const int userId(atoi(testVariables.userId.c_str()));
    const User user(1, userId, keychainKey);
    (void) ParmsDb::instance()->insertUser(user);

    const int accountId(atoi(testVariables.accountId.c_str()));
    const Account account(1, accountId, user.dbId(), "account1");
    (void) ParmsDb::instance()->insertAccount(account);

    const int driveId = atoi(testVariables.driveId.c_str());
    const Drive drive(1, driveId, account.dbId(), std::string(), 0, std::string());
    (void) ParmsDb::instance()->insertDrive(drive);

    _localPath = _localTempDir.path().string() + "/local_sync_directory";
    std::filesystem::create_directories(_localPath);

    _remotePath = testVariables.remotePath;
    Sync sync(1, drive.dbId(), _localPath, "", _remotePath);
    (void) ParmsDb::instance()->insertSync(sync);

    // Create AppServer
    SyncPath exePath = KDC::CommonUtility::applicationFilePath();
    try {
        const std::vector<std::string> args = {Path2Str(exePath)};
        std::vector<char *> argv;
        for (size_t i = 0; i < args.size(); ++i) argv.push_back(const_cast<char *>(args[i].c_str()));
        auto argc = static_cast<int>(args.size());
        _appPtr = new MockAppServer(argc, &argv[0]);
        _appPtr->setParmsDbPath(parmsDbPath);
        _appPtr->init();
    } catch (const std::exception &e) {
        std::cerr << "kDrive server initialization error: " << e.what() << std::endl;
        return;
    }

    // /!\ No event handling (no call to _appPtr->exec())
}

void TestAppServer::tearDown() {
    TestBase::stop();
}

void TestAppServer::testInitAndStopSyncPal() {
    const int syncDbId = 1;

    Sync sync;
    bool found = false;

    CPPUNIT_ASSERT(ParmsDb::instance()->selectSync(syncDbId, sync, found) && found);

    // Check sync nesting
    ExitInfo exitInfo = _appPtr->checkIfSyncIsValid(sync);
    CPPUNIT_ASSERT(exitInfo);
    // Start Vfs
    exitInfo = _appPtr->createAndStartVfs(sync);
    CPPUNIT_ASSERT(exitInfo);
    // Start SyncPal
    const std::chrono::seconds startDelay{0};
    exitInfo = _appPtr->initSyncPal(sync, QSet<QString>(), /*start*/ true, startDelay,
                                    /*resumedByUser*/ false, /*firstInit*/ true);
    CPPUNIT_ASSERT(exitInfo);
    CPPUNIT_ASSERT(syncIsActive(syncDbId));
    // Stop SyncPal (pause by user)
    exitInfo = _appPtr->stopSyncPal(syncDbId, SyncPal::PauseCaller::User);
    CPPUNIT_ASSERT(exitInfo);
    CPPUNIT_ASSERT(waitForSyncStatus(syncDbId, SyncStatus::Stopped));
    // Resume SyncPal
    exitInfo = _appPtr->initSyncPal(sync, QSet<QString>(), /*start*/ true, startDelay,
                                    /*resumedByUser*/ true, /*firstInit*/ false);
    CPPUNIT_ASSERT(exitInfo);
    CPPUNIT_ASSERT(syncIsActive(syncDbId));

    // Stop SyncPal (cleanup)
    exitInfo = _appPtr->stopSyncPal(syncDbId, SyncPal::PauseCaller::Sync, SyncPal::DbBehaviorAfterStop::Remove);
    CPPUNIT_ASSERT(exitInfo);
    CPPUNIT_ASSERT(waitForSyncStatus(syncDbId, SyncStatus::Stopped));

    // Stop Vfs
    exitInfo = _appPtr->stopVfs(syncDbId, /*unregister*/ false);
    CPPUNIT_ASSERT(exitInfo);
}

void TestAppServer::testStartAndStopSync() {
    const int userDbId = 1;
    const int syncDbId = 1;

    User user;
    bool found = false;
    CPPUNIT_ASSERT(ParmsDb::instance()->selectUser(userDbId, user, found) && found);

    // Start syncs (ie. Vfs & SyncPal) for a user
    ExitInfo exitInfo = _appPtr->startSyncs(user);
    CPPUNIT_ASSERT(exitInfo);
    CPPUNIT_ASSERT(_appPtr->syncPalMap[syncDbId]->isRunning());
    CPPUNIT_ASSERT(syncIsActive(syncDbId));

    // Stop sync & clear maps
    _appPtr->stopSyncTask(syncDbId);
    CPPUNIT_ASSERT(_appPtr->syncPalMap.empty());
    CPPUNIT_ASSERT(_appPtr->vfsMap.empty());

    // Start syncs for all users
    exitInfo = _appPtr->startSyncs();
    CPPUNIT_ASSERT(exitInfo);
    CPPUNIT_ASSERT(_appPtr->syncPalMap[syncDbId]->isRunning());
    CPPUNIT_ASSERT(syncIsActive(syncDbId));

    // Stop syncs & clear maps for all users
    _appPtr->stopAllSyncsTask({syncDbId});
    CPPUNIT_ASSERT(_appPtr->syncPalMap.empty());
    CPPUNIT_ASSERT(_appPtr->vfsMap.empty());

    // Update sync local folder with a dummy value
    Sync sync;
    found = false;
    CPPUNIT_ASSERT(ParmsDb::instance()->selectSync(syncDbId, sync, found) && found);

#ifdef KD_WINDOWS
    sync.setLocalPath("Y:\\dummy");
#elif KD_MACOS
    sync.setLocalPath("/Volumes/dummy");
#else
    sync.setLocalPath("/mnt/dummy");
#endif

    CPPUNIT_ASSERT(ParmsDb::instance()->updateSync(sync, found) && found);

    // Start syncs
    exitInfo = _appPtr->startSyncs();
    CPPUNIT_ASSERT_EQUAL(ExitCode::SystemError, exitInfo.code());
    CPPUNIT_ASSERT(exitInfo.cause() == ExitCause::SyncDirDiskMissing);

    // Update sync local folder with the good value
    CPPUNIT_ASSERT(ParmsDb::instance()->updateSync(sync, found) && found);
    sync.setLocalPath(_localPath);
}

void TestAppServer::testCleanup() {
    _appPtr->cleanup();
    delete _appPtr;
    CPPUNIT_ASSERT(true);
}

/**
 * Test:
 * - "driveA" has been moved from "accountA" to "accountB"
 */

ExitInfo mockLoadUserInfo([[maybe_unused]] User &user, bool &updated) {
    updated = false;
    return ExitCode::Ok;
}

const std::string accountNameA = "accountA";
const std::string accountNameB = "accountB";
const uint64_t accountIdA = 111;
const uint64_t accountIdB = 222;
ExitInfo mockLoadAccountInfo(Account &account, bool &updated) {
    const auto accountName = account.dbId() == 11 ? accountNameA : accountNameB;
    updated = account.name() == accountName;
    account.setName(accountName);
    return ExitCode::Ok;
}

ExitInfo mockLoadDriveInfo(Drive &drive, const AccountId previousAccountId, AccountId &newAccountId, bool &updated,
                           bool &quotaUpdated) {
    if (drive.dbId() == 11 && previousAccountId == accountIdA) {
        newAccountId = accountIdB;
        updated = true;
    }
    quotaUpdated = false;
    return ExitCode::Ok;
}

void TestAppServer::testUpdateUserInfo() {
    _appPtr->setLoadUserInfoFunction(mockLoadUserInfo);
    _appPtr->setLoadAccountInfoFunction(mockLoadAccountInfo);
    _appPtr->setLoadDriveInfoFunction(mockLoadDriveInfo);

    // Insert user, account, drive & sync
    User userA(11, 111, "dummy", "userA", "userA@mail.com");
    (void) ParmsDb::instance()->insertUser(userA);

    Account accountA(11, accountIdA, userA.dbId(), accountNameA);
    (void) ParmsDb::instance()->insertAccount(accountA);

    Drive driveA(11, 111, accountA.dbId(), "driveA", 123, "#FF0000");
    (void) ParmsDb::instance()->insertDrive(driveA);

    _appPtr->updateUserInfo(userA);

    std::vector<Account> accounts;
    (void) ParmsDb::instance()->selectAllAccounts(accounts);
    bool foundAccountA = false;
    bool foundAccountB = false;
    uint64_t accountDbIdB = 0;
    for (const auto &account: accounts) {
        if (account.accountId() == accountIdA) foundAccountA = true;
        if (account.accountId() == accountIdB) {
            foundAccountB = true;
            accountDbIdB = static_cast<uint64_t>(account.dbId());
        }
    }
    CPPUNIT_ASSERT(!foundAccountA);
    CPPUNIT_ASSERT(foundAccountB);

    Drive drive;
    bool found = false;
    (void) ParmsDb::instance()->selectDrive(driveA.dbId(), drive, found);
    CPPUNIT_ASSERT(found);
    CPPUNIT_ASSERT(drive.accountDbId() != 11);
    CPPUNIT_ASSERT_EQUAL(accountDbIdB, static_cast<uint64_t>(drive.accountDbId()));
}

bool TestAppServer::waitForSyncStatus(int syncDbId, SyncStatus targetStatus) const {
    int count = 0;
    while (count++ < 100) {
        if (auto status = _appPtr->syncPalMap[syncDbId]->status(); status == targetStatus) return true;
        Utility::msleep(100);
    }
    return false;
}

bool TestAppServer::syncIsActive(int syncDbId) const {
    SyncStatus status = _appPtr->syncPalMap[syncDbId]->status();
    return status == SyncStatus::Starting || status == SyncStatus::Running || status == SyncStatus::Idle;
}

std::filesystem::path MockAppServer::makeDbName() {
    return _parmsDbPath;
}

std::shared_ptr<ParmsDb> MockAppServer::initParmsDB(const std::filesystem::path &dbPath, const std::string &version) {
    return ParmsDb::instance(dbPath, version, false, true);
}

void MockAppServer::cleanup() {
    AppServer::cleanup();

    // Reset static variables
    AppServer::reset();
    SyncJobManagerSingleton::clear();
    ParmsDb::reset();
    ParametersCache::reset();
}

} // namespace KDC
