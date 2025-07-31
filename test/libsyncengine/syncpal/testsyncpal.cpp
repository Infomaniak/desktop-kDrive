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

#include "testsyncpal.h"
#include "syncpal/tmpblacklistmanager.h"
#include "libcommon/keychainmanager/keychainmanager.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/network/proxy.h"
#include "libsyncengine/jobs/network/kDrive_API/movejob.h"
#include "mocks/libcommonserver/db/mockdb.h"
#include "update_detection/file_system_observer/filesystemobserverworker.h"

#include "test_utility/testhelpers.h"

#include <cstdlib>

using namespace CppUnit;

namespace KDC {

void TestSyncPal::setUp() {
    TestBase::start();
    const testhelpers::TestVariables testVariables;

    const std::string localPathStr = _localTempDir.path().string();
    // Insert api token into keystore
    ApiToken apiToken;
    apiToken.setAccessToken(testVariables.apiToken);

    std::string keychainKey("123");
    (void) KeyChainManager::instance(true);
    (void) KeyChainManager::instance()->writeToken(keychainKey, apiToken.reconstructJsonString());

    // Create parmsDb
    bool alreadyExists = false;
    std::filesystem::path parmsDbPath = MockDb::makeDbName(alreadyExists);
    ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);

    // Insert user, account, drive & sync
    int userId = atoi(testVariables.userId.c_str());
    User user(1, userId, keychainKey);
    (void) ParmsDb::instance()->insertUser(user);

    int accountId(atoi(testVariables.accountId.c_str()));
    Account account(1, accountId, user.dbId());
    (void) ParmsDb::instance()->insertAccount(account);

    _driveDbId = 1;
    int driveId = atoi(testVariables.driveId.c_str());
    Drive drive(_driveDbId, driveId, account.dbId(), std::string(), 0, std::string());
    (void) ParmsDb::instance()->insertDrive(drive);

    _localPath = localPathStr;
    _remotePath = testVariables.remotePath;
    Sync sync(1, drive.dbId(), _localPath, "", _remotePath);
    (void) ParmsDb::instance()->insertSync(sync);

    // Setup proxy
    Parameters parameters;
    bool found;
    if (ParmsDb::instance()->selectParameters(parameters, found) && found) {
        Proxy::instance(parameters.proxyConfig());
    }

    _syncPal = std::make_shared<SyncPal>(std::make_shared<VfsOff>(VfsSetupParams(Log::instance()->getLogger())), sync.dbId(),
                                         KDRIVE_VERSION_STRING);
    _syncPal->syncDb()->setAutoDelete(true);
    _syncPal->createSharedObjects();
    _syncPal->createWorkers();
    _syncPal->_tmpBlacklistManager = std::make_shared<TmpBlacklistManager>(_syncPal);
}

void TestSyncPal::tearDown() {
    // Stop SyncPal and delete sync DB
    if (_syncPal) {
        _syncPal->stop(false, true, true);
    }
    ParmsDb::instance()->close();
    ParmsDb::reset();
    TestBase::stop();
}

void TestSyncPal::testUpdateTree() {
    auto updateTree = _syncPal->updateTree(ReplicaSide::Local);
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, updateTree->side());

    updateTree = _syncPal->updateTree(ReplicaSide::Remote);
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Remote, updateTree->side());

    updateTree = _syncPal->updateTree(ReplicaSide::Unknown);
    CPPUNIT_ASSERT_EQUAL(std::shared_ptr<UpdateTree>(nullptr), updateTree);
}

void TestSyncPal::testSnapshot() {
    auto &localLiveSnapshot = _syncPal->liveSnapshot(ReplicaSide::Local);
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, localLiveSnapshot.side());

    auto &remoteLiveSnapshot = _syncPal->liveSnapshot(ReplicaSide::Remote);
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Remote, remoteLiveSnapshot.side());

    _syncPal->copySnapshots();
    LogIfFailSettings::assertEnabled = false;
    auto snapshot = _syncPal->snapshot(ReplicaSide::Unknown);
    LogIfFailSettings::assertEnabled = true;
    CPPUNIT_ASSERT_EQUAL(std::shared_ptr<ConstSnapshot>(nullptr), snapshot);

    auto localSnapshot = _syncPal->snapshot(ReplicaSide::Local);
    CPPUNIT_ASSERT(localSnapshot);
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, localSnapshot->side());

    auto remoteSnapshot = _syncPal->snapshot(ReplicaSide::Remote);
    CPPUNIT_ASSERT(remoteSnapshot);
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Remote, remoteSnapshot->side());
}

void TestSyncPal::testOperationSet() {
    auto operationSet = _syncPal->operationSet(ReplicaSide::Local);
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, operationSet->side());

    operationSet = _syncPal->operationSet(ReplicaSide::Remote);
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Remote, operationSet->side());

    operationSet = _syncPal->operationSet(ReplicaSide::Unknown);
    CPPUNIT_ASSERT_EQUAL(std::shared_ptr<FSOperationSet>(nullptr), operationSet);
}

void TestSyncPal::testCopySnapshots() {
    _syncPal->copySnapshots();

    // Check that the copy is the same as the original
    CPPUNIT_ASSERT_EQUAL(_syncPal->snapshot(ReplicaSide::Local)->nbItems(), _syncPal->liveSnapshot(ReplicaSide::Local).nbItems());
    CPPUNIT_ASSERT_EQUAL(_syncPal->snapshot(ReplicaSide::Local)->rootFolderId(),
                         _syncPal->liveSnapshot(ReplicaSide::Local).rootFolderId());
    CPPUNIT_ASSERT_EQUAL(_syncPal->snapshot(ReplicaSide::Local)->side(), _syncPal->liveSnapshot(ReplicaSide::Local).side());
}
void TestSyncPal::testSyncFileItem() {
    _syncPal->_progressInfo = std::make_shared<ProgressInfo>(_syncPal);

    CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(0), _syncPal->_progressInfo->completedSize());
    CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(0), _syncPal->_progressInfo->completedFiles());
    CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(0), _syncPal->_progressInfo->totalSize());
    CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(0), _syncPal->_progressInfo->totalFiles());

    const SyncPath nfcPath = SyncPath(Str("/") + testhelpers::makeNfcSyncName() + Str("/test.txt")).native();
    const SyncPath nfdPath = SyncPath(Str("/") + testhelpers::makeNfdSyncName() + Str("/test.txt")).native();
    SyncFileItem initItem;
    initItem.setType(NodeType::File);
    initItem.setPath(nfcPath);
    initItem.setLocalNodeId("l123");
    initItem.setDirection(SyncDirection::Up);
    initItem.setInstruction(SyncFileInstruction::Put);
    initItem.setSize(testhelpers::defaultFileSize);
    initItem.setModTime(testhelpers::defaultTime);
    CPPUNIT_ASSERT(_syncPal->initProgress(initItem));

    SyncFileItem testItem;
    CPPUNIT_ASSERT(_syncPal->getSyncFileItem(nfcPath, testItem));
    CPPUNIT_ASSERT(testItem == initItem);
    CPPUNIT_ASSERT(_syncPal->getSyncFileItem(nfdPath, testItem));
    CPPUNIT_ASSERT(testItem == initItem);

    CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(0), _syncPal->_progressInfo->completedSize());
    CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(0), _syncPal->_progressInfo->completedFiles());
    CPPUNIT_ASSERT_EQUAL(testhelpers::defaultFileSize, _syncPal->_progressInfo->totalSize());
    CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(1), _syncPal->_progressInfo->totalFiles());

    constexpr int64_t progress = 15;
    CPPUNIT_ASSERT(_syncPal->setProgress(nfdPath, progress));
    CPPUNIT_ASSERT(_syncPal->getSyncFileItem(nfdPath, testItem));

    CPPUNIT_ASSERT_EQUAL(progress, _syncPal->_progressInfo->completedSize());
    CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(0), _syncPal->_progressInfo->completedFiles());
    CPPUNIT_ASSERT_EQUAL(testhelpers::defaultFileSize, _syncPal->_progressInfo->totalSize());
    CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(1), _syncPal->_progressInfo->totalFiles());

    CPPUNIT_ASSERT(_syncPal->setProgressComplete(nfdPath, SyncFileStatus::Success));

    CPPUNIT_ASSERT_EQUAL(testhelpers::defaultFileSize, _syncPal->_progressInfo->completedSize());
    CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(1), _syncPal->_progressInfo->completedFiles());
    CPPUNIT_ASSERT_EQUAL(testhelpers::defaultFileSize, _syncPal->_progressInfo->totalSize());
    CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(1), _syncPal->_progressInfo->totalFiles());
}

void TestSyncPal::testCheckIfExistsOnServer() {
    // bool exists = false;
    //  CPPUNIT_ASSERT(!_syncPal->checkIfExistsOnServer(SyncPath("dummy"), exists));
}

void TestSyncPal::testBlacklist() {
    /// Insert nodes in DB
    const DbNode nodeDirA(0, _syncPal->syncDb()->rootNode().nodeId(), Str("A"), Str("A"), "la", "ra", testhelpers::defaultTime,
                          testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    const DbNode nodeDirB(0, _syncPal->syncDb()->rootNode().nodeId(), Str("B"), Str("B"), "lb", "rb", testhelpers::defaultTime,
                          testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    DbNodeId dbNodeIdDirA;
    DbNodeId dbNodeIdDirB;
    bool constraintError = false;
    _syncPal->syncDb()->insertNode(nodeDirA, dbNodeIdDirA, constraintError);
    _syncPal->syncDb()->insertNode(nodeDirB, dbNodeIdDirB, constraintError);

    const DbNode nodeFileAA(0, dbNodeIdDirA, Str("AA"), Str("AA"), "laa", "raa", testhelpers::defaultTime,
                            testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File, 0, "cs_aa");
    DbNodeId dbNodeIdFileAA;
    _syncPal->syncDb()->insertNode(nodeFileAA, dbNodeIdFileAA, constraintError);

    /// Init test liveSnapshot
    //// Insert dir in liveSnapshot
    _syncPal->_localFSObserverWorker->_liveSnapshot.updateItem(SnapshotItem(
            nodeDirA.nodeIdLocal().value(), _syncPal->syncDb()->rootNode().nodeIdLocal().value(), nodeDirA.nameLocal(),
            nodeDirA.created().value(), nodeDirA.lastModifiedLocal().value(), nodeDirA.type(), 123, false, true, true));
    _syncPal->_localFSObserverWorker->_liveSnapshot.updateItem(SnapshotItem(
            nodeDirB.nodeIdLocal().value(), _syncPal->syncDb()->rootNode().nodeIdLocal().value(), nodeDirB.nameLocal(),
            nodeDirB.created().value(), nodeDirB.lastModifiedLocal().value(), nodeDirB.type(), 123, false, true, true));

    _syncPal->_remoteFSObserverWorker->_liveSnapshot.updateItem(SnapshotItem(
            nodeDirA.nodeIdRemote().value(), _syncPal->syncDb()->rootNode().nodeIdRemote().value(), nodeDirA.nameRemote(),
            nodeDirA.created().value(), nodeDirA.lastModifiedRemote().value(), nodeDirA.type(), 123, false, true, true));
    _syncPal->_remoteFSObserverWorker->_liveSnapshot.updateItem(SnapshotItem(
            nodeDirB.nodeIdRemote().value(), _syncPal->syncDb()->rootNode().nodeIdRemote().value(), nodeDirB.nameRemote(),
            nodeDirB.created().value(), nodeDirB.lastModifiedRemote().value(), nodeDirB.type(), 123, false, true, true));

    //// Insert files in liveSnapshot
    _syncPal->_localFSObserverWorker->_liveSnapshot.updateItem(SnapshotItem(
            nodeFileAA.nodeIdLocal().value(), nodeDirA.nodeIdLocal().value(), nodeFileAA.nameLocal(),
            nodeFileAA.created().value(), nodeFileAA.lastModifiedLocal().value(), nodeFileAA.type(), 123, false, true, true));
    _syncPal->_remoteFSObserverWorker->_liveSnapshot.updateItem(SnapshotItem(
            nodeFileAA.nodeIdRemote().value(), nodeDirA.nodeIdRemote().value(), nodeFileAA.nameRemote(),
            nodeFileAA.created().value(), nodeFileAA.lastModifiedRemote().value(), nodeFileAA.type(), 123, false, true, true));

    // Make sure the local and remote items are blacklisted
    _syncPal->handleAccessDeniedItem("A", ExitCause::FileAccessError);
    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_tmpBlacklistManager->isTmpBlacklisted(SyncPath("A/AA"), ReplicaSide::Local));
    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_tmpBlacklistManager->isTmpBlacklisted(SyncPath("A/AA"), ReplicaSide::Remote));

    // Make sure the local and remote items are removed from blacklist (and the descendant)
    _syncPal->removeItemFromTmpBlacklist("ra", ReplicaSide::Remote);
    CPPUNIT_ASSERT_EQUAL(false, _syncPal->_tmpBlacklistManager->isTmpBlacklisted(SyncPath("A/AA"), ReplicaSide::Local));
    CPPUNIT_ASSERT_EQUAL(false, _syncPal->_tmpBlacklistManager->isTmpBlacklisted(SyncPath("A/AA"), ReplicaSide::Remote));
}

void TestSyncPal::testAll() {
    // Start sync
    _syncPal->start();
    Utility::msleep(1000);
    CPPUNIT_ASSERT(_syncPal->isRunning());

    // Wait for end of 1st sync
    while (!_syncPal->syncHasFullyCompleted()) {
        Utility::msleep(1000);
    }
    Utility::msleep(60000);

    // Pause sync
    _syncPal->pause();
    Utility::msleep(5000);
    CPPUNIT_ASSERT(_syncPal->isRunning());

    // Apply tests cases
    // CPPUNIT_ASSERT(exec_case_6_4());


    // Unpause sync
    _syncPal->unpause();
    Utility::msleep(10000);
    CPPUNIT_ASSERT(_syncPal->isRunning());

    // Wait for end of sync
    while (!_syncPal->isIdle()) {
        Utility::msleep(1000);
    }

    // Check sync results
    // CPPUNIT_ASSERT(check_case_6_4());


    // Stop sync
    _syncPal->stop(false, true, true);
    CPPUNIT_ASSERT(!_syncPal->isRunning());
}

void TestSyncPal::testConflictQueue() {
    const auto localrootNode = std::make_shared<Node>(std::nullopt, ReplicaSide::Local, Str(""), NodeType::Directory,
                                                      OperationType::None, "lRoot", 1234567890, 1234567890, 12345, nullptr);
    const auto remoterootNode = std::make_shared<Node>(std::nullopt, ReplicaSide::Remote, Str(""), NodeType::Directory,
                                                       OperationType::None, "rRoot", 1234567890, 1234567890, 12345, nullptr);

    // Move_Move_Source & Move_ParentDelete
    const auto localDirA = std::make_shared<Node>(std::nullopt, ReplicaSide::Local, Str("A"), NodeType::Directory,
                                                  OperationType::None, "lDirA", 1234567890, 1234567890, 12345, localrootNode);
    const auto localDirB = std::make_shared<Node>(std::nullopt, ReplicaSide::Local, Str("B"), NodeType::Directory,
                                                  OperationType::None, "lDirB", 1234567890, 1234567890, 12345, localrootNode);
    const auto localFileC1 = std::make_shared<Node>(std::nullopt, ReplicaSide::Local, Str("c1"), NodeType::File,
                                                    OperationType::Move, "lFileC1", 1234567890, 1234567890, 12345, localDirB,
                                                    Node::MoveOriginInfos("A/c1", localrootNode->id().value()));
    const auto localFileC2 = std::make_shared<Node>(std::nullopt, ReplicaSide::Local, Str("c2"), NodeType::File,
                                                    OperationType::Move, "lFileC2", 1234567890, 1234567890, 12345, localDirB,
                                                    Node::MoveOriginInfos("A/c2", localDirA->id().value()));
    const auto remoteDirA = std::make_shared<Node>(std::nullopt, ReplicaSide::Remote, Str("A"), NodeType::Directory,
                                                   OperationType::None, "rDirA", 1234567890, 1234567890, 12345, remoterootNode);
    const auto remoteDirB = std::make_shared<Node>(std::nullopt, ReplicaSide::Remote, Str("B"), NodeType::Directory,
                                                   OperationType::Delete, "rDirB", 1234567890, 1234567890, 12345, remoterootNode);
    const auto remoteFileC1 = std::make_shared<Node>(std::nullopt, ReplicaSide::Remote, Str("c1"), NodeType::File,
                                                     OperationType::Move, "rFileC1", 1234567890, 1234567890, 12345,
                                                     remoterootNode, Node::MoveOriginInfos("A/c1", remoteDirA->id().value()));
    const auto remoteFileC2 = std::make_shared<Node>(std::nullopt, ReplicaSide::Remote, Str("c2"), NodeType::File,
                                                     OperationType::Move, "rlFileC2", 1234567890, 1234567890, 12345,
                                                     remoterootNode, Node::MoveOriginInfos("A/c2", remoteDirA->id().value()));

    // Move_Create & Move_Delete
    const auto localFileC4 = std::make_shared<Node>(std::nullopt, ReplicaSide::Local, Str("c4"), NodeType::File,
                                                    OperationType::Move, "lFileC4", 1234567890, 1234567890, 12345, localrootNode,
                                                    Node::MoveOriginInfos("c3", localrootNode->id().value()));
    const auto remoteFileC3 =
            std::make_shared<Node>(std::nullopt, ReplicaSide::Remote, Str("c4"), NodeType::File, OperationType::Create, "rFileC3",
                                   1234567890, 1234567890, 12345, localrootNode);
    const auto remoteFileC4 =
            std::make_shared<Node>(std::nullopt, ReplicaSide::Remote, Str("c3"), NodeType::File, OperationType::Delete, "rFileC4",
                                   1234567890, 1234567890, 12345, localrootNode);

    // Move_Move_Cycle
    const auto localDirC1 = std::make_shared<Node>(std::nullopt, ReplicaSide::Local, Str("c2"), NodeType::Directory,
                                                   OperationType::None, "lDirC1", 1234567890, 1234567890, 12345, localrootNode);
    const auto localDirC2 = std::make_shared<Node>(std::nullopt, ReplicaSide::Local, Str("c2"), NodeType::Directory,
                                                   OperationType::None, "lDirC2", 1234567890, 1234567890, 12345, localrootNode);
    const auto localDirD1 = std::make_shared<Node>(std::nullopt, ReplicaSide::Local, Str("D1"), NodeType::Directory,
                                                   OperationType::Move, "lDirD1", 1234567890, 1234567890, 12345, localDirC1,
                                                   Node::MoveOriginInfos("D1", localrootNode->id().value()));
    const auto localDirD2 = std::make_shared<Node>(std::nullopt, ReplicaSide::Local, Str("D2"), NodeType::Directory,
                                                   OperationType::Move, "lDirD2", 1234567890, 1234567890, 12345, localDirC2,
                                                   Node::MoveOriginInfos("D2", localrootNode->id().value()));

    const auto remoteDirD1 =
            std::make_shared<Node>(std::nullopt, ReplicaSide::Remote, Str("D1"), NodeType::Directory, OperationType::None,
                                   std::nullopt, 1234567890, 1234567890, 12345, remoterootNode);
    const auto remoteDirD2 =
            std::make_shared<Node>(std::nullopt, ReplicaSide::Remote, Str("D2"), NodeType::Directory, OperationType::None,
                                   std::nullopt, 1234567890, 1234567890, 12345, remoterootNode);
    const auto remoteDirC1 = std::make_shared<Node>(std::nullopt, ReplicaSide::Remote, Str("c2"), NodeType::Directory,
                                                    OperationType::Move, std::nullopt, 1234567890, 1234567890, 12345, remoteDirD1,
                                                    Node::MoveOriginInfos("C1", localrootNode->id().value()));
    const auto remoteDirC2 = std::make_shared<Node>(std::nullopt, ReplicaSide::Remote, Str("c2"), NodeType::Directory,
                                                    OperationType::Move, std::nullopt, 1234567890, 1234567890, 12345, remoteDirD2,
                                                    Node::MoveOriginInfos("C2", localrootNode->id().value()));

    _syncPal->updateTree(ReplicaSide::Local)->insertNode(localrootNode);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(localDirA);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(localDirB);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(localFileC1);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(localFileC2);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(localFileC4);

    _syncPal->updateTree(ReplicaSide::Local)->insertNode(remoterootNode);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(remoteDirA);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(remoteDirB);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(remoteFileC1);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(remoteFileC2);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(remoteFileC3);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(remoteFileC4);

    _syncPal->updateTree(ReplicaSide::Local)->insertNode(localDirC1);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(localDirC2);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(localDirD1);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(localDirD2);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(remoteDirC1);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(remoteDirC2);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(remoteDirD1);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(remoteDirD2);

    Conflict conflict1(localFileC1, remoteFileC1, ConflictType::MoveMoveSource);
    Conflict conflict2(localFileC2, remoteFileC2, ConflictType::MoveMoveSource);
    Conflict conflict3(localFileC1, remoteDirB, ConflictType::MoveParentDelete);
    Conflict conflict4(localFileC2, remoteDirB, ConflictType::MoveParentDelete);
    Conflict conflict5(localFileC4, remoteFileC3, ConflictType::MoveCreate);
    Conflict conflict6(localFileC4, remoteFileC4, ConflictType::MoveDelete);
    Conflict conflict7(localDirD1, remoteDirC1, ConflictType::MoveMoveCycle);
    Conflict conflict8(localDirD2, remoteDirC2, ConflictType::MoveMoveCycle);

    // Insert conflicts in queue
    _syncPal->_conflictQueue->push(conflict7);
    _syncPal->_conflictQueue->push(conflict2);
    _syncPal->_conflictQueue->push(conflict5);
    _syncPal->_conflictQueue->push(conflict1);
    _syncPal->_conflictQueue->push(conflict4);
    _syncPal->_conflictQueue->push(conflict8);
    _syncPal->_conflictQueue->push(conflict3);
    _syncPal->_conflictQueue->push(conflict6);

    // Check order
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictType::MoveParentDelete);
    _syncPal->_conflictQueue->pop();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictType::MoveParentDelete);
    _syncPal->_conflictQueue->pop();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictType::MoveDelete);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().node() == localFileC4);
    _syncPal->_conflictQueue->pop();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictType::MoveMoveSource);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().node() == localFileC1);
    _syncPal->_conflictQueue->pop();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictType::MoveMoveSource);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().node() == localFileC2);
    _syncPal->_conflictQueue->pop();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictType::MoveCreate);
    _syncPal->_conflictQueue->pop();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictType::MoveMoveCycle);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().node() == localDirD1);
    _syncPal->_conflictQueue->pop();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictType::MoveMoveCycle);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().node() == localDirD2);
}

bool TestSyncPal::exec_case_6_4() {
    std::string caseName("case_6.4");

    // Local operations
    SyncPath localCasePath = _localPath / caseName;

    try {
        std::filesystem::rename(localCasePath / "n", localCasePath / "t/q/e/g");
        std::filesystem::rename(localCasePath / "t/q/e", localCasePath / "n");
    } catch (...) {
        return false;
    }

    // Remote operations
    SyncPath remoteCasePath = _remotePath / caseName;
    bool found;

    std::optional<NodeId> driveIdN;
    _syncPal->syncDb()->id(ReplicaSide::Remote, remoteCasePath / "n", driveIdN, found);
    if (!found || !driveIdN.has_value()) {
        return false;
    }

    std::optional<NodeId> driveIdT;
    _syncPal->syncDb()->id(ReplicaSide::Remote, remoteCasePath / "t", driveIdT, found);
    if (!found || !driveIdT.has_value()) {
        return false;
    }

    MoveJob job(_syncPal->vfs(), _driveDbId, localCasePath, driveIdT.value(), driveIdN.value(), Str("w"));
    job.runSynchronously();

    return true;
}

bool TestSyncPal::check_case_6_4() {
    std::string caseName("case_6.4");

    // Check local result
    SyncPath localCasePath = _localPath / caseName;

    if (!std::filesystem::exists(localCasePath / "n/g/w/q")) {
        return false;
    }

    // Check remote result
    SyncPath remoteCasePath = _remotePath / caseName;
    bool found;

    std::optional<NodeId> driveIdQ;
    _syncPal->syncDb()->id(ReplicaSide::Remote, remoteCasePath / "n/g/w/q", driveIdQ, found);
    if (!found || !driveIdQ.has_value()) {
        return false;
    }

    return true;
}
} // namespace KDC
