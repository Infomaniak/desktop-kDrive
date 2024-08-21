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


#include "testsyncpal.h"
#include "jobs/network/movejob.h"
#include "libcommon/keychainmanager/keychainmanager.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/network/proxy.h"

#include <cstdlib>

using namespace CppUnit;

namespace KDC {

// const std::string testExecutorFolderRemoteId = "75";        // In common documents
const std::string testExecutorFolderRemoteId = "5007";  // In root

class SyncPalMock : public SyncPal {
    public:
        SyncPalMock(DbNodeId dbNodeId, const std::string &version) : SyncPal(dbNodeId, version){};
        struct ItemInfo {
                SyncPath driveRootPath;
                SyncPath oldLocalPath;
                NodeId remoteNodeId{-1};
        };
        std::vector<ItemInfo> deletedLocalItemInfos() const { return _deletedLocalItemInfos; };
        void clearDeletedLocalItemInfos() { _deletedLocalItemInfos.clear(); }


    protected:
        std::vector<ItemInfo> _deletedLocalItemInfos;
        void deleteLocalItem(const SyncPath &driveRootPath, const SyncPath &oldLocalPath, const NodeId remoteNodeId) override {
            _deletedLocalItemInfos.push_back(ItemInfo{driveRootPath, oldLocalPath, remoteNodeId});
        };
};


void TestSyncPal::setUp() {
    const std::string userIdStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_USER_ID");
    const std::string accountIdStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_ACCOUNT_ID");
    const std::string driveIdStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_DRIVE_ID");
    const std::string remotePathStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_REMOTE_PATH");
    const std::string apiTokenStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_API_TOKEN");

    if (userIdStr.empty() || accountIdStr.empty() || driveIdStr.empty() || remotePathStr.empty() || apiTokenStr.empty()) {
        throw std::runtime_error("Some environment variables are missing!");
    }
    const std::string localPathStr = _localTempDir.path().string();
    // Insert api token into keystore
    ApiToken apiToken;
    apiToken.setAccessToken(apiTokenStr);

    std::string keychainKey("123");
    KeyChainManager::instance(true);
    KeyChainManager::instance()->writeToken(keychainKey, apiToken.reconstructJsonString());

    // Create parmsDb
    bool alreadyExists;
    std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExists);
    std::filesystem::remove(parmsDbPath);
    ParmsDb::instance(parmsDbPath, "3.4.0", true, true);
    ParmsDb::instance()->setAutoDelete(true);

    // Insert user, account, drive & sync
    int userId = atoi(userIdStr.c_str());
    User user(1, userId, keychainKey);
    ParmsDb::instance()->insertUser(user);

    int accountId(atoi(accountIdStr.c_str()));
    Account account(1, accountId, user.dbId());
    ParmsDb::instance()->insertAccount(account);

    _driveDbId = 1;
    int driveId = atoi(driveIdStr.c_str());
    Drive drive(_driveDbId, driveId, account.dbId(), std::string(), 0, std::string());
    ParmsDb::instance()->insertDrive(drive);

    _localPath = localPathStr;
    _remotePath = remotePathStr;
    Sync sync(1, drive.dbId(), _localPath, _remotePath);
    ParmsDb::instance()->insertSync(sync);

    // Setup proxy
    Parameters parameters;
    bool found;
    if (ParmsDb::instance()->selectParameters(parameters, found) && found) {
        Proxy::instance(parameters.proxyConfig());
    }

    _syncPal = std::shared_ptr<SyncPal>(new SyncPalMock(sync.dbId(), "3.4.0"));
    _syncPal->init();
}

void TestSyncPal::tearDown() {
    // Stop SyncPal and delete sync DB
    if (_syncPal) {
        _syncPal->stop(false, true, true);
    }
    ParmsDb::reset();
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
    auto snapshot = _syncPal->snapshot(ReplicaSide::Local);
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, snapshot->side());

    snapshot = _syncPal->snapshot(ReplicaSide::Remote);
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Remote, snapshot->side());

    snapshot = _syncPal->snapshot(ReplicaSide::Unknown);
    CPPUNIT_ASSERT_EQUAL(std::shared_ptr<Snapshot>(nullptr), snapshot);

    snapshot = _syncPal->snapshotCopy(ReplicaSide::Local);
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, snapshot->side());

    snapshot = _syncPal->snapshot(ReplicaSide::Remote, true);
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Remote, snapshot->side());

    snapshot = _syncPal->snapshot(ReplicaSide::Unknown, true);
    CPPUNIT_ASSERT_EQUAL(std::shared_ptr<Snapshot>(nullptr), snapshot);

    CPPUNIT_ASSERT(_syncPal->snapshotCopy(ReplicaSide::Local).get() != _syncPal->snapshot(ReplicaSide::Local).get());
    CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSide::Remote, true).get() != _syncPal->snapshot(ReplicaSide::Remote, false).get());
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
    CPPUNIT_ASSERT_EQUAL(_syncPal->snapshotCopy(ReplicaSide::Local)->nbItems(),
                         _syncPal->snapshot(ReplicaSide::Local)->nbItems());
    CPPUNIT_ASSERT_EQUAL(_syncPal->snapshotCopy(ReplicaSide::Local)->isValid(),
                         _syncPal->snapshot(ReplicaSide::Local)->isValid());
    CPPUNIT_ASSERT_EQUAL(_syncPal->snapshotCopy(ReplicaSide::Local)->rootFolderId(),
                         _syncPal->snapshot(ReplicaSide::Local)->rootFolderId());
    CPPUNIT_ASSERT_EQUAL(_syncPal->snapshotCopy(ReplicaSide::Local)->side(), _syncPal->snapshot(ReplicaSide::Local)->side());

    // Check that the copy is different object
    CPPUNIT_ASSERT(_syncPal->snapshotCopy(ReplicaSide::Local).get() != _syncPal->snapshot(ReplicaSide::Local).get());
    _syncPal->snapshot(ReplicaSide::Local)->setValid(true);
    _syncPal->snapshotCopy(ReplicaSide::Local)->setValid(false);
    CPPUNIT_ASSERT(_syncPal->snapshotCopy(ReplicaSide::Local)->isValid() != _syncPal->snapshot(ReplicaSide::Local)->isValid());
}

void TestSyncPal::testAll() {
    // Start sync
    _syncPal->start();
    Utility::msleep(1000);
    CPPUNIT_ASSERT(_syncPal->isRunning());

    // Wait for end of 1st sync
    while (!_syncPal->_syncHasFullyCompleted) {
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
    std::shared_ptr<Node> localrootNode = std::shared_ptr<Node>(
        new Node(std::nullopt, _syncPal->updateTree(ReplicaSide::Local)->side(), Str(""), NodeType::Directory,
                 OperationType::None, std::nullopt, 1234567890, 1234567890, 12345, nullptr));
    std::shared_ptr<Node> remoterootNode = std::shared_ptr<Node>(
        new Node(std::nullopt, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str(""), NodeType::Directory,
                 OperationType::None, std::nullopt, 1234567890, 1234567890, 12345, nullptr));

    // Move_Move_Source & Move_ParentDelete
    std::shared_ptr<Node> localDirA = std::shared_ptr<Node>(
        new Node(std::nullopt, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("A"), NodeType::Directory,
                 OperationType::None, std::nullopt, 1234567890, 1234567890, 12345, localrootNode));
    std::shared_ptr<Node> localDirB = std::shared_ptr<Node>(
        new Node(std::nullopt, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("B"), NodeType::Directory,
                 OperationType::None, std::nullopt, 1234567890, 1234567890, 12345, localrootNode));
    std::shared_ptr<Node> localFileC1 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("c1"), NodeType::File,
                                       OperationType::Move, std::nullopt, 1234567890, 1234567890, 12345, localDirB, "A/c1"));
    std::shared_ptr<Node> localFileC2 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("c2"), NodeType::File,
                                       OperationType::Move, std::nullopt, 1234567890, 1234567890, 12345, localDirB, "A/c2"));

    std::shared_ptr<Node> remoteDirA = std::shared_ptr<Node>(
        new Node(std::nullopt, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("A"), NodeType::Directory,
                 OperationType::None, std::nullopt, 1234567890, 1234567890, 12345, remoterootNode));
    std::shared_ptr<Node> remoteDirB = std::shared_ptr<Node>(
        new Node(std::nullopt, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("B"), NodeType::Directory,
                 OperationType::Delete, std::nullopt, 1234567890, 1234567890, 12345, remoterootNode));
    std::shared_ptr<Node> remoteFileC1 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("c1"), NodeType::File,
                                       OperationType::Move, std::nullopt, 1234567890, 1234567890, 12345, remoterootNode, "A/c1"));
    std::shared_ptr<Node> remoteFileC2 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("c2"), NodeType::File,
                                       OperationType::Move, std::nullopt, 1234567890, 1234567890, 12345, remoterootNode, "A/c2"));

    // Move_Create & Move_Delete
    std::shared_ptr<Node> localFileC4 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("c4"), NodeType::File,
                                       OperationType::Move, std::nullopt, 1234567890, 1234567890, 12345, localrootNode, "c3"));

    std::shared_ptr<Node> remoteFileC3 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("c4"), NodeType::File,
                                       OperationType::Create, std::nullopt, 1234567890, 1234567890, 12345, localrootNode));
    std::shared_ptr<Node> remoteFileC4 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("c3"), NodeType::File,
                                       OperationType::Delete, std::nullopt, 1234567890, 1234567890, 12345, localrootNode));

    // Move_Move_Cycle
    std::shared_ptr<Node> localDirC1 = std::shared_ptr<Node>(
        new Node(std::nullopt, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("c2"), NodeType::Directory,
                 OperationType::None, std::nullopt, 1234567890, 1234567890, 12345, localrootNode));
    std::shared_ptr<Node> localDirC2 = std::shared_ptr<Node>(
        new Node(std::nullopt, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("c2"), NodeType::Directory,
                 OperationType::None, std::nullopt, 1234567890, 1234567890, 12345, localrootNode));
    std::shared_ptr<Node> localDirD1 = std::shared_ptr<Node>(
        new Node(std::nullopt, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("D1"), NodeType::Directory,
                 OperationType::Move, std::nullopt, 1234567890, 1234567890, 12345, localDirC1, "D1"));
    std::shared_ptr<Node> localDirD2 = std::shared_ptr<Node>(
        new Node(std::nullopt, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("D2"), NodeType::Directory,
                 OperationType::Move, std::nullopt, 1234567890, 1234567890, 12345, localDirC2, "D2"));

    std::shared_ptr<Node> remoteDirD1 = std::shared_ptr<Node>(
        new Node(std::nullopt, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("D1"), NodeType::Directory,
                 OperationType::None, std::nullopt, 1234567890, 1234567890, 12345, remoterootNode));
    std::shared_ptr<Node> remoteDirD2 = std::shared_ptr<Node>(
        new Node(std::nullopt, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("D2"), NodeType::Directory,
                 OperationType::None, std::nullopt, 1234567890, 1234567890, 12345, remoterootNode));
    std::shared_ptr<Node> remoteDirC1 = std::shared_ptr<Node>(
        new Node(std::nullopt, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("c2"), NodeType::Directory,
                 OperationType::Move, std::nullopt, 1234567890, 1234567890, 12345, remoteDirD1, "C1"));
    std::shared_ptr<Node> remoteDirC2 = std::shared_ptr<Node>(
        new Node(std::nullopt, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("c2"), NodeType::Directory,
                 OperationType::Move, std::nullopt, 1234567890, 1234567890, 12345, remoteDirD2, "C2"));


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

    MoveJob job(_driveDbId, localCasePath, driveIdT.value(), driveIdN.value(), Str("w"));
    job.runSynchronously();

    return true;
}

bool TestSyncPal::check_case_6_4() {
    const std::string caseName("case_6.4");

    // Check local result
    const SyncPath localCasePath = _localPath / caseName;

    if (!std::filesystem::exists(localCasePath / "n/g/w/q")) {
        return false;
    }

    // Check remote result
    const SyncPath remoteCasePath = _remotePath / caseName;
    bool found = false;

    std::optional<NodeId> driveIdQ;
    _syncPal->syncDb()->id(ReplicaSide::Remote, remoteCasePath / "n/g/w/q", driveIdQ, found);
    if (!found || !driveIdQ.has_value()) {
        return false;
    }

    return true;
}

void TestSyncPal::testFixInconsistenFileNames() {
    _syncPal->fixInconsistentFileNames();
    auto syncPalMock = dynamic_cast<SyncPalMock *>(_syncPal.get());
    CPPUNIT_ASSERT(syncPalMock->deletedLocalItemInfos().empty());

    const time_t tLoc = std::time(0);
    const time_t tDrive = std::time(0);
    const auto rootId = syncPalMock->syncDb()->rootNode().nodeId();

    const DbNode nodeFile1(0, rootId, "file 1", "file 1", "id loc 1", "id drive 1", tLoc, tLoc, tDrive, NodeType::File, 0,
                           "cs 2.2");
    const DbNode nodeFile2(0, rootId, "file 2", "FILE 2", "id loc 2", "id drive 2", tLoc, tLoc, tDrive, NodeType::File, 0,
                           "cs 2.2");
    const DbNode nodeFile3(0, rootId, "FILE 3", "file 3", "id loc 3", "id drive 3", tLoc, tLoc, tDrive, NodeType::File, 0,
                           "cs 2.2");

    {
        bool constraintError = false;
        DbNodeId dbNodeId;
        syncPalMock->syncDb()->insertNode(nodeFile1, dbNodeId, constraintError);
        syncPalMock->syncDb()->insertNode(nodeFile2, dbNodeId, constraintError);
        syncPalMock->syncDb()->insertNode(nodeFile3, dbNodeId, constraintError);
    }

    _syncPal->fixInconsistentFileNames();

    CPPUNIT_ASSERT_EQUAL(size_t(2), syncPalMock->deletedLocalItemInfos().size());
    CPPUNIT_ASSERT_EQUAL(SyncPath("file 2"), syncPalMock->deletedLocalItemInfos().at(0).oldLocalPath);
    CPPUNIT_ASSERT_EQUAL(NodeId("id drive 2"), syncPalMock->deletedLocalItemInfos().at(0).remoteNodeId);
    CPPUNIT_ASSERT_EQUAL(_syncPal->localPath(), syncPalMock->deletedLocalItemInfos().at(0).driveRootPath);
    CPPUNIT_ASSERT_EQUAL(SyncPath("FILE 3"), syncPalMock->deletedLocalItemInfos().at(1).oldLocalPath);
    CPPUNIT_ASSERT_EQUAL(NodeId("id drive 3"), syncPalMock->deletedLocalItemInfos().at(1).remoteNodeId);
    CPPUNIT_ASSERT_EQUAL(_syncPal->localPath(), syncPalMock->deletedLocalItemInfos().at(1).driveRootPath);
}
}  // namespace KDC
