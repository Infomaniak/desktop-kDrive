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

#include "testserverrequests.h"

#include "requests/serverrequests.h"
#include "requests/parameterscache.h"
#include "utility/types.h"

#include "libcommonserver/keychainmanager/keychainmanager.h"
#include "libsyncengine/jobs/network/abstracttokennetworkjob.h"

#include "libparms/db/parmsdb.h"

#include "mocks/libcommonserver/db/mockdb.h"

#include "test_utility/remotetemporarydirectory.h"
#include "test_utility/testhelpers.h"

namespace KDC {

void TestServerRequests::setUp() {
    TestBase::start();
    const testhelpers::TestVariables testVariables;

    // Insert api token into keystore
    ApiToken apiToken;
    apiToken.setAccessToken(testVariables.apiToken);

    const std::string keychainKey("123");
    (void) KeyChainManager::instance(true);
    (void) KeyChainManager::instance()->writeToken(keychainKey, apiToken.reconstructJsonString());

    // Create parmsDb
    (void) ParmsDb::instance(_localTempDir.path() / MockDb::makeDbMockFileName(), KDRIVE_VERSION_STRING, true, true);
    ParametersCache::instance()->parameters().setExtendedLog(true);

    // Insert user, account & drive
    const int userId(atoi(testVariables.userId.c_str()));
    const User user(1, userId, keychainKey);
    (void) ParmsDb::instance()->insertUser(user);

    const int accountId(atoi(testVariables.accountId.c_str()));
    const Account account(1, accountId, user.dbId(), "account1");
    (void) ParmsDb::instance()->insertAccount(account);

    _driveDbId = 1;
    const int driveId = atoi(testVariables.driveId.c_str());
    const Drive drive(_driveDbId, driveId, account.dbId(), std::string(), 0, std::string());
    (void) ParmsDb::instance()->insertDrive(drive);
}

void TestServerRequests::tearDown() {
    ParmsDb::instance()->close();
    ParmsDb::reset();
    ParametersCache::reset();
    TestBase::stop();
}

void TestServerRequests::testFixProxyConfig() {
    ProxyConfig proxyConfig = ParametersCache::instance()->parameters().proxyConfig();
    proxyConfig.setType(ProxyType::Undefined);
    ParametersCache::instance()->parameters().setProxyConfig(proxyConfig);
    ParametersCache::instance()->save();

    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, ServerRequests::fixProxyConfig());

    CPPUNIT_ASSERT_EQUAL(ProxyType::None, ParametersCache::instance()->parameters().proxyConfig().type());
}

void TestServerRequests::testGetPublicLink() {
    const RemoteTemporaryDirectory remoteTmpDir(_driveDbId, "1", "testGetPublicLink");

    // 1st call : sent POST request to generate the public share link
    std::string url;
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, ServerRequests::getPublicLinkUrl(_driveDbId, remoteTmpDir.id(), url));
    CPPUNIT_ASSERT(!url.empty());
    // 2nd call : POST request will fail and a GET request should be sent to retrieve existing share link
    url.clear();
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, ServerRequests::getPublicLinkUrl(_driveDbId, remoteTmpDir.id(), url));
    CPPUNIT_ASSERT(!url.empty());
}

void TestServerRequests::testFindGoodPathForNewSync() {
    SyncPath localTempDirPath;
    {
        LocalTemporaryDirectory localTempDir("testFindGoodPathForNewSync");
        localTempDirPath = localTempDir.path();
        const SyncPath defaultPath = localTempDirPath / APPLICATION_NAME;

        // Check with a valid path
        SyncPath returnedPath;
        std::string error;
        CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok, ExitCause::Unknown),
                             ServerRequests::findGoodPathForNewSync(defaultPath, returnedPath, error));
        CPPUNIT_ASSERT_EQUAL(defaultPath, returnedPath);
        CPPUNIT_ASSERT(error.empty());

        // Check with an existing path
        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(IoHelper::createDirectory(defaultPath, false, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

        CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok, ExitCause::Unknown),
                             ServerRequests::findGoodPathForNewSync(defaultPath, returnedPath, error));
        CPPUNIT_ASSERT(!returnedPath.empty());
        CPPUNIT_ASSERT(returnedPath.filename().string().find('2') != std::string::npos);
        CPPUNIT_ASSERT(error.empty());

        // check an already synced & existing path
        const Sync sync(1, _driveDbId, defaultPath, NodeId(), SyncPath(), NodeId());
        (void) ParmsDb::instance()->insertSync(sync);

        CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok, ExitCause::Unknown),
                             ServerRequests::findGoodPathForNewSync(defaultPath, returnedPath, error));
        CPPUNIT_ASSERT(!returnedPath.empty());
        CPPUNIT_ASSERT(returnedPath.filename().string().find('2') != std::string::npos);
        CPPUNIT_ASSERT(error.empty());

        // Check an already synced but non-existing path
        CPPUNIT_ASSERT(IoHelper::deleteItem(defaultPath, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

        CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok, ExitCause::Unknown),
                             ServerRequests::findGoodPathForNewSync(defaultPath, returnedPath, error));
        CPPUNIT_ASSERT(!returnedPath.empty());
        CPPUNIT_ASSERT(returnedPath.filename().string().find('2') != std::string::npos);
        CPPUNIT_ASSERT(error.empty());

        // check with an already synced parent path
        const SyncPath childPath = defaultPath / "childFolder";
        CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::InvalidSync, ExitCause::SyncDirNestingError),
                             ServerRequests::findGoodPathForNewSync(childPath, returnedPath, error));
        CPPUNIT_ASSERT(returnedPath.empty());
        CPPUNIT_ASSERT(!error.empty());

        // Check with an existing path containing an already synced child
        CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok, ExitCause::Unknown),
                             ServerRequests::findGoodPathForNewSync(localTempDirPath, returnedPath, error));
        CPPUNIT_ASSERT(!returnedPath.empty());
        CPPUNIT_ASSERT(returnedPath.filename().string().find('2') != std::string::npos);
        CPPUNIT_ASSERT(error.empty());
    }

    // Check with a non-existing path containing an already synced child
    SyncPath returnedPath;
    std::string error;
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::InvalidSync, ExitCause::SyncDirNestingError),
                         ServerRequests::findGoodPathForNewSync(localTempDirPath, returnedPath, error));
    CPPUNIT_ASSERT(returnedPath.empty());
    CPPUNIT_ASSERT(!error.empty());
}

void TestServerRequests::testDeleteUser() {
    AbstractTokenNetworkJob::_userToApiKeyMap[1] = {nullptr, 0};
    AbstractTokenNetworkJob::_driveToApiKeyMap[1] = {0, 0};

    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), ServerRequests::deleteUser(1));

    // Check that user/account/drive have been removed from db
    User user;
    bool found = false;
    CPPUNIT_ASSERT(ParmsDb::instance()->selectUser(1, user, found));
    CPPUNIT_ASSERT(!found);

    Account account;
    CPPUNIT_ASSERT(ParmsDb::instance()->selectAccount(1, account, found));
    CPPUNIT_ASSERT(!found);

    Drive drive;
    CPPUNIT_ASSERT(ParmsDb::instance()->selectDrive(1, drive, found));
    CPPUNIT_ASSERT(!found);

    // Check that user and drive have been removed from cache
    CPPUNIT_ASSERT(!AbstractTokenNetworkJob::_userToApiKeyMap.contains(1));
    CPPUNIT_ASSERT(!AbstractTokenNetworkJob::_driveToApiKeyMap.contains(1));
}

void TestServerRequests::testDeleteAccount() {
    AbstractTokenNetworkJob::_userToApiKeyMap[1] = {nullptr, 0};
    AbstractTokenNetworkJob::_driveToApiKeyMap[1] = {0, 0};

    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), ServerRequests::deleteAccount(1));

    // Check that account/drive have been removed from db
    Account account;
    bool found = false;
    CPPUNIT_ASSERT(ParmsDb::instance()->selectAccount(1, account, found));
    CPPUNIT_ASSERT(!found);

    Drive drive;
    CPPUNIT_ASSERT(ParmsDb::instance()->selectDrive(1, drive, found));
    CPPUNIT_ASSERT(!found);

    // Check that drive has been removed from cache but not user
    CPPUNIT_ASSERT(!AbstractTokenNetworkJob::_driveToApiKeyMap.contains(1));
}

void TestServerRequests::testDeleteDrive() {
    AbstractTokenNetworkJob::_userToApiKeyMap[1] = {nullptr, 0};
    AbstractTokenNetworkJob::_driveToApiKeyMap[1] = {0, 0};

    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, ServerRequests::deleteDrive(1));

    // Check that drive has been removed from db
    Drive drive;
    bool found = false;
    CPPUNIT_ASSERT(ParmsDb::instance()->selectDrive(1, drive, found));
    CPPUNIT_ASSERT(!found);

    // Check that drive has been removed from cache
    CPPUNIT_ASSERT(!AbstractTokenNetworkJob::_driveToApiKeyMap.contains(1));
}


} // namespace KDC
