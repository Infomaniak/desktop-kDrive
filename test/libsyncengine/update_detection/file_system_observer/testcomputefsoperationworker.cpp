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

#include "testcomputefsoperationworker.h"
#include "libcommon/keychainmanager/keychainmanager.h"
#include "libcommon/utility/utility.h"
#include "requests/exclusiontemplatecache.h"
#include "requests/syncnodecache.h"
#include "requests/parameterscache.h"
#include "test_utility/testhelpers.h"

#include <update_detection/file_system_observer/computefsoperationworker.h>

namespace KDC {

/**
 * init tree:
 *
 *      Root
 *      |-- A
 *      |   |-- AA
 *      |   |-- AB
 *      |   `-- AC
 *      `-- B
 *          |-- BA
 *          `-- BB
 */

void TestComputeFSOperationWorker::setUp() {
    const testhelpers::TestVariables testVariables;
    const std::string localPathStr = _localTempDir.path().string();

    /// Insert api token into keystore
    ApiToken apiToken;
    apiToken.setAccessToken(testVariables.apiToken);

    std::string keychainKey("123");
    KeyChainManager::instance(true);
    KeyChainManager::instance()->writeToken(keychainKey, apiToken.reconstructJsonString());

    /// Create parmsDb
    bool alreadyExists = false;
    std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExists, true);
    ParmsDb::instance(parmsDbPath, "3.4.0", true, true);

    /// Insert user, account, drive & sync
    int userId = atoi(testVariables.userId.c_str());
    User user(1, userId, keychainKey);
    ParmsDb::instance()->insertUser(user);

    int accountId(atoi(testVariables.accountId.c_str()));
    Account account(1, accountId, user.dbId());
    ParmsDb::instance()->insertAccount(account);

    int driveDbId = 1;
    int driveId = atoi(testVariables.driveId.c_str());
    Drive drive(driveDbId, driveId, account.dbId(), std::string(), 0, std::string());
    ParmsDb::instance()->insertDrive(drive);

    Sync sync(1, drive.dbId(), localPathStr, testVariables.remotePath);
    ParmsDb::instance()->insertSync(sync);

    _syncPal = std::make_shared<SyncPal>(sync.dbId(), "3.4.0");
    _syncPal->syncDb()->setAutoDelete(true);

    /// Insert node "AC" in blacklist
    SyncNodeCache::instance()->update(_syncPal->syncDbId(), SyncNodeType::BlackList, {"lac"});

    /// Insert nodes in DB
    DbNode nodeDirA(0, _syncPal->syncDb()->rootNode().nodeId(), Str("A"), Str("A"), "la", "ra", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    DbNode nodeDirB(0, _syncPal->syncDb()->rootNode().nodeId(), Str("B"), Str("B"), "lb", "rb", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    DbNodeId dbNodeIdDirA;
    DbNodeId dbNodeIdDirB;
    bool constraintError = false;
    _syncPal->syncDb()->insertNode(nodeDirA, dbNodeIdDirA, constraintError);
    _syncPal->syncDb()->insertNode(nodeDirB, dbNodeIdDirB, constraintError);

    DbNode nodeFileAA(0, dbNodeIdDirA, Str("AA"), Str("AA"), "laa", "raa", testhelpers::defaultTime, testhelpers::defaultTime,
                      testhelpers::defaultTime, NodeType::File, 0, "cs_aa");
    DbNodeId dbNodeIdFileAA;
    _syncPal->syncDb()->insertNode(nodeFileAA, dbNodeIdFileAA, constraintError);

    DbNode nodeFileAB(0, dbNodeIdDirA, Str("AB"), Str("AB"), "lab", "rab", testhelpers::defaultTime, testhelpers::defaultTime,
                      testhelpers::defaultTime, NodeType::File, 0, "cs_ab");

    DbNodeId dbNodeIdFileAB;
    _syncPal->syncDb()->insertNode(nodeFileAB, dbNodeIdFileAB, constraintError);

    // AC not in db since it should be excluded from sync

    DbNode nodeFileBA(0, dbNodeIdDirB, Str("BA"), Str("BA"), "lba", "rba", testhelpers::defaultTime, testhelpers::defaultTime,
                      testhelpers::defaultTime, NodeType::File, 0, "cs_ba");
    DbNodeId dbNodeIdFileBA;
    _syncPal->syncDb()->insertNode(nodeFileBA, dbNodeIdFileBA, constraintError);

    DbNode nodeFileBB(0, dbNodeIdDirB, Str("BB"), Str("BB"), "lbb", "rbb", testhelpers::defaultTime, testhelpers::defaultTime,
                      testhelpers::defaultTime, NodeType::File, 0, "cs_bb");
    DbNodeId dbNodeIdFileBB;
    _syncPal->syncDb()->insertNode(nodeFileBB, dbNodeIdFileBB, constraintError);

    /// Init test snapshot
    //// Insert dir in snapshot
    _syncPal->_localSnapshot->updateItem(
        SnapshotItem(nodeDirA.nodeIdLocal().value(), _syncPal->syncDb()->rootNode().nodeIdLocal().value(), nodeDirA.nameLocal(),
                     nodeDirA.created().value(), nodeDirA.lastModifiedLocal().value(), nodeDirA.type(), 123));
    _syncPal->_localSnapshot->updateItem(
        SnapshotItem(nodeDirB.nodeIdLocal().value(), _syncPal->syncDb()->rootNode().nodeIdLocal().value(), nodeDirB.nameLocal(),
                     nodeDirB.created().value(), nodeDirB.lastModifiedLocal().value(), nodeDirB.type(), 123));

    _syncPal->_remoteSnapshot->updateItem(SnapshotItem(
        nodeDirA.nodeIdRemote().value(), _syncPal->syncDb()->rootNode().nodeIdRemote().value(), nodeDirA.nameRemote(),
        nodeDirA.created().value(), nodeDirA.lastModifiedRemote().value(), nodeDirA.type(), 123));
    _syncPal->_remoteSnapshot->updateItem(SnapshotItem(
        nodeDirB.nodeIdRemote().value(), _syncPal->syncDb()->rootNode().nodeIdRemote().value(), nodeDirB.nameRemote(),
        nodeDirB.created().value(), nodeDirB.lastModifiedRemote().value(), nodeDirB.type(), 123));

    //// Insert files in snapshot
    _syncPal->_localSnapshot->updateItem(SnapshotItem(nodeFileAA.nodeIdLocal().value(), nodeDirA.nodeIdLocal().value(),
                                                      nodeFileAA.nameLocal(), nodeFileAA.created().value(),
                                                      nodeFileAA.lastModifiedLocal().value(), nodeFileAA.type(), 123));
    _syncPal->_localSnapshot->updateItem(SnapshotItem(nodeFileAB.nodeIdLocal().value(), nodeDirA.nodeIdLocal().value(),
                                                      nodeFileAB.nameLocal(), nodeFileAB.created().value(),
                                                      nodeFileAB.lastModifiedLocal().value(), nodeFileAB.type(), 123));
    _syncPal->_localSnapshot->updateItem(SnapshotItem(nodeFileBA.nodeIdLocal().value(), nodeDirB.nodeIdLocal().value(),
                                                      nodeFileBA.nameLocal(), nodeFileBA.created().value(),
                                                      nodeFileBA.lastModifiedLocal().value(), nodeFileBA.type(), 123));
    _syncPal->_localSnapshot->updateItem(SnapshotItem(nodeFileBB.nodeIdLocal().value(), nodeDirB.nodeIdLocal().value(),
                                                      nodeFileBB.nameLocal(), nodeFileBB.created().value(),
                                                      nodeFileBB.lastModifiedLocal().value(), nodeFileBB.type(), 123));

    _syncPal->_remoteSnapshot->updateItem(SnapshotItem(nodeFileAA.nodeIdRemote().value(), nodeDirA.nodeIdRemote().value(),
                                                       nodeFileAA.nameRemote(), nodeFileAA.created().value(),
                                                       nodeFileAA.lastModifiedRemote().value(), nodeFileAA.type(), 123));
    _syncPal->_remoteSnapshot->updateItem(SnapshotItem(nodeFileAB.nodeIdRemote().value(), nodeDirA.nodeIdRemote().value(),
                                                       nodeFileAB.nameRemote(), nodeFileAB.created().value(),
                                                       nodeFileAB.lastModifiedRemote().value(), nodeFileAB.type(), 123));
    _syncPal->_remoteSnapshot->updateItem(SnapshotItem(nodeFileBA.nodeIdRemote().value(), nodeDirB.nodeIdRemote().value(),
                                                       nodeFileBA.nameRemote(), nodeFileBA.created().value(),
                                                       nodeFileBA.lastModifiedRemote().value(), nodeFileBA.type(), 123));
    _syncPal->_remoteSnapshot->updateItem(SnapshotItem(nodeFileBB.nodeIdRemote().value(), nodeDirB.nodeIdRemote().value(),
                                                       nodeFileBB.nameRemote(), nodeFileBB.created().value(),
                                                       nodeFileBB.lastModifiedRemote().value(), nodeFileBB.type(), 123));
    _syncPal->_remoteSnapshot->updateItem(SnapshotItem("rac", nodeDirA.nodeIdRemote().value(), Str("AC"),
                                                       testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory,
                                                       123));

    // Insert items to excluded templates in DB
    std::vector<ExclusionTemplate> templateVec = {ExclusionTemplate("*.lnk", true)};
    ExclusionTemplateCache::instance()->update(true, templateVec);

    /// Activate big folder limit
    ParametersCache::instance()->parameters().setUseBigFolderSizeLimit(true);

    _syncPal->_computeFSOperationsWorker =
        std::make_shared<ComputeFSOperationWorker>(_syncPal, "Test Compute FS Operations", "TCOP");
    _syncPal->computeFSOperationsWorker()->setTesting(true);
    _syncPal->_localPath = testhelpers::localTestDirPath;
}

void TestComputeFSOperationWorker::tearDown() {
    ParmsDb::instance()->close();
    if (_syncPal && _syncPal->syncDb()) {
        _syncPal->syncDb()->close();
    }
    ParmsDb::reset();
}

void TestComputeFSOperationWorker::testNoOps() {
    _syncPal->copySnapshots();
    _syncPal->computeFSOperationsWorker()->execute();
    CPPUNIT_ASSERT_EQUAL(uint64_t(0), _syncPal->operationSet(ReplicaSide::Local)->nbOps());
}

void TestComputeFSOperationWorker::testDeletionOfNestedFolders() {
    // Delete operations
    _syncPal->_localSnapshot->removeItem("laa");  // Folder "AA" is contained in folder "A".
    _syncPal->_localSnapshot->removeItem("lab");  // Folder "AB" is contained in folder "A".
    _syncPal->_localSnapshot->removeItem("lac");  // Folder "AC" is contained in folder "A" but is blacklisted.
    _syncPal->_localSnapshot->removeItem("la");

    _syncPal->copySnapshots();
    _syncPal->computeFSOperationsWorker()->execute();

    FSOpPtr tmpOp = nullptr;
    CPPUNIT_ASSERT(_syncPal->_localOperationSet->findOp("la", OperationType::Delete, tmpOp));
    CPPUNIT_ASSERT(_syncPal->_localOperationSet->findOp("laa", OperationType::Delete, tmpOp));
    CPPUNIT_ASSERT(_syncPal->_localOperationSet->findOp("lab", OperationType::Delete, tmpOp));

    CPPUNIT_ASSERT(!_syncPal->_localOperationSet->findOp("lac", OperationType::Delete, tmpOp));

    CPPUNIT_ASSERT_EQUAL(uint64_t(3), _syncPal->operationSet(ReplicaSide::Local)->nbOps());
}


void TestComputeFSOperationWorker::testMultipleOps() {
    // On local replica
    // Create operation
    _syncPal->_localSnapshot->updateItem(
        SnapshotItem("lad", "la", Str("AD"), testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File, 123));
    // Edit operation
    _syncPal->_localSnapshot->setLastModified("laa", testhelpers::defaultTime + 60);
    // Move operation
    _syncPal->_localSnapshot->setParentId("lab", "lb");
    // Rename operation
    _syncPal->_localSnapshot->setName("lba", Str("BA-renamed"));
    // Delete operation
    _syncPal->_localSnapshot->removeItem("lbb");

    // Create operation on a too big directory
    _syncPal->_remoteSnapshot->updateItem(
        SnapshotItem("raf", "ra", Str("AF_too_big"), testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0));
    _syncPal->_remoteSnapshot->updateItem(SnapshotItem("rafa", "raf", Str("AFA"), testhelpers::defaultTime,
                                                       testhelpers::defaultTime, NodeType::File,
                                                       550 * 1024 * 1024));  // File size: 550MB
    // Rename operation on a blacklisted directory
    _syncPal->_remoteSnapshot->setName("rac", Str("AC-renamed"));

    _syncPal->copySnapshots();
    _syncPal->computeFSOperationsWorker()->execute();

    FSOpPtr tmpOp = nullptr;
    CPPUNIT_ASSERT(_syncPal->_localOperationSet->findOp("lad", OperationType::Create, tmpOp));
    CPPUNIT_ASSERT(_syncPal->_localOperationSet->findOp("laa", OperationType::Edit, tmpOp));
    CPPUNIT_ASSERT(_syncPal->_localOperationSet->findOp("lab", OperationType::Move, tmpOp));
    CPPUNIT_ASSERT(_syncPal->_localOperationSet->findOp("lba", OperationType::Move, tmpOp));
    CPPUNIT_ASSERT(_syncPal->_localOperationSet->findOp("lbb", OperationType::Delete, tmpOp));
    CPPUNIT_ASSERT(!_syncPal->_localOperationSet->findOp("lae", OperationType::Create, tmpOp));

    // On remote replica
    // Create operation but folder too big (should be ignored on local replica)
    CPPUNIT_ASSERT(!_syncPal->_localOperationSet->findOp("raf", OperationType::Create, tmpOp));
    CPPUNIT_ASSERT(!_syncPal->_localOperationSet->findOp("rafa", OperationType::Create, tmpOp));
    CPPUNIT_ASSERT(!_syncPal->_localOperationSet->findOp("rac", OperationType::Move, tmpOp));
}

void TestComputeFSOperationWorker::testLnkFileAlreadySynchronized() {
    // Add file in DB
    DbNode nodeTest(0, _syncPal->syncDb()->rootNode().nodeId(), Str("test.lnk"), Str("test.lnk"), "ltest", "rtest",
                    testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File, 0,
                    std::nullopt);
    DbNodeId dbNodeIdTest;
    bool constraintError = false;
    _syncPal->syncDb()->insertNode(nodeTest, dbNodeIdTest, constraintError);

    // File is excluded by template, it does not appear in snapshot
    _syncPal->copySnapshots();
    _syncPal->computeFSOperationsWorker()->execute();
    CPPUNIT_ASSERT_EQUAL(uint64_t(0), _syncPal->_localOperationSet->nbOps());
}

void TestComputeFSOperationWorker::testDifferentEncoding_NFC_NFD() {
    // NFC in DB, NFD on FS
    DbNode nodeTest(0, _syncPal->syncDb()->rootNode().nodeId(), Str("testé.txt"), Str("testé.txt"), "ltest", "rtest",
                    testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File,
                    testhelpers::defaultFileSize, std::nullopt);
    DbNodeId dbNodeIdTest;
    bool constraintError = false;
    _syncPal->syncDb()->insertNode(nodeTest, dbNodeIdTest, constraintError);

    _syncPal->_localSnapshot->updateItem(SnapshotItem("ltest", *_syncPal->syncDb()->rootNode().nodeIdLocal(), Str("testé.txt"),
                                                      testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File,
                                                      testhelpers::defaultFileSize));

    _syncPal->copySnapshots();
    _syncPal->_computeFSOperationsWorker->execute();
    FSOpPtr tmpOp = nullptr;
    CPPUNIT_ASSERT(_syncPal->_localOperationSet->findOp("ltest", OperationType::Move, tmpOp));
}

void TestComputeFSOperationWorker::testDifferentEncoding_NFD_NFC() {
    // NFD in DB, NFC on FS
    DbNode nodeTest(0, _syncPal->syncDb()->rootNode().nodeId(), Str("testé.txt"), Str("testé.txt"), "ltest", "rtest",
                    testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File,
                    testhelpers::defaultFileSize, std::nullopt);
    DbNodeId dbNodeIdTest;
    bool constraintError = false;
    _syncPal->syncDb()->insertNode(nodeTest, dbNodeIdTest, constraintError);

    _syncPal->_localSnapshot->updateItem(SnapshotItem("ltest", *_syncPal->syncDb()->rootNode().nodeIdLocal(), Str("testé.txt"),
                                                      testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File,
                                                      testhelpers::defaultFileSize));

    _syncPal->copySnapshots();
    _syncPal->_computeFSOperationsWorker->execute();
    FSOpPtr tmpOp = nullptr;
    CPPUNIT_ASSERT(_syncPal->_localOperationSet->findOp("ltest", OperationType::Move, tmpOp));
}

void TestComputeFSOperationWorker::testDifferentEncoding_NFD_NFD() {
    // NFD in DB, NFD on FS
    DbNode nodeTest(0, _syncPal->syncDb()->rootNode().nodeId(), Str("testé.txt"), Str("testé.txt"), "ltest", "rtest",
                    testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File,
                    testhelpers::defaultFileSize, std::nullopt);
    DbNodeId dbNodeIdTest;
    bool constraintError = false;
    _syncPal->syncDb()->insertNode(nodeTest, dbNodeIdTest, constraintError);

    _syncPal->_localSnapshot->updateItem(SnapshotItem("ltest", *_syncPal->syncDb()->rootNode().nodeIdLocal(), Str("testé.txt"),
                                                      testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File,
                                                      testhelpers::defaultFileSize));

    _syncPal->copySnapshots();
    _syncPal->_computeFSOperationsWorker->execute();
    FSOpPtr tmpOp = nullptr;
    CPPUNIT_ASSERT(!_syncPal->_localOperationSet->findOp("ltest", OperationType::Move, tmpOp));
    CPPUNIT_ASSERT(_syncPal->_localOperationSet->nbOps() == 0);
}

void TestComputeFSOperationWorker::testDifferentEncoding_NFC_NFC() {
    // NFC in DB, NFC on FS
    DbNode nodeTest(0, _syncPal->syncDb()->rootNode().nodeId(), Str("testé.txt"), Str("testé.txt"), "ltest", "rtest",
                    testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File,
                    testhelpers::defaultFileSize, std::nullopt);
    DbNodeId dbNodeIdTest;
    bool constraintError = false;
    _syncPal->syncDb()->insertNode(nodeTest, dbNodeIdTest, constraintError);

    _syncPal->_localSnapshot->updateItem(SnapshotItem("ltest", *_syncPal->syncDb()->rootNode().nodeIdLocal(), Str("testé.txt"),
                                                      testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File,
                                                      testhelpers::defaultFileSize));

    _syncPal->copySnapshots();
    _syncPal->_computeFSOperationsWorker->execute();
    FSOpPtr tmpOp = nullptr;
    CPPUNIT_ASSERT(!_syncPal->_localOperationSet->findOp("ltest", OperationType::Move, tmpOp));
    CPPUNIT_ASSERT(_syncPal->_localOperationSet->nbOps() == 0);
}

}  // namespace KDC
