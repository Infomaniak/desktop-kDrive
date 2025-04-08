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

#include "testoperationgeneratorworker.h"

#include <memory>

#include "test_utility/testhelpers.h"

namespace KDC {

void KDC::TestOperationGeneratorWorker::setUp() {
    TestBase::start();
    _logger = Log::instance()->getLogger();

    LOGW_DEBUG(_logger, L"$$$$$ Set Up $$$$$");

    // Create SyncPal
    bool alreadyExists;
    std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExists, true);
    ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);

    SyncPath syncDbPath = Db::makeDbName(1, 1, 1, 1, alreadyExists);
    std::filesystem::remove(syncDbPath);
    _syncPal = std::make_shared<SyncPal>(std::make_shared<VfsOff>(VfsSetupParams(Log::instance()->getLogger())), syncDbPath,
                                         KDRIVE_VERSION_STRING, true);
    _syncPal->syncDb()->setAutoDelete(true);
    _syncPal->createSharedObjects();

    _syncPal->_operationsGeneratorWorker = std::make_shared<OperationGeneratorWorker>(_syncPal, "Operation Generator", "OPGE");

    /**
     * Initial update tree:
     *
     *            root
     *        _____|_____
     *       |          |
     *       A          B
     *    ___|___       |
     *   |      |       |
     *  AA     AB      BA
     *  |
     * AAA
     */

    // Setup DB
    DbNodeId dbNodeIdA;
    DbNodeId dbNodeIdB;
    DbNodeId dbNodeIdAA;
    DbNodeId dbNodeIdAB;
    DbNodeId dbNodeIdBA;
    DbNodeId dbNodeIdAAA;

    bool constraintError = false;
    DbNode dbNodeA(0, _syncPal->syncDb()->rootNode().nodeId(), Str("A"), Str("A"), "lA", "rA", testhelpers::defaultTime,
                   testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(dbNodeA, dbNodeIdA, constraintError);
    DbNode dbNodeB(0, _syncPal->syncDb()->rootNode().nodeId(), Str("B"), Str("B"), "lB", "rB", testhelpers::defaultTime,
                   testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(dbNodeB, dbNodeIdB, constraintError);
    DbNode dbNodeAA(0, dbNodeIdA, Str("AA"), Str("AA"), "lAA", "rAA", testhelpers::defaultTime, testhelpers::defaultTime,
                    testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(dbNodeAA, dbNodeIdAA, constraintError);
    DbNode dbNodeAB(0, dbNodeIdA, Str("AB"), Str("AB"), "lAB", "rAB", testhelpers::defaultTime, testhelpers::defaultTime,
                    testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(dbNodeAB, dbNodeIdAB, constraintError);
    DbNode dbNodeBA(0, dbNodeIdB, Str("BA"), Str("BA"), "lBA", "rBA", testhelpers::defaultTime, testhelpers::defaultTime,
                    testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(dbNodeBA, dbNodeIdBA, constraintError);
    DbNode dbNodeAAA(0, dbNodeIdAA, Str("AAA"), Str("AAA"), "lAAA", "rAAA", testhelpers::defaultTime, testhelpers::defaultTime,
                     testhelpers::defaultTime, NodeType::File, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(dbNodeAAA, dbNodeIdAAA, constraintError);

    // Build update trees
    std::shared_ptr<Node> lNodeA =
            std::make_shared<Node>(dbNodeIdA, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("A"), NodeType::Directory,
                                   OperationType::None, "lA", testhelpers::defaultTime, testhelpers::defaultTime,
                                   testhelpers::defaultFileSize, _syncPal->updateTree(ReplicaSide::Local)->rootNode());
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->insertChildren(lNodeA));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(lNodeA);
    std::shared_ptr<Node> lNodeB =
            std::make_shared<Node>(dbNodeIdB, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("B"), NodeType::Directory,
                                   OperationType::None, "lB", testhelpers::defaultTime, testhelpers::defaultTime,
                                   testhelpers::defaultFileSize, _syncPal->updateTree(ReplicaSide::Local)->rootNode());
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->insertChildren(lNodeB));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(lNodeB);
    std::shared_ptr<Node> lNodeAA = std::make_shared<Node>(
            dbNodeIdAA, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("AA"), NodeType::Directory, OperationType::None,
            "lAA", testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, lNodeA);
    CPPUNIT_ASSERT(lNodeA->insertChildren(lNodeAA));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(lNodeAA);
    std::shared_ptr<Node> lNodeAB = std::make_shared<Node>(
            dbNodeIdAB, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("AB"), NodeType::Directory, OperationType::None,
            "lAB", testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, lNodeA);
    CPPUNIT_ASSERT(lNodeA->insertChildren(lNodeAB));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(lNodeAB);
    std::shared_ptr<Node> lNodeBA = std::make_shared<Node>(
            dbNodeIdBA, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("BA"), NodeType::Directory, OperationType::None,
            "lBA", testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, lNodeB);
    CPPUNIT_ASSERT(lNodeB->insertChildren(lNodeBA));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(lNodeBA);
    std::shared_ptr<Node> lNodeAAA = std::make_shared<Node>(
            dbNodeIdAAA, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("AAA"), NodeType::File, OperationType::None,
            "lAAA", testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, lNodeAA);
    CPPUNIT_ASSERT(lNodeAA->insertChildren(lNodeAAA));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(lNodeAAA);

    std::shared_ptr<Node> rNodeA =
            std::make_shared<Node>(dbNodeIdA, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("A"), NodeType::Directory,
                                   OperationType::None, "rA", testhelpers::defaultTime, testhelpers::defaultTime,
                                   testhelpers::defaultFileSize, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNodeA));
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeA);
    std::shared_ptr<Node> rNodeB =
            std::make_shared<Node>(dbNodeIdB, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("B"), NodeType::Directory,
                                   OperationType::None, "rB", testhelpers::defaultTime, testhelpers::defaultTime,
                                   testhelpers::defaultFileSize, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNodeB));
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeB);
    std::shared_ptr<Node> rNodeAA = std::make_shared<Node>(
            dbNodeIdAA, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("AA"), NodeType::Directory, OperationType::None,
            "rAA", testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, rNodeA);
    CPPUNIT_ASSERT(rNodeA->insertChildren(rNodeAA));
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeAA);
    std::shared_ptr<Node> rNodeAB = std::make_shared<Node>(
            dbNodeIdAB, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("AB"), NodeType::Directory, OperationType::None,
            "rAB", testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, rNodeA);
    CPPUNIT_ASSERT(rNodeA->insertChildren(rNodeAB));
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeAB);
    std::shared_ptr<Node> rNodeBA = std::make_shared<Node>(
            dbNodeIdBA, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("BA"), NodeType::Directory, OperationType::None,
            "rBA", testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, rNodeB);
    CPPUNIT_ASSERT(rNodeB->insertChildren(rNodeBA));
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeBA);
    std::shared_ptr<Node> rNodeAAA = std::make_shared<Node>(
            dbNodeIdAAA, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("AAA"), NodeType::File, OperationType::None,
            "rAAA", testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, rNodeAA);
    CPPUNIT_ASSERT(rNodeAA->insertChildren(rNodeAAA));
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeAAA);
}

void KDC::TestOperationGeneratorWorker::tearDown() {
    LOGW_DEBUG(_logger, L"$$$$$ Tear Down $$$$$");

    ParmsDb::instance()->close();
    ParmsDb::reset();
    if (_syncPal && _syncPal->syncDb()) {
        _syncPal->syncDb()->close();
    }
    TestBase::stop();
}

void TestOperationGeneratorWorker::testCreateOp() {
    LOGW_DEBUG(_logger, L"$$$$$ TestOperationGeneratorWorker::testCreateOp $$$$$");
    // Simulate file creation on local replica
    std::shared_ptr<Node> lNodeAA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAA");
    std::shared_ptr<Node> lNodeAAB = std::make_shared<Node>(
            std::nullopt, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("AAB"), NodeType::File, OperationType::Create,
            "lAAB", testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, lNodeAA);
    CPPUNIT_ASSERT(lNodeAA->insertChildren(lNodeAAB));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(lNodeAAB);

    _syncPal->_operationsGeneratorWorker->execute();

    CPPUNIT_ASSERT(!_syncPal->syncOps()->isEmpty());

    SyncOpPtr op = _syncPal->syncOps()->getOp(_syncPal->syncOps()->_opSortedList.front());
    CPPUNIT_ASSERT(op->type() == OperationType::Create);
    CPPUNIT_ASSERT(op->affectedNode() == lNodeAAB);
    CPPUNIT_ASSERT(op->omit() == false);
}

void TestOperationGeneratorWorker::testCreateOpWithPseudoConflict() {
    LOGW_DEBUG(_logger, L"$$$$$ TestOperationGeneratorWorker::testCreateOpWithPseudoConflict $$$$$");
    // Simulate file creation on both replica
    std::shared_ptr<Node> lNodeAA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAA");
    std::shared_ptr<Node> lNodeAAB = std::make_shared<Node>(
            std::nullopt, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("AAB"), NodeType::File, OperationType::Create,
            "lAAB", testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, lNodeAA);
    CPPUNIT_ASSERT(lNodeAA->insertChildren(lNodeAAB));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(lNodeAAB);

    std::shared_ptr<Node> rNodeAA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rAA");
    std::shared_ptr<Node> rNodeAAB = std::make_shared<Node>(
            std::nullopt, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("AAB"), NodeType::File, OperationType::Create,
            "rAAB", testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, rNodeAA);
    CPPUNIT_ASSERT(rNodeAA->insertChildren(rNodeAAB));
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeAAB);

    _syncPal->_operationsGeneratorWorker->execute();

    CPPUNIT_ASSERT(!_syncPal->syncOps()->isEmpty());
    SyncOpPtr op = _syncPal->syncOps()->getOp(_syncPal->syncOps()->_opSortedList.front());
    CPPUNIT_ASSERT(op->type() == OperationType::Create);
    CPPUNIT_ASSERT(op->omit() == true);
}

void TestOperationGeneratorWorker::testMoveOp() {
    LOGW_DEBUG(_logger, L"$$$$$ TestOperationGeneratorWorker::testMoveOp $$$$$");
    // Simulate move of item AA under parent B on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rA");
    std::shared_ptr<Node> rNodeB = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rB");
    std::shared_ptr<Node> rNodeAA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rAA");
    SyncPath rNodeAAPath = rNodeAA->getPath();
    CPPUNIT_ASSERT(rNodeAA->setParentNode(rNodeB));
    rNodeAA->setChangeEvents(OperationType::Move);
    rNodeAA->setMoveOriginParentDbId(rNodeA->idb());
    rNodeAA->setMoveOrigin(rNodeAAPath);
    rNodeA->deleteChildren(rNodeAA);
    CPPUNIT_ASSERT(rNodeB->insertChildren(rNodeAA));

    _syncPal->_operationsGeneratorWorker->execute();

    CPPUNIT_ASSERT(!_syncPal->syncOps()->isEmpty());
    SyncOpPtr op = _syncPal->syncOps()->getOp(_syncPal->syncOps()->_opSortedList.front());
    CPPUNIT_ASSERT(op->type() == OperationType::Move);
    CPPUNIT_ASSERT(op->affectedNode() == rNodeAA);
    CPPUNIT_ASSERT(op->omit() == false);
}

void TestOperationGeneratorWorker::testMoveOpWithPseudoConflict() {
    LOGW_DEBUG(_logger, L"$$$$$ TestOperationGeneratorWorker::testMoveOpWithPseudoConflict $$$$$");
    // Simulate move of item AA under parent B on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rA");
    std::shared_ptr<Node> rNodeB = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rB");
    std::shared_ptr<Node> rNodeAA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rAA");
    CPPUNIT_ASSERT(rNodeAA->setParentNode(rNodeB));
    rNodeAA->setChangeEvents(OperationType::Move);
    rNodeAA->setMoveOriginParentDbId(rNodeA->idb());
    rNodeA->deleteChildren(rNodeAA);
    CPPUNIT_ASSERT(rNodeB->insertChildren(rNodeAA));

    // Simulate move of item AA under parent B on local replica
    std::shared_ptr<Node> lNodeA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lA");
    std::shared_ptr<Node> lNodeB = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lB");
    std::shared_ptr<Node> lNodeAA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAA");
    CPPUNIT_ASSERT(lNodeAA->setParentNode(lNodeB));
    lNodeAA->setChangeEvents(OperationType::Move);
    lNodeAA->setMoveOriginParentDbId(lNodeA->idb());
    lNodeA->deleteChildren(lNodeAA);
    CPPUNIT_ASSERT(lNodeB->insertChildren(lNodeAA));

    _syncPal->_operationsGeneratorWorker->execute();

    CPPUNIT_ASSERT(!_syncPal->syncOps()->isEmpty());
    SyncOpPtr op = _syncPal->syncOps()->getOp(_syncPal->syncOps()->_opSortedList.front());
    CPPUNIT_ASSERT(op->type() == OperationType::Move);
    CPPUNIT_ASSERT(op->omit() == true);
}

void TestOperationGeneratorWorker::testMoveOpWithPseudoConflictButDifferentEncoding() {
    LOGW_DEBUG(_logger, L"$$$$$ TestOperationGeneratorWorker::testMoveOpWithPseudoConflictButDifferentEncoding $$$$$");
    // Simulate move of item AA under parent B on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rA");
    std::shared_ptr<Node> rNodeB = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rB");
    std::shared_ptr<Node> rNodeAA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rAA");
    CPPUNIT_ASSERT(rNodeAA->setParentNode(rNodeB));
    rNodeAA->setChangeEvents(OperationType::Move);
    rNodeAA->setMoveOriginParentDbId(rNodeA->idb());
    rNodeAA->setName(Str("testé.txt")); // Name with NFC encoding
    rNodeA->deleteChildren(rNodeAA);
    CPPUNIT_ASSERT(rNodeB->insertChildren(rNodeAA));

    // Simulate move of item AA under parent B on local replica
    std::shared_ptr<Node> lNodeA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lA");
    std::shared_ptr<Node> lNodeB = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lB");
    std::shared_ptr<Node> lNodeAA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAA");
    CPPUNIT_ASSERT(lNodeAA->setParentNode(lNodeB));
    lNodeAA->setChangeEvents(OperationType::Move);
    lNodeAA->setMoveOriginParentDbId(lNodeA->idb());
    lNodeAA->setName(Str("testé.txt")); // Name with NFD encoding
    lNodeA->deleteChildren(lNodeAA);
    CPPUNIT_ASSERT(lNodeB->insertChildren(lNodeAA));

    _syncPal->_operationsGeneratorWorker->execute();

    CPPUNIT_ASSERT(!_syncPal->syncOps()->isEmpty());
    SyncOpPtr op = _syncPal->syncOps()->getOp(_syncPal->syncOps()->_opSortedList.front());
    CPPUNIT_ASSERT(op->type() == OperationType::Move);
    CPPUNIT_ASSERT(op->omit() == true);
}

void TestOperationGeneratorWorker::testEditOp() {
    LOGW_DEBUG(_logger, L"$$$$$ TestOperationGeneratorWorker::testEditOp $$$$$");
    // Simulate edit of item AAA on the local replica
    std::shared_ptr<Node> lNodeAAA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAA");
    lNodeAAA->setChangeEvents(OperationType::Edit);

    _syncPal->_operationsGeneratorWorker->execute();

    CPPUNIT_ASSERT(!_syncPal->syncOps()->isEmpty());
    SyncOpPtr op = _syncPal->syncOps()->getOp(_syncPal->syncOps()->_opSortedList.front());
    CPPUNIT_ASSERT(op->type() == OperationType::Edit);
    CPPUNIT_ASSERT(op->affectedNode() == lNodeAAA);
    CPPUNIT_ASSERT(op->omit() == false);
}

void TestOperationGeneratorWorker::testEditOpWithPseudoConflict() {
    LOGW_DEBUG(_logger, L"$$$$$ TestOperationGeneratorWorker::testEditOpWithPseudoConflict $$$$$");
    // Simulate edit of item AAA on the local replica
    std::shared_ptr<Node> lNodeAAA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAAA");
    lNodeAAA->setChangeEvents(OperationType::Edit);

    // Simulate edit of item AAA on the remote replica
    std::shared_ptr<Node> rNodeAAA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rAAA");
    rNodeAAA->setChangeEvents(OperationType::Edit);

    _syncPal->_operationsGeneratorWorker->execute();

    CPPUNIT_ASSERT(!_syncPal->syncOps()->isEmpty());
    SyncOpPtr op = _syncPal->syncOps()->getOp(_syncPal->syncOps()->_opSortedList.front());
    CPPUNIT_ASSERT(op->type() == OperationType::Edit);
    CPPUNIT_ASSERT(op->omit() == true);
}

void TestOperationGeneratorWorker::testDeleteOp() {
    LOGW_DEBUG(_logger, L"$$$$$ TestOperationGeneratorWorker::testDeleteOp $$$$$");
    // Simulate delete of item A on the local replica
    std::shared_ptr<Node> lNodeA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lA");
    std::shared_ptr<Node> lNodeAA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAA");
    std::shared_ptr<Node> lNodeAAA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAAA");
    lNodeA->setChangeEvents(OperationType::Delete);

    _syncPal->_operationsGeneratorWorker->execute();

    CPPUNIT_ASSERT(!_syncPal->syncOps()->isEmpty());
    SyncOpPtr op = _syncPal->syncOps()->getOp(_syncPal->syncOps()->_opSortedList.front());
    CPPUNIT_ASSERT(op->type() == OperationType::Delete);
    CPPUNIT_ASSERT(op->affectedNode() == lNodeA);
    CPPUNIT_ASSERT(op->omit() == false);
}

void TestOperationGeneratorWorker::testDeleteOpWithPseudoConflict() {
    LOGW_DEBUG(_logger, L"$$$$$ TestOperationGeneratorWorker::testDeleteOpWithPseudoConflict $$$$$");
    // Simulate delete of item A on the local replica
    std::shared_ptr<Node> lNodeA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lA");
    lNodeA->setChangeEvents(OperationType::Delete);

    // Simulate delete of item A on the remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rA");
    rNodeA->setChangeEvents(OperationType::Delete);

    _syncPal->_operationsGeneratorWorker->execute();

    CPPUNIT_ASSERT(!_syncPal->syncOps()->isEmpty());
    SyncOpPtr op = _syncPal->syncOps()->getOp(_syncPal->syncOps()->_opSortedList.front());
    CPPUNIT_ASSERT(op->type() == OperationType::Delete);
    CPPUNIT_ASSERT(op->omit() == true);
}

void TestOperationGeneratorWorker::testMoveEditOps() {
    LOGW_DEBUG(_logger, L"$$$$$ TestOperationGeneratorWorker::testMoveEditOps $$$$$");
    // Simulate move and edit of item AAA under parent BA on remote replica
    std::shared_ptr<Node> rNodeAA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rAA");
    std::shared_ptr<Node> rNodeBA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rBA");
    std::shared_ptr<Node> rNodeAAA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rAAA");
    SyncPath rNodeAAAPath = rNodeAAA->getPath();
    CPPUNIT_ASSERT(rNodeAAA->setParentNode(rNodeBA));
    rNodeAAA->setChangeEvents(OperationType::Move | OperationType::Edit);
    rNodeAAA->setMoveOriginParentDbId(rNodeAA->idb());
    rNodeAAA->setMoveOrigin(rNodeAAAPath);
    rNodeAA->deleteChildren(rNodeAA);
    CPPUNIT_ASSERT(rNodeBA->insertChildren(rNodeAA));

    _syncPal->_operationsGeneratorWorker->execute();

    bool hasMoveOp = false;
    bool hasEditOp = false;
    for (auto &opId: _syncPal->syncOps()->opSortedList()) {
        SyncOpPtr op = _syncPal->syncOps()->getOp(opId);
        if (op->type() == OperationType::Move) {
            hasMoveOp = true;
        }
        if (op->type() == OperationType::Edit) {
            hasEditOp = true;
        }
    }

    CPPUNIT_ASSERT(_syncPal->syncOps()->size() == 2);
    CPPUNIT_ASSERT(hasMoveOp);
    CPPUNIT_ASSERT(hasEditOp);
    UniqueId opId = _syncPal->syncOps()->_opSortedList.front();
    SyncOpPtr op = _syncPal->syncOps()->getOp(opId);
    CPPUNIT_ASSERT(op->omit() == false);
}

} // namespace KDC
