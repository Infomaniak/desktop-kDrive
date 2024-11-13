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

#include "testexecutorworker.h"

#include "propagation/executor/executorworker.h"
#include "io/filestat.h"
#include "io/iohelper.h"
#include "keychainmanager/keychainmanager.h"
#include "network/proxy.h"
#include "test_utility/testhelpers.h"

#include <memory>

namespace KDC {

void TestExecutorWorker::setUp() {
    const testhelpers::TestVariables testVariables;

    const std::string localPathStr = _localTempDir.path().string();

    // Insert api token into keystore
    std::string keychainKey("123");
    KeyChainManager::instance(true);
    KeyChainManager::instance()->writeToken(keychainKey, testVariables.apiToken);

    // Create parmsDb
    bool alreadyExists = false;
    std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExists, true);
    std::filesystem::remove(parmsDbPath);
    ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);

    // Insert user, account, drive & sync
    int userId(12321);
    User user(1, userId, keychainKey);
    ParmsDb::instance()->insertUser(user);

    int accountId(atoi(testVariables.accountId.c_str()));
    Account account(1, accountId, user.dbId());
    ParmsDb::instance()->insertAccount(account);

    int driveDbId = 1;
    int driveId = atoi(testVariables.driveId.c_str());
    Drive drive(driveDbId, driveId, account.dbId(), std::string(), 0, std::string());
    ParmsDb::instance()->insertDrive(drive);

    _sync = Sync(1, drive.dbId(), localPathStr, testVariables.remotePath);
    ParmsDb::instance()->insertSync(_sync);

    // Setup proxy
    Parameters parameters;
    if (bool found = false; ParmsDb::instance()->selectParameters(parameters, found) && found) {
        Proxy::instance(parameters.proxyConfig());
    }

    _syncPal = std::make_shared<SyncPal>(_sync.dbId(), KDRIVE_VERSION_STRING);
    _executorWorker = std::shared_ptr<ExecutorWorker>(new ExecutorWorker(_syncPal, "Executor", "EXEC"));
    _syncPal->syncDb()->setAutoDelete(true);
}

void TestExecutorWorker::tearDown() {
    ParmsDb::instance()->close();
    ParmsDb::reset();
    if (_syncPal && _syncPal->syncDb()) {
        _syncPal->syncDb()->close();
    }
}

void TestExecutorWorker::testCheckLiteSyncInfoForCreate() {
#ifdef __APPLE__
    // Setup dummy values. Test inputs are set in the callbacks defined below.
    const auto opPtr = std::make_shared<SyncOperation>();
    opPtr->setTargetSide(ReplicaSide::Remote);
    const auto node = std::make_shared<Node>(1, ReplicaSide::Local, "test_file.txt", NodeType::File, OperationType::None, "1234",
                                             testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize,
                                             _syncPal->updateTree(ReplicaSide::Local)->rootNode());
    opPtr->setAffectedNode(node);

    // A hydrated placeholder.
    {
        _syncPal->setVfsStatusCallback([]([[maybe_unused]] int syncDbId, [[maybe_unused]] const SyncPath &itemPath,
                                          bool &isPlaceholder, bool &isHydrated, bool &isSyncing, int &progress) -> bool {
            isPlaceholder = true;
            isHydrated = true;
            isSyncing = false;
            progress = 0;
            return true;
        });

        bool isDehydratedPlaceholder = false;
        _executorWorker->checkLiteSyncInfoForCreate(opPtr, "/", isDehydratedPlaceholder);

        CPPUNIT_ASSERT(!isDehydratedPlaceholder);
    }

    // A dehydrated placeholder.
    {
        _syncPal->setVfsStatusCallback([]([[maybe_unused]] int syncDbId, [[maybe_unused]] const SyncPath &itemPath,
                                          bool &isPlaceholder, bool &isHydrated, bool &isSyncing, int &progress) -> bool {
            isPlaceholder = true;
            isHydrated = false;
            isSyncing = false;
            progress = 0;
            return true;
        });

        bool isDehydratedPlaceholder = false;
        _executorWorker->checkLiteSyncInfoForCreate(opPtr, "/", isDehydratedPlaceholder);

        CPPUNIT_ASSERT(isDehydratedPlaceholder);
    }

    // A partially hydrated placeholder (syncing item).
    {
        _syncPal->setVfsStatusCallback([]([[maybe_unused]] int syncDbId, [[maybe_unused]] const SyncPath &itemPath,
                                          bool &isPlaceholder, bool &isHydrated, bool &isSyncing, int &progress) -> bool {
            isPlaceholder = true;
            isHydrated = false;
            isSyncing = true;
            progress = 30;
            return true;
        });

        bool isDehydratedPlaceholder = false;
        _executorWorker->checkLiteSyncInfoForCreate(opPtr, "/", isDehydratedPlaceholder);

        CPPUNIT_ASSERT(!isDehydratedPlaceholder);
    }

    // Not a placeholder.
    {
        _syncPal->setVfsStatusCallback([]([[maybe_unused]] int syncDbId, [[maybe_unused]] const SyncPath &itemPath,
                                          bool &isPlaceholder, bool &isHydrated, bool &isSyncing, int &progress) -> bool {
            isPlaceholder = false;
            isHydrated = false;
            isSyncing = false;
            progress = 0;
            return true;
        });

        bool isDehydratedPlaceholder = false;
        _executorWorker->checkLiteSyncInfoForCreate(opPtr, "/", isDehydratedPlaceholder);

        CPPUNIT_ASSERT(!isDehydratedPlaceholder);
    }
#endif
}

SyncOpPtr TestExecutorWorker::generateSyncOperation(const DbNodeId dbNodeId, const SyncName &filename,
                                                    const OperationType opType) {
    auto node = std::make_shared<Node>(dbNodeId, ReplicaSide::Local, filename, NodeType::File, OperationType::None, "lid",
                                       testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize,
                                       _syncPal->updateTree(ReplicaSide::Local)->rootNode());
    auto correspondingNode = std::make_shared<Node>(
            dbNodeId, ReplicaSide::Remote, filename, NodeType::File, OperationType::None, "rid", testhelpers::defaultTime,
            testhelpers::defaultTime, testhelpers::defaultFileSize, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());

    SyncOpPtr op = std::make_shared<SyncOperation>();
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

    auto node =
            std::make_shared<Node>(dbNodeId, ReplicaSide::Local, "test_file.txt", nodeType, OperationType::None, "local_child_id",
                                   testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, parentNode);


    auto correspondingNode =
            std::make_shared<Node>(dbNodeId, ReplicaSide::Remote, "test_file.txt", nodeType, OperationType::None,
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
            ExecutorWorker(syncPal, name, shortName){};

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
    // False if the item is created on the local replica is not at the root of the synchronisation folder and has no
    // corresponding parent node.
    {
        SyncOpPtr op = generateSyncOperationWithNestedNodes(1, Str("test_file.txt"), OperationType::Create, NodeType::File);
        executorWorkerMock->setCorrespondingNodeInOtherTree({{op->affectedNode()->parentNode(), nullptr}});
        op->setTargetSide(ReplicaSide::Remote);
        CPPUNIT_ASSERT(!executorWorkerMock->isValidDestination(op));
    }

    const auto root = _syncPal->updateTree(ReplicaSide::Remote)->rootNode();

    // False if the item is created on the local replica is not at the root of the synchronisation folder and has a
    // corresponding parent node with no id.
    {
        const auto correspondingParentNode = std::make_shared<Node>(666, ReplicaSide::Remote, "parent_dir", NodeType::Directory,
                                                                    OperationType::None, std::nullopt, testhelpers::defaultTime,
                                                                    testhelpers::defaultTime, testhelpers::defaultFileSize, root);


        SyncOpPtr op = generateSyncOperationWithNestedNodes(1, Str("test_file.txt"), OperationType::Create, NodeType::File);
        executorWorkerMock->setCorrespondingNodeInOtherTree({{op->affectedNode()->parentNode(), correspondingParentNode}});
        op->setTargetSide(ReplicaSide::Remote);
        CPPUNIT_ASSERT(!executorWorkerMock->isValidDestination(op));
    }

    const auto correspondingParentCommonDocsNode = std::make_shared<Node>(
            666, ReplicaSide::Remote, Utility::commonDocumentsFolderName(), NodeType::Directory, OperationType::None,
            "common_docs_id", testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, root);

    // False if the item is created on the local replica is a file and has Common Documents as corresponding parent node.
    {
        SyncOpPtr op = generateSyncOperationWithNestedNodes(1, Str("test_file.txt"), OperationType::Create, NodeType::File);
        op->setTargetSide(ReplicaSide::Remote);
        executorWorkerMock->setCorrespondingNodeInOtherTree(
                {{op->affectedNode()->parentNode(), correspondingParentCommonDocsNode}});
        CPPUNIT_ASSERT(!executorWorkerMock->isValidDestination(op));
    }

    // True if the item is created on the local replica is a directory and has Common Documents as corresponding parent node.
    {
        SyncOpPtr op = generateSyncOperationWithNestedNodes(1, Str("test_dir"), OperationType::Create, NodeType::Directory);
        op->setTargetSide(ReplicaSide::Remote);
        executorWorkerMock->setCorrespondingNodeInOtherTree(
                {{op->affectedNode()->parentNode(), correspondingParentCommonDocsNode}});
        CPPUNIT_ASSERT(executorWorkerMock->isValidDestination(op));
    }
}

void TestExecutorWorker::testLogCorrespondingNodeErrorMsg() {
    SyncOpPtr op = generateSyncOperation(1, Str("test_file.txt"));
    _executorWorker->logCorrespondingNodeErrorMsg(op);

    op->setCorrespondingNode(nullptr);
    _executorWorker->logCorrespondingNodeErrorMsg(op);
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
    CPPUNIT_ASSERT_EQUAL(testhelpers::defaultTime, filestat.modtime);
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
