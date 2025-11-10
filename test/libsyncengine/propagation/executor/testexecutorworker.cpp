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

#include "testexecutorworker.h"

#include "propagation/executor/executorworker.h"
#include "io/filestat.h"
#include "io/iohelper.h"
#include "keychainmanager/keychainmanager.h"
#include "mocks/libsyncengine/vfs/mockvfs.h"
#include "network/proxy.h"
#include "propagation/executor/filerescuer.h"
#include "jobs/network/kDrive_API/upload/uploadjob.h"

#include "mocks/libcommonserver/db/mockdb.h"
#include "test_classes/testsituationgenerator.h"
#include "test_utility/testhelpers.h"
#include "test_utility/localtemporarydirectory.h"
#include "test_utility/remotetemporarydirectory.h"

#include <memory>

namespace KDC {

void TestExecutorWorker::setUp() {
    TestBase::start();
    const testhelpers::TestVariables testVariables;

    const std::string localPathStr = _localTempDir.path().string();

    // Insert api token into keystore
    ApiToken apiToken;
    apiToken.setAccessToken(testVariables.apiToken);

    const std::string keychainKey("123");
    (void) KeyChainManager::instance(true);
    (void) KeyChainManager::instance()->writeToken(keychainKey, apiToken.reconstructJsonString());

    // Create parmsDb
    bool alreadyExists = false;
    std::filesystem::path parmsDbPath = MockDb::makeDbName(alreadyExists);
    (void) std::filesystem::remove(parmsDbPath);
    ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);

    // Insert user, account, drive & sync
    int userId(12321);
    User user(1, userId, keychainKey);
    (void) ParmsDb::instance()->insertUser(user);

    int accountId(atoi(testVariables.accountId.c_str()));
    Account account(1, accountId, user.dbId());
    (void) ParmsDb::instance()->insertAccount(account);

    _driveDbId = 1;
    int driveId = atoi(testVariables.driveId.c_str());
    Drive drive(_driveDbId, driveId, account.dbId(), std::string(), 0, std::string());
    (void) ParmsDb::instance()->insertDrive(drive);

    _sync = Sync(1, drive.dbId(), localPathStr, "", testVariables.remotePath);
    (void) ParmsDb::instance()->insertSync(_sync);

    // Setup proxy
    Parameters parameters;
    if (bool found = false; ParmsDb::instance()->selectParameters(parameters, found) && found) {
        Proxy::instance(parameters.proxyConfig());
    }

    _mockVfs = std::make_shared<MockVfs<VfsOff>>(VfsSetupParams(Log::instance()->getLogger()));
    _syncPal = std::make_shared<SyncPal>(_mockVfs, _sync.dbId(), KDRIVE_VERSION_STRING);
    _syncPal->createSharedObjects();
    _syncPal->createWorkers(std::chrono::seconds(0));
    _syncPal->syncDb()->setAutoDelete(true);

    _executorWorker = std::make_shared<ExecutorWorker>(_syncPal, "Executor", "EXEC");
}

void TestExecutorWorker::tearDown() {
    ParmsDb::instance()->close();
    ParmsDb::reset();
    if (_syncPal && _syncPal->syncDb()) {
        _syncPal->syncDb()->close();
    }
    TestBase::stop();
}

void TestExecutorWorker::testCheckLiteSyncInfoForCreate() {
#if defined(KD_MACOS)
    _executorWorker = std::make_shared<ExecutorWorker>(_syncPal, "Executor", "EXEC");


    //   Setup dummy values. Test inputs are set in the callbacks defined below.
    const auto opPtr = std::make_shared<SyncOperation>();
    opPtr->setTargetSide(ReplicaSide::Remote);
    const auto node = std::make_shared<Node>(1, ReplicaSide::Local, Str2SyncName("test_file.txt"), NodeType::File,
                                             OperationType::None, "1234", testhelpers::defaultTime, testhelpers::defaultTime,
                                             testhelpers::defaultFileSize, _syncPal->updateTree(ReplicaSide::Local)->rootNode());
    opPtr->setAffectedNode(node);
    // A hydrated placeholder.
    {
        auto mockStatus = []([[maybe_unused]] const SyncPath &absolutePath, VfsStatus &vfsStatus) {
            vfsStatus.isPlaceholder = true;
            vfsStatus.isHydrated = true;
            vfsStatus.isSyncing = false;
            vfsStatus.progress = 0;
            return ExitCode::Ok;
        };
        _mockVfs->setMockStatus(mockStatus);
        bool isDehydratedPlaceholder = false;
        _executorWorker->checkLiteSyncInfoForCreate(opPtr, "/", isDehydratedPlaceholder);

        CPPUNIT_ASSERT(!isDehydratedPlaceholder);
    }

    // A dehydrated placeholder.
    {
        auto mockStatus = []([[maybe_unused]] const SyncPath &absolutePath, VfsStatus &vfsStatus) {
            vfsStatus.isPlaceholder = true;
            vfsStatus.isHydrated = false;
            vfsStatus.isSyncing = false;
            vfsStatus.progress = 0;
            return ExitCode::Ok;
        };
        _mockVfs->setMockStatus(mockStatus);
        bool isDehydratedPlaceholder = false;
        _executorWorker->checkLiteSyncInfoForCreate(opPtr, "/", isDehydratedPlaceholder);

        CPPUNIT_ASSERT(isDehydratedPlaceholder);
    }

    // A partially hydrated placeholder (syncing item).
    {
        auto mockStatus = [&]([[maybe_unused]] const SyncPath &absolutePath, VfsStatus &vfsStatus) {
            vfsStatus.isPlaceholder = true;
            vfsStatus.isHydrated = false;
            vfsStatus.isSyncing = true;
            vfsStatus.progress = 30;
            return ExitCode::Ok;
        };
        _mockVfs->setMockStatus(mockStatus);
        bool isDehydratedPlaceholder = false;
        _executorWorker->checkLiteSyncInfoForCreate(opPtr, "/", isDehydratedPlaceholder);

        CPPUNIT_ASSERT(!isDehydratedPlaceholder);
    }

    // Not a placeholder.
    {
        auto mockStatus = [&]([[maybe_unused]] const SyncPath &absolutePath, VfsStatus &vfsStatus) {
            vfsStatus.isPlaceholder = false;
            vfsStatus.isHydrated = false;
            vfsStatus.isSyncing = false;
            vfsStatus.progress = 0;
            return ExitCode::Ok;
        };
        _mockVfs->setMockStatus(mockStatus);
        bool isDehydratedPlaceholder = false;
        _executorWorker->checkLiteSyncInfoForCreate(opPtr, "/", isDehydratedPlaceholder);

        CPPUNIT_ASSERT(!isDehydratedPlaceholder);
    }
#endif
}

SyncOpPtr TestExecutorWorker::generateSyncOperation(const DbNodeId dbNodeId, const SyncName &filename,
                                                    const OperationType opType) {
    const auto node = std::make_shared<Node>(dbNodeId, ReplicaSide::Local, filename, NodeType::File, OperationType::None, "lid",
                                             testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize,
                                             _syncPal->updateTree(ReplicaSide::Local)->rootNode());
    const auto correspondingNode = std::make_shared<Node>(
            dbNodeId, ReplicaSide::Remote, filename, NodeType::File, OperationType::None, "rid", testhelpers::defaultTime,
            testhelpers::defaultTime, testhelpers::defaultFileSize, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());

    auto op = std::make_shared<SyncOperation>();
    op->setAffectedNode(node);
    op->setCorrespondingNode(correspondingNode);
    op->setType(opType);

    return op;
}


SyncOpPtr TestExecutorWorker::generateSyncOperationWithNestedNodes(const DbNodeId dbNodeId, const SyncName &parentFilename,
                                                                   const OperationType opType, const NodeType nodeType) {
    auto parentNode =
            std::make_shared<Node>(dbNodeId, ReplicaSide::Local, parentFilename, NodeType::Directory, OperationType::None,
                                   "local_parent_id", testhelpers::defaultTime, testhelpers::defaultTime,
                                   testhelpers::defaultFileSize, _syncPal->updateTree(ReplicaSide::Local)->rootNode());

    auto node = std::make_shared<Node>(dbNodeId, ReplicaSide::Local, Str("test_file.txt"), nodeType, OperationType::None,
                                       "local_child_id", testhelpers::defaultTime, testhelpers::defaultTime,
                                       testhelpers::defaultFileSize, parentNode);


    auto correspondingNode =
            std::make_shared<Node>(dbNodeId, ReplicaSide::Remote, Str("test_file.txt"), nodeType, OperationType::None,
                                   "remote_child_id", testhelpers::defaultTime, testhelpers::defaultTime,
                                   testhelpers::defaultFileSize, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());

    SyncOpPtr op = std::make_shared<SyncOperation>();
    op->setAffectedNode(node);
    op->setCorrespondingNode(correspondingNode);
    op->setType(opType);

    return op;
}

class ExecutorWorkerMock : public ExecutorWorker {
    public:
        ExecutorWorkerMock(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName) :
            ExecutorWorker(syncPal, name, shortName) {};

        using ArgsMap = std::map<std::shared_ptr<Node>, std::shared_ptr<Node>>;
        void setCorrespondingNodeInOtherTree(ArgsMap nodeMap) { _correspondingNodeInOtherTree = nodeMap; };

    protected:
        ArgsMap _correspondingNodeInOtherTree;
        virtual std::shared_ptr<Node> correspondingNodeInOtherTree(const std::shared_ptr<Node> node) {
            if (auto it = _correspondingNodeInOtherTree.find(node); it != _correspondingNodeInOtherTree.cend()) {
                return it->second;
            }

            return nullptr;
        };
};

void TestExecutorWorker::testIsValidDestination() {
    // Always true if the target side is local or unknown
    {
        SyncOpPtr op = generateSyncOperation(1, Str("test_file.txt"));
        CPPUNIT_ASSERT(_executorWorker->isValidDestination(op));
    }
    // Always true if the operation is not of type Create
    {
        SyncOpPtr op = generateSyncOperation(1, Str("test_file.txt"));
        op->setTargetSide(ReplicaSide::Remote);
        CPPUNIT_ASSERT(_executorWorker->isValidDestination(op));
    }
    // Always true if the item is created on the local replica at the root of the synchronisation folder
    {
        SyncOpPtr op = generateSyncOperation(1, Str("parent_dir"));
        op->setTargetSide(ReplicaSide::Remote);
        CPPUNIT_ASSERT(_executorWorker->isValidDestination(op));
    }


    const auto executorWorkerMock = std::shared_ptr<ExecutorWorkerMock>(new ExecutorWorkerMock(_syncPal, "Executor", "EXEC"));
    // False if the item created on the local replica is not at the root of the synchronisation folder and has no
    // corresponding parent node.
    {
        SyncOpPtr op = generateSyncOperationWithNestedNodes(1, Str("test_file.txt"), OperationType::Create, NodeType::File);
        executorWorkerMock->setCorrespondingNodeInOtherTree({{op->affectedNode()->parentNode(), nullptr}});
        op->setTargetSide(ReplicaSide::Remote);
        CPPUNIT_ASSERT(!executorWorkerMock->isValidDestination(op));
    }

    const auto root = _syncPal->updateTree(ReplicaSide::Remote)->rootNode();

    const auto correspondingParentCommonDocsNode = std::make_shared<Node>(
            666, ReplicaSide::Remote, Utility::commonDocumentsFolderName(), NodeType::Directory, OperationType::None,
            "common_docs_id", testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, root);

    // False if the item created on the local replica is a file and has Common Documents as corresponding parent node.
    {
        SyncOpPtr op = generateSyncOperationWithNestedNodes(1, Str("test_file.txt"), OperationType::Create, NodeType::File);
        op->setTargetSide(ReplicaSide::Remote);
        executorWorkerMock->setCorrespondingNodeInOtherTree(
                {{op->affectedNode()->parentNode(), correspondingParentCommonDocsNode}});
        CPPUNIT_ASSERT(!executorWorkerMock->isValidDestination(op));
    }

    // True if the item created on the local replica is a directory and has Common Documents as corresponding parent node.
    {
        SyncOpPtr op = generateSyncOperationWithNestedNodes(1, Str("test_dir"), OperationType::Create, NodeType::Directory);
        op->setTargetSide(ReplicaSide::Remote);
        executorWorkerMock->setCorrespondingNodeInOtherTree(
                {{op->affectedNode()->parentNode(), correspondingParentCommonDocsNode}});
        CPPUNIT_ASSERT(executorWorkerMock->isValidDestination(op));
    }

    const auto correspondingParentSharedNode = std::make_shared<Node>(
            777, ReplicaSide::Remote, Utility::sharedFolderName(), NodeType::Directory, OperationType::None, "shared_id",
            testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, root);

    // False if the item is created on the local replica is a file and has Shared as corresponding parent node.
    {
        SyncOpPtr op = generateSyncOperationWithNestedNodes(1, Str("test_file.txt"), OperationType::Create, NodeType::File);
        op->setTargetSide(ReplicaSide::Remote);
        executorWorkerMock->setCorrespondingNodeInOtherTree({{op->affectedNode()->parentNode(), correspondingParentSharedNode}});
        CPPUNIT_ASSERT(!executorWorkerMock->isValidDestination(op));
    }

    // False if the item created on the local replica is a directory and has Shared as corresponding parent node.
    {
        SyncOpPtr op = generateSyncOperationWithNestedNodes(1, Str("test_dir"), OperationType::Create, NodeType::Directory);
        op->setTargetSide(ReplicaSide::Remote);
        executorWorkerMock->setCorrespondingNodeInOtherTree({{op->affectedNode()->parentNode(), correspondingParentSharedNode}});
        CPPUNIT_ASSERT(!executorWorkerMock->isValidDestination(op));
    }
}

void TestExecutorWorker::testTerminatedJobsQueue() {
    TerminatedJobsQueue terminatedJobsQueue;

    int ended = 0; // count the number of ended threads

    // Function objects to be used in the thread
    std::function inserter = [&terminatedJobsQueue, &ended](const UniqueId id) {
        terminatedJobsQueue.push(id);
        ended++;
    };
    std::function popper = [&terminatedJobsQueue, &ended]() {
        terminatedJobsQueue.pop();
        ended++;
    };
    std::function fronter = [&terminatedJobsQueue, &ended]() {
        [[maybe_unused]] auto foo = terminatedJobsQueue.front();
        ended++;
    };
    std::function emptyChecker = [&terminatedJobsQueue, &ended]() {
        [[maybe_unused]] auto foo = terminatedJobsQueue.empty();
        ended++;
    };

    // Check that all functions are thread safe
    terminatedJobsQueue.lock(); // Lock the queue for the current thread

    std::thread t1(inserter, 1);
    Utility::msleep(10); // Give enough time for the thread to terminate
    CPPUNIT_ASSERT_EQUAL(0, ended);

    std::thread t2(fronter);
    Utility::msleep(10);
    CPPUNIT_ASSERT_EQUAL(0, ended);

    std::thread t3(popper);
    Utility::msleep(10);
    CPPUNIT_ASSERT_EQUAL(0, ended);

    std::thread t4(emptyChecker);
    Utility::msleep(10);
    CPPUNIT_ASSERT_EQUAL(0, ended);

    terminatedJobsQueue.unlock(); // Unlock the queue for the current thread
    Utility::msleep(10);
    CPPUNIT_ASSERT_EQUAL(4, ended);

    t1.join();
    t2.join();
    t3.join();
    t4.join(); // Wait for all threads to finish.
}

void TestExecutorWorker::testPropagateConflictToDbAndTree() {
    bool propagateChange = false;
    const auto syncOp = generateSyncOperation(1, Str("test"));

    // Conflict types involving no special treatment, just propagate changes to DB.
    syncOp->setConflict(Conflict(syncOp->affectedNode(), syncOp->correspondingNode(), ConflictType::CreateParentDelete));
    _executorWorker->propagateConflictToDbAndTree(syncOp, propagateChange);
    CPPUNIT_ASSERT_EQUAL(true, propagateChange);

    syncOp->setConflict(Conflict(syncOp->affectedNode(), syncOp->correspondingNode(), ConflictType::MoveDelete));
    _executorWorker->propagateConflictToDbAndTree(syncOp, propagateChange);
    CPPUNIT_ASSERT_EQUAL(true, propagateChange);

    syncOp->setConflict(Conflict(syncOp->affectedNode(), syncOp->correspondingNode(), ConflictType::MoveParentDelete));
    _executorWorker->propagateConflictToDbAndTree(syncOp, propagateChange);
    CPPUNIT_ASSERT_EQUAL(true, propagateChange);

    syncOp->setConflict(Conflict(syncOp->affectedNode(), syncOp->correspondingNode(), ConflictType::MoveMoveCycle));
    _executorWorker->propagateConflictToDbAndTree(syncOp, propagateChange);
    CPPUNIT_ASSERT_EQUAL(true, propagateChange);

    syncOp->setConflict(Conflict(syncOp->affectedNode(), syncOp->correspondingNode(), ConflictType::None));
    _executorWorker->propagateConflictToDbAndTree(syncOp, propagateChange);
    CPPUNIT_ASSERT_EQUAL(true, propagateChange);

    /// EditDelete conflict : apply normal behavior only if the operation type is Delete.
    syncOp->setConflict(Conflict(syncOp->affectedNode(), syncOp->correspondingNode(), ConflictType::EditDelete));
    syncOp->setType(OperationType::Delete);
    _executorWorker->propagateConflictToDbAndTree(syncOp, propagateChange);
    CPPUNIT_ASSERT_EQUAL(true, propagateChange);

    // EditDelete conflict : Do nothing if operation type is different from Delete.
    syncOp->setConflict(Conflict(syncOp->affectedNode(), syncOp->correspondingNode(), ConflictType::EditDelete));
    syncOp->setType(OperationType::Move);
    _executorWorker->propagateConflictToDbAndTree(syncOp, propagateChange);
    CPPUNIT_ASSERT_EQUAL(false, propagateChange);

    // Conflict types involving special treatment
    syncOp->setConflict(Conflict(syncOp->affectedNode(), syncOp->correspondingNode(), ConflictType::EditEdit));
    _executorWorker->propagateConflictToDbAndTree(syncOp, propagateChange);
    CPPUNIT_ASSERT_EQUAL(false, propagateChange);

    syncOp->setConflict(Conflict(syncOp->affectedNode(), syncOp->correspondingNode(), ConflictType::CreateCreate));
    _executorWorker->propagateConflictToDbAndTree(syncOp, propagateChange);
    CPPUNIT_ASSERT_EQUAL(false, propagateChange);

    syncOp->setConflict(Conflict(syncOp->affectedNode(), syncOp->correspondingNode(), ConflictType::MoveCreate));
    _executorWorker->propagateConflictToDbAndTree(syncOp, propagateChange);
    CPPUNIT_ASSERT_EQUAL(false, propagateChange);

    syncOp->setConflict(Conflict(syncOp->affectedNode(), syncOp->correspondingNode(), ConflictType::MoveMoveDest));
    _executorWorker->propagateConflictToDbAndTree(syncOp, propagateChange);
    CPPUNIT_ASSERT_EQUAL(false, propagateChange);

    syncOp->setConflict(Conflict(syncOp->affectedNode(), syncOp->correspondingNode(), ConflictType::MoveMoveSource));
    _executorWorker->propagateConflictToDbAndTree(syncOp, propagateChange);
    CPPUNIT_ASSERT_EQUAL(false, propagateChange);
}

void TestExecutorWorker::testInitSyncFileItem() {
    const auto localNode = std::make_shared<Node>(
            ReplicaSide::Local, Str("test_file.txt"), NodeType::File, OperationType::None, "lid", testhelpers::defaultTime,
            testhelpers::defaultTime + 1, testhelpers::defaultFileSize, _syncPal->updateTree(ReplicaSide::Local)->rootNode());
    const auto remoteNode = std::make_shared<Node>(
            ReplicaSide::Remote, Str("test_file.txt"), NodeType::File, OperationType::None, "rid", testhelpers::defaultTime + 2,
            testhelpers::defaultTime + 3, testhelpers::defaultFileSize, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());

    // General case
    {
        auto syncOp = std::make_shared<SyncOperation>();
        syncOp->setAffectedNode(localNode);
        syncOp->setCorrespondingNode(remoteNode);
        syncOp->setType(OperationType::Create);
        syncOp->setTargetSide(ReplicaSide::Remote);

        SyncFileItem syncFileItem;
        _executorWorker->initSyncFileItem(syncOp, syncFileItem);
        CPPUNIT_ASSERT_EQUAL(NodeType::File, syncFileItem.type());
        CPPUNIT_ASSERT_EQUAL(ConflictType::None, syncFileItem.conflict());
        CPPUNIT_ASSERT_EQUAL(testhelpers::defaultTime, syncFileItem.creationTime());
        CPPUNIT_ASSERT_EQUAL(testhelpers::defaultTime + 1, syncFileItem.modTime());
        CPPUNIT_ASSERT_EQUAL(testhelpers::defaultFileSize, syncFileItem.size());
        CPPUNIT_ASSERT_EQUAL(SyncPath("test_file.txt"), syncFileItem.path());
        CPPUNIT_ASSERT(!syncFileItem.newPath().has_value());
        CPPUNIT_ASSERT(syncFileItem.localNodeId().has_value());
        CPPUNIT_ASSERT_EQUAL(std::string("lid"), syncFileItem.localNodeId().value());
        CPPUNIT_ASSERT(syncFileItem.remoteNodeId().has_value());
        CPPUNIT_ASSERT_EQUAL(std::string("rid"), syncFileItem.remoteNodeId().value());
    }

    // Test SyncItem::instruction and SyncItem::direction
    {
        auto syncOp = std::make_shared<SyncOperation>();
        syncOp->setAffectedNode(localNode);
        syncOp->setCorrespondingNode(remoteNode);
        syncOp->setTargetSide(ReplicaSide::Remote);
        SyncFileItem syncFileItem;

        syncOp->setType(OperationType::Create);
        _executorWorker->initSyncFileItem(syncOp, syncFileItem);
        CPPUNIT_ASSERT_EQUAL(SyncFileInstruction::Put,
                             syncFileItem.instruction()); // Local create to be propagated to remote
        CPPUNIT_ASSERT_EQUAL(SyncDirection::Up, syncFileItem.direction());

        syncOp->setType(OperationType::Delete);
        _executorWorker->initSyncFileItem(syncOp, syncFileItem);
        CPPUNIT_ASSERT_EQUAL(SyncFileInstruction::Remove,
                             syncFileItem.instruction()); // Local delete to be propagated to remote
        CPPUNIT_ASSERT_EQUAL(SyncDirection::Up, syncFileItem.direction());

        syncOp->setType(OperationType::Edit);
        _executorWorker->initSyncFileItem(syncOp, syncFileItem);
        CPPUNIT_ASSERT_EQUAL(SyncFileInstruction::Update,
                             syncFileItem.instruction()); // Local edit to be propagated to remote
        CPPUNIT_ASSERT_EQUAL(SyncDirection::Up, syncFileItem.direction());

        localNode->setMoveOriginInfos(
                {Str("test_file2.txt"), _syncPal->updateTree(ReplicaSide::Local)->rootNode()->id().value()});
        syncOp->setType(OperationType::Move);
        _executorWorker->initSyncFileItem(syncOp, syncFileItem);
        CPPUNIT_ASSERT_EQUAL(SyncFileInstruction::Move,
                             syncFileItem.instruction()); // Local move to be propagated to remote
        CPPUNIT_ASSERT_EQUAL(SyncDirection::Up, syncFileItem.direction());

        syncOp->setType(OperationType::MoveEdit);
        _executorWorker->initSyncFileItem(syncOp, syncFileItem);
        CPPUNIT_ASSERT_EQUAL(SyncFileInstruction::Move,
                             syncFileItem.instruction()); // Local move + edit to be propagated to remote
        CPPUNIT_ASSERT_EQUAL(SyncDirection::Up, syncFileItem.direction());

        syncOp->setAffectedNode(remoteNode);
        syncOp->setCorrespondingNode(localNode);
        syncOp->setTargetSide(ReplicaSide::Local);

        syncOp->setType(OperationType::Create);
        _executorWorker->initSyncFileItem(syncOp, syncFileItem);
        CPPUNIT_ASSERT_EQUAL(SyncFileInstruction::Get,
                             syncFileItem.instruction()); // Remote create to be propagated to local
        CPPUNIT_ASSERT_EQUAL(SyncDirection::Down, syncFileItem.direction());

        syncOp->setType(OperationType::Delete);
        _executorWorker->initSyncFileItem(syncOp, syncFileItem);
        CPPUNIT_ASSERT_EQUAL(SyncFileInstruction::Remove,
                             syncFileItem.instruction()); // Remote delete to be propagated to local
        CPPUNIT_ASSERT_EQUAL(SyncDirection::Down, syncFileItem.direction());

        syncOp->setType(OperationType::Edit);
        _executorWorker->initSyncFileItem(syncOp, syncFileItem);
        CPPUNIT_ASSERT_EQUAL(SyncFileInstruction::Update,
                             syncFileItem.instruction()); // Remote edit to be propagated to local
        CPPUNIT_ASSERT_EQUAL(SyncDirection::Down, syncFileItem.direction());

        remoteNode->setMoveOriginInfos(
                {Str("test_file2.txt"), _syncPal->updateTree(ReplicaSide::Remote)->rootNode()->id().value()});
        syncOp->setType(OperationType::Move);
        _executorWorker->initSyncFileItem(syncOp, syncFileItem);
        CPPUNIT_ASSERT_EQUAL(SyncFileInstruction::Move,
                             syncFileItem.instruction()); // Remote move to be propagated to local
        CPPUNIT_ASSERT_EQUAL(SyncDirection::Down, syncFileItem.direction());

        syncOp->setType(OperationType::MoveEdit);
        _executorWorker->initSyncFileItem(syncOp, syncFileItem);
        CPPUNIT_ASSERT_EQUAL(SyncFileInstruction::Move,
                             syncFileItem.instruction()); // Remote move + edit to be propagated to local
        CPPUNIT_ASSERT_EQUAL(SyncDirection::Down, syncFileItem.direction());
    }

    // Move special case
    {
        const auto localParentNode = std::make_shared<Node>(
                ReplicaSide::Local, Str("parent_dir"), NodeType::Directory, OperationType::None, "lid", testhelpers::defaultTime,
                testhelpers::defaultTime + 1, testhelpers::defaultFileSize, _syncPal->updateTree(ReplicaSide::Local)->rootNode());
        const auto remoteParentNode =
                std::make_shared<Node>(ReplicaSide::Remote, Str("parent_dir"), NodeType::Directory, OperationType::None, "rid",
                                       testhelpers::defaultTime + 2, testhelpers::defaultTime + 3, testhelpers::defaultFileSize,
                                       _syncPal->updateTree(ReplicaSide::Remote)->rootNode());

        CPPUNIT_ASSERT(remoteNode->setParentNode(remoteParentNode));
        CPPUNIT_ASSERT(remoteParentNode->insertChildren(remoteNode));

        // Local move from parent_dir/test_file.txt to /test_file.txt
        localNode->setMoveOriginInfos({"parent_dir/test_file.txt", localParentNode->id().value()});
        localNode->setChangeEvents(OperationType::Move);

        auto syncOp = std::make_shared<SyncOperation>();
        syncOp->setAffectedNode(localNode);
        syncOp->setCorrespondingNode(remoteNode);
        syncOp->setType(OperationType::Move);
        syncOp->setTargetSide(ReplicaSide::Remote);
        SyncFileItem syncFileItem;
        _executorWorker->initSyncFileItem(syncOp, syncFileItem);
        CPPUNIT_ASSERT_EQUAL(NodeType::File, syncFileItem.type());
        CPPUNIT_ASSERT_EQUAL(ConflictType::None, syncFileItem.conflict());
        CPPUNIT_ASSERT_EQUAL(testhelpers::defaultTime, syncFileItem.creationTime());
        CPPUNIT_ASSERT_EQUAL(testhelpers::defaultTime + 1, syncFileItem.modTime());
        CPPUNIT_ASSERT_EQUAL(testhelpers::defaultFileSize, syncFileItem.size());
        CPPUNIT_ASSERT_EQUAL(SyncPath("parent_dir/test_file.txt"), syncFileItem.path());
        CPPUNIT_ASSERT(syncFileItem.newPath().has_value());
        CPPUNIT_ASSERT_EQUAL(SyncPath("test_file.txt"), syncFileItem.newPath().value());
        CPPUNIT_ASSERT(syncFileItem.localNodeId().has_value());
        CPPUNIT_ASSERT_EQUAL(std::string("lid"), syncFileItem.localNodeId().value());
        CPPUNIT_ASSERT(syncFileItem.remoteNodeId().has_value());
        CPPUNIT_ASSERT_EQUAL(std::string("rid"), syncFileItem.remoteNodeId().value());
    }
}

void TestExecutorWorker::testDeleteOpNodes() {
    const auto syncOp = generateSyncOperation(1, Str("test"));
    syncOp->setTargetSide(ReplicaSide::Remote);

    {
        _syncPal->updateTree(ReplicaSide::Local)->insertNode(syncOp->affectedNode());
        _syncPal->updateTree(ReplicaSide::Remote)->insertNode(syncOp->correspondingNode());
        CPPUNIT_ASSERT(_executorWorker->deleteOpNodes(syncOp));
        CPPUNIT_ASSERT(!_syncPal->updateTree(ReplicaSide::Local)->exists(*syncOp->affectedNode()->id()));
        CPPUNIT_ASSERT(!_syncPal->updateTree(ReplicaSide::Remote)->exists(*syncOp->correspondingNode()->id()));
    }

    {
        // No corresponding node
        syncOp->setCorrespondingNode(nullptr);
        _syncPal->updateTree(ReplicaSide::Local)->insertNode(syncOp->affectedNode());
        CPPUNIT_ASSERT(_executorWorker->deleteOpNodes(syncOp));
        CPPUNIT_ASSERT(!_syncPal->updateTree(ReplicaSide::Local)->exists(*syncOp->affectedNode()->id()));
    }
}

void TestExecutorWorker::testCheckAlreadyExcluded() {
    const testhelpers::TestVariables testVariables;
    const RemoteTemporaryDirectory remoteTmpDir(_driveDbId, testVariables.remoteDirId, "testCheckAlreadyExcluded");

    const SyncPath localFilePath1 = _localTempDir.path() / "abcd.txt";
    std::ofstream(localFilePath1).close();
    const SyncPath localFilePath2 = _localTempDir.path() / "䈀椀爀挀栀 鈁";
    std::ofstream(localFilePath2).close();

    // Upload file 1
    UploadJob uploadJob1(nullptr, _driveDbId, localFilePath1, localFilePath1.filename(), remoteTmpDir.id(), 0, 0);
    uploadJob1.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, uploadJob1.exitInfo().code());

    ExitInfo exitInfo = _executorWorker->checkAlreadyExcluded(localFilePath1, remoteTmpDir.id());
    CPPUNIT_ASSERT(exitInfo == ExitInfo(ExitCode::DataError, ExitCause::FileExists));

    exitInfo = _executorWorker->checkAlreadyExcluded(localFilePath2, remoteTmpDir.id());
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitInfo.code());

    // Upload file 2
    UploadJob uploadJob2(nullptr, _driveDbId, localFilePath2, localFilePath2.filename(), remoteTmpDir.id(), 0, 0);
    uploadJob2.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, uploadJob2.exitInfo().code());

    exitInfo = _executorWorker->checkAlreadyExcluded(localFilePath2, remoteTmpDir.id());
    CPPUNIT_ASSERT(exitInfo == ExitInfo(ExitCode::DataError, ExitCause::FileExists));
}

void TestExecutorWorker::testFixModificationDate() {
    // Create temp directory
    const LocalTemporaryDirectory temporaryDirectory;
    // Create file
    const SyncName filename = Str("test_file.txt");
    const SyncPath path = temporaryDirectory.path() / filename;
    {
        std::ofstream ofs(path);
        ofs << "abc";
        ofs.close();
    }

    // Update DB
    DbNode dbNode(0, _syncPal->syncDb()->rootNode().nodeId(), filename, filename, "lid", "rid", testhelpers::defaultTime,
                  testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File, testhelpers::defaultFileSize, "cs");
    DbNodeId dbNodeId;
    bool constraintError = false;
    _syncPal->syncDb()->insertNode(dbNode, dbNodeId, constraintError);

    SyncOpPtr op = generateSyncOperation(dbNodeId, filename);
    CPPUNIT_ASSERT(_executorWorker->fixModificationDate(op, path));

    FileStat filestat;
    IoError ioError = IoError::Unknown;
    IoHelper::getFileStat(path, &filestat, ioError);

    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT_EQUAL(testhelpers::defaultTime, filestat.modificationTime);
}

void TestExecutorWorker::testAffectedUpdateTree() {
    // Normal cases
    auto syncOp = std::make_shared<SyncOperation>();
    syncOp->setTargetSide(ReplicaSide::Local);

    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Remote, _executorWorker->affectedUpdateTree(syncOp)->side());

    syncOp->setTargetSide(ReplicaSide::Remote);
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, _executorWorker->affectedUpdateTree(syncOp)->side());

    // ReplicaSide::Unknown case
    syncOp->setTargetSide(ReplicaSide::Unknown);
    CPPUNIT_ASSERT_EQUAL(std::shared_ptr<UpdateTree>(nullptr), _executorWorker->affectedUpdateTree(syncOp));
}

void TestExecutorWorker::testTargetUpdateTree() {
    // Normal cases
    auto syncOp = std::make_shared<SyncOperation>();
    syncOp->setTargetSide(ReplicaSide::Local);
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, _executorWorker->targetUpdateTree(syncOp)->side());

    syncOp->setTargetSide(ReplicaSide::Remote);
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Remote, _executorWorker->targetUpdateTree(syncOp)->side());

    // ReplicaSide::Unknown case
    syncOp->setTargetSide(ReplicaSide::Unknown);
    CPPUNIT_ASSERT_EQUAL(std::shared_ptr<UpdateTree>(nullptr), _executorWorker->targetUpdateTree(syncOp));
}

void TestExecutorWorker::testRemoveDependentOps() {
    {
        // Nested create.
        _syncPal->_syncOps->clear();

        auto op1Create = std::make_shared<SyncOperation>(); // a/ is created.
        auto op2Create = std::make_shared<SyncOperation>(); // a/b/ is created.
        auto op3Create = std::make_shared<SyncOperation>(); // a/b/c/ is created.
        auto affectedNode1 = std::make_shared<Node>(ReplicaSide::Remote, Str2SyncName("an1"), NodeType::Unknown,
                                                    OperationType::None, "idAn1", 1234, 1234, 1, nullptr);
        auto affectedNode2 = std::make_shared<Node>(ReplicaSide::Remote, Str2SyncName("an2"), NodeType::Unknown,
                                                    OperationType::None, "idAn2", 1234, 1234, 1, nullptr);
        auto affectedNode3 = std::make_shared<Node>(ReplicaSide::Remote, Str2SyncName("an3"), NodeType::Unknown,
                                                    OperationType::None, "idAn3", 1234, 1234, 1, nullptr);
        auto affectedNode4 = std::make_shared<Node>(ReplicaSide::Remote, Str2SyncName("an4"), NodeType::Unknown,
                                                    OperationType::None, "idAn4", 1234, 1234, 1, nullptr);

        affectedNode4->setParentNode(affectedNode3);
        affectedNode3->setParentNode(affectedNode2);
        affectedNode2->setParentNode(affectedNode1);

        op1Create->setAffectedNode(affectedNode2);
        op2Create->setAffectedNode(affectedNode3);
        op3Create->setAffectedNode(affectedNode4);

        _syncPal->_syncOps->pushOp(op1Create);
        _syncPal->_syncOps->pushOp(op2Create);
        _syncPal->_syncOps->pushOp(op3Create);

        _executorWorker->_opList = _syncPal->_syncOps->opSortedList();
        _executorWorker->removeDependentOps(op1Create); // op1Create failed, we should remove op2Create and op3Create.

        CPPUNIT_ASSERT(opsExist(op1Create));
        CPPUNIT_ASSERT(!opsExist(op2Create));
        CPPUNIT_ASSERT(!opsExist(op3Create));
    }

    {
        // Nested Move.
        _syncPal->_syncOps->clear();

        auto op1Create = std::make_shared<SyncOperation>(); // a/ is created.
        auto op2Move = std::make_shared<SyncOperation>(); // b/ is moved to a/b/.

        auto affectedNode1 = std::make_shared<Node>(ReplicaSide::Remote, Str2SyncName("an1"), NodeType::Unknown,
                                                    OperationType::None, "idAn1", 1234, 1234, 1, nullptr); // a/
        auto affectedNode2 = std::make_shared<Node>(ReplicaSide::Remote, Str2SyncName("an2"), NodeType::Unknown,
                                                    OperationType::None, "idAn2", 1234, 1234, 1, nullptr); // a/b

        auto correspondingNode2 = std::make_shared<Node>(ReplicaSide::Local, Str2SyncName("cn2"), NodeType::Unknown,
                                                         OperationType::None, "idCn2", 1234, 1234, 1, nullptr); // b/

        affectedNode2->setParentNode(affectedNode1);

        op1Create->setAffectedNode(affectedNode1);
        op2Move->setAffectedNode(affectedNode2);
        op2Move->setCorrespondingNode(correspondingNode2);

        _syncPal->_syncOps->pushOp(op1Create);
        _syncPal->_syncOps->pushOp(op2Move);

        _executorWorker->_opList = _syncPal->_syncOps->opSortedList();
        _executorWorker->removeDependentOps(op1Create); // op2Move failed, we should remove op2Edit.
        CPPUNIT_ASSERT(opsExist(op1Create));
        CPPUNIT_ASSERT(!opsExist(op2Move));
    }

    {
        // Double Move.
        _syncPal->_syncOps->clear();

        auto op1Move = std::make_shared<SyncOperation>(); // a is moved to b.
        auto op2Move = std::make_shared<SyncOperation>(); // y is moved to b/y.

        auto affectedNode1 = std::make_shared<Node>(ReplicaSide::Remote, Str2SyncName("an1"), NodeType::Unknown,
                                                    OperationType::None, "idAn1", 1234, 1234, 1, nullptr); // b
        auto affectedNode2 = std::make_shared<Node>(ReplicaSide::Remote, Str2SyncName("an2"), NodeType::Unknown,
                                                    OperationType::None, "idAn2", 1234, 1234, 1, nullptr); // b/y

        auto correspondingNode1 = std::make_shared<Node>(ReplicaSide::Local, Str2SyncName("cn1"), NodeType::Unknown,
                                                         OperationType::None, "idCn1", 1234, 1234, 1, nullptr); // a
        auto correspondingNode2 = std::make_shared<Node>(ReplicaSide::Local, Str2SyncName("cn2"), NodeType::Unknown,
                                                         OperationType::None, "idCn2", 1234, 1234, 1, nullptr); // y

        affectedNode2->setParentNode(affectedNode1);

        op1Move->setAffectedNode(affectedNode1);
        op1Move->setCorrespondingNode(correspondingNode1);

        op2Move->setAffectedNode(affectedNode2);
        op2Move->setCorrespondingNode(correspondingNode2);

        _syncPal->_syncOps->pushOp(op1Move);
        _syncPal->_syncOps->pushOp(op2Move);

        _executorWorker->_opList = _syncPal->_syncOps->opSortedList();
        _executorWorker->removeDependentOps(op1Move); // op2Move failed, we should remove op2Edit.
        CPPUNIT_ASSERT(opsExist(op1Move));
        CPPUNIT_ASSERT(!opsExist(op2Move));
    }
}

bool TestExecutorWorker::opsExist(SyncOpPtr op) {
    for (const auto &opId: _executorWorker->_opList) {
        if (_syncPal->_syncOps->getOp(opId) == op) {
            return true;
        }
    }

    return false;
}

} // namespace KDC
