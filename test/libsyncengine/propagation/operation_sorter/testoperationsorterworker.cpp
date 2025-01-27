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

#include "testoperationsorterworker.h"
#include "test_utility/testhelpers.h"

#include <memory>

using namespace CppUnit;

namespace KDC {

void TestOperationSorterWorker::setUp() {
    // Create SyncPal
    bool alreadyExists = false;
    const auto parmsDbPath = Db::makeDbName(alreadyExists, true);
    ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);

    SyncPath syncDbPath = Db::makeDbName(1, 1, 1, 1, alreadyExists);
    std::filesystem::remove(syncDbPath);
    _syncPal = std::make_shared<SyncPal>(syncDbPath, KDRIVE_VERSION_STRING, true);
    _syncPal->syncDb()->setAutoDelete(true);
    _syncPal->createSharedObjects();

    _syncPal->_operationsSorterWorker = std::make_shared<OperationSorterWorker>(_syncPal, "Operation Sorter", "OPSO");
}

void TestOperationSorterWorker::tearDown() {
    ParmsDb::instance()->close();
    ParmsDb::reset();
    _syncPal->syncDb()->close();
}

void TestOperationSorterWorker::testMoveFirstAfterSecond() {
    const auto nodeA =
            std::make_shared<Node>(ReplicaSide::Local, Str("A"), NodeType::Directory, OperationType::None, std::nullopt,
                                   testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, nullptr);
    const auto nodeB =
            std::make_shared<Node>(ReplicaSide::Local, Str("B"), NodeType::Directory, OperationType::None, std::nullopt,
                                   testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, nullptr);
    const auto nodeC =
            std::make_shared<Node>(ReplicaSide::Local, Str("C"), NodeType::Directory, OperationType::None, std::nullopt,
                                   testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, nullptr);
    const auto op1 = std::make_shared<SyncOperation>();
    const auto op2 = std::make_shared<SyncOperation>();
    const auto op3 = std::make_shared<SyncOperation>();
    op1->setAffectedNode(nodeA);
    op2->setAffectedNode(nodeB);
    op3->setAffectedNode(nodeC);
    _syncPal->_syncOps->pushOp(op1);
    _syncPal->_syncOps->pushOp(op2);
    _syncPal->_syncOps->pushOp(op3);

    // Move op2 after op1 -> nothing happens, op2 is already after op1.
    _syncPal->_operationsSorterWorker->moveFirstAfterSecond(op2, op1);
    CPPUNIT_ASSERT_EQUAL(op1->id(), _syncPal->_syncOps->opSortedList().front());
    CPPUNIT_ASSERT_EQUAL(false, _syncPal->_operationsSorterWorker->hasOrderChanged());

    // Move op1 after op2.
    _syncPal->_operationsSorterWorker->moveFirstAfterSecond(op1, op2);
    CPPUNIT_ASSERT_EQUAL(op2->id(), _syncPal->_syncOps->opSortedList().front());
    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    CPPUNIT_ASSERT_EQUAL(op3->id(), _syncPal->_syncOps->opSortedList().back());
}

void TestOperationSorterWorker::testFixDeleteBeforeMove() {
    const DbNode nodeDir1(0, _syncPal->syncDb()->rootNode().nodeId(), Str("A"), Str("A"), "la", "ra", testhelpers::defaultTime,
                          testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, testhelpers::defaultFileSize);
    _syncPal->syncDb()->insertNode(nodeDir1);
    const DbNode nodeDir11(0, _syncPal->syncDb()->rootNode().nodeId(), Str("B"), Str("B"), "lb", "rb", testhelpers::defaultTime,
                           testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, testhelpers::defaultFileSize);
    _syncPal->syncDb()->insertNode(nodeDir11);


    const auto rootNode = _syncPal->updateTree(ReplicaSide::Local)->rootNode();
    const auto nodeA =
            std::make_shared<Node>(ReplicaSide::Local, Str("A"), NodeType::Directory, OperationType::None, Str("la"),
                                   testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, rootNode);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeA);
    (void) rootNode->insertChildren(nodeA);

    const auto nodeB =
            std::make_shared<Node>(ReplicaSide::Local, Str("B"), NodeType::Directory, OperationType::None, Str("lb"),
                                   testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, rootNode);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeB);
    (void) rootNode->insertChildren(nodeB);

    // Delete node A
    nodeA->insertChangeEvent(OperationType::Delete);
    const auto deleteOp = std::make_shared<SyncOperation>();
    deleteOp->setType(OperationType::Delete);
    deleteOp->setAffectedNode(nodeA);
    deleteOp->setTargetSide(ReplicaSide::Remote);

    // Move B into A
    nodeB->insertChangeEvent(OperationType::Move);
    nodeB->setParentNode(nodeA);
    rootNode->deleteChildren(nodeB);
    (void) nodeA->insertChildren(nodeB);
    const auto moveOp = std::make_shared<SyncOperation>();
    moveOp->setType(OperationType::Move);
    moveOp->setAffectedNode(nodeB);
    moveOp->setNewParentNode(nodeA);
    moveOp->setTargetSide(ReplicaSide::Remote);

    _syncPal->_syncOps->pushOp(deleteOp);
    _syncPal->_syncOps->pushOp(moveOp);
    _syncPal->_operationsSorterWorker->_unsortedList = *_syncPal->_syncOps;
    _syncPal->_operationsSorterWorker->fixDeleteBeforeMove();

    CPPUNIT_ASSERT_EQUAL(moveOp->id(), _syncPal->_syncOps->opSortedList().front());
    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    CPPUNIT_ASSERT_EQUAL(deleteOp->id(), _syncPal->_syncOps->opSortedList().back());
}

void TestOperationSorterWorker::testFixMoveBeforeCreate() {
    DbNodeId dbNodeIdDir1;
    DbNodeId dbNodeIdDir11;
    DbNodeId dbNodeIdDir111;
    DbNodeId dbNodeIdFile1111;
    DbNodeId dbNodeIdDir2;
    DbNodeId dbNodeIdDir3;
    DbNodeId dbnodeIdDir4;
    DbNodeId dbnodeIdDir5;

    bool constraintError = false;
    DbNode nodeDir1(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 1"), Str("Dir 1"), "d1", "r1", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir1, dbNodeIdDir1, constraintError);
    DbNode nodeDir11(0, dbNodeIdDir1, Str("Dir 1.1"), Str("Dir 1.1"), "11", "id drive 1.1", testhelpers::defaultTime,
                     testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir11, dbNodeIdDir11, constraintError);
    DbNode nodeDir111(0, dbNodeIdDir11, Str("Dir 1.1.1"), Str("Dir 1.1.1"), "111", "id drive 111", testhelpers::defaultTime,
                      testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir111, dbNodeIdDir111, constraintError);
    DbNode nodeFile1111(0, dbNodeIdDir111, Str("File 1.1.1.1"), Str("File 1.1.1.1"), "1111", "r1111", testhelpers::defaultTime,
                        testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File, 0, "cs 1.1.1");
    _syncPal->syncDb()->insertNode(nodeFile1111, dbNodeIdFile1111, constraintError);
    DbNode nodeDir2(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 2"), Str("Dir 2"), "2", "r2", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir2, dbNodeIdDir2, constraintError);
    DbNode nodeDir3(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 3"), Str("Dir 3"), "3", "r3", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir3, dbNodeIdDir3, constraintError);
    DbNode nodeDir4(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 4"), Str("Dir 4"), "4", "r4", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir4, dbnodeIdDir4, constraintError);
    DbNode nodeDir5(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 5"), Str("Dir 5"), "5", "r5", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir5, dbnodeIdDir5, constraintError);

    std::shared_ptr<Node> rootNode(new Node(_syncPal->_syncDb->rootNode().nodeId(), _syncPal->_localUpdateTree->side(), Str(""),
                                            NodeType::Directory, OperationType::None, _syncPal->_syncDb->rootNode().nodeIdLocal(),
                                            testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize,
                                            nullptr));
    std::shared_ptr<Node> node1(new Node(dbNodeIdDir1, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 1"),
                                         NodeType::Directory, OperationType::None, "d1", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize, rootNode));
    std::shared_ptr<Node> node11(new Node(dbNodeIdDir11, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 1.1"),
                                          NodeType::Directory, OperationType::None, "11", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, node1));
    std::shared_ptr<Node> node111(new Node(dbNodeIdDir111, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 1.1.1"),
                                           NodeType::Directory, OperationType::None, "111", testhelpers::defaultTime,
                                           testhelpers::defaultTime, testhelpers::defaultFileSize, node11));
    std::shared_ptr<Node> node1111(new Node(dbNodeIdFile1111, _syncPal->updateTree(ReplicaSide::Local)->side(),
                                            Str("File 1.1.1.1"), NodeType::File, OperationType::None, "1111",
                                            testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize,
                                            node111));
    std::shared_ptr<Node> node2(new Node(dbNodeIdDir2, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 2"),
                                         NodeType::Directory, OperationType::None, "2", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize, rootNode));
    std::shared_ptr<Node> node3(new Node(dbNodeIdDir3, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 3"),
                                         NodeType::Directory, OperationType::None, "3", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize, rootNode));
    std::shared_ptr<Node> node4(new Node(dbnodeIdDir4, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 4"),
                                         NodeType::Directory, OperationType::None, "4", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize, rootNode));
    std::shared_ptr<Node> node5(new Node(dbnodeIdDir5, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 5"),
                                         NodeType::Directory, OperationType::None, "5", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize, rootNode));
    std::shared_ptr<Node> rrootNode(new Node(_syncPal->_syncDb->rootNode().nodeId(), _syncPal->_remoteUpdateTree->side(), Str(""),
                                             NodeType::Directory, OperationType::None,
                                             _syncPal->_syncDb->rootNode().nodeIdRemote(), testhelpers::defaultTime,
                                             testhelpers::defaultTime, testhelpers::defaultFileSize, nullptr));
    std::shared_ptr<Node> rNode1(new Node(dbNodeIdDir1, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 1"),
                                          NodeType::Directory, OperationType::None, "r1", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, rrootNode));
    std::shared_ptr<Node> rNode11(new Node(dbNodeIdDir11, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 1.1"),
                                           NodeType::Directory, OperationType::None, "r11", testhelpers::defaultTime,
                                           testhelpers::defaultTime, testhelpers::defaultFileSize, rNode1));
    std::shared_ptr<Node> rNode111(new Node(dbNodeIdDir111, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 1.1.1"),
                                            NodeType::Directory, OperationType::None, "r111", testhelpers::defaultTime,
                                            testhelpers::defaultTime, testhelpers::defaultFileSize, rNode11));
    std::shared_ptr<Node> rNode1111(new Node(dbNodeIdFile1111, _syncPal->updateTree(ReplicaSide::Remote)->side(),
                                             Str("File 1.1.1.1"), NodeType::File, OperationType::None, "r1111",
                                             testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize,
                                             rNode111));
    std::shared_ptr<Node> rNode2(new Node(dbNodeIdDir2, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 2"),
                                          NodeType::Directory, OperationType::None, "r2", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, rrootNode));
    std::shared_ptr<Node> rNode3(new Node(dbNodeIdDir3, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 3"),
                                          NodeType::Directory, OperationType::None, "r3", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, rrootNode));
    std::shared_ptr<Node> rNode4(new Node(dbnodeIdDir4, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 4"),
                                          NodeType::Directory, OperationType::None, "r4", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, rrootNode));
    std::shared_ptr<Node> rNode5(new Node(dbnodeIdDir5, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 5"),
                                          NodeType::Directory, OperationType::None, "r5", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, rrootNode));
    std::shared_ptr<Node> node111C(new Node(std::nullopt, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 1.1.1"),
                                            NodeType::Directory, OperationType::Create, "111c", testhelpers::defaultTime,
                                            testhelpers::defaultTime, testhelpers::defaultFileSize, node3));

    node111->setMoveOrigin("Dir 3/Dir 1.1.1");
    node111->setMoveOriginParentDbId(dbNodeIdDir3);

    SyncOpPtr op1 = std::make_shared<SyncOperation>();
    SyncOpPtr op2 = std::make_shared<SyncOperation>();
    SyncOpPtr op3 = std::make_shared<SyncOperation>();
    SyncOpPtr op4 = std::make_shared<SyncOperation>();
    SyncOpPtr op5 = std::make_shared<SyncOperation>();
    op1->setAffectedNode(node111);
    op1->setTargetSide(node111->side());
    op1->setType(OperationType::Move);
    op2->setAffectedNode(node111C);
    op2->setTargetSide(node111C->side());
    op2->setType(OperationType::Create);
    op3->setAffectedNode(node3);
    op4->setAffectedNode(node4);
    op5->setAffectedNode(node5);

    _syncPal->_syncOps->pushOp(op5);
    _syncPal->_syncOps->pushOp(op2);
    _syncPal->_syncOps->pushOp(op4);
    _syncPal->_syncOps->pushOp(op3);
    _syncPal->_syncOps->pushOp(op1);
    _syncPal->_operationsSorterWorker->_unsortedList = *_syncPal->_syncOps;

    _syncPal->_operationsSorterWorker->fixMoveBeforeCreate();
    CPPUNIT_ASSERT(_syncPal->_syncOps->_opSortedList.back() == op2->id());
    _syncPal->_syncOps->_opSortedList.pop_back();
    CPPUNIT_ASSERT(_syncPal->_syncOps->_opSortedList.back() == op1->id());
}

void TestOperationSorterWorker::testFixMoveBeforeDelete() {
    DbNodeId dbNodeIdDir1;
    DbNodeId dbNodeIdDir11;
    DbNodeId dbNodeIdDir111;
    DbNodeId dbNodeIdFile1111;
    DbNodeId dbNodeIdDir2;
    DbNodeId dbNodeIdDir3;
    DbNodeId dbnodeIdDir4;
    DbNodeId dbnodeIdDir5;

    bool constraintError = false;
    DbNode nodeDir1(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 1"), Str("Dir 1"), "d1", "r1", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir1, dbNodeIdDir1, constraintError);
    DbNode nodeDir11(0, dbNodeIdDir1, Str("Dir 1.1"), Str("Dir 1.1"), "11", "r11", testhelpers::defaultTime,
                     testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir11, dbNodeIdDir11, constraintError);
    DbNode nodeDir111(0, dbNodeIdDir11, Str("Dir 1.1.1"), Str("Dir 1.1.1"), "111", "r111", testhelpers::defaultTime,
                      testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir111, dbNodeIdDir111, constraintError);
    DbNode nodeFile1111(0, dbNodeIdDir111, Str("File 1.1.1.1"), Str("File 1.1.1.1"), "1111", "r1111", testhelpers::defaultTime,
                        testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File, 0, "cs 1.1.1");
    _syncPal->syncDb()->insertNode(nodeFile1111, dbNodeIdFile1111, constraintError);
    DbNode nodeDir2(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 2"), Str("Dir 2"), "2", "r2", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir2, dbNodeIdDir2, constraintError);
    DbNode nodeDir3(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 3"), Str("Dir 3"), "3", "r3", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir3, dbNodeIdDir3, constraintError);
    DbNode nodeDir4(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 4"), Str("Dir 4"), "4", "r4", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir4, dbnodeIdDir4, constraintError);
    DbNode nodeDir5(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 5"), Str("Dir 5"), "5", "r5", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir5, dbnodeIdDir5, constraintError);

    std::shared_ptr<Node> rootNode(
            new Node(_syncPal->syncDb()->rootNode().nodeId(), _syncPal->updateTree(ReplicaSide::Local)->side(), Str(""),
                     NodeType::Directory, OperationType::None, _syncPal->syncDb()->rootNode().nodeIdLocal(),
                     testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, nullptr));
    std::shared_ptr<Node> node1(new Node(dbNodeIdDir1, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 1"),
                                         NodeType::Directory, OperationType::None, "d1", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize, rootNode));
    std::shared_ptr<Node> node11(new Node(dbNodeIdDir11, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 1.1"),
                                          NodeType::Directory, OperationType::None, "11", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, node1));
    std::shared_ptr<Node> node2(new Node(dbNodeIdDir2, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 2"),
                                         NodeType::Directory, OperationType::None, "2", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize, rootNode));
    std::shared_ptr<Node> node111(new Node(dbNodeIdDir111, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 1.1.1"),
                                           NodeType::Directory, OperationType::None, "111", testhelpers::defaultTime,
                                           testhelpers::defaultTime, testhelpers::defaultFileSize, node2));
    std::shared_ptr<Node> node1111(new Node(dbNodeIdFile1111, _syncPal->updateTree(ReplicaSide::Local)->side(),
                                            Str("File 1.1.1.1"), NodeType::File, OperationType::None, "1111",
                                            testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize,
                                            node111));
    std::shared_ptr<Node> node3(new Node(dbNodeIdDir3, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 3"),
                                         NodeType::Directory, OperationType::None, "3", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize, rootNode));
    std::shared_ptr<Node> node4(new Node(dbnodeIdDir4, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 4"),
                                         NodeType::Directory, OperationType::None, "4", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize, rootNode));
    std::shared_ptr<Node> node5(new Node(dbnodeIdDir5, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 5"),
                                         NodeType::Directory, OperationType::None, "5", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize, rootNode));
    std::shared_ptr<Node> rrootNode(
            new Node(_syncPal->syncDb()->rootNode().nodeId(), _syncPal->updateTree(ReplicaSide::Remote)->side(), Str(""),
                     NodeType::Directory, OperationType::None, _syncPal->syncDb()->rootNode().nodeIdRemote(),
                     testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, nullptr));
    std::shared_ptr<Node> rNode1(new Node(dbNodeIdDir1, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 1"),
                                          NodeType::Directory, OperationType::None, "r1", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, rrootNode));
    std::shared_ptr<Node> rNode11(new Node(dbNodeIdDir11, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 1.1"),
                                           NodeType::Directory, OperationType::None, "r11", testhelpers::defaultTime,
                                           testhelpers::defaultTime, testhelpers::defaultFileSize, rNode1));
    std::shared_ptr<Node> rNode111(new Node(dbNodeIdDir111, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 1.1.1"),
                                            NodeType::Directory, OperationType::None, "r111", testhelpers::defaultTime,
                                            testhelpers::defaultTime, testhelpers::defaultFileSize, rNode11));
    std::shared_ptr<Node> rNode1111(new Node(dbNodeIdFile1111, _syncPal->updateTree(ReplicaSide::Remote)->side(),
                                             Str("File 1.1.1.1"), NodeType::File, OperationType::None, "r1111",
                                             testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize,
                                             rNode111));
    std::shared_ptr<Node> rNode2(new Node(dbNodeIdDir2, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 2"),
                                          NodeType::Directory, OperationType::None, "r2", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, rrootNode));
    std::shared_ptr<Node> rNode3(new Node(dbNodeIdDir3, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 3"),
                                          NodeType::Directory, OperationType::None, "r3", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, rrootNode));
    std::shared_ptr<Node> rNode4(new Node(dbnodeIdDir4, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 4"),
                                          NodeType::Directory, OperationType::None, "r4", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, rrootNode));
    std::shared_ptr<Node> rNode5(new Node(dbnodeIdDir5, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 5"),
                                          NodeType::Directory, OperationType::None, "r5", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, rrootNode));

    node111->setMoveOrigin("Dir 1/Dir 1.1/Dir 1.1.1");
    node111->setMoveOriginParentDbId(dbNodeIdDir11);

    SyncOpPtr op1 = std::make_shared<SyncOperation>();
    SyncOpPtr op2 = std::make_shared<SyncOperation>();
    SyncOpPtr op3 = std::make_shared<SyncOperation>();
    SyncOpPtr op4 = std::make_shared<SyncOperation>();
    SyncOpPtr op5 = std::make_shared<SyncOperation>();
    op1->setAffectedNode(node111);
    op1->setTargetSide(node111->side());
    op1->setType(OperationType::Move);
    op2->setAffectedNode(node1);
    op2->setTargetSide(node1->side());
    op2->setType(OperationType::Delete);
    op3->setAffectedNode(node3);
    op4->setAffectedNode(node4);
    op5->setAffectedNode(node5);

    _syncPal->_syncOps->pushOp(op5);
    _syncPal->_syncOps->pushOp(op2);
    _syncPal->_syncOps->pushOp(op4);
    _syncPal->_syncOps->pushOp(op3);
    _syncPal->_syncOps->pushOp(op1);
    _syncPal->_operationsSorterWorker->_unsortedList = *_syncPal->_syncOps;

    _syncPal->_operationsSorterWorker->fixMoveBeforeDelete();
    CPPUNIT_ASSERT(_syncPal->_syncOps->_opSortedList.back() == op2->id());
    _syncPal->_syncOps->_opSortedList.pop_back();
    CPPUNIT_ASSERT(_syncPal->_syncOps->_opSortedList.back() == op1->id());
}

void TestOperationSorterWorker::testFixCreateBeforeMove() {
    DbNodeId dbNodeIdDir1;
    DbNodeId dbNodeIdDir11;
    DbNodeId dbNodeIdDir111;
    DbNodeId dbNodeIdFile1111;
    DbNodeId dbNodeIdDir2;
    DbNodeId dbNodeIdDir3;
    DbNodeId dbnodeIdDir4;
    DbNodeId dbnodeIdDir5;

    bool constraintError = false;
    DbNode nodeDir1(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 1"), Str("Dir 1"), "d1", "r1", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir1, dbNodeIdDir1, constraintError);
    DbNode nodeDir11(0, dbNodeIdDir1, Str("Dir 1.1"), Str("Dir 1.1"), "11", "id drive 1.1", testhelpers::defaultTime,
                     testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir11, dbNodeIdDir11, constraintError);
    DbNode nodeDir111(0, dbNodeIdDir11, Str("Dir 1.1.1"), Str("Dir 1.1.1"), "111", "id drive 111", testhelpers::defaultTime,
                      testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir111, dbNodeIdDir111, constraintError);
    DbNode nodeFile1111(0, dbNodeIdDir111, Str("File 1.1.1.1"), Str("File 1.1.1.1"), "1111", "r1111", testhelpers::defaultTime,
                        testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File, 0, "cs 1.1.1");
    _syncPal->syncDb()->insertNode(nodeFile1111, dbNodeIdFile1111, constraintError);
    DbNode nodeDir2(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 2"), Str("Dir 2"), "2", "r2", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir2, dbNodeIdDir2, constraintError);
    DbNode nodeDir3(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 3"), Str("Dir 3"), "3", "r3", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir3, dbNodeIdDir3, constraintError);
    DbNode nodeDir4(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 4"), Str("Dir 4"), "4", "r4", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir4, dbnodeIdDir4, constraintError);
    DbNode nodeDir5(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 5"), Str("Dir 5"), "5", "r5", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir5, dbnodeIdDir5, constraintError);

    std::shared_ptr<Node> rootNode(new Node(_syncPal->_syncDb->rootNode().nodeId(), _syncPal->_localUpdateTree->side(), Str(""),
                                            NodeType::Directory, OperationType::None,
                                            _syncPal->syncDb()->rootNode().nodeIdLocal(), testhelpers::defaultTime,
                                            testhelpers::defaultTime, testhelpers::defaultFileSize, nullptr));
    std::shared_ptr<Node> node1(new Node(dbNodeIdDir1, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 1"),
                                         NodeType::Directory, OperationType::None, "d1", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize, rootNode));
    std::shared_ptr<Node> node11(new Node(std::nullopt, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 1.1"),
                                          NodeType::Directory, OperationType::None, "11", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, node1));
    std::shared_ptr<Node> node111(new Node(dbNodeIdDir111, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 1.1.1"),
                                           NodeType::Directory, OperationType::None, "111", testhelpers::defaultTime,
                                           testhelpers::defaultTime, testhelpers::defaultFileSize, node11));
    std::shared_ptr<Node> node1111(new Node(dbNodeIdFile1111, _syncPal->updateTree(ReplicaSide::Local)->side(),
                                            Str("File 1.1.1.1"), NodeType::File, OperationType::None, "1111",
                                            testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize,
                                            node111));
    std::shared_ptr<Node> node2(new Node(dbNodeIdDir2, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 2"),
                                         NodeType::Directory, OperationType::None, "2", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize, rootNode));
    std::shared_ptr<Node> node3(new Node(dbNodeIdDir3, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 3"),
                                         NodeType::Directory, OperationType::None, "3", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize, rootNode));
    std::shared_ptr<Node> node4(new Node(dbnodeIdDir4, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 4"),
                                         NodeType::Directory, OperationType::None, "4", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize, rootNode));
    std::shared_ptr<Node> node5(new Node(dbnodeIdDir5, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 5"),
                                         NodeType::Directory, OperationType::None, "5", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize, rootNode));
    std::shared_ptr<Node> rrootNode(new Node(_syncPal->_syncDb->rootNode().nodeId(), _syncPal->_remoteUpdateTree->side(), Str(""),
                                             NodeType::Directory, OperationType::None,
                                             _syncPal->syncDb()->rootNode().nodeIdRemote(), testhelpers::defaultTime,
                                             testhelpers::defaultTime, testhelpers::defaultFileSize, nullptr));
    std::shared_ptr<Node> rNode1(new Node(dbNodeIdDir1, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 1"),
                                          NodeType::Directory, OperationType::None, "r1", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, rrootNode));
    std::shared_ptr<Node> rNode11(new Node(dbNodeIdDir11, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 1.1"),
                                           NodeType::Directory, OperationType::None, "r11", testhelpers::defaultTime,
                                           testhelpers::defaultTime, testhelpers::defaultFileSize, rNode1));
    std::shared_ptr<Node> rNode111(new Node(dbNodeIdDir111, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 1.1.1"),
                                            NodeType::Directory, OperationType::None, "r111", testhelpers::defaultTime,
                                            testhelpers::defaultTime, testhelpers::defaultFileSize, rNode11));
    std::shared_ptr<Node> rNode1111(new Node(dbNodeIdFile1111, _syncPal->updateTree(ReplicaSide::Remote)->side(),
                                             Str("File 1.1.1.1"), NodeType::File, OperationType::None, "r1111",
                                             testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize,
                                             rNode111));
    std::shared_ptr<Node> rNode2(new Node(dbNodeIdDir2, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 2"),
                                          NodeType::Directory, OperationType::None, "r2", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, rrootNode));
    std::shared_ptr<Node> rNode3(new Node(dbNodeIdDir3, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 3"),
                                          NodeType::Directory, OperationType::None, "r3", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, rrootNode));
    std::shared_ptr<Node> rNode4(new Node(dbnodeIdDir4, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 4"),
                                          NodeType::Directory, OperationType::None, "r4", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, rrootNode));
    std::shared_ptr<Node> rNode5(new Node(dbnodeIdDir5, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 5"),
                                          NodeType::Directory, OperationType::None, "r5", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, rrootNode));

    CPPUNIT_ASSERT(rootNode->insertChildren(node1));
    CPPUNIT_ASSERT(node1->insertChildren(node11));
    CPPUNIT_ASSERT(node11->insertChildren(node111));
    CPPUNIT_ASSERT(node111->insertChildren(node1111));

    SyncOpPtr op1 = std::make_shared<SyncOperation>();
    SyncOpPtr op2 = std::make_shared<SyncOperation>();
    SyncOpPtr op3 = std::make_shared<SyncOperation>();
    SyncOpPtr op4 = std::make_shared<SyncOperation>();
    SyncOpPtr op5 = std::make_shared<SyncOperation>();
    op1->setAffectedNode(node111);
    op1->setTargetSide(node111->side());
    op1->setType(OperationType::Move);
    op2->setAffectedNode(node11);
    op2->setTargetSide(node11->side());
    op2->setType(OperationType::Create);
    op3->setAffectedNode(node3);
    op4->setAffectedNode(node4);
    op5->setAffectedNode(node5);

    _syncPal->_syncOps->pushOp(op5);
    _syncPal->_syncOps->pushOp(op1);
    _syncPal->_syncOps->pushOp(op4);
    _syncPal->_syncOps->pushOp(op3);
    _syncPal->_syncOps->pushOp(op2);
    _syncPal->_operationsSorterWorker->_unsortedList = *_syncPal->_syncOps;

    _syncPal->_operationsSorterWorker->fixCreateBeforeMove();
    CPPUNIT_ASSERT(_syncPal->_syncOps->_opSortedList.back() == op1->id());
    _syncPal->_syncOps->_opSortedList.pop_back();
    CPPUNIT_ASSERT(_syncPal->_syncOps->_opSortedList.back() == op2->id());
}

void TestOperationSorterWorker::testFixCreateBeforeMoveBis() {
    DbNodeId dbNodeIdDirA;

    bool constraintError = false;
    DbNode nodeDir1(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir A"), Str("Dir A"), "dA", "rdA", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir1, dbNodeIdDirA, constraintError);

    std::shared_ptr<Node> rootNode(
            new Node(_syncPal->_syncDb->rootNode().nodeId(), _syncPal->updateTree(ReplicaSide::Local)->side(), Str(""),
                     NodeType::Directory, OperationType::None, _syncPal->syncDb()->rootNode().nodeIdLocal(),
                     testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, nullptr));
    std::shared_ptr<Node> nodeA(new Node(dbNodeIdDirA, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir A"),
                                         NodeType::Directory, OperationType::Move, "dA", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize, rootNode));
    std::shared_ptr<Node> rrootNode(new Node(_syncPal->_syncDb->rootNode().nodeId(), _syncPal->_remoteUpdateTree->side(), Str(""),
                                             NodeType::Directory, OperationType::None,
                                             _syncPal->syncDb()->rootNode().nodeIdRemote(), testhelpers::defaultTime,
                                             testhelpers::defaultTime, testhelpers::defaultFileSize, nullptr));
    std::shared_ptr<Node> rNodeA(new Node(dbNodeIdDirA, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir A"),
                                          NodeType::Directory, OperationType::None, "rdA", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, rrootNode));


    CPPUNIT_ASSERT(rootNode->insertChildren(nodeA));

    // Rename Dir A -> Dir B
    nodeA->setMoveOrigin("Dir A");
    nodeA->setMoveOriginParentDbId(_syncPal->syncDb()->rootNode().nodeId());
    nodeA->setName(Str("Dir B"));

    // Create Dir A
    std::shared_ptr<Node> nodeABis(new Node(std::nullopt, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir A"),
                                            NodeType::Directory, OperationType::Create, "dABis", testhelpers::defaultTime,
                                            testhelpers::defaultTime, testhelpers::defaultFileSize, rootNode));

    SyncOpPtr op1 = std::make_shared<SyncOperation>();
    SyncOpPtr op2 = std::make_shared<SyncOperation>();
    op1->setAffectedNode(nodeA);
    op1->setTargetSide(nodeA->side());
    op1->setType(OperationType::Move);
    op2->setAffectedNode(nodeABis);
    op2->setTargetSide(nodeABis->side());
    op2->setType(OperationType::Create);

    _syncPal->_syncOps->pushOp(op2);
    _syncPal->_syncOps->pushOp(op1);
    _syncPal->_operationsSorterWorker->_unsortedList = *_syncPal->_syncOps;

    _syncPal->_operationsSorterWorker->fixMoveBeforeCreate();
    CPPUNIT_ASSERT(_syncPal->_syncOps->_opSortedList.back() == op2->id());
    _syncPal->_syncOps->_opSortedList.pop_back();
    CPPUNIT_ASSERT(_syncPal->_syncOps->_opSortedList.back() == op1->id());
}

void TestOperationSorterWorker::testFixDeleteBeforeCreate() {
    DbNodeId dbNodeIdDir1;
    DbNodeId dbNodeIdDir11;
    DbNodeId dbNodeIdDir111;
    DbNodeId dbNodeIdFile1111;
    DbNodeId dbNodeIdDir2;
    DbNodeId dbNodeIdDir3;

    bool constraintError = false;
    DbNode nodeDir1(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 1"), Str("Dir 1"), "d1", "r1", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir1, dbNodeIdDir1, constraintError);
    DbNode nodeDir11(0, dbNodeIdDir1, Str("Dir 1.1"), Str("Dir 1.1"), "11", "r11", testhelpers::defaultTime,
                     testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir11, dbNodeIdDir11, constraintError);
    DbNode nodeDir111(0, dbNodeIdDir11, Str("Dir 1.1.1"), Str("Dir 1.1.1"), "111", "r111", testhelpers::defaultTime,
                      testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir111, dbNodeIdDir111, constraintError);
    DbNode nodeFile1111(0, dbNodeIdDir111, Str("File 1.1.1.1"), Str("File 1.1.1.1"), "1111", "r1111", testhelpers::defaultTime,
                        testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File, 0, "cs 1.1.1");
    _syncPal->syncDb()->insertNode(nodeFile1111, dbNodeIdFile1111, constraintError);
    DbNode nodeDir2(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 2"), Str("Dir 2"), "2", "r2", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir2, dbNodeIdDir2, constraintError);
    DbNode nodeDir3(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 3"), Str("Dir 3"), "3", "r3", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir3, dbNodeIdDir3, constraintError);

    std::shared_ptr<Node> rootNode(
            new Node(_syncPal->_syncDb->rootNode().nodeId(), _syncPal->updateTree(ReplicaSide::Local)->side(), Str(""),
                     NodeType::Directory, OperationType::None, _syncPal->syncDb()->rootNode().nodeIdLocal(),
                     testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, nullptr));
    std::shared_ptr<Node> node1(new Node(dbNodeIdDir1, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 1"),
                                         NodeType::Directory, OperationType::None, "d1", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize, rootNode));
    std::shared_ptr<Node> node11(new Node(dbNodeIdDir11, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 1.1"),
                                          NodeType::Directory, OperationType::None, "11", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, node1));
    std::shared_ptr<Node> node111(new Node(dbNodeIdDir111, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 1.1.1"),
                                           NodeType::Directory, OperationType::None, "111", testhelpers::defaultTime,
                                           testhelpers::defaultTime, testhelpers::defaultFileSize, node11));
    std::shared_ptr<Node> node1111(new Node(dbNodeIdFile1111, _syncPal->updateTree(ReplicaSide::Local)->side(),
                                            Str("File 1.1.1.1"), NodeType::File, OperationType::None, "1111",
                                            testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize,
                                            node111));
    std::shared_ptr<Node> node2(new Node(dbNodeIdDir2, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 2"),
                                         NodeType::Directory, OperationType::None, "2", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize, rootNode));
    std::shared_ptr<Node> node3(new Node(dbNodeIdDir3, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 3"),
                                         NodeType::Directory, OperationType::None, "3", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize, rootNode));
    std::shared_ptr<Node> rrootNode(new Node(_syncPal->_syncDb->rootNode().nodeId(), _syncPal->_remoteUpdateTree->side(), Str(""),
                                             NodeType::Directory, OperationType::None,
                                             _syncPal->syncDb()->rootNode().nodeIdRemote(), testhelpers::defaultTime,
                                             testhelpers::defaultTime, testhelpers::defaultFileSize, nullptr));
    std::shared_ptr<Node> rNode1(new Node(dbNodeIdDir1, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 1"),
                                          NodeType::Directory, OperationType::None, "r1", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, rrootNode));
    std::shared_ptr<Node> rNode11(new Node(dbNodeIdDir11, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 1.1"),
                                           NodeType::Directory, OperationType::None, "r11", testhelpers::defaultTime,
                                           testhelpers::defaultTime, testhelpers::defaultFileSize, rNode1));
    std::shared_ptr<Node> rNode111(new Node(dbNodeIdDir111, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 1.1.1"),
                                            NodeType::Directory, OperationType::None, "r111", testhelpers::defaultTime,
                                            testhelpers::defaultTime, testhelpers::defaultFileSize, rNode11));
    std::shared_ptr<Node> rNode1111(new Node(dbNodeIdFile1111, _syncPal->updateTree(ReplicaSide::Remote)->side(),
                                             Str("File 1.1.1.1"), NodeType::File, OperationType::None, "r1111",
                                             testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize,
                                             rNode111));
    std::shared_ptr<Node> rNode2(new Node(dbNodeIdDir2, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 2"),
                                          NodeType::Directory, OperationType::None, "r2", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, rrootNode));
    std::shared_ptr<Node> rNode3(new Node(dbNodeIdDir3, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 3"),
                                          NodeType::Directory, OperationType::None, "r3", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, rrootNode));

    _syncPal->_localSnapshot->updateItem(SnapshotItem("d1", _syncPal->syncDb()->rootNode().nodeIdLocal().value(), Str("Dir 1"),
                                                      testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory,
                                                      testhelpers::defaultFileSize, false, true, true));
    _syncPal->_localSnapshot->updateItem(SnapshotItem("11", "d1", Str("Dir 1.1"), testhelpers::defaultTime,
                                                      testhelpers::defaultTime, NodeType::Directory, testhelpers::defaultFileSize,
                                                      false, true, true));
    _syncPal->_localSnapshot->updateItem(SnapshotItem("111", "11", Str("Dir 1.1.1"), testhelpers::defaultTime,
                                                      testhelpers::defaultTime, NodeType::Directory, testhelpers::defaultFileSize,
                                                      false, true, true));
    _syncPal->_localSnapshot->updateItem(SnapshotItem("1111c", "111", Str("File 1.1.1.1"), testhelpers::defaultTime,
                                                      testhelpers::defaultTime, NodeType::Directory, testhelpers::defaultFileSize,
                                                      false, true, true));
    _syncPal->_localSnapshot->updateItem(SnapshotItem("2", _syncPal->syncDb()->rootNode().nodeIdLocal().value(), Str("Dir 2"),
                                                      testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory,
                                                      testhelpers::defaultFileSize, false, true, true));
    _syncPal->_localSnapshot->updateItem(SnapshotItem("3", _syncPal->syncDb()->rootNode().nodeIdLocal().value(), Str("Dir 3"),
                                                      testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory,
                                                      testhelpers::defaultFileSize, false, true, true));

    _syncPal->_remoteSnapshot->updateItem(SnapshotItem("r1", _syncPal->syncDb()->rootNode().nodeIdRemote().value(), Str("Dir 1"),
                                                       testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory,
                                                       testhelpers::defaultFileSize, false, true, true));
    _syncPal->_remoteSnapshot->updateItem(SnapshotItem("r11", "r1", Str("Dir 1.1"), testhelpers::defaultTime,
                                                       testhelpers::defaultTime, NodeType::Directory,
                                                       testhelpers::defaultFileSize, false, true, true));
    _syncPal->_remoteSnapshot->updateItem(SnapshotItem("r111", "r11", Str("Dir 1.1.1"), testhelpers::defaultTime,
                                                       testhelpers::defaultTime, NodeType::Directory,
                                                       testhelpers::defaultFileSize, false, true, true));
    _syncPal->_remoteSnapshot->updateItem(SnapshotItem("r1111", "r111", Str("File 1.1.1.1"), testhelpers::defaultTime,
                                                       testhelpers::defaultTime, NodeType::Directory,
                                                       testhelpers::defaultFileSize, false, true, true));
    _syncPal->_remoteSnapshot->updateItem(SnapshotItem("r2", _syncPal->syncDb()->rootNode().nodeIdRemote().value(), Str("Dir 2"),
                                                       testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory,
                                                       testhelpers::defaultFileSize, false, true, true));
    _syncPal->_remoteSnapshot->updateItem(SnapshotItem("r3", _syncPal->syncDb()->rootNode().nodeIdRemote().value(), Str("Dir 3"),
                                                       testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory,
                                                       testhelpers::defaultFileSize, false, true, true));

    std::shared_ptr<Node> node1111C(new Node(std::nullopt, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("File 1.1.1.1"),
                                             NodeType::File, OperationType::Create, "1111c", testhelpers::defaultTime,
                                             testhelpers::defaultTime, testhelpers::defaultFileSize, node111));

    SyncOpPtr op1 = std::make_shared<SyncOperation>();
    SyncOpPtr op2 = std::make_shared<SyncOperation>();
    SyncOpPtr op3 = std::make_shared<SyncOperation>();
    SyncOpPtr op4 = std::make_shared<SyncOperation>();
    SyncOpPtr op5 = std::make_shared<SyncOperation>();
    op1->setAffectedNode(node1111);
    op1->setTargetSide(node1111->side());
    op1->setType(OperationType::Delete);
    op2->setAffectedNode(node1111C);
    op2->setTargetSide(node1111C->side());
    op2->setType(OperationType::Create);
    op3->setAffectedNode(node3);
    op4->setAffectedNode(node1);
    op5->setAffectedNode(node2);

    _syncPal->_syncOps->pushOp(op5);
    _syncPal->_syncOps->pushOp(op2);
    _syncPal->_syncOps->pushOp(op4);
    _syncPal->_syncOps->pushOp(op3);
    _syncPal->_syncOps->pushOp(op1);
    _syncPal->_operationsSorterWorker->_unsortedList = *_syncPal->_syncOps;

    _syncPal->_operationsSorterWorker->fixDeleteBeforeCreate();
    CPPUNIT_ASSERT(_syncPal->_syncOps->_opSortedList.back() == op2->id());
    _syncPal->_syncOps->_opSortedList.pop_back();
    CPPUNIT_ASSERT(_syncPal->_syncOps->_opSortedList.back() == op1->id());
}

void TestOperationSorterWorker::testFixMoveBeforeMoveOccupied() {
    DbNodeId dbNodeIdDir1;
    DbNodeId dbNodeIdDir11;
    DbNodeId dbNodeIdDir2;
    DbNodeId dbNodeIdDir3;

    bool constraintError = true;
    DbNode nodeDir1(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 1"), Str("Dir 1"), "d1", "r1", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir1, dbNodeIdDir1, constraintError);
    DbNode nodeDir11(0, dbNodeIdDir1, Str("Dir 1.1"), Str("Dir 1.1"), "11", "r11", testhelpers::defaultTime,
                     testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir11, dbNodeIdDir11, constraintError);
    DbNode nodeDir2(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 2"), Str("Dir 2"), "2", "r2", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir2, dbNodeIdDir2, constraintError);
    DbNode nodeDir3(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 3"), Str("Dir 3"), "3", "r3", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir3, dbNodeIdDir3, constraintError);

    std::shared_ptr<Node> rootNode(new Node(_syncPal->_syncDb->rootNode().nodeId(), _syncPal->_localUpdateTree->side(), Str(""),
                                            NodeType::Directory, OperationType::None, _syncPal->_syncDb->rootNode().nodeIdLocal(),
                                            testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize,
                                            nullptr));
    std::shared_ptr<Node> node1(new Node(dbNodeIdDir1, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 1"),
                                         NodeType::Directory, OperationType::None, "d1", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize, rootNode));
    std::shared_ptr<Node> node11(new Node(dbNodeIdDir11, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 1.1"),
                                          NodeType::Directory, OperationType::None, "11", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, node1));
    std::shared_ptr<Node> node2(new Node(dbNodeIdDir2, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 2"),
                                         NodeType::Directory, OperationType::None, "2", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize, rootNode));
    std::shared_ptr<Node> node3(new Node(dbNodeIdDir3, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 3"),
                                         NodeType::Directory, OperationType::None, "3", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize, rootNode));
    std::shared_ptr<Node> rrootNode(
            new Node(_syncPal->_syncDb->rootNode().nodeId(), _syncPal->updateTree(ReplicaSide::Remote)->side(), Str(""),
                     NodeType::Directory, OperationType::None, _syncPal->syncDb()->rootNode().nodeIdRemote(),
                     testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, nullptr));
    std::shared_ptr<Node> rNode1(new Node(dbNodeIdDir1, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 1"),
                                          NodeType::Directory, OperationType::None, "r1", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, rrootNode));
    std::shared_ptr<Node> rNode11(new Node(dbNodeIdDir11, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 1.1"),
                                           NodeType::Directory, OperationType::None, "r11", testhelpers::defaultTime,
                                           testhelpers::defaultTime, testhelpers::defaultFileSize, rNode1));
    std::shared_ptr<Node> rNode2(new Node(dbNodeIdDir2, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 2"),
                                          NodeType::Directory, OperationType::None, "r2", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, rrootNode));
    std::shared_ptr<Node> rNode3(new Node(dbNodeIdDir3, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 3"),
                                          NodeType::Directory, OperationType::None, "r3", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, rrootNode));

    node11->setMoveOrigin("Dir 1/Dir 1.1");
    node11->setMoveOriginParentDbId(dbNodeIdDir1);
    CPPUNIT_ASSERT(node11->setParentNode(node2));
    CPPUNIT_ASSERT(node2->insertChildren(node11));
    node1->deleteChildren(node11);
    node11->insertChangeEvent(OperationType::Move);

    node3->insertChangeEvent(OperationType::Move);
    CPPUNIT_ASSERT(node3->setParentNode(node1));
    node3->setName(Str("Dir 1.1"));
    node3->setMoveOriginParentDbId(_syncPal->syncDb()->rootNode().nodeId());
    node3->setMoveOrigin("Dir 3");
    CPPUNIT_ASSERT(node1->insertChildren(node3));
    rootNode->deleteChildren(node3);

    SyncOpPtr op1 = std::make_shared<SyncOperation>();
    SyncOpPtr op2 = std::make_shared<SyncOperation>();
    SyncOpPtr op3 = std::make_shared<SyncOperation>();
    SyncOpPtr op4 = std::make_shared<SyncOperation>();
    SyncOpPtr op5 = std::make_shared<SyncOperation>();
    op1->setAffectedNode(node11);
    op1->setTargetSide(node11->side());
    op1->setType(OperationType::Move);
    op2->setAffectedNode(node3);
    op2->setTargetSide(node3->side());
    op2->setType(OperationType::Move);
    op3->setAffectedNode(node3);
    op4->setAffectedNode(node1);
    op5->setAffectedNode(node2);

    _syncPal->_syncOps->pushOp(op1);
    _syncPal->_syncOps->pushOp(op5);
    _syncPal->_syncOps->pushOp(op4);
    _syncPal->_syncOps->pushOp(op3);
    _syncPal->_syncOps->pushOp(op2);
    _syncPal->_operationsSorterWorker->_unsortedList = *_syncPal->_syncOps;

    _syncPal->_operationsSorterWorker->fixMoveBeforeMoveOccupied();
    CPPUNIT_ASSERT(_syncPal->_syncOps->_opSortedList.back() == op1->id());
    _syncPal->_syncOps->_opSortedList.pop_back();
    CPPUNIT_ASSERT(_syncPal->_syncOps->_opSortedList.back() == op2->id());
}

void TestOperationSorterWorker::testFixCreateBeforeCreate() {
    /**
     *
     *                            root
     *                        _____|_____
     *                       |          |
     *                       A          B
     *                  _____|_____
     *                 |          |
     *                AA         AB
     *           _____|_____
     *          |          |
     *         AAA        AAB
     */

    DbNodeId dbNodeIdDirA;
    DbNodeId dbNodeIdDirB;
    DbNodeId dbNodeIdDirAA;
    DbNodeId dbNodeIdDirAB;
    DbNodeId dbNodeIdDirAAA;
    DbNodeId dbNodeIdDirAAB;

    bool constraintError = false;
    DbNode nodeDirA(0, _syncPal->syncDb()->rootNode().nodeId(), Str("A"), Str("A"), "la", "ra", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirA, dbNodeIdDirA, constraintError);
    DbNode nodeDirB(0, _syncPal->syncDb()->rootNode().nodeId(), Str("B"), Str("B"), "lb", "rb", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirB, dbNodeIdDirB, constraintError);
    DbNode nodeDirAA(0, dbNodeIdDirA, Str("AA"), Str("AA"), "laa", "raa", testhelpers::defaultTime, testhelpers::defaultTime,
                     testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirAA, dbNodeIdDirAA, constraintError);
    DbNode nodeDirAB(0, dbNodeIdDirA, Str("AB"), Str("AB"), "lab", "rab", testhelpers::defaultTime, testhelpers::defaultTime,
                     testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirAB, dbNodeIdDirAB, constraintError);
    DbNode nodeDirAAA(0, dbNodeIdDirAA, Str("AAA"), Str("AAA"), "laaa", "raaa", testhelpers::defaultTime,
                      testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirAAA, dbNodeIdDirAAA, constraintError);
    DbNode nodeDirAAB(0, dbNodeIdDirAA, Str("AAB"), Str("AAB"), "laab", "raab", testhelpers::defaultTime,
                      testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirAAB, dbNodeIdDirAAB, constraintError);

    std::shared_ptr<Node> rootNode(
            new Node(_syncPal->_syncDb->rootNode().nodeId(), _syncPal->updateTree(ReplicaSide::Local)->side(), Str(""),
                     NodeType::Directory, OperationType::None, _syncPal->syncDb()->rootNode().nodeIdLocal(),
                     testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, nullptr));
    std::shared_ptr<Node> nodeA(new Node(dbNodeIdDirA, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("A"),
                                         NodeType::Directory, OperationType::Create, "a", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize, rootNode));
    std::shared_ptr<Node> nodeB(new Node(dbNodeIdDirB, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("B"),
                                         NodeType::Directory, OperationType::Create, "b", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize, rootNode));
    std::shared_ptr<Node> nodeAA(new Node(dbNodeIdDirAA, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("AA"),
                                          NodeType::Directory, OperationType::Create, "aa", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, nodeA));
    std::shared_ptr<Node> nodeAB(new Node(dbNodeIdDirAB, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("AB"),
                                          NodeType::Directory, OperationType::Create, "ab", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, nodeA));
    std::shared_ptr<Node> nodeAAA(new Node(dbNodeIdDirAAA, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("AAA"),
                                           NodeType::File, OperationType::Create, "aaa", testhelpers::defaultTime,
                                           testhelpers::defaultTime, testhelpers::defaultFileSize, nodeAA));
    std::shared_ptr<Node> nodeAAB(new Node(dbNodeIdDirAAB, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("AAB"),
                                           NodeType::File, OperationType::Create, "aab", testhelpers::defaultTime,
                                           testhelpers::defaultTime, testhelpers::defaultFileSize, nodeAA));

    SyncOpPtr opA = std::make_shared<SyncOperation>();
    opA->setAffectedNode(nodeA);
    opA->setTargetSide(nodeA->side());
    opA->setType(OperationType::Create);

    SyncOpPtr opB = std::make_shared<SyncOperation>();
    opB->setAffectedNode(nodeB);
    opB->setTargetSide(nodeB->side());
    opB->setType(OperationType::Create);

    SyncOpPtr opAA = std::make_shared<SyncOperation>();
    opAA->setAffectedNode(nodeAA);
    opAA->setTargetSide(nodeAA->side());
    opAA->setType(OperationType::Create);

    SyncOpPtr opAB = std::make_shared<SyncOperation>();
    opAB->setAffectedNode(nodeAB);
    opAB->setTargetSide(nodeAB->side());
    opAB->setType(OperationType::Create);

    SyncOpPtr opAAA = std::make_shared<SyncOperation>();
    opAAA->setAffectedNode(nodeAAA);
    opAAA->setTargetSide(nodeAAA->side());
    opAAA->setType(OperationType::Create);

    SyncOpPtr opAAB = std::make_shared<SyncOperation>();
    opAAB->setAffectedNode(nodeAAB);
    opAAB->setTargetSide(nodeAAB->side());
    opAAB->setType(OperationType::Create);

    // Case 1 : A AAA AA AAB AB B
    {
        _syncPal->_syncOps->pushOp(opA);
        _syncPal->_syncOps->pushOp(opAAA);
        _syncPal->_syncOps->pushOp(opAA);
        _syncPal->_syncOps->pushOp(opAAB);
        _syncPal->_syncOps->pushOp(opAB);
        _syncPal->_syncOps->pushOp(opB);
        _syncPal->_operationsSorterWorker->_unsortedList = *_syncPal->_syncOps;
        // Expected order: A AA AAA AAB AB B
        for (const auto &opId: _syncPal->_syncOps->_opSortedList) {
            SyncOpPtr op = _syncPal->_syncOps->_allOps[opId];
        }

        std::vector<SyncOpPtr> expectedRes = {opA, opAA, opAAA, opAAB, opAB, opB};

        _syncPal->_operationsSorterWorker->fixCreateBeforeCreate();

        for (const auto &opId: _syncPal->_syncOps->_opSortedList) {
            SyncOpPtr op = _syncPal->_syncOps->_allOps[opId];
        }

        size_t index = 0;
        std::list<UniqueId>::iterator it = _syncPal->_syncOps->_opSortedList.begin();
        for (; it != _syncPal->_syncOps->_opSortedList.end(); it++) {
            CPPUNIT_ASSERT(*it == expectedRes[index++]->id());
        }
    }

    // Case 2 : AAA AAB AA AB A B
    _syncPal->_syncOps->clear();
    {
        _syncPal->_syncOps->pushOp(opAAA);
        _syncPal->_syncOps->pushOp(opAAB);
        _syncPal->_syncOps->pushOp(opAA);
        _syncPal->_syncOps->pushOp(opAB);
        _syncPal->_syncOps->pushOp(opA);
        _syncPal->_syncOps->pushOp(opB);
        _syncPal->_operationsSorterWorker->_unsortedList = *_syncPal->_syncOps;
        // Expected order: A AB AA AAB AAA B
        for (const auto &opId: _syncPal->_syncOps->_opSortedList) {
            SyncOpPtr op = _syncPal->_syncOps->_allOps[opId];
        }
        std::vector<SyncOpPtr> expectedRes = {opA, opAB, opAA, opAAB, opAAA, opB};

        _syncPal->_operationsSorterWorker->fixCreateBeforeCreate();

        for (const auto &opId: _syncPal->_syncOps->_opSortedList) {
            SyncOpPtr op = _syncPal->_syncOps->_allOps[opId];
        }

        size_t index = 0;
        for (auto it = _syncPal->_syncOps->_opSortedList.begin(); it != _syncPal->_syncOps->_opSortedList.end(); ++it) {
            CPPUNIT_ASSERT(*it == expectedRes[index++]->id());
        }
    }

    // Case 3 : AA AAA B AAB AB A
    _syncPal->_syncOps->clear();
    {
        _syncPal->_syncOps->pushOp(opAA);
        _syncPal->_syncOps->pushOp(opAAA);
        _syncPal->_syncOps->pushOp(opB);
        _syncPal->_syncOps->pushOp(opAAB);
        _syncPal->_syncOps->pushOp(opAB);
        _syncPal->_syncOps->pushOp(opA);
        _syncPal->_operationsSorterWorker->_unsortedList = *_syncPal->_syncOps;
        // Expected order: B A AB AA AAB AAA
        for (const auto &opId: _syncPal->_syncOps->_opSortedList) {
            SyncOpPtr op = _syncPal->_syncOps->_allOps[opId];
        }
        std::vector<SyncOpPtr> expectedRes = {opB, opA, opAB, opAA, opAAB, opAAA};

        _syncPal->_operationsSorterWorker->fixCreateBeforeCreate();

        for (const auto &opId: _syncPal->_syncOps->_opSortedList) {
            SyncOpPtr op = _syncPal->_syncOps->_allOps[opId];
        }

        size_t index = 0;
        for (auto it = _syncPal->_syncOps->_opSortedList.begin(); it != _syncPal->_syncOps->_opSortedList.end(); ++it) {
            CPPUNIT_ASSERT(*it == expectedRes[index++]->id());
        }
    }
}

void TestOperationSorterWorker::testFixMoveBeforeMoveParentChildFilp() {
    DbNodeId dbNodeIdDir1;
    DbNodeId dbNodeIdDir11;
    DbNodeId dbNodeIdDir2;
    DbNodeId dbNodeIdDir3;

    bool constraintError = false;
    DbNode nodeDir1(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 1"), Str("Dir 1"), "d1", "r1", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir1, dbNodeIdDir1, constraintError);
    DbNode nodeDir11(0, dbNodeIdDir1, Str("Dir 1.1"), Str("Dir 1.1"), "11", "r11", testhelpers::defaultTime,
                     testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir11, dbNodeIdDir11, constraintError);
    DbNode nodeDir2(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 2"), Str("Dir 2"), "2", "r2", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir2, dbNodeIdDir2, constraintError);
    DbNode nodeDir3(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 3"), Str("Dir 3"), "3", "r3", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir3, dbNodeIdDir3, constraintError);

    std::shared_ptr<Node> rootNode(
            new Node(_syncPal->_syncDb->rootNode().nodeId(), _syncPal->updateTree(ReplicaSide::Local)->side(), Str(""),
                     NodeType::Directory, OperationType::None, _syncPal->syncDb()->rootNode().nodeIdLocal(),
                     testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, nullptr));
    std::shared_ptr<Node> node1(new Node(dbNodeIdDir1, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 1"),
                                         NodeType::Directory, OperationType::Create, "d1", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize, rootNode));
    std::shared_ptr<Node> node11(new Node(dbNodeIdDir11, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 1.1"),
                                          NodeType::Directory, OperationType::Create, "11", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, node1));
    std::shared_ptr<Node> node2(new Node(dbNodeIdDir2, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 2"),
                                         NodeType::Directory, OperationType::None, "2", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize, rootNode));
    std::shared_ptr<Node> node3(new Node(dbNodeIdDir3, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("Dir 3"),
                                         NodeType::Directory, OperationType::None, "3", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize, rootNode));
    std::shared_ptr<Node> rrootNode(
            new Node(_syncPal->_syncDb->rootNode().nodeId(), _syncPal->updateTree(ReplicaSide::Remote)->side(), Str(""),
                     NodeType::Directory, OperationType::None, _syncPal->syncDb()->rootNode().nodeIdRemote(),
                     testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, nullptr));
    std::shared_ptr<Node> rNode1(new Node(dbNodeIdDir1, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 1"),
                                          NodeType::Directory, OperationType::None, "r1", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, rrootNode));
    std::shared_ptr<Node> rNode11(new Node(dbNodeIdDir11, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 1.1"),
                                           NodeType::Directory, OperationType::None, "r11", testhelpers::defaultTime,
                                           testhelpers::defaultTime, testhelpers::defaultFileSize, rNode1));
    std::shared_ptr<Node> rNode2(new Node(dbNodeIdDir2, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 2"),
                                          NodeType::Directory, OperationType::None, "r2", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, rrootNode));
    std::shared_ptr<Node> rNode3(new Node(dbNodeIdDir3, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("Dir 3"),
                                          NodeType::Directory, OperationType::None, "r3", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, rrootNode));

    rootNode->deleteChildren(node1);
    node1->deleteChildren(node11);
    CPPUNIT_ASSERT(node1->setParentNode(node11));
    CPPUNIT_ASSERT(node11->insertChildren(node1));
    node1->insertChangeEvent(OperationType::Move);
    node1->setMoveOrigin("Dir 1");
    node1->setMoveOriginParentDbId(_syncPal->syncDb()->rootNode().nodeId());
    CPPUNIT_ASSERT(rootNode->insertChildren(node11));
    CPPUNIT_ASSERT(node11->setParentNode(rootNode));
    node11->insertChangeEvent(OperationType::Move);
    node11->setMoveOrigin("Dir 1/Dir 1.1");
    node11->setMoveOriginParentDbId(dbNodeIdDir1);

    SyncOpPtr op1 = std::make_shared<SyncOperation>();
    SyncOpPtr op2 = std::make_shared<SyncOperation>();
    SyncOpPtr op3 = std::make_shared<SyncOperation>();
    SyncOpPtr op4 = std::make_shared<SyncOperation>();
    op1->setAffectedNode(node11);
    op1->setTargetSide(node11->side());
    op1->setType(OperationType::Move);
    op2->setAffectedNode(node2);
    op3->setAffectedNode(node1);
    op3->setTargetSide(node1->side());
    op3->setType(OperationType::Move);
    op4->setAffectedNode(node3);

    _syncPal->_syncOps->pushOp(op2);
    _syncPal->_syncOps->pushOp(op3);
    _syncPal->_syncOps->pushOp(op4);
    _syncPal->_syncOps->pushOp(op1);
    _syncPal->_operationsSorterWorker->_unsortedList = *_syncPal->_syncOps;

    _syncPal->_operationsSorterWorker->fixMoveBeforeMoveHierarchyFlip();
    CPPUNIT_ASSERT(_syncPal->_syncOps->_opSortedList.back() == op3->id());
    _syncPal->_syncOps->_opSortedList.pop_back();
    CPPUNIT_ASSERT(_syncPal->_syncOps->_opSortedList.back() == op1->id());
}

void TestOperationSorterWorker::testFixImpossibleFirstMoveOp() {
    /*
    // initial situation

    //            S
    //           / \
    //         /    \
    //        N      T
    //               |
    //               Q
    //               |
    //               E
    */

    DbNodeId dbNodeIdDirn;
    DbNodeId dbNodeIdDirt;
    DbNodeId dbNodeIdDirq;
    DbNodeId dbNodeIdDire;

    bool constraintError = false;
    DbNode nodeDirn(0, _syncPal->syncDb()->rootNode().nodeId(), Str("n"), Str("n"), "n", "re", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirn, dbNodeIdDirn, constraintError);
    DbNode nodeDirt(0, dbNodeIdDirn, Str("t"), Str("t"), "t", "rq", testhelpers::defaultTime, testhelpers::defaultTime,
                    testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirt, dbNodeIdDirt, constraintError);
    DbNode nodeDirq(0, dbNodeIdDirt, Str("q"), Str("q"), "q", "rt", testhelpers::defaultTime, testhelpers::defaultTime,
                    testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirq, dbNodeIdDirq, constraintError);
    DbNode nodeDire(0, dbNodeIdDirq, Str("e"), Str("e"), "e", "rn", testhelpers::defaultTime, testhelpers::defaultTime,
                    testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDire, dbNodeIdDire, constraintError);

    std::shared_ptr<Node> nodeN(new Node(dbNodeIdDirn, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("n"),
                                         NodeType::Directory, OperationType::None, "n", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize,
                                         _syncPal->updateTree(ReplicaSide::Local)->rootNode()));
    std::shared_ptr<Node> nodeT(new Node(dbNodeIdDirt, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("t"),
                                         NodeType::Directory, OperationType::None, "t", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize,
                                         _syncPal->updateTree(ReplicaSide::Local)->rootNode()));
    std::shared_ptr<Node> nodeQ(new Node(dbNodeIdDirq, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("q"),
                                         NodeType::Directory, OperationType::None, "q", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize, nodeT));
    std::shared_ptr<Node> nodeE(new Node(dbNodeIdDire, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("e"),
                                         NodeType::Directory, OperationType::None, "e", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize, nodeQ));
    std::shared_ptr<Node> rNodeN(new Node(dbNodeIdDirn, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("n"),
                                          NodeType::Directory, OperationType::None, "rn", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize,
                                          _syncPal->updateTree(ReplicaSide::Remote)->rootNode()));
    std::shared_ptr<Node> rNodeT(new Node(dbNodeIdDirt, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("t"),
                                          NodeType::Directory, OperationType::None, "rt", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize,
                                          _syncPal->updateTree(ReplicaSide::Remote)->rootNode()));
    std::shared_ptr<Node> rNodeQ(new Node(dbNodeIdDirq, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("q"),
                                          NodeType::Directory, OperationType::None, "rq", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, rNodeT));
    std::shared_ptr<Node> rNodeE(new Node(dbNodeIdDire, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("e"),
                                          NodeType::Directory, OperationType::None, "re", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, rNodeQ));

    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeN);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeT);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeQ);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeE);

    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeN);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeT);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeQ);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeE);

    // local changes
    nodeQ->deleteChildren(nodeE);
    nodeE->insertChangeEvent(OperationType::Move);
    nodeE->setName(Str("n"));
    nodeE->setMoveOrigin("t/q/e");
    nodeE->setMoveOriginParentDbId(dbNodeIdDirq);
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->insertChildren(nodeE));
    _syncPal->updateTree(ReplicaSide::Local)->rootNode()->deleteChildren(nodeN);
    CPPUNIT_ASSERT(nodeE->setParentNode(_syncPal->updateTree(ReplicaSide::Local)->rootNode()));
    CPPUNIT_ASSERT(nodeE->insertChildren(nodeN));
    nodeN->insertChangeEvent(OperationType::Move);
    CPPUNIT_ASSERT(nodeN->setParentNode(nodeE));
    nodeN->setName(Str("g"));
    nodeN->setMoveOrigin("n");
    nodeN->setMoveOriginParentDbId(dbNodeIdDirn);

    // remote changes
    CPPUNIT_ASSERT(rNodeT->setParentNode(rNodeN));
    CPPUNIT_ASSERT(rNodeN->insertChildren(rNodeT));
    _syncPal->updateTree(ReplicaSide::Remote)->rootNode()->deleteChildren(rNodeT);
    rNodeT->insertChangeEvent(OperationType::Move);

    SyncOpPtr op1 = std::make_shared<SyncOperation>();
    SyncOpPtr op2 = std::make_shared<SyncOperation>();
    SyncOpPtr op3 = std::make_shared<SyncOperation>();
    op1->setType(OperationType::Move);
    op1->setTargetSide(ReplicaSide::Local);
    op1->setAffectedNode(nodeN);
    op1->setNewName(Str("g"));
    op1->setNewParentNode(nodeE);
    op1->setType(OperationType::Move);
    op2->setTargetSide(ReplicaSide::Local);
    op2->setAffectedNode(nodeE);
    op2->setNewName(Str("n"));
    op2->setNewParentNode(_syncPal->updateTree(ReplicaSide::Local)->rootNode());
    op3->setType(OperationType::Move);
    op3->setTargetSide(ReplicaSide::Remote);
    op3->setAffectedNode(rNodeT);
    op3->setNewName(Str("w"));

    _syncPal->_syncOps->setOpList({op1, op2, op3});
    const auto reshuffledOp = _syncPal->_operationsSorterWorker->fixImpossibleFirstMoveOp();
    CPPUNIT_ASSERT(reshuffledOp);
    CPPUNIT_ASSERT(reshuffledOp->_opSortedList.size() == 1);
    CPPUNIT_ASSERT(reshuffledOp->_opSortedList.back() == op3->id());
}

void TestOperationSorterWorker::testFindCompleteCycles() {
    DbNodeId dbNodeIdA;
    DbNodeId dbNodeIdB;
    DbNodeId dbNodeIdC;
    DbNodeId dbNodeIdD;

    bool constraintError = false;
    DbNode nodeDirn(0, _syncPal->syncDb()->rootNode().nodeId(), Str("A"), Str("A"), "la", "ra", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirn, dbNodeIdA, constraintError);
    DbNode nodeDirt(0, _syncPal->syncDb()->rootNode().nodeId(), Str("B"), Str("B"), "lb", "rb", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirt, dbNodeIdB, constraintError);
    DbNode nodeDirq(0, _syncPal->syncDb()->rootNode().nodeId(), Str("C"), Str("C"), "lc", "rc", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirq, dbNodeIdC, constraintError);
    DbNode nodeDire(0, _syncPal->syncDb()->rootNode().nodeId(), Str("D"), Str("D"), "ld", "rd", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDire, dbNodeIdD, constraintError);

    std::shared_ptr<Node> nodeA(new Node(dbNodeIdA, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("A"),
                                         NodeType::Directory, OperationType::None, "a", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize,
                                         _syncPal->updateTree(ReplicaSide::Local)->rootNode()));
    std::shared_ptr<Node> nodeB(new Node(dbNodeIdB, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("B"),
                                         NodeType::Directory, OperationType::None, "b", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize,
                                         _syncPal->updateTree(ReplicaSide::Local)->rootNode()));
    std::shared_ptr<Node> nodeC(new Node(dbNodeIdC, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("C"),
                                         NodeType::Directory, OperationType::None, "c", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize,
                                         _syncPal->updateTree(ReplicaSide::Local)->rootNode()));
    std::shared_ptr<Node> nodeD(new Node(dbNodeIdD, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("D"),
                                         NodeType::Directory, OperationType::None, "d", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize,
                                         _syncPal->updateTree(ReplicaSide::Local)->rootNode()));

    SyncOpPtr opA = std::make_shared<SyncOperation>();
    opA->setAffectedNode(nodeA);
    opA->setNewName(Str("A*"));
    SyncOpPtr opB = std::make_shared<SyncOperation>();
    opB->setAffectedNode(nodeB);
    opB->setNewName(Str("B*"));
    SyncOpPtr opC = std::make_shared<SyncOperation>();
    opC->setAffectedNode(nodeC);
    opC->setNewName(Str("C*"));
    SyncOpPtr opD = std::make_shared<SyncOperation>();
    opD->setAffectedNode(nodeD);
    opD->setNewName(Str("D*"));

    _syncPal->_operationsSorterWorker->_reorderings = {{opB, opC}, {opD, opA}, {opA, opB}, {opC, opD}};
    std::list<SyncOperationList> cycles = _syncPal->_operationsSorterWorker->findCompleteCycles();
    CPPUNIT_ASSERT(cycles.size() == 1);
    CPPUNIT_ASSERT(cycles.back()._opSortedList.back() == opD->id());
    cycles.back()._opSortedList.pop_back();
    CPPUNIT_ASSERT(cycles.back()._opSortedList.back() == opC->id());
    cycles.back()._opSortedList.pop_back();
    CPPUNIT_ASSERT(cycles.back()._opSortedList.back() == opB->id());
    cycles.back()._opSortedList.pop_back();
    CPPUNIT_ASSERT(cycles.back()._opSortedList.back() == opA->id());

    _syncPal->_operationsSorterWorker->_reorderings = {{opB, opA}, {opA, opC}, {opC, opA}};
    cycles = _syncPal->_operationsSorterWorker->findCompleteCycles();
    CPPUNIT_ASSERT(cycles.size() == 1);
    CPPUNIT_ASSERT(cycles.back()._opSortedList.back() == opC->id());
    cycles.back()._opSortedList.pop_back();
    CPPUNIT_ASSERT(cycles.back()._opSortedList.back() == opA->id());
}

void TestOperationSorterWorker::testBreakCycleEx1() {
    DbNodeId dbNodeIdDirA;

    bool constraintError = false;
    const DbNode nodeDirA(0, _syncPal->syncDb()->rootNode().nodeId(), Str("A"), Str("A"), "A", "rA", testhelpers::defaultTime,
                          testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirA, dbNodeIdDirA, constraintError);

    // initial situation

    //        S
    //        |
    //        A

    std::shared_ptr<Node> nodeA(new Node(dbNodeIdDirA, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("A"), NodeType::File,
                                         OperationType::None, "A", testhelpers::defaultTime, testhelpers::defaultTime,
                                         testhelpers::defaultFileSize, _syncPal->updateTree(ReplicaSide::Local)->rootNode()));
    std::shared_ptr<Node> rNodeA(new Node(dbNodeIdDirA, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("A"),
                                          NodeType::File, OperationType::None, "rA", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize,
                                          _syncPal->updateTree(ReplicaSide::Remote)->rootNode()));

    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeA);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeA);

    std::shared_ptr<Node> nodeNewA(new Node(std::nullopt, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("A"),
                                            NodeType::Directory, OperationType::None, "newA", testhelpers::defaultTime,
                                            testhelpers::defaultTime, testhelpers::defaultFileSize,
                                            _syncPal->updateTree(ReplicaSide::Local)->rootNode()));
    CPPUNIT_ASSERT(nodeNewA->insertChildren(nodeA));
    CPPUNIT_ASSERT(nodeA->setParentNode(nodeNewA));
    nodeA->setName(Str("subpath"));
    nodeA->setMoveOriginParentDbId(_syncPal->syncDb()->rootNode().nodeId());
    nodeA->setMoveOrigin("A");

    SyncOpPtr op1 = std::make_shared<SyncOperation>();
    SyncOpPtr op2 = std::make_shared<SyncOperation>();
    op1->setAffectedNode(nodeA);
    op1->setTargetSide(ReplicaSide::Local);
    op1->setType(OperationType::Move);
    op1->setNewName(Str("subpath"));
    op1->setNewParentNode(nodeNewA);
    op2->setAffectedNode(nodeNewA);
    op2->setTargetSide(ReplicaSide::Local);
    op2->setType(OperationType::Create);

    const auto resolutionOp = std::make_shared<SyncOperation>();
    SyncOperationList cycle;
    cycle.setOpList({op1, op2});
    _syncPal->_operationsSorterWorker->breakCycle(cycle, resolutionOp);
    CPPUNIT_ASSERT(resolutionOp->type() == OperationType::Move);
    CPPUNIT_ASSERT(resolutionOp->targetSide() == ReplicaSide::Remote);
    CPPUNIT_ASSERT(resolutionOp->affectedNode()->id() == "rA");
    CPPUNIT_ASSERT(resolutionOp->affectedNode()->idb() == op1->affectedNode()->idb());
}

void TestOperationSorterWorker::testBreakCycleEx2() {
    DbNodeId dbNodeIdDirA;
    DbNodeId dbNodeIdDirB;

    bool constraintError = false;
    DbNode nodeDirA(0, _syncPal->syncDb()->rootNode().nodeId(), Str("A"), Str("A"), "A", "rA", testhelpers::defaultTime,
                    testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirA, dbNodeIdDirA, constraintError);
    DbNode nodeDirB(0, dbNodeIdDirA, Str("B"), Str("B"), "B", "rB", testhelpers::defaultTime, testhelpers::defaultTime,
                    testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirB, dbNodeIdDirB, constraintError);

    // initial situation

    //        S
    //        |
    //        A
    //        |
    //        B
    std::shared_ptr<Node> nodeA(new Node(dbNodeIdDirA, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("A"),
                                         NodeType::Directory, OperationType::None, "A", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize,
                                         _syncPal->updateTree(ReplicaSide::Local)->rootNode()));
    std::shared_ptr<Node> nodeB(new Node(dbNodeIdDirB, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("B"),
                                         NodeType::Directory, OperationType::None, "B", testhelpers::defaultTime,
                                         testhelpers::defaultTime, testhelpers::defaultFileSize, nodeA));
    std::shared_ptr<Node> rNodeA(new Node(dbNodeIdDirA, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("A"),
                                          NodeType::Directory, OperationType::None, "rA", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize,
                                          _syncPal->updateTree(ReplicaSide::Remote)->rootNode()));
    std::shared_ptr<Node> rNodeB(new Node(dbNodeIdDirB, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("B"),
                                          NodeType::Directory, OperationType::None, "rB", testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, rNodeA));

    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->insertChildren(nodeA));
    CPPUNIT_ASSERT(nodeA->insertChildren(nodeB));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeA);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeB);
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNodeA));
    CPPUNIT_ASSERT(rNodeA->insertChildren(rNodeB));
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeA);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(nodeB);

    nodeA->insertChangeEvent(OperationType::Delete);
    nodeA->deleteChildren(nodeB);
    nodeB->insertChangeEvent(OperationType::Move);
    nodeB->setName(Str("A"));
    CPPUNIT_ASSERT(nodeB->setParentNode(_syncPal->updateTree(ReplicaSide::Local)->rootNode()));
    nodeB->setMoveOrigin("A/B");
    nodeB->setMoveOriginParentDbId(dbNodeIdDirA);

    SyncOpPtr op1 = std::make_shared<SyncOperation>();
    SyncOpPtr op2 = std::make_shared<SyncOperation>();
    op1->setAffectedNode(nodeA);
    op1->setType(OperationType::Delete);
    op1->setTargetSide(op1->affectedNode()->side());
    op2->setAffectedNode(nodeB);
    op2->setType(OperationType::Move);
    op2->setNewName(Str("A"));
    op2->setNewParentNode(_syncPal->updateTree(ReplicaSide::Local)->rootNode());
    op2->setTargetSide(op2->affectedNode()->side());

    SyncOpPtr resolutionOp = std::make_shared<SyncOperation>();
    SyncOperationList cycle;
    cycle.setOpList({op1, op2});
    _syncPal->_operationsSorterWorker->breakCycle(cycle, resolutionOp);
    CPPUNIT_ASSERT(resolutionOp->type() == OperationType::Move);
    CPPUNIT_ASSERT(resolutionOp->targetSide() == ReplicaSide::Remote);
    CPPUNIT_ASSERT(resolutionOp->affectedNode()->id() == "rA");
    CPPUNIT_ASSERT(resolutionOp->affectedNode()->type() == NodeType::Directory);
    CPPUNIT_ASSERT(resolutionOp->affectedNode()->idb() == op1->affectedNode()->idb());
}

void TestOperationSorterWorker::testBreakCycle() {
    DbNodeId dbNodeIdDirA;
    bool constraintError = false;
    const DbNode nodeDirA(0, _syncPal->syncDb()->rootNode().nodeId(), Str("A"), Str("A"), "la", "ra", testhelpers::defaultTime,
                          testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirA, dbNodeIdDirA, constraintError);

    // Initial situation
    //
    //       Root
    //        |
    //        A (la)
    const std::shared_ptr<Node> nodeLA(new Node(dbNodeIdDirA, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("A"),
                                                NodeType::Directory, OperationType::None, "la", testhelpers::defaultTime,
                                                testhelpers::defaultTime, testhelpers::defaultFileSize,
                                                _syncPal->updateTree(ReplicaSide::Local)->rootNode()));
    const std::shared_ptr<Node> nodeRA(new Node(dbNodeIdDirA, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("A"),
                                                NodeType::Directory, OperationType::None, "ra", testhelpers::defaultTime,
                                                testhelpers::defaultTime, testhelpers::defaultFileSize,
                                                _syncPal->updateTree(ReplicaSide::Remote)->rootNode()));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeLA);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(nodeRA);

    // Final situation
    //
    //       Root
    //        |
    //        A (la_new)
    //        |
    //        A* (la)
    const std::shared_ptr<Node> nodeLA_new(new Node(std::nullopt, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("A"),
                                                    NodeType::Directory, OperationType::None, "la_new", testhelpers::defaultTime,
                                                    testhelpers::defaultTime, testhelpers::defaultFileSize,
                                                    _syncPal->updateTree(ReplicaSide::Local)->rootNode()));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeLA_new);
    CPPUNIT_ASSERT(nodeLA_new->insertChildren(nodeLA));
    _syncPal->updateTree(ReplicaSide::Local)->rootNode()->deleteChildren(*nodeLA->id());

    nodeLA->setName(Str("A*"));
    nodeLA->setParentNode(nodeLA_new);
    CPPUNIT_ASSERT(nodeLA_new->insertChildren(nodeLA));

    const auto moveOp = std::make_shared<SyncOperation>();
    moveOp->setType(OperationType::Move);
    moveOp->setAffectedNode(nodeLA);
    moveOp->setCorrespondingNode(nodeRA);
    moveOp->setNewName("A*");
    moveOp->setTargetSide(ReplicaSide::Remote);
    const auto createOp = std::make_shared<SyncOperation>();
    createOp->setType(OperationType::Create);
    createOp->setAffectedNode(nodeLA_new);
    createOp->setTargetSide(ReplicaSide::Remote);

    SyncOperationList cycle;
    cycle.pushOp(moveOp);
    cycle.pushOp(createOp);

    const auto breakCycleOp = std::make_shared<SyncOperation>();
    _syncPal->_operationsSorterWorker->breakCycle(cycle, breakCycleOp);

    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Remote, breakCycleOp->targetSide());
    CPPUNIT_ASSERT_EQUAL(nodeLA, breakCycleOp->affectedNode());
    CPPUNIT_ASSERT_EQUAL(nodeRA, breakCycleOp->correspondingNode());
    CPPUNIT_ASSERT_EQUAL(true, breakCycleOp->isBreakingCycleOp());
    CPPUNIT_ASSERT(!breakCycleOp->newName().empty());
    CPPUNIT_ASSERT(breakCycleOp->newParentNode());
}

} // namespace KDC
