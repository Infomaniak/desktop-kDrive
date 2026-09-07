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

#include "testapitranslator.h"

#include "jobs/network/kDrive_API/apitranslator.h"

#include "jobs/syncjobmanager.h"
#include "network/proxy.h"
#include "requests/parameterscache.h"

#include "libcommonserver/keychainmanager/keychainmanager.h"

#include "libparms/db/parmsdb.h"
#include "mocks/libcommonserver/db/mockdb.h"

#include "test_utility/testhelpers.h"
#include "test_utility/iohelpertestutilities.h"

using namespace CppUnit;

namespace KDC {

void TestApiTranslator::setUp() {
    TestBase::start();
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ Set Up");

    const testhelpers::TestVariables testVariables;

    // Insert api token into keystore
    _apiToken.setAccessToken(testVariables.apiToken);

    const std::string keychainKey("123");
    (void) KeyChainManager::instance(true);
    (void) KeyChainManager::instance()->writeToken(keychainKey, _apiToken.reconstructJsonString());

    // Create parmsDb
    (void) ParmsDb::instance(_localParmsDbTempDir.path() / MockDb::makeDbMockFileName(), KDRIVE_VERSION_STRING, true, true);
    ParametersCache::instance()->parameters().setExtendedLog(true);

    // Insert user, account & drive
    const int userId(atoi(testVariables.userId.c_str()));
    User user(1, userId, keychainKey);
    (void) ParmsDb::instance()->insertUser(user);
    _userDbId = user.dbId();

    const int accountId(atoi(testVariables.accountId.c_str()));
    Account account(1, accountId, user.dbId(), "account1");
    (void) ParmsDb::instance()->insertAccount(account);

    _driveDbId = 1;
    _driveId = atoi(testVariables.driveId.c_str());
    Drive drive(static_cast<int>(_driveDbId), static_cast<int>(_driveId), account.dbId(), std::string(), 0, std::string());
    (void) ParmsDb::instance()->insertDrive(drive);

    // Setup proxy
    Parameters parameters;
    bool found = false;
    if (ParmsDb::instance()->selectParameters(parameters, found) && found) {
        (void) Proxy::instance(parameters.proxyConfig());
    }
}

void TestApiTranslator::tearDown() {
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ Tear Down");

    ParmsDb::instance()->close();
    ParmsDb::reset();
    ParametersCache::reset();
    SyncJobManagerSingleton::instance()->stop();
    SyncJobManagerSingleton::clear();
    IoHelperTestUtilities::resetFunctions();
    TestBase::stop();
}

void TestApiTranslator::testGetDriveDbId() {
    DriveDbId driveDbId = 0;
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), ApiTranslator::getDriveDbId(_driveId, driveDbId));
    CPPUNIT_ASSERT_GREATER(DriveDbId{0}, driveDbId);
}

void TestApiTranslator::testTranslateV2ToV3() {
    RemoteNodeId remoteNodeId = ApiTranslator::v2RootFolderRemoteId();
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), ApiTranslator::translateV2ToV3(_driveDbId, remoteNodeId));
    CPPUNIT_ASSERT(ApiTranslator::v2RootFolderRemoteId() != remoteNodeId);

    remoteNodeId = ApiTranslator::v2RootFolderRemoteId() + "666";
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), ApiTranslator::translateV2ToV3(_driveDbId, remoteNodeId));
    CPPUNIT_ASSERT_EQUAL(ApiTranslator::v2RootFolderRemoteId() + "666", remoteNodeId);
}

void TestApiTranslator::testTranslateV3ToV2() {
    SyncPath remotePath = SyncPath{ApiTranslator::v3SpecialFolderNames.at(ApiTranslator::SpecialFolder::Private)} /
                          Str("folder_A") / Str("text.txt");
    ApiTranslator::translateV3ToV2(remotePath);

    const auto expectedPath = SyncPath{Str("folder_A")} / Str("text.txt");
    CPPUNIT_ASSERT_EQUAL(expectedPath, remotePath);

    remotePath = SyncPath{"Common documents"} / Str("folder_A");
    ApiTranslator::translateV3ToV2(remotePath);
    CPPUNIT_ASSERT_EQUAL(SyncPath{"Common documents"} / Str("folder_A"), remotePath);

    remotePath = SyncPath{};
    ApiTranslator::translateV3ToV2(remotePath);
    CPPUNIT_ASSERT_EQUAL(SyncPath{}, remotePath);

    remotePath = SyncPath{ApiTranslator::v3SpecialFolderNames.at(ApiTranslator::SpecialFolder::Private)};
    ApiTranslator::translateV3ToV2(remotePath);
    CPPUNIT_ASSERT_EQUAL(SyncPath{}, remotePath);

    RemoteNodeId remoteNodeId;
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), ApiTranslator::getSpecialFolderRemoteId(
                                                         _driveDbId, ApiTranslator::SpecialFolder::Private, remoteNodeId));
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), ApiTranslator::translateV3ToV2(_driveDbId, remoteNodeId));
    CPPUNIT_ASSERT_EQUAL(ApiTranslator::v2RootFolderRemoteId(), remoteNodeId);

    CPPUNIT_ASSERT_EQUAL(
            ExitInfo(ExitCode::Ok),
            ApiTranslator::getSpecialFolderRemoteId(_driveDbId, ApiTranslator::SpecialFolder::CommonDocuments, remoteNodeId));
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), ApiTranslator::translateV3ToV2(_driveDbId, remoteNodeId));
    CPPUNIT_ASSERT(ApiTranslator::v2RootFolderRemoteId() != remoteNodeId);
}

void TestApiTranslator::translateRemoteNodeInfoListFromV3ToV2() {
    RemoteNodeId privateFolderId;
    (void) ApiTranslator::getSpecialFolderRemoteId(_driveDbId, ApiTranslator::SpecialFolder::Private, privateFolderId);
    const auto privateFolderIdQStr = QString::fromStdString(privateFolderId);

    NodeInfo nodeInfo1;
    nodeInfo1.setParentNodeId(QString::fromStdString(ApiTranslator::v2RootFolderRemoteId()));
    nodeInfo1.setNodeId(privateFolderIdQStr);

    NodeInfo nodeInfo2;
    nodeInfo2.setParentNodeId(privateFolderIdQStr);
    nodeInfo2.setNodeId("private_sub_folder");

    NodeInfo nodeInfo3;
    RemoteNodeId parentId;
    (void) ApiTranslator::getSpecialFolderRemoteId(_driveDbId, ApiTranslator::SpecialFolder::CommonDocuments, parentId);
    nodeInfo3.setParentNodeId(QString::fromStdString(parentId));
    nodeInfo3.setNodeId("sub_common_documents_folder");

    RemoteNodeInfoList v3RemoteNodeInfoList{nodeInfo1, nodeInfo2, nodeInfo3};

    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), ApiTranslator::translateV3ToV2(_driveDbId, v3RemoteNodeInfoList));
    for (const auto &v2NodeInfo: v3RemoteNodeInfoList) {
        CPPUNIT_ASSERT(privateFolderIdQStr != v2NodeInfo.parentNodeId());
        CPPUNIT_ASSERT(privateFolderIdQStr != v2NodeInfo.nodeId());
    }
}

void TestApiTranslator::testGetSpecialFolders() {
    RemoteNodeId id1;
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok),
                         ApiTranslator::getSpecialFolderRemoteId(_driveDbId, ApiTranslator::SpecialFolder::CommonDocuments, id1));
    CPPUNIT_ASSERT(!id1.empty());

    RemoteNodeId id2;
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok),
                         ApiTranslator::getSpecialFolderRemoteId(_driveDbId, ApiTranslator::SpecialFolder::Private, id2));
    CPPUNIT_ASSERT(!id2.empty());

    RemoteNodeId id3;
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok),
                         ApiTranslator::getSpecialFolderRemoteId(_driveDbId, ApiTranslator::SpecialFolder::Shared, id3));
    CPPUNIT_ASSERT(id3.empty()); // There is no 'Shared' sub-folder in the test drive.

    const RemoteNodeIdSet folderIds{id1, id2, id3};
    CPPUNIT_ASSERT_EQUAL(size_t{3}, folderIds.size());
}


} // namespace KDC
