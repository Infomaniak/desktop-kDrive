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

#include "testapitranslator.h"

#include "jobs/network/apitranslator.h"
#include "jobs/network/kDrive_API/getallfilesindirectoryjob.h"
#include "jobs/syncjobmanager.h"
#include "network/proxy.h"
#include "requests/parameterscache.h"

#include "libcommonserver/keychainmanager/keychainmanager.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/jsonparserutility.h"

#include "libparms/db/parmsdb.h"

#include "mocks/libsyncengine/vfs/mockvfs.h"
#include "mocks/libcommonserver/db/mockdb.h"

#include "test_utility/localtemporarydirectory.h"
#include "test_utility/remotetemporarydirectory.h"
#include "test_utility/testhelpers.h"
#include "test_utility/iohelpertestutilities.h"
#include "update_detection/file_system_observer/snapshot/snapshotitem.h"

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
        Proxy::instance(parameters.proxyConfig());
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
    CPPUNIT_ASSERT_EQUAL(_driveDbId, ApiTranslator::getDriveDbId(_driveId));
}

void TestApiTranslator::testTranslateV2ToV3() {
    RemoteNodeId remoteNodeId{ApiTranslator::v2RootFolderRemoteId};
    ApiTranslator::translateV2ToV3(_driveDbId, remoteNodeId);
    CPPUNIT_ASSERT(ApiTranslator::v2RootFolderRemoteId != remoteNodeId);

    remoteNodeId = ApiTranslator::v2RootFolderRemoteId + "666";
    ApiTranslator::translateV2ToV3(_driveDbId, remoteNodeId);
    CPPUNIT_ASSERT_EQUAL(ApiTranslator::v2RootFolderRemoteId + "666", remoteNodeId);
}

void TestApiTranslator::testTranslateV3ToV2() {
    SyncPath remotePath = SyncPath{ApiTranslator::v3UserPrivate} / Str("folder_A") / Str("text.txt");
    ApiTranslator::translateV3ToV2(remotePath);

    const auto expectedPath = SyncPath{Str("folder_A")} / Str("text.txt");
    CPPUNIT_ASSERT_EQUAL(expectedPath, remotePath);

    remotePath = SyncPath{"Common documents"} / Str("folder_A");
    ApiTranslator::translateV3ToV2(remotePath);
    CPPUNIT_ASSERT_EQUAL(SyncPath{"Common documents"} / Str("folder_A"), remotePath);

    remotePath = SyncPath{};
    ApiTranslator::translateV3ToV2(remotePath);
    CPPUNIT_ASSERT_EQUAL(SyncPath{}, remotePath);

    RemoteNodeId remoteNodeId = ApiTranslator::getUserPrivateFolderRemoteId(_driveDbId);
    ApiTranslator::translateV3ToV2(_driveDbId, remoteNodeId);
    CPPUNIT_ASSERT_EQUAL(ApiTranslator::v2RootFolderRemoteId, remoteNodeId);

    remoteNodeId = ApiTranslator::getCommonDocumentsRemoteId(_driveDbId);
    ApiTranslator::translateV3ToV2(_driveDbId, remoteNodeId);
    CPPUNIT_ASSERT(ApiTranslator::v2RootFolderRemoteId != remoteNodeId);
}

void TestApiTranslator::translateNodeInfoListFromV3ToV2() {
    const auto privateFolderId = QString::fromStdString(ApiTranslator::getUserPrivateFolderRemoteId(_driveDbId));
    NodeInfo nodeInfo1;
    nodeInfo1.setParentNodeId(QString::fromStdString(ApiTranslator::v2RootFolderRemoteId));
    nodeInfo1.setNodeId(privateFolderId);

    NodeInfo nodeInfo2;
    nodeInfo2.setParentNodeId(privateFolderId);
    nodeInfo2.setNodeId("private_sub_folder");

    NodeInfo nodeInfo3;
    const auto parentId = ApiTranslator::getCommonDocumentsRemoteId(_driveDbId);
    nodeInfo3.setParentNodeId(QString::fromStdString(parentId));
    nodeInfo3.setNodeId("sub_common_documents_folder");

    NodeInfoList v3NodeInfoList{nodeInfo1, nodeInfo2, nodeInfo3};

    ApiTranslator::translateV3ToV2(_driveDbId, v3NodeInfoList);
    for (const auto &v2NodeInfo: v3NodeInfoList) {
        CPPUNIT_ASSERT(privateFolderId != v2NodeInfo.parentNodeId());
        CPPUNIT_ASSERT(privateFolderId != v2NodeInfo.nodeId());
    }
}

void TestApiTranslator::testGetSpecialFolders() {
    const auto id1 = ApiTranslator::getCommonDocumentsRemoteId(_driveDbId);
    CPPUNIT_ASSERT(!id1.empty());
    const auto id2 = ApiTranslator::getUserPrivateFolderRemoteId(_driveDbId);
    CPPUNIT_ASSERT(!id2.empty());
    const auto id3 = ApiTranslator::getSharedRemoteId(_driveDbId);
    CPPUNIT_ASSERT(id3.empty()); // There is no 'Shared' sub-folder in the test drive.

    const RemoteNodeIdSet folderIds{id1, id2, id3};
    CPPUNIT_ASSERT_EQUAL(size_t{3}, folderIds.size());
}


} // namespace KDC
