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
    _syncPal->_syncDb->setAutoDelete(true);

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
    DbNode dbNodeA(0, _syncPal->_syncDb->rootNode().nodeId(), Str("A"), Str("A"), "lA", "rA", tLoc, tLoc, tRemote,
                   NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(dbNodeA, dbNodeIdA, constraintError);
    DbNode dbNodeB(0, _syncPal->_syncDb->rootNode().nodeId(), Str("B"), Str("B"), "lB", "rB", tLoc, tLoc, tRemote,
                   NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(dbNodeB, dbNodeIdB, constraintError);
    DbNode dbNodeAA(0, dbNodeIdA, Str("AA"), Str("AA"), "lAA", "rAA", tLoc, tLoc, tRemote, NodeType::NodeTypeDirectory, 0,
                    std::nullopt);
    _syncPal->_syncDb->insertNode(dbNodeAA, dbNodeIdAA, constraintError);
    DbNode dbNodeAB(0, dbNodeIdA, Str("AB"), Str("AB"), "lAB", "rAB", tLoc, tLoc, tRemote, NodeType::NodeTypeDirectory, 0,
                    std::nullopt);
    _syncPal->_syncDb->insertNode(dbNodeAB, dbNodeIdAB, constraintError);
    DbNode dbNodeBA(0, dbNodeIdB, Str("BA"), Str("BA"), "lBA", "rBA", tLoc, tLoc, tRemote, NodeType::NodeTypeDirectory, 0,
                    std::nullopt);
    _syncPal->_syncDb->insertNode(dbNodeBA, dbNodeIdBA, constraintError);
    DbNode dbNodeAAA(0, dbNodeIdAA, Str("AAA"), Str("AAA"), "lAAA", "rAAA", tLoc, tLoc, tRemote, NodeType::NodeTypeFile, 0,
                     std::nullopt);
    _syncPal->_syncDb->insertNode(dbNodeAAA, dbNodeIdAAA, constraintError);

    // Build update trees
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;
    std::shared_ptr<Node> lNodeA = std::shared_ptr<Node>(new Node(dbNodeIdA, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("A"),
                                                                  NodeTypeDirectory, OperationTypeNone, "lA", createdAt,
                                                                  lastmodified, size, _syncPal->updateTree(ReplicaSideLocal)->rootNode()));
    _syncPal->updateTree(ReplicaSideLocal)->rootNode()->insertChildren(lNodeA);
    _syncPal->updateTree(ReplicaSideLocal)->insertNode(lNodeA);
    std::shared_ptr<Node> lNodeB = std::shared_ptr<Node>(new Node(dbNodeIdB, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("B"),
                                                                  NodeTypeDirectory, OperationTypeNone, "lB", createdAt,
                                                                  lastmodified, size, _syncPal->updateTree(ReplicaSideLocal)->rootNode()));
    _syncPal->updateTree(ReplicaSideLocal)->rootNode()->insertChildren(lNodeB);
    _syncPal->updateTree(ReplicaSideLocal)->insertNode(lNodeB);
    std::shared_ptr<Node> lNodeAA =
        std::shared_ptr<Node>(new Node(dbNodeIdAA, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("AA"), NodeTypeDirectory,
                                       OperationTypeNone, "lAA", createdAt, lastmodified, size, lNodeA));
    lNodeA->insertChildren(lNodeAA);
    _syncPal->updateTree(ReplicaSideLocal)->insertNode(lNodeAA);
    std::shared_ptr<Node> lNodeAB =
        std::shared_ptr<Node>(new Node(dbNodeIdAB, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("AB"), NodeTypeDirectory,
                                       OperationTypeNone, "lAB", createdAt, lastmodified, size, lNodeA));
    lNodeA->insertChildren(lNodeAB);
    _syncPal->updateTree(ReplicaSideLocal)->insertNode(lNodeAB);
    std::shared_ptr<Node> lNodeBA =
        std::shared_ptr<Node>(new Node(dbNodeIdBA, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("BA"), NodeTypeDirectory,
                                       OperationTypeNone, "lBA", createdAt, lastmodified, size, lNodeB));
    lNodeB->insertChildren(lNodeBA);
    _syncPal->updateTree(ReplicaSideLocal)->insertNode(lNodeBA);
    std::shared_ptr<Node> lNodeAAA =
        std::shared_ptr<Node>(new Node(dbNodeIdAAA, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("AAA"), NodeTypeFile,
                                       OperationTypeNone, "lAAA", createdAt, lastmodified, size, lNodeAA));
    lNodeAA->insertChildren(lNodeAAA);
    _syncPal->updateTree(ReplicaSideLocal)->insertNode(lNodeAAA);

    std::shared_ptr<Node> rNodeA = std::shared_ptr<Node>(new Node(dbNodeIdA, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("A"),
                                                                  NodeTypeDirectory, OperationTypeNone, "rA", createdAt,
                                                                  lastmodified, size, _syncPal->updateTree(ReplicaSideRemote)->rootNode()));
    _syncPal->updateTree(ReplicaSideRemote)->rootNode()->insertChildren(rNodeA);
    _syncPal->updateTree(ReplicaSideRemote)->insertNode(rNodeA);
    std::shared_ptr<Node> rNodeB = std::shared_ptr<Node>(new Node(dbNodeIdB, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("B"),
                                                                  NodeTypeDirectory, OperationTypeNone, "rB", createdAt,
                                                                  lastmodified, size, _syncPal->updateTree(ReplicaSideRemote)->rootNode()));
    _syncPal->updateTree(ReplicaSideRemote)->rootNode()->insertChildren(rNodeB);
    _syncPal->updateTree(ReplicaSideRemote)->insertNode(rNodeB);
    std::shared_ptr<Node> rNodeAA =
        std::shared_ptr<Node>(new Node(dbNodeIdAA, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("AA"), NodeTypeDirectory,
                                       OperationTypeNone, "rAA", createdAt, lastmodified, size, rNodeA));
    rNodeA->insertChildren(rNodeAA);
    _syncPal->updateTree(ReplicaSideRemote)->insertNode(rNodeAA);
    std::shared_ptr<Node> rNodeAB =
        std::shared_ptr<Node>(new Node(dbNodeIdAB, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("AB"), NodeTypeDirectory,
                                       OperationTypeNone, "rAB", createdAt, lastmodified, size, rNodeA));
    rNodeA->insertChildren(rNodeAB);
    _syncPal->updateTree(ReplicaSideRemote)->insertNode(rNodeAB);
    std::shared_ptr<Node> rNodeBA =
        std::shared_ptr<Node>(new Node(dbNodeIdBA, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("BA"), NodeTypeDirectory,
                                       OperationTypeNone, "rBA", createdAt, lastmodified, size, rNodeB));
    rNodeB->insertChildren(rNodeBA);
    _syncPal->updateTree(ReplicaSideRemote)->insertNode(rNodeBA);
    std::shared_ptr<Node> rNodeAAA =
        std::shared_ptr<Node>(new Node(dbNodeIdAAA, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("AAA"), NodeTypeFile,
                                       OperationTypeNone, "rAAA", createdAt, lastmodified, size, rNodeAA));
    rNodeAA->insertChildren(rNodeAAA);
    _syncPal->updateTree(ReplicaSideRemote)->insertNode(rNodeAAA);
}

void KDC::TestOperationGeneratorWorker::tearDown() {
    LOGW_DEBUG(_logger, L"$$$$$ Tear Down $$$$$");
    ParmsDb::instance()->close();
    _syncPal->_syncDb->close();
}

void TestOperationGeneratorWorker::testCreateOp() {
    LOGW_DEBUG(_logger, L"$$$$$ TestOperationGeneratorWorker::testCreateOp $$$$$");
    // Simulate file creation on local replica
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;
    std::shared_ptr<Node> lNodeAA = _syncPal->updateTree(ReplicaSideLocal)->getNodeById("lAA");
    std::shared_ptr<Node> lNodeAAB =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("AAB"), NodeTypeFile,
                                       OperationTypeCreate, "lAAB", createdAt, lastmodified, size, lNodeAA));
    lNodeAA->insertChildren(lNodeAAB);
    _syncPal->updateTree(ReplicaSideLocal)->insertNode(lNodeAAB);

    _syncPal->_operationsGeneratorWorker->execute();

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() > 0);

    SyncOpPtr op = _syncPal->_syncOps->getOp(_syncPal->_syncOps->_opSortedList.front());
    CPPUNIT_ASSERT(op->type() == OperationTypeCreate);
    CPPUNIT_ASSERT(op->affectedNode() == lNodeAAB);
    CPPUNIT_ASSERT(op->omit() == false);
}

void TestOperationGeneratorWorker::testCreateOpWithPseudoConflict() {
    LOGW_DEBUG(_logger, L"$$$$$ TestOperationGeneratorWorker::testCreateOpWithPseudoConflict $$$$$");
    // Simulate file creation on both replica
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;

    std::shared_ptr<Node> lNodeAA = _syncPal->updateTree(ReplicaSideLocal)->getNodeById("lAA");
    std::shared_ptr<Node> lNodeAAB =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("AAB"), NodeTypeFile,
                                       OperationTypeCreate, "lAAB", createdAt, lastmodified, size, lNodeAA));
    lNodeAA->insertChildren(lNodeAAB);
    _syncPal->updateTree(ReplicaSideLocal)->insertNode(lNodeAAB);

    std::shared_ptr<Node> rNodeAA = _syncPal->updateTree(ReplicaSideRemote)->getNodeById("rAA");
    std::shared_ptr<Node> rNodeAAB =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("AAB"), NodeTypeFile,
                                       OperationTypeCreate, "rAAB", createdAt, lastmodified, size, rNodeAA));
    rNodeAA->insertChildren(rNodeAAB);
    _syncPal->updateTree(ReplicaSideRemote)->insertNode(rNodeAAB);

    _syncPal->_operationsGeneratorWorker->execute();

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() > 0);
    SyncOpPtr op = _syncPal->_syncOps->getOp(_syncPal->_syncOps->_opSortedList.front());
    CPPUNIT_ASSERT(op->type() == OperationTypeCreate);
    CPPUNIT_ASSERT(op->omit() == true);
}

void TestOperationGeneratorWorker::testMoveOp() {
    LOGW_DEBUG(_logger, L"$$$$$ TestOperationGeneratorWorker::testMoveOp $$$$$");
    // Simulate move of item AA under parent B on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->updateTree(ReplicaSideRemote)->getNodeById("rA");
    std::shared_ptr<Node> rNodeB = _syncPal->updateTree(ReplicaSideRemote)->getNodeById("rB");
    std::shared_ptr<Node> rNodeAA = _syncPal->updateTree(ReplicaSideRemote)->getNodeById("rAA");
    SyncPath rNodeAAPath = rNodeAA->getPath();
    rNodeAA->setParentNode(rNodeB);
    rNodeAA->setChangeEvents(OperationTypeMove);
    rNodeAA->setMoveOriginParentDbId(rNodeA->idb());
    rNodeAA->setMoveOrigin(rNodeAAPath);
    rNodeA->deleteChildren(rNodeAA);
    rNodeB->insertChildren(rNodeAA);

    _syncPal->_operationsGeneratorWorker->execute();

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() > 0);
    SyncOpPtr op = _syncPal->_syncOps->getOp(_syncPal->_syncOps->_opSortedList.front());
    CPPUNIT_ASSERT(op->type() == OperationTypeMove);
    CPPUNIT_ASSERT(op->affectedNode() == rNodeAA);
    CPPUNIT_ASSERT(op->omit() == false);
}

void TestOperationGeneratorWorker::testMoveOpWithPseudoConflict() {
    LOGW_DEBUG(_logger, L"$$$$$ TestOperationGeneratorWorker::testMoveOpWithPseudoConflict $$$$$");
    // Simulate move of item AA under parent B on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->updateTree(ReplicaSideRemote)->getNodeById("rA");
    std::shared_ptr<Node> rNodeB = _syncPal->updateTree(ReplicaSideRemote)->getNodeById("rB");
    std::shared_ptr<Node> rNodeAA = _syncPal->updateTree(ReplicaSideRemote)->getNodeById("rAA");
    rNodeAA->setParentNode(rNodeB);
    rNodeAA->setChangeEvents(OperationTypeMove);
    rNodeAA->setMoveOriginParentDbId(rNodeA->idb());
    rNodeA->deleteChildren(rNodeAA);
    rNodeB->insertChildren(rNodeAA);

    // Simulate move of item AA under parent B on local replica
    std::shared_ptr<Node> lNodeA = _syncPal->updateTree(ReplicaSideLocal)->getNodeById("lA");
    std::shared_ptr<Node> lNodeB = _syncPal->updateTree(ReplicaSideLocal)->getNodeById("lB");
    std::shared_ptr<Node> lNodeAA = _syncPal->updateTree(ReplicaSideLocal)->getNodeById("lAA");
    lNodeAA->setParentNode(lNodeB);
    lNodeAA->setChangeEvents(OperationTypeMove);
    lNodeAA->setMoveOriginParentDbId(lNodeA->idb());
    lNodeA->deleteChildren(lNodeAA);
    lNodeB->insertChildren(lNodeAA);

    _syncPal->_operationsGeneratorWorker->execute();

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() > 0);
    SyncOpPtr op = _syncPal->_syncOps->getOp(_syncPal->_syncOps->_opSortedList.front());
    CPPUNIT_ASSERT(op->type() == OperationTypeMove);
    CPPUNIT_ASSERT(op->omit() == true);
}

void TestOperationGeneratorWorker::testEditOp() {
    LOGW_DEBUG(_logger, L"$$$$$ TestOperationGeneratorWorker::testEditOp $$$$$");
    // Simulate edit of item AAA on the local replica
    std::shared_ptr<Node> lNodeAAA = _syncPal->updateTree(ReplicaSideLocal)->getNodeById("lAA");
    lNodeAAA->setChangeEvents(OperationTypeEdit);

    _syncPal->_operationsGeneratorWorker->execute();

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() > 0);
    SyncOpPtr op = _syncPal->_syncOps->getOp(_syncPal->_syncOps->_opSortedList.front());
    CPPUNIT_ASSERT(op->type() == OperationTypeEdit);
    CPPUNIT_ASSERT(op->affectedNode() == lNodeAAA);
    CPPUNIT_ASSERT(op->omit() == false);
}

void TestOperationGeneratorWorker::testEditOpWithPseudoConflict() {
    LOGW_DEBUG(_logger, L"$$$$$ TestOperationGeneratorWorker::testEditOpWithPseudoConflict $$$$$");
    // Simulate edit of item AAA on the local replica
    std::shared_ptr<Node> lNodeAAA = _syncPal->updateTree(ReplicaSideLocal)->getNodeById("lAAA");
    lNodeAAA->setChangeEvents(OperationTypeEdit);

    // Simulate edit of item AAA on the remote replica
    std::shared_ptr<Node> rNodeAAA = _syncPal->updateTree(ReplicaSideRemote)->getNodeById("rAAA");
    rNodeAAA->setChangeEvents(OperationTypeEdit);

    _syncPal->_operationsGeneratorWorker->execute();

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() > 0);
    SyncOpPtr op = _syncPal->_syncOps->getOp(_syncPal->_syncOps->_opSortedList.front());
    CPPUNIT_ASSERT(op->type() == OperationTypeEdit);
    CPPUNIT_ASSERT(op->omit() == true);
}

void TestOperationGeneratorWorker::testDeleteOp() {
    LOGW_DEBUG(_logger, L"$$$$$ TestOperationGeneratorWorker::testDeleteOp $$$$$");
    // Simulate delete of item A on the local replica
    std::shared_ptr<Node> lNodeA = _syncPal->updateTree(ReplicaSideLocal)->getNodeById("lA");
    std::shared_ptr<Node> lNodeAA = _syncPal->updateTree(ReplicaSideLocal)->getNodeById("lAA");
    std::shared_ptr<Node> lNodeAAA = _syncPal->updateTree(ReplicaSideLocal)->getNodeById("lAAA");
    lNodeA->setChangeEvents(OperationTypeDelete);

    _syncPal->_operationsGeneratorWorker->execute();

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() > 0);
    SyncOpPtr op = _syncPal->_syncOps->getOp(_syncPal->_syncOps->_opSortedList.front());
    CPPUNIT_ASSERT(op->type() == OperationTypeDelete);
    CPPUNIT_ASSERT(op->affectedNode() == lNodeA);
    //    CPPUNIT_ASSERT(op->affectedNodeChildren().find(lNodeAA) != op->affectedNodeChildren().end());
    //    CPPUNIT_ASSERT(op->affectedNodeChildren().find(lNodeAAA) != op->affectedNodeChildren().end());
    CPPUNIT_ASSERT(op->omit() == false);
}

void TestOperationGeneratorWorker::testDeleteOpWithPseudoConflict() {
    LOGW_DEBUG(_logger, L"$$$$$ TestOperationGeneratorWorker::testDeleteOpWithPseudoConflict $$$$$");
    // Simulate delete of item A on the local replica
    std::shared_ptr<Node> lNodeA = _syncPal->updateTree(ReplicaSideLocal)->getNodeById("lA");
    lNodeA->setChangeEvents(OperationTypeDelete);

    // Simulate delete of item A on the remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->updateTree(ReplicaSideRemote)->getNodeById("rA");
    rNodeA->setChangeEvents(OperationTypeDelete);

    _syncPal->_operationsGeneratorWorker->execute();

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() > 0);
    SyncOpPtr op = _syncPal->_syncOps->getOp(_syncPal->_syncOps->_opSortedList.front());
    CPPUNIT_ASSERT(op->type() == OperationTypeDelete);
    CPPUNIT_ASSERT(op->omit() == true);
}

void TestOperationGeneratorWorker::testMoveEditOps() {
    LOGW_DEBUG(_logger, L"$$$$$ TestOperationGeneratorWorker::testMoveEditOps $$$$$");
    // Simulate move and edit of item AAA under parent BA on remote replica
    std::shared_ptr<Node> rNodeAA = _syncPal->updateTree(ReplicaSideRemote)->getNodeById("rAA");
    std::shared_ptr<Node> rNodeBA = _syncPal->updateTree(ReplicaSideRemote)->getNodeById("rBA");
    std::shared_ptr<Node> rNodeAAA = _syncPal->updateTree(ReplicaSideRemote)->getNodeById("rAAA");
    SyncPath rNodeAAAPath = rNodeAAA->getPath();
    rNodeAAA->setParentNode(rNodeBA);
    rNodeAAA->setChangeEvents(OperationTypeMove | OperationTypeEdit);
    rNodeAAA->setMoveOriginParentDbId(rNodeAA->idb());
    rNodeAAA->setMoveOrigin(rNodeAAAPath);
    rNodeAA->deleteChildren(rNodeAA);
    rNodeBA->insertChildren(rNodeAA);

    _syncPal->_operationsGeneratorWorker->execute();

    bool hasMoveOp = false;
    bool hasEditOp = false;
    for (auto &opId : _syncPal->_syncOps->opSortedList()) {
        SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
        if (op->type() == OperationTypeMove) {
            hasMoveOp = true;
        }
        if (op->type() == OperationTypeEdit) {
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
