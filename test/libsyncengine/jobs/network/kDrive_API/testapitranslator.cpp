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
#include "requests/parameterscache.h"

#include "libcommonserver/keychainmanager/keychainmanager.h"
#include "mocks/mockkeychainstorage.h"

#include "libparms/db/parmsdb.h"
#include "mocks/libcommonserver/db/mockdb.h"

#include "test_utility/testhelpers.h"
#include "test_utility/iohelpertestutilities.h"

#include <algorithm>

using namespace CppUnit;

namespace KDC {

void TestApiTranslator::setUp() {
    TestBase::start();
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ Set Up");

    initParmsDb();
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
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), ApiTranslator::translateV2ToV3(_userDbId, _driveId, remoteNodeId));
    CPPUNIT_ASSERT(ApiTranslator::v2RootFolderRemoteId() != remoteNodeId);

    remoteNodeId = ApiTranslator::v2RootFolderRemoteId() + "666";
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), ApiTranslator::translateV2ToV3(_userDbId, _driveId, remoteNodeId));
    CPPUNIT_ASSERT_EQUAL(ApiTranslator::v2RootFolderRemoteId() + "666", remoteNodeId);
}

void TestApiTranslator::testTranslateV3ToV2() {
    RemoteNodeId remoteNodeId;
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), ApiTranslator::getSpecialFolderRemoteId(
                                                         _userDbId, _driveId, SpecialRemoteFolder::Private, remoteNodeId));
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), ApiTranslator::translateV3ToV2(_userDbId, _driveId, remoteNodeId));
    CPPUNIT_ASSERT_EQUAL(ApiTranslator::v2RootFolderRemoteId(), remoteNodeId);

    CPPUNIT_ASSERT_EQUAL(
            ExitInfo(ExitCode::Ok),
            ApiTranslator::getSpecialFolderRemoteId(_userDbId, _driveId, SpecialRemoteFolder::CommonDocuments, remoteNodeId));
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), ApiTranslator::translateV3ToV2(_userDbId, _driveId, remoteNodeId));
    CPPUNIT_ASSERT(ApiTranslator::v2RootFolderRemoteId() != remoteNodeId);
}

void TestApiTranslator::testTranslateV3ToV2Paths() {
    // Pure path conversion tests, no API access required.
    // The API v3 only sends absolute paths ("/Private/...", "/Common documents/...", "/Shared/..."): the private space
    // is the root of the synchronized tree, while "Common documents" and "Shared" stay folders in it. Relative paths
    // are outside of the contract and are not covered here.
    const SyncName privateFolderName = ApiTranslator::v3SpecialFolderNames.at(SpecialRemoteFolder::Private);
    const SyncName commonDocumentsFolderName = ApiTranslator::v3SpecialFolderNames.at(SpecialRemoteFolder::CommonDocuments);
    const SyncName sharedFolderName = ApiTranslator::v3SpecialFolderNames.at(SpecialRemoteFolder::Shared);
    const SyncName rootSeparator = Str("/");

    // Absolute private path: the private folder name is stripped, the deeper components are preserved. The result is a
    // path relative to the drive root.
    SyncName remotePath = rootSeparator + privateFolderName + Str("/folder_A/text.txt");
    ApiTranslator::translateV3ToV2(remotePath);
    CPPUNIT_ASSERT_EQUAL(std::string("folder_A/text.txt"), SyncName2Str(remotePath));

    // A component deeper than the private folder may be named like the private folder: only the leading one is
    // stripped.
    remotePath = rootSeparator + privateFolderName + Str("/") + privateFolderName + Str("/text.txt");
    ApiTranslator::translateV3ToV2(remotePath);
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(privateFolderName) + std::string("/text.txt"), SyncName2Str(remotePath));

    // A first component that merely starts with the private folder name is a non-private path: unchanged.
    remotePath = rootSeparator + Str("Privatex") + Str("/folder_A");
    ApiTranslator::translateV3ToV2(remotePath);
    CPPUNIT_ASSERT_EQUAL(std::string("/Privatex/folder_A"), SyncName2Str(remotePath));

    // The private folder itself translates to the empty path, which represents the V2 drive root.
    remotePath = rootSeparator + privateFolderName;
    ApiTranslator::translateV3ToV2(remotePath);
    CPPUNIT_ASSERT_EQUAL(std::string(""), SyncName2Str(remotePath));

    // The drive root translates to itself.
    remotePath = rootSeparator;
    ApiTranslator::translateV3ToV2(remotePath);
    CPPUNIT_ASSERT_EQUAL(std::string("/"), SyncName2Str(remotePath));

    // Non-private absolute paths are unchanged, as the V2 tree uses the same paths for the other roots.
    remotePath = rootSeparator + commonDocumentsFolderName + Str("/folder_A");
    ApiTranslator::translateV3ToV2(remotePath);
    CPPUNIT_ASSERT_EQUAL(std::string("/") + SyncName2Str(commonDocumentsFolderName) + std::string("/folder_A"),
                         SyncName2Str(remotePath));

    remotePath = rootSeparator + sharedFolderName + Str("/folder_A/text.txt");
    ApiTranslator::translateV3ToV2(remotePath);
    CPPUNIT_ASSERT_EQUAL(std::string("/") + SyncName2Str(sharedFolderName) + std::string("/folder_A/text.txt"),
                         SyncName2Str(remotePath));

    // Empty path: unchanged.
    remotePath = SyncName{};
    ApiTranslator::translateV3ToV2(remotePath);
    CPPUNIT_ASSERT_EQUAL(std::string(""), SyncName2Str(remotePath));

#if defined(KD_WINDOWS)
    // On Windows, the engine uses the native wide strings while the raw strings extracted from the API replies are
    // UTF-8: check that the narrow overload applies the same translation rules.
    const std::string privateSegment = std::string("/") + SyncName2Str(privateFolderName);

    std::string remotePathStr = privateSegment + "/folder_A/text.txt";
    ApiTranslator::translateV3ToV2(remotePathStr);
    CPPUNIT_ASSERT_EQUAL(std::string("folder_A/text.txt"), remotePathStr);

    // The private folder itself translates to the empty path.
    remotePathStr = privateSegment;
    ApiTranslator::translateV3ToV2(remotePathStr);
    CPPUNIT_ASSERT_EQUAL(std::string(""), remotePathStr);

    // A first component that merely starts with the private folder name is a non-private path: unchanged.
    remotePathStr = "/Privatex/folder_A";
    ApiTranslator::translateV3ToV2(remotePathStr);
    CPPUNIT_ASSERT_EQUAL(std::string("/Privatex/folder_A"), remotePathStr);
#endif
}

void TestApiTranslator::translateRemoteNodeInfoListFromV3ToV2() {
    const auto privateFolderNameQStr = SyncName2QStr(ApiTranslator::v3SpecialFolderNames.at(SpecialRemoteFolder::Private));
    const auto commonDocumentsFolderNameQStr =
            SyncName2QStr(ApiTranslator::v3SpecialFolderNames.at(SpecialRemoteFolder::CommonDocuments));
    const auto sharedFolderNameQStr = SyncName2QStr(ApiTranslator::v3SpecialFolderNames.at(SpecialRemoteFolder::Shared));

    RemoteNodeId privateFolderId;
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), ApiTranslator::getSpecialFolderRemoteId(
                                                         _userDbId, _driveId, SpecialRemoteFolder::Private, privateFolderId));
    const auto privateFolderIdQStr = QString::fromStdString(privateFolderId);

    RemoteNodeId commonDocumentsFolderId;
    (void) ApiTranslator::getSpecialFolderRemoteId(_userDbId, _driveId, SpecialRemoteFolder::CommonDocuments,
                                                   commonDocumentsFolderId);
    RemoteNodeId sharedFolderId;
    (void) ApiTranslator::getSpecialFolderRemoteId(_userDbId, _driveId, SpecialRemoteFolder::Shared, sharedFolderId);

    // The private folder node: removed from the list by the translation.
    NodeInfo privateFolderNodeInfo;
    privateFolderNodeInfo.setParentNodeId(QString::fromStdString(ApiTranslator::v2RootFolderRemoteId()));
    privateFolderNodeInfo.setNodeId(privateFolderIdQStr);
    privateFolderNodeInfo.setPath(QString("/") + privateFolderNameQStr);

    // Immediate child of the private folder: re-parented to the V2 root folder, private folder name stripped from
    // its path.
    NodeInfo privateChildNodeInfo;
    privateChildNodeInfo.setParentNodeId(privateFolderIdQStr);
    privateChildNodeInfo.setNodeId("private_sub_folder");
    privateChildNodeInfo.setPath(QString("/") + privateFolderNameQStr + "/private_sub_folder");

    // Immediate child named like the private folder: only the leading private folder name is stripped.
    NodeInfo privateNamedChildNodeInfo;
    privateNamedChildNodeInfo.setParentNodeId(privateFolderIdQStr);
    privateNamedChildNodeInfo.setNodeId("private_named_sub_folder");
    privateNamedChildNodeInfo.setPath(QString("/") + privateFolderNameQStr + "/" + privateFolderNameQStr);

    // Deeper node of the private tree: private folder name stripped from its path, parent node left untouched.
    NodeInfo nestedPrivateChildNodeInfo;
    nestedPrivateChildNodeInfo.setParentNodeId(privateChildNodeInfo.nodeId());
    nestedPrivateChildNodeInfo.setNodeId("nested_file");
    nestedPrivateChildNodeInfo.setPath(QString("/") + privateFolderNameQStr + "/private_sub_folder/nested_file.txt");

    // Child of Common documents: path unchanged.
    NodeInfo commonDocumentsChildNodeInfo;
    commonDocumentsChildNodeInfo.setParentNodeId(QString::fromStdString(commonDocumentsFolderId));
    commonDocumentsChildNodeInfo.setNodeId("sub_common_documents_folder");
    commonDocumentsChildNodeInfo.setPath(QString("/") + commonDocumentsFolderNameQStr + "/sub_common_documents_folder");

    // Child of Shared: path unchanged.
    NodeInfo sharedChildNodeInfo;
    sharedChildNodeInfo.setParentNodeId(QString::fromStdString(sharedFolderId));
    sharedChildNodeInfo.setNodeId("sub_shared_folder");
    sharedChildNodeInfo.setPath(QString("/") + sharedFolderNameQStr + "/sub_shared_folder");

    RemoteNodeInfoList v3RemoteNodeInfoList{privateFolderNodeInfo,      privateChildNodeInfo,         privateNamedChildNodeInfo,
                                            nestedPrivateChildNodeInfo, commonDocumentsChildNodeInfo, sharedChildNodeInfo};

    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), ApiTranslator::translateV3ToV2(_userDbId, _driveId, v3RemoteNodeInfoList));

    // The private folder node is removed from the list.
    CPPUNIT_ASSERT_EQUAL(size_t{5}, v3RemoteNodeInfoList.size());
    for (const auto &v2NodeInfo: v3RemoteNodeInfoList) {
        CPPUNIT_ASSERT(privateFolderIdQStr != v2NodeInfo.nodeId());
        CPPUNIT_ASSERT(privateFolderIdQStr != v2NodeInfo.parentNodeId());
    }

    const auto findNode = [&v3RemoteNodeInfoList](const QString &nodeId) {
        return std::ranges::find_if(v3RemoteNodeInfoList,
                                    [&nodeId](const NodeInfo &nodeInfo) { return nodeInfo.nodeId() == nodeId; });
    };

    // The parent of an immediate private child is the V2 root folder.
    const auto privateSubFolderIt = findNode("private_sub_folder");
    CPPUNIT_ASSERT(privateSubFolderIt != v3RemoteNodeInfoList.cend());
    CPPUNIT_ASSERT(privateSubFolderIt->parentNodeId() == QString::fromStdString(ApiTranslator::v2RootFolderRemoteId()));
    CPPUNIT_ASSERT(privateSubFolderIt->path() == QString("/private_sub_folder"));

    const auto privateNamedSubFolderIt = findNode("private_named_sub_folder");
    CPPUNIT_ASSERT(privateNamedSubFolderIt != v3RemoteNodeInfoList.cend());
    CPPUNIT_ASSERT(privateNamedSubFolderIt->path() == QString("/") + privateFolderNameQStr);

    const auto nestedFileIt = findNode("nested_file");
    CPPUNIT_ASSERT(nestedFileIt != v3RemoteNodeInfoList.cend());
    CPPUNIT_ASSERT(nestedFileIt->parentNodeId() == QString("private_sub_folder"));
    CPPUNIT_ASSERT(nestedFileIt->path() == QString("/private_sub_folder/nested_file.txt"));

    const auto commonDocumentsSubFolderIt = findNode("sub_common_documents_folder");
    CPPUNIT_ASSERT(commonDocumentsSubFolderIt != v3RemoteNodeInfoList.cend());
    CPPUNIT_ASSERT(commonDocumentsSubFolderIt->path() ==
                   QString("/") + commonDocumentsFolderNameQStr + "/sub_common_documents_folder");

    const auto sharedSubFolderIt = findNode("sub_shared_folder");
    CPPUNIT_ASSERT(sharedSubFolderIt != v3RemoteNodeInfoList.cend());
    CPPUNIT_ASSERT(sharedSubFolderIt->path() == QString("/") + sharedFolderNameQStr + "/sub_shared_folder");
}

void TestApiTranslator::testGetSpecialFolders() {
    RemoteNodeId id1;
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok),
                         ApiTranslator::getSpecialFolderRemoteId(_userDbId, _driveId, SpecialRemoteFolder::CommonDocuments, id1));
    CPPUNIT_ASSERT(!id1.empty());

    RemoteNodeId id2;
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok),
                         ApiTranslator::getSpecialFolderRemoteId(_userDbId, _driveId, SpecialRemoteFolder::Private, id2));
    CPPUNIT_ASSERT(!id2.empty());

    RemoteNodeId id3;
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok),
                         ApiTranslator::getSpecialFolderRemoteId(_userDbId, _driveId, SpecialRemoteFolder::Shared, id3));
    CPPUNIT_ASSERT(!id3.empty()); // The API returns a valid identifier even when the 'Shared' folder contains no shared files.

    const RemoteNodeIdSet folderIds{id1, id2, id3};
    CPPUNIT_ASSERT_EQUAL(size_t{3}, folderIds.size());
}


} // namespace KDC
