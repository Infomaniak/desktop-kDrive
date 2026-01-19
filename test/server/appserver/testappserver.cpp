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

#include "testappserver.h"

#include "utility/types.h"
#include "requests/parameterscache.h"
#include "libcommon/keychainmanager/keychainmanager.h"
#include "libcommon/utility/utility.h"
#include "libsyncengine/jobs/syncjobmanager.h"
#include "mocks/libcommonserver/db/mockdb.h"

#include "test_utility/testhelpers.h"

namespace KDC {

void TestAppServer::setUp() {
    // TODO: Remove debug logs once blocking issue is resolved
    std::cout << "[DEBUG] TestAppServer::setUp - START" << std::endl;

    TestBase::start();
    std::cout << "[DEBUG] TestAppServer::setUp - TestBase::start() done" << std::endl;

    // Create parmsDb
    const SyncPath parmsDbPath = _localTempDir.path() / MockDb::makeDbMockFileName();
    std::cout << "[DEBUG] TestAppServer::setUp - Creating ParmsDb" << std::endl;
    (void) ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);
    ParametersCache::instance()->parameters().setExtendedLog(true);
    std::cout << "[DEBUG] TestAppServer::setUp - ParmsDb created" << std::endl;

    if (QCoreApplication::instance()) {
        std::cout << "[DEBUG] TestAppServer::setUp - QCoreApplication already exists" << std::endl;
        _appPtr = dynamic_cast<MockAppServer *>(QCoreApplication::instance());
        return;
    }

    const testhelpers::TestVariables testVariables;

    // Insert api token into keystore
    std::cout << "[DEBUG] TestAppServer::setUp - Setting up KeyChainManager" << std::endl;
    ApiToken apiToken;
    apiToken.setAccessToken(testVariables.apiToken);

    const std::string keychainKey("123");
    (void) KeyChainManager::instance(true);
    std::cout << "[DEBUG] TestAppServer::setUp - KeyChainManager instance created" << std::endl;
    (void) KeyChainManager::instance()->writeToken(keychainKey, apiToken.reconstructJsonString());
    std::cout << "[DEBUG] TestAppServer::setUp - Token written to keychain" << std::endl;

    // Insert user, account, drive & sync
    std::cout << "[DEBUG] TestAppServer::setUp - Inserting user" << std::endl;
    const int userId(atoi(testVariables.userId.c_str()));
    const User user(1, userId, keychainKey);
    (void) ParmsDb::instance()->insertUser(user);

    std::cout << "[DEBUG] TestAppServer::setUp - Inserting account" << std::endl;
    const int accountId(atoi(testVariables.accountId.c_str()));
    const Account account(1, accountId, user.dbId());
    (void) ParmsDb::instance()->insertAccount(account);

    std::cout << "[DEBUG] TestAppServer::setUp - Inserting drive" << std::endl;
    const int driveId = atoi(testVariables.driveId.c_str());
    const Drive drive(1, driveId, account.dbId(), std::string(), 0, std::string());
    (void) ParmsDb::instance()->insertDrive(drive);

    std::cout << "[DEBUG] TestAppServer::setUp - Creating local directory" << std::endl;
    _localPath = _localTempDir.path().string() + "/local_sync_directory";
    std::filesystem::create_directories(_localPath);

    std::cout << "[DEBUG] TestAppServer::setUp - Inserting sync" << std::endl;
    _remotePath = testVariables.remotePath;
    Sync sync(1, drive.dbId(), _localPath, "", _remotePath);
    (void) ParmsDb::instance()->insertSync(sync);

    // Create AppServer
    std::cout << "[DEBUG] TestAppServer::setUp - Creating AppServer" << std::endl;
    SyncPath exePath = KDC::CommonUtility::applicationFilePath();
    try {
        const std::vector<std::string> args = {Path2Str(exePath)};
        std::vector<char *> argv;
        for (size_t i = 0; i < args.size(); ++i) argv.push_back(const_cast<char *>(args[i].c_str()));
        auto argc = static_cast<int>(args.size());
        _appPtr = new MockAppServer(argc, &argv[0]);
        std::cout << "[DEBUG] TestAppServer::setUp - MockAppServer created" << std::endl;
        _appPtr->setParmsDbPath(parmsDbPath);
        std::cout << "[DEBUG] TestAppServer::setUp - Starting AppServer init()" << std::endl;
        _appPtr->init();
        std::cout << "[DEBUG] TestAppServer::setUp - AppServer init() COMPLETED" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "kDrive server initialization error: " << e.what() << std::endl;
        return;
    }

    std::cout << "[DEBUG] TestAppServer::setUp - END (success)" << std::endl;
    // /!\ No event handling (no call to _appPtr->exec())
}

void TestAppServer::tearDown() {
    TestBase::stop();
}

void TestAppServer::testInitAndStopSyncPal() {
    // TODO: Remove debug logs once blocking issue is resolved
    std::cout << "[DEBUG] ========================================" << std::endl;
    std::cout << "[DEBUG] testInitAndStopSyncPal - START" << std::endl;
    std::cout << "[DEBUG] ========================================" << std::endl;

    const int syncDbId = 1;

    Sync sync;
    bool found = false;

    std::cout << "[DEBUG] testInitAndStopSyncPal - Selecting sync from DB" << std::endl;
    CPPUNIT_ASSERT(ParmsDb::instance()->selectSync(syncDbId, sync, found) && found);
    std::cout << "[DEBUG] testInitAndStopSyncPal - Sync selected" << std::endl;

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
    exitInfo = _appPtr->stopSyncPal(syncDbId, /*pausedByUser*/ true);
    CPPUNIT_ASSERT(exitInfo);
    CPPUNIT_ASSERT(waitForSyncStatus(syncDbId, SyncStatus::Stopped));
    // Resume SyncPal
    exitInfo = _appPtr->initSyncPal(sync, QSet<QString>(), /*start*/ true, startDelay,
                                    /*resumedByUser*/ true, /*firstInit*/ false);
    CPPUNIT_ASSERT(exitInfo);
    CPPUNIT_ASSERT(syncIsActive(syncDbId));

    // Stop SyncPal (cleanup)
    exitInfo = _appPtr->stopSyncPal(syncDbId, /*pausedByUser*/ false, /*quit*/ true, /*clear*/ true);
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
    CPPUNIT_ASSERT(AppServer::syncPalMap[syncDbId]->isRunning());
    CPPUNIT_ASSERT(syncIsActive(syncDbId));

    // Stop sync & clear maps
    _appPtr->stopSyncTask(syncDbId);
    CPPUNIT_ASSERT(AppServer::syncPalMap.empty());
    CPPUNIT_ASSERT(AppServer::vfsMap.empty());

    // Start syncs for all users
    exitInfo = _appPtr->startSyncs();
    CPPUNIT_ASSERT(exitInfo);
    CPPUNIT_ASSERT(AppServer::syncPalMap[syncDbId]->isRunning());
    CPPUNIT_ASSERT(syncIsActive(syncDbId));

    // Stop syncs & clear maps for all users
    _appPtr->stopAllSyncsTask({syncDbId});
    CPPUNIT_ASSERT(AppServer::syncPalMap.empty());
    CPPUNIT_ASSERT(AppServer::vfsMap.empty());

    // Update sync local folder with a dummy value
    Sync sync;
    found = false;
    CPPUNIT_ASSERT(ParmsDb::instance()->selectSync(syncDbId, sync, found) && found);

    sync.setLocalPath("/dummy");
    CPPUNIT_ASSERT(ParmsDb::instance()->updateSync(sync, found) && found);

    // Start syncs
    exitInfo = _appPtr->startSyncs();
    CPPUNIT_ASSERT_EQUAL(ExitCode::SystemError, exitInfo.code());
    CPPUNIT_ASSERT_EQUAL(ExitCause::SyncDirAccessError, exitInfo.cause());

    // Update sync local folder with the good value
    CPPUNIT_ASSERT(ParmsDb::instance()->updateSync(sync, found) && found);
    sync.setLocalPath(_localPath);
}

void TestAppServer::testCleanup() {
    _appPtr->cleanup();
    delete _appPtr;
    CPPUNIT_ASSERT(true);
}

bool TestAppServer::waitForSyncStatus(int syncDbId, SyncStatus targetStatus) const {
    int count = 0;
    while (count++ < 100) {
        if (auto status = AppServer::syncPalMap[syncDbId]->status(); status == targetStatus) return true;
        Utility::msleep(100);
    }
    return false;
}

bool TestAppServer::syncIsActive(int syncDbId) const {
    SyncStatus status = AppServer::syncPalMap[syncDbId]->status();
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
