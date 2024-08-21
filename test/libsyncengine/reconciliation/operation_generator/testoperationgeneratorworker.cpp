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

#include "testoperationgeneratorworker.h"

namespace KDC {

void KDC::TestOperationGeneratorWorker::setUp() {
    _logger = Log::instance()->getLogger();

    LOGW_DEBUG(_logger, L"$$$$$ Set Up $$$$$");

    // Create SyncPal
    bool alreadyExists;
    std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExists);
    std::filesystem::remove(parmsDbPath);
    ParmsDb::instance(parmsDbPath, "3.4.0", true, true);
    ParmsDb::instance()->setAutoDelete(true);

    SyncPath syncDbPath = Db::makeDbName(1, 1, 1, 1, alreadyExists);
    std::filesystem::remove(syncDbPath);
    _syncPal = std::shared_ptr<SyncPal>(new SyncPal(syncDbPath, "3.4.0", true));
    _syncPal->syncDb()->setAutoDelete(true);

    _syncPal->_operationsGeneratorWorker =
        std::shared_ptr<OperationGeneratorWorker>(new OperationGeneratorWorker(_syncPal, "Operation Generator", "OPGE"));

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
    time_t tLoc = std::time(0);
    time_t tRemote = std::time(0);
    DbNodeId dbNodeIdA;
    DbNodeId dbNodeIdB;
    DbNodeId dbNodeIdAA;
    DbNodeId dbNodeIdAB;
    DbNodeId dbNodeIdBA;
    DbNodeId dbNodeIdAAA;

    bool constraintError = false;
    DbNode dbNodeA(0, _syncPal->syncDb()->rootNode().nodeId(), Str("A"), Str("A"), "lA", "rA", tLoc, tLoc, tRemote,
                   NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(dbNodeA, dbNodeIdA, constraintError);
    DbNode dbNodeB(0, _syncPal->syncDb()->rootNode().nodeId(), Str("B"), Str("B"), "lB", "rB", tLoc, tLoc, tRemote,
                   NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(dbNodeB, dbNodeIdB, constraintError);
    DbNode dbNodeAA(0, dbNodeIdA, Str("AA"), Str("AA"), "lAA", "rAA", tLoc, tLoc, tRemote, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(dbNodeAA, dbNodeIdAA, constraintError);
    DbNode dbNodeAB(0, dbNodeIdA, Str("AB"), Str("AB"), "lAB", "rAB", tLoc, tLoc, tRemote, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(dbNodeAB, dbNodeIdAB, constraintError);
    DbNode dbNodeBA(0, dbNodeIdB, Str("BA"), Str("BA"), "lBA", "rBA", tLoc, tLoc, tRemote, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(dbNodeBA, dbNodeIdBA, constraintError);
    DbNode dbNodeAAA(0, dbNodeIdAA, Str("AAA"), Str("AAA"), "lAAA", "rAAA", tLoc, tLoc, tRemote, NodeType::File, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(dbNodeAAA, dbNodeIdAAA, constraintError);

    // Build update trees
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;
    std::shared_ptr<Node> lNodeA = std::shared_ptr<Node>(
        new Node(dbNodeIdA, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("A"), NodeType::Directory, OperationType::None,
                 "lA", createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Local)->rootNode()));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->insertChildren(lNodeA));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(lNodeA);
    std::shared_ptr<Node> lNodeB = std::shared_ptr<Node>(
        new Node(dbNodeIdB, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("B"), NodeType::Directory, OperationType::None,
                 "lB", createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Local)->rootNode()));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->insertChildren(lNodeB));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(lNodeB);
    std::shared_ptr<Node> lNodeAA =
        std::shared_ptr<Node>(new Node(dbNodeIdAA, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("AA"),
                                       NodeType::Directory, OperationType::None, "lAA", createdAt, lastmodified, size, lNodeA));
    CPPUNIT_ASSERT(lNodeA->insertChildren(lNodeAA));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(lNodeAA);
    std::shared_ptr<Node> lNodeAB =
        std::shared_ptr<Node>(new Node(dbNodeIdAB, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("AB"),
                                       NodeType::Directory, OperationType::None, "lAB", createdAt, lastmodified, size, lNodeA));
    CPPUNIT_ASSERT(lNodeA->insertChildren(lNodeAB));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(lNodeAB);
    std::shared_ptr<Node> lNodeBA =
        std::shared_ptr<Node>(new Node(dbNodeIdBA, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("BA"),
                                       NodeType::Directory, OperationType::None, "lBA", createdAt, lastmodified, size, lNodeB));
    CPPUNIT_ASSERT(lNodeB->insertChildren(lNodeBA));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(lNodeBA);
    std::shared_ptr<Node> lNodeAAA =
        std::shared_ptr<Node>(new Node(dbNodeIdAAA, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("AAA"), NodeType::File,
                                       OperationType::None, "lAAA", createdAt, lastmodified, size, lNodeAA));
    CPPUNIT_ASSERT(lNodeAA->insertChildren(lNodeAAA));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(lNodeAAA);

    std::shared_ptr<Node> rNodeA = std::shared_ptr<Node>(
        new Node(dbNodeIdA, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("A"), NodeType::Directory, OperationType::None,
                 "rA", createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Remote)->rootNode()));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNodeA));
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeA);
    std::shared_ptr<Node> rNodeB = std::shared_ptr<Node>(
        new Node(dbNodeIdB, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("B"), NodeType::Directory, OperationType::None,
                 "rB", createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Remote)->rootNode()));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNodeB));
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeB);
    std::shared_ptr<Node> rNodeAA =
        std::shared_ptr<Node>(new Node(dbNodeIdAA, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("AA"),
                                       NodeType::Directory, OperationType::None, "rAA", createdAt, lastmodified, size, rNodeA));
    CPPUNIT_ASSERT(rNodeA->insertChildren(rNodeAA));
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeAA);
    std::shared_ptr<Node> rNodeAB =
        std::shared_ptr<Node>(new Node(dbNodeIdAB, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("AB"),
                                       NodeType::Directory, OperationType::None, "rAB", createdAt, lastmodified, size, rNodeA));
    CPPUNIT_ASSERT(rNodeA->insertChildren(rNodeAB));
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeAB);
    std::shared_ptr<Node> rNodeBA =
        std::shared_ptr<Node>(new Node(dbNodeIdBA, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("BA"),
                                       NodeType::Directory, OperationType::None, "rBA", createdAt, lastmodified, size, rNodeB));
    CPPUNIT_ASSERT(rNodeB->insertChildren(rNodeBA));
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeBA);
    std::shared_ptr<Node> rNodeAAA =
        std::shared_ptr<Node>(new Node(dbNodeIdAAA, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("AAA"), NodeType::File,
                                       OperationType::None, "rAAA", createdAt, lastmodified, size, rNodeAA));
    CPPUNIT_ASSERT(rNodeAA->insertChildren(rNodeAAA));
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeAAA);
}

void KDC::TestOperationGeneratorWorker::tearDown() {
    LOGW_DEBUG(_logger, L"$$$$$ Tear Down $$$$$");
    ParmsDb::instance()->close();
    _syncPal->syncDb()->close();
}

void TestOperationGeneratorWorker::testCreateOp() {
    LOGW_DEBUG(_logger, L"$$$$$ TestOperationGeneratorWorker::testCreateOp $$$$$");
    // Simulate file creation on local replica
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;
    std::shared_ptr<Node> lNodeAA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAA");
    std::shared_ptr<Node> lNodeAAB =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("AAB"), NodeType::File,
                                       OperationType::Create, "lAAB", createdAt, lastmodified, size, lNodeAA));
    CPPUNIT_ASSERT(lNodeAA->insertChildren(lNodeAAB));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(lNodeAAB);

    _syncPal->_operationsGeneratorWorker->execute();

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() > 0);

    SyncOpPtr op = _syncPal->_syncOps->getOp(_syncPal->_syncOps->_opSortedList.front());
    CPPUNIT_ASSERT(op->type() == OperationType::Create);
    CPPUNIT_ASSERT(op->affectedNode() == lNodeAAB);
    CPPUNIT_ASSERT(op->omit() == false);
}

void TestOperationGeneratorWorker::testCreateOpWithPseudoConflict() {
    LOGW_DEBUG(_logger, L"$$$$$ TestOperationGeneratorWorker::testCreateOpWithPseudoConflict $$$$$");
    // Simulate file creation on both replica
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;

    std::shared_ptr<Node> lNodeAA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAA");
    std::shared_ptr<Node> lNodeAAB =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("AAB"), NodeType::File,
                                       OperationType::Create, "lAAB", createdAt, lastmodified, size, lNodeAA));
    CPPUNIT_ASSERT(lNodeAA->insertChildren(lNodeAAB));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(lNodeAAB);

    std::shared_ptr<Node> rNodeAA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rAA");
    std::shared_ptr<Node> rNodeAAB =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("AAB"),
                                       NodeType::File, OperationType::Create, "rAAB", createdAt, lastmodified, size, rNodeAA));
    CPPUNIT_ASSERT(rNodeAA->insertChildren(rNodeAAB));
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeAAB);

    _syncPal->_operationsGeneratorWorker->execute();

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() > 0);
    SyncOpPtr op = _syncPal->_syncOps->getOp(_syncPal->_syncOps->_opSortedList.front());
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

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() > 0);
    SyncOpPtr op = _syncPal->_syncOps->getOp(_syncPal->_syncOps->_opSortedList.front());
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

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() > 0);
    SyncOpPtr op = _syncPal->_syncOps->getOp(_syncPal->_syncOps->_opSortedList.front());
    CPPUNIT_ASSERT(op->type() == OperationType::Move);
    CPPUNIT_ASSERT(op->omit() == true);
}

void TestOperationGeneratorWorker::testEditOp() {
    LOGW_DEBUG(_logger, L"$$$$$ TestOperationGeneratorWorker::testEditOp $$$$$");
    // Simulate edit of item AAA on the local replica
    std::shared_ptr<Node> lNodeAAA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAA");
    lNodeAAA->setChangeEvents(OperationType::Edit);

    _syncPal->_operationsGeneratorWorker->execute();

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() > 0);
    SyncOpPtr op = _syncPal->_syncOps->getOp(_syncPal->_syncOps->_opSortedList.front());
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

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() > 0);
    SyncOpPtr op = _syncPal->_syncOps->getOp(_syncPal->_syncOps->_opSortedList.front());
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

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() > 0);
    SyncOpPtr op = _syncPal->_syncOps->getOp(_syncPal->_syncOps->_opSortedList.front());
    CPPUNIT_ASSERT(op->type() == OperationType::Delete);
    CPPUNIT_ASSERT(op->affectedNode() == lNodeA);
    //    CPPUNIT_ASSERT(op->affectedNodeChildren().find(lNodeAA) != op->affectedNodeChildren().end());
    //    CPPUNIT_ASSERT(op->affectedNodeChildren().find(lNodeAAA) != op->affectedNodeChildren().end());
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

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() > 0);
    SyncOpPtr op = _syncPal->_syncOps->getOp(_syncPal->_syncOps->_opSortedList.front());
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
    for (auto &opId : _syncPal->_syncOps->opSortedList()) {
        SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
        if (op->type() == OperationType::Move) {
            hasMoveOp = true;
        }
        if (op->type() == OperationType::Edit) {
            hasEditOp = true;
        }
    }

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() == 2);
    CPPUNIT_ASSERT(hasMoveOp);
    CPPUNIT_ASSERT(hasEditOp);
    UniqueId opId = _syncPal->_syncOps->_opSortedList.front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(op->omit() == false);
}

}  // namespace KDC
