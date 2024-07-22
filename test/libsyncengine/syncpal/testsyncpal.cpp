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

    _syncPal = std::shared_ptr<SyncPal>(new SyncPal(sync.dbId(), "3.4.0"));
}

void TestSyncPal::tearDown() {
    // Stop SyncPal and delete sync DB
    if (_syncPal) {
        _syncPal->stop(false, true, true);
    }
    ParmsDb::reset();
}

void TestSyncPal::testUpdateTree() {
    auto updateTree = _syncPal->updateTree(ReplicaSideLocal);
    CPPUNIT_ASSERT_EQUAL(ReplicaSideLocal, updateTree->side());

    updateTree = _syncPal->updateTree(ReplicaSideRemote);
    CPPUNIT_ASSERT_EQUAL(ReplicaSideRemote, updateTree->side());

    updateTree = _syncPal->updateTree(ReplicaSideUnknown);
    CPPUNIT_ASSERT_EQUAL(std::shared_ptr<UpdateTree>(nullptr), updateTree);
}

void TestSyncPal::testSnapshot() {
    auto snapshot = _syncPal->snapshot(ReplicaSideLocal);
    CPPUNIT_ASSERT_EQUAL(ReplicaSideLocal, snapshot->side());

    snapshot = _syncPal->snapshot(ReplicaSideRemote);
    CPPUNIT_ASSERT_EQUAL(ReplicaSideRemote, snapshot->side());

    snapshot = _syncPal->snapshot(ReplicaSideUnknown);
    CPPUNIT_ASSERT_EQUAL(std::shared_ptr<Snapshot>(nullptr), snapshot);

    snapshot = _syncPal->snapshot(ReplicaSideLocal, true);
    CPPUNIT_ASSERT_EQUAL(ReplicaSideLocal, snapshot->side());

    snapshot = _syncPal->snapshot(ReplicaSideRemote, true);
    CPPUNIT_ASSERT_EQUAL(ReplicaSideRemote, snapshot->side());

    snapshot = _syncPal->snapshot(ReplicaSideUnknown, true);
    CPPUNIT_ASSERT_EQUAL(std::shared_ptr<Snapshot>(nullptr), snapshot);

    CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSideLocal, true).get() != _syncPal->snapshot(ReplicaSideLocal, false).get());
    CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSideRemote, true).get() != _syncPal->snapshot(ReplicaSideRemote, false).get());
}

void TestSyncPal::testOperationSet() {
    auto operationSet = _syncPal->operationSet(ReplicaSideLocal);
    CPPUNIT_ASSERT_EQUAL(ReplicaSideLocal, operationSet->side());

    operationSet = _syncPal->operationSet(ReplicaSideRemote);
    CPPUNIT_ASSERT_EQUAL(ReplicaSideRemote, operationSet->side());

    operationSet = _syncPal->operationSet(ReplicaSideUnknown);
    CPPUNIT_ASSERT_EQUAL(std::shared_ptr<FSOperationSet>(nullptr), operationSet);
}

void TestSyncPal::testCopySnapshots() {
    _syncPal->copySnapshots();

    // Check that the copy is the same as the original
    CPPUNIT_ASSERT_EQUAL(_syncPal->snapshot(ReplicaSideLocal, true)->nbItems(),
                         _syncPal->snapshot(ReplicaSideLocal, false)->nbItems());
    CPPUNIT_ASSERT_EQUAL(_syncPal->snapshot(ReplicaSideLocal, true)->isValid(),
                         _syncPal->snapshot(ReplicaSideLocal, false)->isValid());
    CPPUNIT_ASSERT_EQUAL(_syncPal->snapshot(ReplicaSideLocal, true)->rootFolderId(),
                         _syncPal->snapshot(ReplicaSideLocal, false)->rootFolderId());
    CPPUNIT_ASSERT_EQUAL(_syncPal->snapshot(ReplicaSideLocal, true)->side(), _syncPal->snapshot(ReplicaSideLocal, false)->side());

    // Check that the copy is different object
    CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSideLocal, true).get() != _syncPal->snapshot(ReplicaSideLocal, false).get());
    _syncPal->snapshot(ReplicaSideLocal, false)->setValid(true);
    _syncPal->snapshot(ReplicaSideLocal, true)->setValid(false);
    CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSideLocal, true)->isValid() !=
                   _syncPal->snapshot(ReplicaSideLocal, false)->isValid());
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
    std::shared_ptr<Node> localrootNode =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_localUpdateTree->side(), Str(""), NodeTypeDirectory,
                                       OperationTypeNone, std::nullopt, 1234567890, 1234567890, 12345, nullptr));
    std::shared_ptr<Node> remoterootNode =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str(""), NodeTypeDirectory,
                                       OperationTypeNone, std::nullopt, 1234567890, 1234567890, 12345, nullptr));

    // Move_Move_Source & Move_ParentDelete
    std::shared_ptr<Node> localDirA =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_localUpdateTree->side(), Str("A"), NodeTypeDirectory,
                                       OperationTypeNone, std::nullopt, 1234567890, 1234567890, 12345, localrootNode));
    std::shared_ptr<Node> localDirB =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_localUpdateTree->side(), Str("B"), NodeTypeDirectory,
                                       OperationTypeNone, std::nullopt, 1234567890, 1234567890, 12345, localrootNode));
    std::shared_ptr<Node> localFileC1 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_localUpdateTree->side(), Str("c1"), NodeTypeFile,
                                       OperationTypeMove, std::nullopt, 1234567890, 1234567890, 12345, localDirB, "A/c1"));
    std::shared_ptr<Node> localFileC2 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_localUpdateTree->side(), Str("c2"), NodeTypeFile,
                                       OperationTypeMove, std::nullopt, 1234567890, 1234567890, 12345, localDirB, "A/c2"));

    std::shared_ptr<Node> remoteDirA =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("A"), NodeTypeDirectory,
                                       OperationTypeNone, std::nullopt, 1234567890, 1234567890, 12345, remoterootNode));
    std::shared_ptr<Node> remoteDirB =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("B"), NodeTypeDirectory,
                                       OperationTypeDelete, std::nullopt, 1234567890, 1234567890, 12345, remoterootNode));
    std::shared_ptr<Node> remoteFileC1 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("c1"), NodeTypeFile,
                                       OperationTypeMove, std::nullopt, 1234567890, 1234567890, 12345, remoterootNode, "A/c1"));
    std::shared_ptr<Node> remoteFileC2 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("c2"), NodeTypeFile,
                                       OperationTypeMove, std::nullopt, 1234567890, 1234567890, 12345, remoterootNode, "A/c2"));

    // Move_Create & Move_Delete
    std::shared_ptr<Node> localFileC4 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_localUpdateTree->side(), Str("c4"), NodeTypeFile,
                                       OperationTypeMove, std::nullopt, 1234567890, 1234567890, 12345, localrootNode, "c3"));

    std::shared_ptr<Node> remoteFileC3 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("c4"), NodeTypeFile,
                                       OperationTypeCreate, std::nullopt, 1234567890, 1234567890, 12345, localrootNode));
    std::shared_ptr<Node> remoteFileC4 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("c3"), NodeTypeFile,
                                       OperationTypeDelete, std::nullopt, 1234567890, 1234567890, 12345, localrootNode));

    // Move_Move_Cycle
    std::shared_ptr<Node> localDirC1 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_localUpdateTree->side(), Str("c2"), NodeTypeDirectory,
                                       OperationTypeNone, std::nullopt, 1234567890, 1234567890, 12345, localrootNode));
    std::shared_ptr<Node> localDirC2 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_localUpdateTree->side(), Str("c2"), NodeTypeDirectory,
                                       OperationTypeNone, std::nullopt, 1234567890, 1234567890, 12345, localrootNode));
    std::shared_ptr<Node> localDirD1 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_localUpdateTree->side(), Str("D1"), NodeTypeDirectory,
                                       OperationTypeMove, std::nullopt, 1234567890, 1234567890, 12345, localDirC1, "D1"));
    std::shared_ptr<Node> localDirD2 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_localUpdateTree->side(), Str("D2"), NodeTypeDirectory,
                                       OperationTypeMove, std::nullopt, 1234567890, 1234567890, 12345, localDirC2, "D2"));

    std::shared_ptr<Node> remoteDirD1 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("D1"), NodeTypeDirectory,
                                       OperationTypeNone, std::nullopt, 1234567890, 1234567890, 12345, remoterootNode));
    std::shared_ptr<Node> remoteDirD2 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("D2"), NodeTypeDirectory,
                                       OperationTypeNone, std::nullopt, 1234567890, 1234567890, 12345, remoterootNode));
    std::shared_ptr<Node> remoteDirC1 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("c2"), NodeTypeDirectory,
                                       OperationTypeMove, std::nullopt, 1234567890, 1234567890, 12345, remoteDirD1, "C1"));
    std::shared_ptr<Node> remoteDirC2 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("c2"), NodeTypeDirectory,
                                       OperationTypeMove, std::nullopt, 1234567890, 1234567890, 12345, remoteDirD2, "C2"));


    _syncPal->_localUpdateTree->insertNode(localrootNode);
    _syncPal->_localUpdateTree->insertNode(localDirA);
    _syncPal->_localUpdateTree->insertNode(localDirB);
    _syncPal->_localUpdateTree->insertNode(localFileC1);
    _syncPal->_localUpdateTree->insertNode(localFileC2);
    _syncPal->_localUpdateTree->insertNode(localFileC4);

    _syncPal->_localUpdateTree->insertNode(remoterootNode);
    _syncPal->_localUpdateTree->insertNode(remoteDirA);
    _syncPal->_localUpdateTree->insertNode(remoteDirB);
    _syncPal->_localUpdateTree->insertNode(remoteFileC1);
    _syncPal->_localUpdateTree->insertNode(remoteFileC2);
    _syncPal->_localUpdateTree->insertNode(remoteFileC3);
    _syncPal->_localUpdateTree->insertNode(remoteFileC4);

    _syncPal->_localUpdateTree->insertNode(localDirC1);
    _syncPal->_localUpdateTree->insertNode(localDirC2);
    _syncPal->_localUpdateTree->insertNode(localDirD1);
    _syncPal->_localUpdateTree->insertNode(localDirD2);
    _syncPal->_localUpdateTree->insertNode(remoteDirC1);
    _syncPal->_localUpdateTree->insertNode(remoteDirC2);
    _syncPal->_localUpdateTree->insertNode(remoteDirD1);
    _syncPal->_localUpdateTree->insertNode(remoteDirD2);

    Conflict conflict1(localFileC1, remoteFileC1, ConflictTypeMoveMoveSource);
    Conflict conflict2(localFileC2, remoteFileC2, ConflictTypeMoveMoveSource);
    Conflict conflict3(localFileC1, remoteDirB, ConflictTypeMoveParentDelete);
    Conflict conflict4(localFileC2, remoteDirB, ConflictTypeMoveParentDelete);
    Conflict conflict5(localFileC4, remoteFileC3, ConflictTypeMoveCreate);
    Conflict conflict6(localFileC4, remoteFileC4, ConflictTypeMoveDelete);
    Conflict conflict7(localDirD1, remoteDirC1, ConflictTypeMoveMoveCycle);
    Conflict conflict8(localDirD2, remoteDirC2, ConflictTypeMoveMoveCycle);

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
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictTypeMoveParentDelete);
    _syncPal->_conflictQueue->pop();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictTypeMoveParentDelete);
    _syncPal->_conflictQueue->pop();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictTypeMoveDelete);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().node() == localFileC4);
    _syncPal->_conflictQueue->pop();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictTypeMoveMoveSource);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().node() == localFileC1);
    _syncPal->_conflictQueue->pop();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictTypeMoveMoveSource);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().node() == localFileC2);
    _syncPal->_conflictQueue->pop();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictTypeMoveCreate);
    _syncPal->_conflictQueue->pop();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictTypeMoveMoveCycle);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().node() == localDirD1);
    _syncPal->_conflictQueue->pop();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictTypeMoveMoveCycle);
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
    _syncPal->syncDb()->id(ReplicaSide::ReplicaSideRemote, remoteCasePath / "n", driveIdN, found);
    if (!found || !driveIdN.has_value()) {
        return false;
    }

    std::optional<NodeId> driveIdT;
    _syncPal->syncDb()->id(ReplicaSide::ReplicaSideRemote, remoteCasePath / "t", driveIdT, found);
    if (!found || !driveIdT.has_value()) {
        return false;
    }

    MoveJob job(_driveDbId, localCasePath, driveIdT.value(), driveIdN.value(), Str("w"));
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
    _syncPal->syncDb()->id(ReplicaSide::ReplicaSideRemote, remoteCasePath / "n/g/w/q", driveIdQ, found);
    if (!found || !driveIdQ.has_value()) {
        return false;
    }

    return true;
}

}  // namespace KDC
