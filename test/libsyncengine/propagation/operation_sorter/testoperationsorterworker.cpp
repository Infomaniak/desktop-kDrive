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
using namespace CppUnit;

namespace KDC {

#ifdef _WIN32
auto &Console = std::wcout;
#else
auto &Console = std::cout;
#endif


void TestOperationSorterWorker::setUp() {
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

    _syncPal->_operationsSorterWorker =
        std::shared_ptr<OperationSorterWorker>(new OperationSorterWorker(_syncPal, "Operation Sorter", "OPSO"));
}

void TestOperationSorterWorker::tearDown() {
    ParmsDb::instance()->close();
    _syncPal->syncDb()->close();
}

void TestOperationSorterWorker::testMoveFirstAfterSecond() {
    std::shared_ptr<Node> node1 =
        std::shared_ptr<Node>(new Node(std::nullopt, ReplicaSideLocal, Str("1"), NodeTypeDirectory, std::nullopt, 0, 0, 12345));
    std::shared_ptr<Node> node2 =
        std::shared_ptr<Node>(new Node(std::nullopt, ReplicaSideLocal, Str("2"), NodeTypeDirectory, std::nullopt, 0, 0, 12345));
    std::shared_ptr<Node> node3 =
        std::shared_ptr<Node>(new Node(std::nullopt, ReplicaSideLocal, Str("3"), NodeTypeDirectory, std::nullopt, 0, 0, 12345));
    std::shared_ptr<Node> node4 =
        std::shared_ptr<Node>(new Node(std::nullopt, ReplicaSideLocal, Str("4"), NodeTypeDirectory, std::nullopt, 0, 0, 12345));
    std::shared_ptr<Node> node5 =
        std::shared_ptr<Node>(new Node(std::nullopt, ReplicaSideLocal, Str("5"), NodeTypeDirectory, std::nullopt, 0, 0, 12345));
    SyncOpPtr op1 = std::make_shared<SyncOperation>();
    SyncOpPtr op2 = std::make_shared<SyncOperation>();
    SyncOpPtr op3 = std::make_shared<SyncOperation>();
    SyncOpPtr op4 = std::make_shared<SyncOperation>();
    SyncOpPtr op5 = std::make_shared<SyncOperation>();
    op1->setAffectedNode(node1);
    op2->setAffectedNode(node2);
    op3->setAffectedNode(node3);
    op4->setAffectedNode(node4);
    op5->setAffectedNode(node5);
    _syncPal->_syncOps->pushOp(op1);
    _syncPal->_syncOps->pushOp(op2);
    _syncPal->_syncOps->pushOp(op3);
    _syncPal->_syncOps->pushOp(op4);
    _syncPal->_syncOps->pushOp(op5);
    _syncPal->_operationsSorterWorker->_unsortedList = *_syncPal->_syncOps;
    CPPUNIT_ASSERT(_syncPal->_syncOps->size() == 5);

    // op2 is moved after op4
    _syncPal->_operationsSorterWorker->moveFirstAfterSecond(op2, op4);
    int opFirstId;
    int opSecondId;
    int i = 1;
    for (const auto &opId : _syncPal->_syncOps->opSortedList()) {
        SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
        if (op == op2) {
            opFirstId = i;
        }
        if (op == op4) {
            opSecondId = i;
        }
        i++;
    }
    CPPUNIT_ASSERT(opFirstId == 4);
    CPPUNIT_ASSERT(opSecondId == 3);
    // nothing moved bc op5 is at 5 and op2 at 4
    _syncPal->_operationsSorterWorker->moveFirstAfterSecond(op5, op2);
    i = 1;
    for (const auto &opId : _syncPal->_syncOps->opSortedList()) {
        SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
        if (op == op5) {
            opFirstId = i;
        }
        if (op == op2) {
            opSecondId = i;
        }
        i++;
    }
    CPPUNIT_ASSERT(opFirstId == 5);
    CPPUNIT_ASSERT(opSecondId == 4);
    // op4 is moved at 5 and op5 at 4
    _syncPal->_operationsSorterWorker->moveFirstAfterSecond(op4, op5);
    i = 1;
    for (const auto &opId : _syncPal->_syncOps->opSortedList()) {
        SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
        if (op == op4) {
            opFirstId = i;
        }
        if (op == op5) {
            opSecondId = i;
        }
        i++;
    }
    CPPUNIT_ASSERT(opFirstId == 5);
    CPPUNIT_ASSERT(opSecondId == 4);
}

void TestOperationSorterWorker::testFixDeleteBeforeMove() {
    time_t tLoc = std::time(0);
    time_t tDrive = std::time(0);
    DbNodeId dbNodeIdDir1;
    DbNodeId dbNodeIdDir11;
    DbNodeId dbNodeIdDir111;
    DbNodeId dbNodeIdFile1111;
    DbNodeId dbNodeIdDir2;
    DbNodeId dbNodeIdDir3;
    DbNodeId dbnodeIdDir4;
    DbNodeId dbnodeIdDir5;

    bool constraintError = false;
    DbNode nodeDir1(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 1"), Str("Dir 1"), "d1", "r1", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir1, dbNodeIdDir1, constraintError);
    DbNode nodeDir11(0, dbNodeIdDir1, Str("Dir 1.1"), Str("Dir 1.1"), "11", "id drive 1.1", tLoc, tLoc, tDrive,
                     NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir11, dbNodeIdDir11, constraintError);
    DbNode nodeDir111(0, dbNodeIdDir11, Str("Dir 1.1.1"), Str("Dir 1.1.1"), "111", "id drive 111", tLoc, tLoc, tDrive,
                      NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir111, dbNodeIdDir111, constraintError);
    DbNode nodeFile1111(0, dbNodeIdDir111, Str("File 1.1.1.1"), Str("File 1.1.1.1"), "1111", "r1111", tLoc, tLoc, tDrive,
                        NodeType::NodeTypeFile, 0, "cs 1.1.1");
    _syncPal->syncDb()->insertNode(nodeFile1111, dbNodeIdFile1111, constraintError);
    DbNode nodeDir2(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 2"), Str("Dir 2"), "2", "r2", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir2, dbNodeIdDir2, constraintError);
    DbNode nodeDir3(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 3"), Str("Dir 3"), "3", "r3", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir3, dbNodeIdDir3, constraintError);
    DbNode nodeDir4(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 4"), Str("Dir 4"), "4", "r4", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir4, dbnodeIdDir4, constraintError);
    DbNode nodeDir5(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 5"), Str("Dir 5"), "5", "r5", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir5, dbnodeIdDir5, constraintError);
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;

    std::shared_ptr<Node> rootNode(new Node(_syncPal->syncDb()->rootNode().nodeId(), _syncPal->updateTree(ReplicaSideLocal)->side(), Str(""),
                                            NodeTypeDirectory, OperationTypeNone, _syncPal->syncDb()->rootNode().nodeIdLocal(),
                                            createdAt, lastmodified, size, nullptr));
    std::shared_ptr<Node> node1(new Node(dbNodeIdDir1, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 1"), NodeTypeDirectory,
                                         OperationTypeNone, "d1", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> node11(new Node(dbNodeIdDir11, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 1.1"), NodeTypeDirectory,
                                          OperationTypeNone, "11", createdAt, lastmodified, size, node1));
    std::shared_ptr<Node> node111(new Node(dbNodeIdDir111, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 1.1.1"),
                                           NodeTypeDirectory, OperationTypeNone, "111", createdAt, lastmodified, size, node11));
    std::shared_ptr<Node> node1111(new Node(dbNodeIdFile1111, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("File 1.1.1.1"),
                                            NodeTypeFile, OperationTypeNone, "1111", createdAt, lastmodified, size, node111));
    std::shared_ptr<Node> node2(new Node(dbNodeIdDir2, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 2"), NodeTypeDirectory,
                                         OperationTypeNone, "2", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> node3(new Node(dbNodeIdDir3, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 3"), NodeTypeDirectory,
                                         OperationTypeNone, "3", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> node4(new Node(dbnodeIdDir4, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 4"), NodeTypeDirectory,
                                         OperationTypeNone, "4", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> node5(new Node(dbnodeIdDir5, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 5"), NodeTypeDirectory,
                                         OperationTypeNone, "5", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> rrootNode(new Node(_syncPal->syncDb()->rootNode().nodeId(), _syncPal->updateTree(ReplicaSideRemote)->side(), Str(""),
                                             NodeTypeDirectory, OperationTypeNone, _syncPal->syncDb()->rootNode().nodeIdRemote(),
                                             createdAt, lastmodified, size, nullptr));
    std::shared_ptr<Node> rNode1(new Node(dbNodeIdDir1, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 1"), NodeTypeDirectory,
                                          OperationTypeNone, "r1", createdAt, lastmodified, size, rrootNode));
    std::shared_ptr<Node> rNode11(new Node(dbNodeIdDir11, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 1.1"), NodeTypeDirectory,
                                           OperationTypeNone, "r11", createdAt, lastmodified, size, rNode1));
    std::shared_ptr<Node> rNode111(new Node(dbNodeIdDir111, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 1.1.1"),
                                            NodeTypeDirectory, OperationTypeNone, "r111", createdAt, lastmodified, size,
                                            rNode11));
    std::shared_ptr<Node> rNode1111(new Node(dbNodeIdFile1111, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("File 1.1.1.1"),
                                             NodeTypeFile, OperationTypeNone, "r1111", createdAt, lastmodified, size, rNode111));
    std::shared_ptr<Node> rNode2(new Node(dbNodeIdDir2, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 2"), NodeTypeDirectory,
                                          OperationTypeNone, "r2", createdAt, lastmodified, size, rrootNode));
    std::shared_ptr<Node> rNode3(new Node(dbNodeIdDir3, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 3"), NodeTypeDirectory,
                                          OperationTypeNone, "r3", createdAt, lastmodified, size, rrootNode));
    std::shared_ptr<Node> rNode4(new Node(dbnodeIdDir4, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 4"), NodeTypeDirectory,
                                          OperationTypeNone, "r4", createdAt, lastmodified, size, rrootNode));
    std::shared_ptr<Node> rNode5(new Node(dbnodeIdDir5, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 5"), NodeTypeDirectory,
                                          OperationTypeNone, "r5", createdAt, lastmodified, size, rrootNode));

    SyncOpPtr op1 = std::make_shared<SyncOperation>();
    SyncOpPtr op2 = std::make_shared<SyncOperation>();
    SyncOpPtr op3 = std::make_shared<SyncOperation>();
    SyncOpPtr op4 = std::make_shared<SyncOperation>();
    SyncOpPtr op5 = std::make_shared<SyncOperation>();
    op1->setAffectedNode(node111);
    op1->setTargetSide(node111->side());
    op1->setType(OperationTypeMove);
    op2->setAffectedNode(node111);
    op2->setTargetSide(node111->side());
    op2->setType(OperationTypeDelete);
    op3->setAffectedNode(node3);

    op4->setAffectedNode(node4);
    op5->setAffectedNode(node5);

    _syncPal->_syncOps->pushOp(op5);
    _syncPal->_syncOps->pushOp(op1);
    _syncPal->_syncOps->pushOp(op4);
    _syncPal->_syncOps->pushOp(op3);
    _syncPal->_syncOps->pushOp(op2);
    _syncPal->_operationsSorterWorker->_unsortedList = *_syncPal->_syncOps;

    _syncPal->_operationsSorterWorker->fixDeleteBeforeMove();
    CPPUNIT_ASSERT(_syncPal->_syncOps->_opSortedList.back() == op1->id());
    _syncPal->_syncOps->_opSortedList.pop_back();
    CPPUNIT_ASSERT(_syncPal->_syncOps->_opSortedList.back() == op2->id());
}

void TestOperationSorterWorker::testFixMoveBeforeCreate() {
    time_t tLoc = std::time(0);
    time_t tDrive = std::time(0);
    DbNodeId dbNodeIdDir1;
    DbNodeId dbNodeIdDir11;
    DbNodeId dbNodeIdDir111;
    DbNodeId dbNodeIdFile1111;
    DbNodeId dbNodeIdDir2;
    DbNodeId dbNodeIdDir3;
    DbNodeId dbnodeIdDir4;
    DbNodeId dbnodeIdDir5;

    bool constraintError = false;
    DbNode nodeDir1(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 1"), Str("Dir 1"), "d1", "r1", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir1, dbNodeIdDir1, constraintError);
    DbNode nodeDir11(0, dbNodeIdDir1, Str("Dir 1.1"), Str("Dir 1.1"), "11", "id drive 1.1", tLoc, tLoc, tDrive,
                     NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir11, dbNodeIdDir11, constraintError);
    DbNode nodeDir111(0, dbNodeIdDir11, Str("Dir 1.1.1"), Str("Dir 1.1.1"), "111", "id drive 111", tLoc, tLoc, tDrive,
                      NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir111, dbNodeIdDir111, constraintError);
    DbNode nodeFile1111(0, dbNodeIdDir111, Str("File 1.1.1.1"), Str("File 1.1.1.1"), "1111", "r1111", tLoc, tLoc, tDrive,
                        NodeType::NodeTypeFile, 0, "cs 1.1.1");
    _syncPal->syncDb()->insertNode(nodeFile1111, dbNodeIdFile1111, constraintError);
    DbNode nodeDir2(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 2"), Str("Dir 2"), "2", "r2", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir2, dbNodeIdDir2, constraintError);
    DbNode nodeDir3(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 3"), Str("Dir 3"), "3", "r3", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir3, dbNodeIdDir3, constraintError);
    DbNode nodeDir4(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 4"), Str("Dir 4"), "4", "r4", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir4, dbnodeIdDir4, constraintError);
    DbNode nodeDir5(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 5"), Str("Dir 5"), "5", "r5", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir5, dbnodeIdDir5, constraintError);
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;

    std::shared_ptr<Node> rootNode(new Node(_syncPal->_syncDb->rootNode().nodeId(), _syncPal->_localUpdateTree->side(), Str(""),
                                            NodeTypeDirectory, OperationTypeNone, _syncPal->_syncDb->rootNode().nodeIdLocal(),
                                            createdAt, lastmodified, size, nullptr));
    std::shared_ptr<Node> node1(new Node(dbNodeIdDir1, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 1"), NodeTypeDirectory,
                                         OperationTypeNone, "d1", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> node11(new Node(dbNodeIdDir11, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 1.1"), NodeTypeDirectory,
                                          OperationTypeNone, "11", createdAt, lastmodified, size, node1));
    std::shared_ptr<Node> node111(new Node(dbNodeIdDir111, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 1.1.1"),
                                           NodeTypeDirectory, OperationTypeNone, "111", createdAt, lastmodified, size, node11));
    std::shared_ptr<Node> node1111(new Node(dbNodeIdFile1111, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("File 1.1.1.1"),
                                            NodeTypeFile, OperationTypeNone, "1111", createdAt, lastmodified, size, node111));
    std::shared_ptr<Node> node2(new Node(dbNodeIdDir2, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 2"), NodeTypeDirectory,
                                         OperationTypeNone, "2", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> node3(new Node(dbNodeIdDir3, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 3"), NodeTypeDirectory,
                                         OperationTypeNone, "3", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> node4(new Node(dbnodeIdDir4, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 4"), NodeTypeDirectory,
                                         OperationTypeNone, "4", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> node5(new Node(dbnodeIdDir5, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 5"), NodeTypeDirectory,
                                         OperationTypeNone, "5", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> rrootNode(new Node(_syncPal->_syncDb->rootNode().nodeId(), _syncPal->_remoteUpdateTree->side(), Str(""),
                                             NodeTypeDirectory, OperationTypeNone, _syncPal->_syncDb->rootNode().nodeIdRemote(),
                                             createdAt, lastmodified, size, nullptr));
    std::shared_ptr<Node> rNode1(new Node(dbNodeIdDir1, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 1"), NodeTypeDirectory,
                                          OperationTypeNone, "r1", createdAt, lastmodified, size, rrootNode));
    std::shared_ptr<Node> rNode11(new Node(dbNodeIdDir11, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 1.1"), NodeTypeDirectory,
                                           OperationTypeNone, "r11", createdAt, lastmodified, size, rNode1));
    std::shared_ptr<Node> rNode111(new Node(dbNodeIdDir111, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 1.1.1"),
                                            NodeTypeDirectory, OperationTypeNone, "r111", createdAt, lastmodified, size,
                                            rNode11));
    std::shared_ptr<Node> rNode1111(new Node(dbNodeIdFile1111, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("File 1.1.1.1"),
                                             NodeTypeFile, OperationTypeNone, "r1111", createdAt, lastmodified, size, rNode111));
    std::shared_ptr<Node> rNode2(new Node(dbNodeIdDir2, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 2"), NodeTypeDirectory,
                                          OperationTypeNone, "r2", createdAt, lastmodified, size, rrootNode));
    std::shared_ptr<Node> rNode3(new Node(dbNodeIdDir3, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 3"), NodeTypeDirectory,
                                          OperationTypeNone, "r3", createdAt, lastmodified, size, rrootNode));
    std::shared_ptr<Node> rNode4(new Node(dbnodeIdDir4, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 4"), NodeTypeDirectory,
                                          OperationTypeNone, "r4", createdAt, lastmodified, size, rrootNode));
    std::shared_ptr<Node> rNode5(new Node(dbnodeIdDir5, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 5"), NodeTypeDirectory,
                                          OperationTypeNone, "r5", createdAt, lastmodified, size, rrootNode));
    std::shared_ptr<Node> node111C(new Node(std::nullopt, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 1.1.1"), NodeTypeDirectory,
                                            OperationTypeCreate, "111c", createdAt, lastmodified, size, node3));

    node111->setMoveOrigin("Dir 3/Dir 1.1.1");
    node111->setMoveOriginParentDbId(dbNodeIdDir3);

    SyncOpPtr op1 = std::make_shared<SyncOperation>();
    SyncOpPtr op2 = std::make_shared<SyncOperation>();
    SyncOpPtr op3 = std::make_shared<SyncOperation>();
    SyncOpPtr op4 = std::make_shared<SyncOperation>();
    SyncOpPtr op5 = std::make_shared<SyncOperation>();
    op1->setAffectedNode(node111);
    op1->setTargetSide(node111->side());
    op1->setType(OperationTypeMove);
    op2->setAffectedNode(node111C);
    op2->setTargetSide(node111C->side());
    op2->setType(OperationTypeCreate);
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
    time_t tLoc = std::time(0);
    time_t tDrive = std::time(0);
    DbNodeId dbNodeIdDir1;
    DbNodeId dbNodeIdDir11;
    DbNodeId dbNodeIdDir111;
    DbNodeId dbNodeIdFile1111;
    DbNodeId dbNodeIdDir2;
    DbNodeId dbNodeIdDir3;
    DbNodeId dbnodeIdDir4;
    DbNodeId dbnodeIdDir5;

    bool constraintError = false;
    DbNode nodeDir1(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 1"), Str("Dir 1"), "d1", "r1", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir1, dbNodeIdDir1, constraintError);
    DbNode nodeDir11(0, dbNodeIdDir1, Str("Dir 1.1"), Str("Dir 1.1"), "11", "r11", tLoc, tLoc, tDrive,
                     NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir11, dbNodeIdDir11, constraintError);
    DbNode nodeDir111(0, dbNodeIdDir11, Str("Dir 1.1.1"), Str("Dir 1.1.1"), "111", "r111", tLoc, tLoc, tDrive,
                      NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir111, dbNodeIdDir111, constraintError);
    DbNode nodeFile1111(0, dbNodeIdDir111, Str("File 1.1.1.1"), Str("File 1.1.1.1"), "1111", "r1111", tLoc, tLoc, tDrive,
                        NodeType::NodeTypeFile, 0, "cs 1.1.1");
    _syncPal->syncDb()->insertNode(nodeFile1111, dbNodeIdFile1111, constraintError);
    DbNode nodeDir2(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 2"), Str("Dir 2"), "2", "r2", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir2, dbNodeIdDir2, constraintError);
    DbNode nodeDir3(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 3"), Str("Dir 3"), "3", "r3", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir3, dbNodeIdDir3, constraintError);
    DbNode nodeDir4(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 4"), Str("Dir 4"), "4", "r4", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir4, dbnodeIdDir4, constraintError);
    DbNode nodeDir5(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 5"), Str("Dir 5"), "5", "r5", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir5, dbnodeIdDir5, constraintError);
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;

    std::shared_ptr<Node> rootNode(new Node(_syncPal->syncDb()->rootNode().nodeId(), _syncPal->updateTree(ReplicaSideLocal)->side(), Str(""),
                                            NodeTypeDirectory, OperationTypeNone, _syncPal->syncDb()->rootNode().nodeIdLocal(),
                                            createdAt, lastmodified, size, nullptr));
    std::shared_ptr<Node> node1(new Node(dbNodeIdDir1, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 1"), NodeTypeDirectory,
                                         OperationTypeNone, "d1", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> node11(new Node(dbNodeIdDir11, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 1.1"), NodeTypeDirectory,
                                          OperationTypeNone, "11", createdAt, lastmodified, size, node1));
    std::shared_ptr<Node> node2(new Node(dbNodeIdDir2, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 2"), NodeTypeDirectory,
                                         OperationTypeNone, "2", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> node111(new Node(dbNodeIdDir111, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 1.1.1"),
                                           NodeTypeDirectory, OperationTypeNone, "111", createdAt, lastmodified, size, node2));
    std::shared_ptr<Node> node1111(new Node(dbNodeIdFile1111, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("File 1.1.1.1"),
                                            NodeTypeFile, OperationTypeNone, "1111", createdAt, lastmodified, size, node111));
    std::shared_ptr<Node> node3(new Node(dbNodeIdDir3, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 3"), NodeTypeDirectory,
                                         OperationTypeNone, "3", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> node4(new Node(dbnodeIdDir4, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 4"), NodeTypeDirectory,
                                         OperationTypeNone, "4", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> node5(new Node(dbnodeIdDir5, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 5"), NodeTypeDirectory,
                                         OperationTypeNone, "5", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> rrootNode(new Node(_syncPal->syncDb()->rootNode().nodeId(), _syncPal->updateTree(ReplicaSideRemote)->side(), Str(""),
                                             NodeTypeDirectory, OperationTypeNone, _syncPal->syncDb()->rootNode().nodeIdRemote(),
                                             createdAt, lastmodified, size, nullptr));
    std::shared_ptr<Node> rNode1(new Node(dbNodeIdDir1, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 1"), NodeTypeDirectory,
                                          OperationTypeNone, "r1", createdAt, lastmodified, size, rrootNode));
    std::shared_ptr<Node> rNode11(new Node(dbNodeIdDir11, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 1.1"), NodeTypeDirectory,
                                           OperationTypeNone, "r11", createdAt, lastmodified, size, rNode1));
    std::shared_ptr<Node> rNode111(new Node(dbNodeIdDir111, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 1.1.1"),
                                            NodeTypeDirectory, OperationTypeNone, "r111", createdAt, lastmodified, size,
                                            rNode11));
    std::shared_ptr<Node> rNode1111(new Node(dbNodeIdFile1111, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("File 1.1.1.1"),
                                             NodeTypeFile, OperationTypeNone, "r1111", createdAt, lastmodified, size, rNode111));
    std::shared_ptr<Node> rNode2(new Node(dbNodeIdDir2, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 2"), NodeTypeDirectory,
                                          OperationTypeNone, "r2", createdAt, lastmodified, size, rrootNode));
    std::shared_ptr<Node> rNode3(new Node(dbNodeIdDir3, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 3"), NodeTypeDirectory,
                                          OperationTypeNone, "r3", createdAt, lastmodified, size, rrootNode));
    std::shared_ptr<Node> rNode4(new Node(dbnodeIdDir4, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 4"), NodeTypeDirectory,
                                          OperationTypeNone, "r4", createdAt, lastmodified, size, rrootNode));
    std::shared_ptr<Node> rNode5(new Node(dbnodeIdDir5, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 5"), NodeTypeDirectory,
                                          OperationTypeNone, "r5", createdAt, lastmodified, size, rrootNode));

    node111->setMoveOrigin("Dir 1/Dir 1.1/Dir 1.1.1");
    node111->setMoveOriginParentDbId(dbNodeIdDir11);

    SyncOpPtr op1 = std::make_shared<SyncOperation>();
    SyncOpPtr op2 = std::make_shared<SyncOperation>();
    SyncOpPtr op3 = std::make_shared<SyncOperation>();
    SyncOpPtr op4 = std::make_shared<SyncOperation>();
    SyncOpPtr op5 = std::make_shared<SyncOperation>();
    op1->setAffectedNode(node111);
    op1->setTargetSide(node111->side());
    op1->setType(OperationTypeMove);
    op2->setAffectedNode(node1);
    op2->setTargetSide(node1->side());
    op2->setType(OperationTypeDelete);
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
    time_t tLoc = std::time(0);
    time_t tDrive = std::time(0);
    DbNodeId dbNodeIdDir1;
    DbNodeId dbNodeIdDir11;
    DbNodeId dbNodeIdDir111;
    DbNodeId dbNodeIdFile1111;
    DbNodeId dbNodeIdDir2;
    DbNodeId dbNodeIdDir3;
    DbNodeId dbnodeIdDir4;
    DbNodeId dbnodeIdDir5;

    bool constraintError = false;
    DbNode nodeDir1(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 1"), Str("Dir 1"), "d1", "r1", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir1, dbNodeIdDir1, constraintError);
    DbNode nodeDir11(0, dbNodeIdDir1, Str("Dir 1.1"), Str("Dir 1.1"), "11", "id drive 1.1", tLoc, tLoc, tDrive,
                     NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir11, dbNodeIdDir11, constraintError);
    DbNode nodeDir111(0, dbNodeIdDir11, Str("Dir 1.1.1"), Str("Dir 1.1.1"), "111", "id drive 111", tLoc, tLoc, tDrive,
                      NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir111, dbNodeIdDir111, constraintError);
    DbNode nodeFile1111(0, dbNodeIdDir111, Str("File 1.1.1.1"), Str("File 1.1.1.1"), "1111", "r1111", tLoc, tLoc, tDrive,
                        NodeType::NodeTypeFile, 0, "cs 1.1.1");
    _syncPal->syncDb()->insertNode(nodeFile1111, dbNodeIdFile1111, constraintError);
    DbNode nodeDir2(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 2"), Str("Dir 2"), "2", "r2", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir2, dbNodeIdDir2, constraintError);
    DbNode nodeDir3(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 3"), Str("Dir 3"), "3", "r3", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir3, dbNodeIdDir3, constraintError);
    DbNode nodeDir4(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 4"), Str("Dir 4"), "4", "r4", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir4, dbnodeIdDir4, constraintError);
    DbNode nodeDir5(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 5"), Str("Dir 5"), "5", "r5", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir5, dbnodeIdDir5, constraintError);
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;

    std::shared_ptr<Node> rootNode(new Node(_syncPal->_syncDb->rootNode().nodeId(), _syncPal->_localUpdateTree->side(), Str(""),
                                            NodeTypeDirectory, OperationTypeNone, _syncPal->syncDb()->rootNode().nodeIdLocal(),
                                            createdAt, lastmodified, size, nullptr));
    std::shared_ptr<Node> node1(new Node(dbNodeIdDir1, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 1"), NodeTypeDirectory,
                                         OperationTypeNone, "d1", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> node11(new Node(std::nullopt, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 1.1"), NodeTypeDirectory,
                                          OperationTypeNone, "11", createdAt, lastmodified, size, node1));
    std::shared_ptr<Node> node111(new Node(dbNodeIdDir111, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 1.1.1"),
                                           NodeTypeDirectory, OperationTypeNone, "111", createdAt, lastmodified, size, node11));
    std::shared_ptr<Node> node1111(new Node(dbNodeIdFile1111, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("File 1.1.1.1"),
                                            NodeTypeFile, OperationTypeNone, "1111", createdAt, lastmodified, size, node111));
    std::shared_ptr<Node> node2(new Node(dbNodeIdDir2, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 2"), NodeTypeDirectory,
                                         OperationTypeNone, "2", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> node3(new Node(dbNodeIdDir3, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 3"), NodeTypeDirectory,
                                         OperationTypeNone, "3", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> node4(new Node(dbnodeIdDir4, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 4"), NodeTypeDirectory,
                                         OperationTypeNone, "4", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> node5(new Node(dbnodeIdDir5, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 5"), NodeTypeDirectory,
                                         OperationTypeNone, "5", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> rrootNode(new Node(_syncPal->_syncDb->rootNode().nodeId(), _syncPal->_remoteUpdateTree->side(), Str(""),
                                             NodeTypeDirectory, OperationTypeNone, _syncPal->syncDb()->rootNode().nodeIdRemote(),
                                             createdAt, lastmodified, size, nullptr));
    std::shared_ptr<Node> rNode1(new Node(dbNodeIdDir1, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 1"), NodeTypeDirectory,
                                          OperationTypeNone, "r1", createdAt, lastmodified, size, rrootNode));
    std::shared_ptr<Node> rNode11(new Node(dbNodeIdDir11, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 1.1"), NodeTypeDirectory,
                                           OperationTypeNone, "r11", createdAt, lastmodified, size, rNode1));
    std::shared_ptr<Node> rNode111(new Node(dbNodeIdDir111, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 1.1.1"),
                                            NodeTypeDirectory, OperationTypeNone, "r111", createdAt, lastmodified, size,
                                            rNode11));
    std::shared_ptr<Node> rNode1111(new Node(dbNodeIdFile1111, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("File 1.1.1.1"),
                                             NodeTypeFile, OperationTypeNone, "r1111", createdAt, lastmodified, size, rNode111));
    std::shared_ptr<Node> rNode2(new Node(dbNodeIdDir2, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 2"), NodeTypeDirectory,
                                          OperationTypeNone, "r2", createdAt, lastmodified, size, rrootNode));
    std::shared_ptr<Node> rNode3(new Node(dbNodeIdDir3, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 3"), NodeTypeDirectory,
                                          OperationTypeNone, "r3", createdAt, lastmodified, size, rrootNode));
    std::shared_ptr<Node> rNode4(new Node(dbnodeIdDir4, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 4"), NodeTypeDirectory,
                                          OperationTypeNone, "r4", createdAt, lastmodified, size, rrootNode));
    std::shared_ptr<Node> rNode5(new Node(dbnodeIdDir5, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 5"), NodeTypeDirectory,
                                          OperationTypeNone, "r5", createdAt, lastmodified, size, rrootNode));

    rootNode->insertChildren(node1);
    node1->insertChildren(node11);
    node11->insertChildren(node111);
    node111->insertChildren(node1111);

    SyncOpPtr op1 = std::make_shared<SyncOperation>();
    SyncOpPtr op2 = std::make_shared<SyncOperation>();
    SyncOpPtr op3 = std::make_shared<SyncOperation>();
    SyncOpPtr op4 = std::make_shared<SyncOperation>();
    SyncOpPtr op5 = std::make_shared<SyncOperation>();
    op1->setAffectedNode(node111);
    op1->setTargetSide(node111->side());
    op1->setType(OperationTypeMove);
    op2->setAffectedNode(node11);
    op2->setTargetSide(node11->side());
    op2->setType(OperationTypeCreate);
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
    time_t tLoc = std::time(0);
    time_t tDrive = std::time(0);
    DbNodeId dbNodeIdDirA;

    bool constraintError = false;
    DbNode nodeDir1(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir A"), Str("Dir A"), "dA", "rdA", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir1, dbNodeIdDirA, constraintError);

    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;

    std::shared_ptr<Node> rootNode(new Node(_syncPal->_syncDb->rootNode().nodeId(), _syncPal->updateTree(ReplicaSideLocal)->side(), Str(""),
                                            NodeTypeDirectory, OperationTypeNone, _syncPal->syncDb()->rootNode().nodeIdLocal(),
                                            createdAt, lastmodified, size, nullptr));
    std::shared_ptr<Node> nodeA(new Node(dbNodeIdDirA, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir A"), NodeTypeDirectory,
                                         OperationTypeMove, "dA", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> rrootNode(new Node(_syncPal->_syncDb->rootNode().nodeId(), _syncPal->_remoteUpdateTree->side(), Str(""),
                                             NodeTypeDirectory, OperationTypeNone, _syncPal->syncDb()->rootNode().nodeIdRemote(),
                                             createdAt, lastmodified, size, nullptr));
    std::shared_ptr<Node> rNodeA(new Node(dbNodeIdDirA, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir A"), NodeTypeDirectory,
                                          OperationTypeNone, "rdA", createdAt, lastmodified, size, rrootNode));


    rootNode->insertChildren(nodeA);

    // Rename Dir A -> Dir B
    nodeA->setMoveOrigin("Dir A");
    nodeA->setMoveOriginParentDbId(_syncPal->syncDb()->rootNode().nodeId());
    nodeA->setName(Str("Dir B"));

    // Create Dir A
    std::shared_ptr<Node> nodeABis(new Node(std::nullopt, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir A"), NodeTypeDirectory,
                                            OperationTypeCreate, "dABis", createdAt, lastmodified, size, rootNode));

    SyncOpPtr op1 = std::make_shared<SyncOperation>();
    SyncOpPtr op2 = std::make_shared<SyncOperation>();
    op1->setAffectedNode(nodeA);
    op1->setTargetSide(nodeA->side());
    op1->setType(OperationTypeMove);
    op2->setAffectedNode(nodeABis);
    op2->setTargetSide(nodeABis->side());
    op2->setType(OperationTypeCreate);

    _syncPal->_syncOps->pushOp(op2);
    _syncPal->_syncOps->pushOp(op1);
    _syncPal->_operationsSorterWorker->_unsortedList = *_syncPal->_syncOps;

    _syncPal->_operationsSorterWorker->fixMoveBeforeCreate();
    CPPUNIT_ASSERT(_syncPal->_syncOps->_opSortedList.back() == op2->id());
    _syncPal->_syncOps->_opSortedList.pop_back();
    CPPUNIT_ASSERT(_syncPal->_syncOps->_opSortedList.back() == op1->id());
}

void TestOperationSorterWorker::testFixDeleteBeforeCreate() {
    time_t tLoc = std::time(0);
    time_t tDrive = std::time(0);
    DbNodeId dbNodeIdDir1;
    DbNodeId dbNodeIdDir11;
    DbNodeId dbNodeIdDir111;
    DbNodeId dbNodeIdFile1111;
    DbNodeId dbNodeIdDir2;
    DbNodeId dbNodeIdDir3;

    bool constraintError = false;
    DbNode nodeDir1(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 1"), Str("Dir 1"), "d1", "r1", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir1, dbNodeIdDir1, constraintError);
    DbNode nodeDir11(0, dbNodeIdDir1, Str("Dir 1.1"), Str("Dir 1.1"), "11", "r11", tLoc, tLoc, tDrive,
                     NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir11, dbNodeIdDir11, constraintError);
    DbNode nodeDir111(0, dbNodeIdDir11, Str("Dir 1.1.1"), Str("Dir 1.1.1"), "111", "r111", tLoc, tLoc, tDrive,
                      NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir111, dbNodeIdDir111, constraintError);
    DbNode nodeFile1111(0, dbNodeIdDir111, Str("File 1.1.1.1"), Str("File 1.1.1.1"), "1111", "r1111", tLoc, tLoc, tDrive,
                        NodeType::NodeTypeFile, 0, "cs 1.1.1");
    _syncPal->syncDb()->insertNode(nodeFile1111, dbNodeIdFile1111, constraintError);
    DbNode nodeDir2(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 2"), Str("Dir 2"), "2", "r2", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir2, dbNodeIdDir2, constraintError);
    DbNode nodeDir3(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 3"), Str("Dir 3"), "3", "r3", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir3, dbNodeIdDir3, constraintError);
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;

    std::shared_ptr<Node> rootNode(new Node(_syncPal->_syncDb->rootNode().nodeId(), _syncPal->updateTree(ReplicaSideLocal)->side(), Str(""),
                                            NodeTypeDirectory, OperationTypeNone, _syncPal->syncDb()->rootNode().nodeIdLocal(),
                                            createdAt, lastmodified, size, nullptr));
    std::shared_ptr<Node> node1(new Node(dbNodeIdDir1, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 1"), NodeTypeDirectory,
                                         OperationTypeNone, "d1", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> node11(new Node(dbNodeIdDir11, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 1.1"), NodeTypeDirectory,
                                          OperationTypeNone, "11", createdAt, lastmodified, size, node1));
    std::shared_ptr<Node> node111(new Node(dbNodeIdDir111, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 1.1.1"),
                                           NodeTypeDirectory, OperationTypeNone, "111", createdAt, lastmodified, size, node11));
    std::shared_ptr<Node> node1111(new Node(dbNodeIdFile1111, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("File 1.1.1.1"),
                                            NodeTypeFile, OperationTypeNone, "1111", createdAt, lastmodified, size, node111));
    std::shared_ptr<Node> node2(new Node(dbNodeIdDir2, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 2"), NodeTypeDirectory,
                                         OperationTypeNone, "2", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> node3(new Node(dbNodeIdDir3, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 3"), NodeTypeDirectory,
                                         OperationTypeNone, "3", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> rrootNode(new Node(_syncPal->_syncDb->rootNode().nodeId(), _syncPal->_remoteUpdateTree->side(), Str(""),
                                             NodeTypeDirectory, OperationTypeNone, _syncPal->syncDb()->rootNode().nodeIdRemote(),
                                             createdAt, lastmodified, size, nullptr));
    std::shared_ptr<Node> rNode1(new Node(dbNodeIdDir1, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 1"), NodeTypeDirectory,
                                          OperationTypeNone, "r1", createdAt, lastmodified, size, rrootNode));
    std::shared_ptr<Node> rNode11(new Node(dbNodeIdDir11, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 1.1"), NodeTypeDirectory,
                                           OperationTypeNone, "r11", createdAt, lastmodified, size, rNode1));
    std::shared_ptr<Node> rNode111(new Node(dbNodeIdDir111, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 1.1.1"),
                                            NodeTypeDirectory, OperationTypeNone, "r111", createdAt, lastmodified, size,
                                            rNode11));
    std::shared_ptr<Node> rNode1111(new Node(dbNodeIdFile1111, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("File 1.1.1.1"),
                                             NodeTypeFile, OperationTypeNone, "r1111", createdAt, lastmodified, size, rNode111));
    std::shared_ptr<Node> rNode2(new Node(dbNodeIdDir2, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 2"), NodeTypeDirectory,
                                          OperationTypeNone, "r2", createdAt, lastmodified, size, rrootNode));
    std::shared_ptr<Node> rNode3(new Node(dbNodeIdDir3, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 3"), NodeTypeDirectory,
                                          OperationTypeNone, "r3", createdAt, lastmodified, size, rrootNode));

    _syncPal->_localSnapshot->updateItem(SnapshotItem("d1", _syncPal->syncDb()->rootNode().nodeIdLocal().value(), Str("Dir 1"),
                                                      createdAt, lastmodified, NodeTypeDirectory, size));
    _syncPal->_localSnapshot->updateItem(
        SnapshotItem("11", "d1", Str("Dir 1.1"), createdAt, lastmodified, NodeTypeDirectory, size));
    _syncPal->_localSnapshot->updateItem(
        SnapshotItem("111", "11", Str("Dir 1.1.1"), createdAt, lastmodified, NodeTypeDirectory, size));
    _syncPal->_localSnapshot->updateItem(
        SnapshotItem("1111c", "111", Str("File 1.1.1.1"), createdAt, lastmodified, NodeTypeDirectory, size));
    _syncPal->_localSnapshot->updateItem(SnapshotItem("2", _syncPal->syncDb()->rootNode().nodeIdLocal().value(), Str("Dir 2"),
                                                      createdAt, lastmodified, NodeTypeDirectory, size));
    _syncPal->_localSnapshot->updateItem(SnapshotItem("3", _syncPal->syncDb()->rootNode().nodeIdLocal().value(), Str("Dir 3"),
                                                      createdAt, lastmodified, NodeTypeDirectory, size));

    _syncPal->_remoteSnapshot->updateItem(SnapshotItem("r1", _syncPal->syncDb()->rootNode().nodeIdRemote().value(), Str("Dir 1"),
                                                       createdAt, lastmodified, NodeTypeDirectory, size));
    _syncPal->_remoteSnapshot->updateItem(
        SnapshotItem("r11", "r1", Str("Dir 1.1"), createdAt, lastmodified, NodeTypeDirectory, size));
    _syncPal->_remoteSnapshot->updateItem(
        SnapshotItem("r111", "r11", Str("Dir 1.1.1"), createdAt, lastmodified, NodeTypeDirectory, size));
    _syncPal->_remoteSnapshot->updateItem(
        SnapshotItem("r1111", "r111", Str("File 1.1.1.1"), createdAt, lastmodified, NodeTypeDirectory, size));
    _syncPal->_remoteSnapshot->updateItem(SnapshotItem("r2", _syncPal->syncDb()->rootNode().nodeIdRemote().value(), Str("Dir 2"),
                                                       createdAt, lastmodified, NodeTypeDirectory, size));
    _syncPal->_remoteSnapshot->updateItem(SnapshotItem("r3", _syncPal->syncDb()->rootNode().nodeIdRemote().value(), Str("Dir 3"),
                                                       createdAt, lastmodified, NodeTypeDirectory, size));

    std::shared_ptr<Node> node1111C(new Node(std::nullopt, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("File 1.1.1.1"), NodeTypeFile,
                                             OperationTypeCreate, "1111c", createdAt, lastmodified, size, node111));

    SyncOpPtr op1 = std::make_shared<SyncOperation>();
    SyncOpPtr op2 = std::make_shared<SyncOperation>();
    SyncOpPtr op3 = std::make_shared<SyncOperation>();
    SyncOpPtr op4 = std::make_shared<SyncOperation>();
    SyncOpPtr op5 = std::make_shared<SyncOperation>();
    op1->setAffectedNode(node1111);
    op1->setTargetSide(node1111->side());
    op1->setType(OperationTypeDelete);
    op2->setAffectedNode(node1111C);
    op2->setTargetSide(node1111C->side());
    op2->setType(OperationTypeCreate);
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
    time_t tLoc = std::time(0);
    time_t tDrive = std::time(0);
    DbNodeId dbNodeIdDir1;
    DbNodeId dbNodeIdDir11;
    DbNodeId dbNodeIdDir2;
    DbNodeId dbNodeIdDir3;

    bool constraintError = true;
    DbNode nodeDir1(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 1"), Str("Dir 1"), "d1", "r1", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir1, dbNodeIdDir1, constraintError);
    DbNode nodeDir11(0, dbNodeIdDir1, Str("Dir 1.1"), Str("Dir 1.1"), "11", "r11", tLoc, tLoc, tDrive,
                     NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir11, dbNodeIdDir11, constraintError);
    DbNode nodeDir2(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 2"), Str("Dir 2"), "2", "r2", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir2, dbNodeIdDir2, constraintError);
    DbNode nodeDir3(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 3"), Str("Dir 3"), "3", "r3", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir3, dbNodeIdDir3, constraintError);
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;

    std::shared_ptr<Node> rootNode(new Node(_syncPal->_syncDb->rootNode().nodeId(), _syncPal->_localUpdateTree->side(), Str(""),
                                            NodeTypeDirectory, OperationTypeNone, _syncPal->_syncDb->rootNode().nodeIdLocal(),
                                            createdAt, lastmodified, size, nullptr));
    std::shared_ptr<Node> node1(new Node(dbNodeIdDir1, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 1"), NodeTypeDirectory,
                                         OperationTypeNone, "d1", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> node11(new Node(dbNodeIdDir11, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 1.1"), NodeTypeDirectory,
                                          OperationTypeNone, "11", createdAt, lastmodified, size, node1));
    std::shared_ptr<Node> node2(new Node(dbNodeIdDir2, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 2"), NodeTypeDirectory,
                                         OperationTypeNone, "2", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> node3(new Node(dbNodeIdDir3, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 3"), NodeTypeDirectory,
                                         OperationTypeNone, "3", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> rrootNode(new Node(_syncPal->_syncDb->rootNode().nodeId(), _syncPal->updateTree(ReplicaSideRemote)->side(), Str(""),
                                             NodeTypeDirectory, OperationTypeNone, _syncPal->syncDb()->rootNode().nodeIdRemote(),
                                             createdAt, lastmodified, size, nullptr));
    std::shared_ptr<Node> rNode1(new Node(dbNodeIdDir1, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 1"), NodeTypeDirectory,
                                          OperationTypeNone, "r1", createdAt, lastmodified, size, rrootNode));
    std::shared_ptr<Node> rNode11(new Node(dbNodeIdDir11, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 1.1"), NodeTypeDirectory,
                                           OperationTypeNone, "r11", createdAt, lastmodified, size, rNode1));
    std::shared_ptr<Node> rNode2(new Node(dbNodeIdDir2, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 2"), NodeTypeDirectory,
                                          OperationTypeNone, "r2", createdAt, lastmodified, size, rrootNode));
    std::shared_ptr<Node> rNode3(new Node(dbNodeIdDir3, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 3"), NodeTypeDirectory,
                                          OperationTypeNone, "r3", createdAt, lastmodified, size, rrootNode));

    node11->setMoveOrigin("Dir 1/Dir 1.1");
    node11->setMoveOriginParentDbId(dbNodeIdDir1);
    node11->setParentNode(node2);
    node2->insertChildren(node11);
    node1->deleteChildren(node11);
    node11->insertChangeEvent(OperationTypeMove);

    node3->insertChangeEvent(OperationTypeMove);
    node3->setParentNode(node1);
    node3->setName(Str("Dir 1.1"));
    node3->setMoveOriginParentDbId(_syncPal->syncDb()->rootNode().nodeId());
    node3->setMoveOrigin("Dir 3");
    node1->insertChildren(node3);
    rootNode->deleteChildren(node3);

    SyncOpPtr op1 = std::make_shared<SyncOperation>();
    SyncOpPtr op2 = std::make_shared<SyncOperation>();
    SyncOpPtr op3 = std::make_shared<SyncOperation>();
    SyncOpPtr op4 = std::make_shared<SyncOperation>();
    SyncOpPtr op5 = std::make_shared<SyncOperation>();
    op1->setAffectedNode(node11);
    op1->setTargetSide(node11->side());
    op1->setType(OperationTypeMove);
    op2->setAffectedNode(node3);
    op2->setTargetSide(node3->side());
    op2->setType(OperationTypeMove);
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

    time_t tLoc = std::time(0);
    time_t tDrive = std::time(0);
    DbNodeId dbNodeIdDirA;
    DbNodeId dbNodeIdDirB;
    DbNodeId dbNodeIdDirAA;
    DbNodeId dbNodeIdDirAB;
    DbNodeId dbNodeIdDirAAA;
    DbNodeId dbNodeIdDirAAB;

    bool constraintError = false;
    DbNode nodeDirA(0, _syncPal->syncDb()->rootNode().nodeId(), Str("A"), Str("A"), "la", "ra", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirA, dbNodeIdDirA, constraintError);
    DbNode nodeDirB(0, _syncPal->syncDb()->rootNode().nodeId(), Str("B"), Str("B"), "lb", "rb", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirB, dbNodeIdDirB, constraintError);
    DbNode nodeDirAA(0, dbNodeIdDirA, Str("AA"), Str("AA"), "laa", "raa", tLoc, tLoc, tDrive, NodeType::NodeTypeDirectory, 0,
                     std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirAA, dbNodeIdDirAA, constraintError);
    DbNode nodeDirAB(0, dbNodeIdDirA, Str("AB"), Str("AB"), "lab", "rab", tLoc, tLoc, tDrive, NodeType::NodeTypeDirectory, 0,
                     std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirAB, dbNodeIdDirAB, constraintError);
    DbNode nodeDirAAA(0, dbNodeIdDirAA, Str("AAA"), Str("AAA"), "laaa", "raaa", tLoc, tLoc, tDrive, NodeType::NodeTypeFile, 0,
                      std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirAAA, dbNodeIdDirAAA, constraintError);
    DbNode nodeDirAAB(0, dbNodeIdDirAA, Str("AAB"), Str("AAB"), "laab", "raab", tLoc, tLoc, tDrive, NodeType::NodeTypeFile, 0,
                      std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirAAB, dbNodeIdDirAAB, constraintError);

    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;
    std::shared_ptr<Node> rootNode(new Node(_syncPal->_syncDb->rootNode().nodeId(), _syncPal->updateTree(ReplicaSideLocal)->side(), Str(""),
                                            NodeTypeDirectory, OperationTypeNone, _syncPal->syncDb()->rootNode().nodeIdLocal(),
                                            createdAt, lastmodified, size, nullptr));
    std::shared_ptr<Node> nodeA(new Node(dbNodeIdDirA, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("A"), NodeTypeDirectory,
                                         OperationTypeCreate, "a", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> nodeB(new Node(dbNodeIdDirB, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("B"), NodeTypeDirectory,
                                         OperationTypeCreate, "b", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> nodeAA(new Node(dbNodeIdDirAA, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("AA"), NodeTypeDirectory,
                                          OperationTypeCreate, "aa", createdAt, lastmodified, size, nodeA));
    std::shared_ptr<Node> nodeAB(new Node(dbNodeIdDirAB, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("AB"), NodeTypeDirectory,
                                          OperationTypeCreate, "ab", createdAt, lastmodified, size, nodeA));
    std::shared_ptr<Node> nodeAAA(new Node(dbNodeIdDirAAA, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("AAA"), NodeTypeFile,
                                           OperationTypeCreate, "aaa", createdAt, lastmodified, size, nodeAA));
    std::shared_ptr<Node> nodeAAB(new Node(dbNodeIdDirAAB, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("AAB"), NodeTypeFile,
                                           OperationTypeCreate, "aab", createdAt, lastmodified, size, nodeAA));

    SyncOpPtr opA = std::make_shared<SyncOperation>();
    opA->setAffectedNode(nodeA);
    opA->setTargetSide(nodeA->side());
    opA->setType(OperationTypeCreate);

    SyncOpPtr opB = std::make_shared<SyncOperation>();
    opB->setAffectedNode(nodeB);
    opB->setTargetSide(nodeB->side());
    opB->setType(OperationTypeCreate);

    SyncOpPtr opAA = std::make_shared<SyncOperation>();
    opAA->setAffectedNode(nodeAA);
    opAA->setTargetSide(nodeAA->side());
    opAA->setType(OperationTypeCreate);

    SyncOpPtr opAB = std::make_shared<SyncOperation>();
    opAB->setAffectedNode(nodeAB);
    opAB->setTargetSide(nodeAB->side());
    opAB->setType(OperationTypeCreate);

    SyncOpPtr opAAA = std::make_shared<SyncOperation>();
    opAAA->setAffectedNode(nodeAAA);
    opAAA->setTargetSide(nodeAAA->side());
    opAAA->setType(OperationTypeCreate);

    SyncOpPtr opAAB = std::make_shared<SyncOperation>();
    opAAB->setAffectedNode(nodeAAB);
    opAAB->setTargetSide(nodeAAB->side());
    opAAB->setType(OperationTypeCreate);

    // Case 1 : A AAA AA AAB AB B
    Console << std::endl;
    {
        _syncPal->_syncOps->pushOp(opA);
        _syncPal->_syncOps->pushOp(opAAA);
        _syncPal->_syncOps->pushOp(opAA);
        _syncPal->_syncOps->pushOp(opAAB);
        _syncPal->_syncOps->pushOp(opAB);
        _syncPal->_syncOps->pushOp(opB);
        _syncPal->_operationsSorterWorker->_unsortedList = *_syncPal->_syncOps;
        // Expected order: A AA AAA AAB AB B
        Console << Str("Initial ops order : ");
        for (const auto &opId : _syncPal->_syncOps->_opSortedList) {
            SyncOpPtr op = _syncPal->_syncOps->_allOps[opId];
            Console << op->affectedNode()->name().c_str() << Str(" ");
        }
        Console << std::endl;

        std::vector<SyncOpPtr> expectedRes = {opA, opAA, opAAA, opAAB, opAB, opB};

        _syncPal->_operationsSorterWorker->fixCreateBeforeCreate();

        Console << Str("Final ops order : ");
        for (const auto &opId : _syncPal->_syncOps->_opSortedList) {
            SyncOpPtr op = _syncPal->_syncOps->_allOps[opId];
            Console << op->affectedNode()->name().c_str() << Str(" ");
        }
        Console << std::endl;

        int index = 0;
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
        Console << Str("Initial ops order : ");
        for (const auto &opId : _syncPal->_syncOps->_opSortedList) {
            SyncOpPtr op = _syncPal->_syncOps->_allOps[opId];
            Console << op->affectedNode()->name().c_str() << Str(" ");
        }
        Console << std::endl;
        std::vector<SyncOpPtr> expectedRes = {opA, opAB, opAA, opAAB, opAAA, opB};

        _syncPal->_operationsSorterWorker->fixCreateBeforeCreate();

        Console << Str("Final ops order : ");
        for (const auto &opId : _syncPal->_syncOps->_opSortedList) {
            SyncOpPtr op = _syncPal->_syncOps->_allOps[opId];
            Console << op->affectedNode()->name().c_str() << Str(" ");
        }
        Console << std::endl;

        int index = 0;
        std::list<UniqueId>::iterator it = _syncPal->_syncOps->_opSortedList.begin();
        for (; it != _syncPal->_syncOps->_opSortedList.end(); it++) {
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
        Console << Str("Initial ops order : ");
        for (const auto &opId : _syncPal->_syncOps->_opSortedList) {
            SyncOpPtr op = _syncPal->_syncOps->_allOps[opId];
            Console << op->affectedNode()->name().c_str() << Str(" ");
        }
        Console << std::endl;
        std::vector<SyncOpPtr> expectedRes = {opB, opA, opAB, opAA, opAAB, opAAA};

        _syncPal->_operationsSorterWorker->fixCreateBeforeCreate();

        Console << Str("Final ops order : ");
        for (const auto &opId : _syncPal->_syncOps->_opSortedList) {
            SyncOpPtr op = _syncPal->_syncOps->_allOps[opId];
            Console << op->affectedNode()->name().c_str() << Str(" ");
        }
        Console << std::endl;

        int index = 0;
        std::list<UniqueId>::iterator it = _syncPal->_syncOps->_opSortedList.begin();
        for (; it != _syncPal->_syncOps->_opSortedList.end(); it++) {
            CPPUNIT_ASSERT(*it == expectedRes[index++]->id());
        }
    }
}

void TestOperationSorterWorker::testFixMoveBeforeMoveParentChildFilp() {
    time_t tLoc = std::time(0);
    time_t tDrive = std::time(0);
    DbNodeId dbNodeIdDir1;
    DbNodeId dbNodeIdDir11;
    DbNodeId dbNodeIdDir2;
    DbNodeId dbNodeIdDir3;

    bool constraintError = false;
    DbNode nodeDir1(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 1"), Str("Dir 1"), "d1", "r1", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir1, dbNodeIdDir1, constraintError);
    DbNode nodeDir11(0, dbNodeIdDir1, Str("Dir 1.1"), Str("Dir 1.1"), "11", "r11", tLoc, tLoc, tDrive,
                     NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir11, dbNodeIdDir11, constraintError);
    DbNode nodeDir2(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 2"), Str("Dir 2"), "2", "r2", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir2, dbNodeIdDir2, constraintError);
    DbNode nodeDir3(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 3"), Str("Dir 3"), "3", "r3", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir3, dbNodeIdDir3, constraintError);
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;

    std::shared_ptr<Node> rootNode(new Node(_syncPal->_syncDb->rootNode().nodeId(), _syncPal->updateTree(ReplicaSideLocal)->side(), Str(""),
                                            NodeTypeDirectory, OperationTypeNone, _syncPal->syncDb()->rootNode().nodeIdLocal(),
                                            createdAt, lastmodified, size, nullptr));
    std::shared_ptr<Node> node1(new Node(dbNodeIdDir1, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 1"), NodeTypeDirectory,
                                         OperationTypeCreate, "d1", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> node11(new Node(dbNodeIdDir11, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 1.1"), NodeTypeDirectory,
                                          OperationTypeCreate, "11", createdAt, lastmodified, size, node1));
    std::shared_ptr<Node> node2(new Node(dbNodeIdDir2, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 2"), NodeTypeDirectory,
                                         OperationTypeNone, "2", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> node3(new Node(dbNodeIdDir3, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("Dir 3"), NodeTypeDirectory,
                                         OperationTypeNone, "3", createdAt, lastmodified, size, rootNode));
    std::shared_ptr<Node> rrootNode(new Node(_syncPal->_syncDb->rootNode().nodeId(), _syncPal->updateTree(ReplicaSideRemote)->side(), Str(""),
                                             NodeTypeDirectory, OperationTypeNone, _syncPal->syncDb()->rootNode().nodeIdRemote(),
                                             createdAt, lastmodified, size, nullptr));
    std::shared_ptr<Node> rNode1(new Node(dbNodeIdDir1, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 1"), NodeTypeDirectory,
                                          OperationTypeNone, "r1", createdAt, lastmodified, size, rrootNode));
    std::shared_ptr<Node> rNode11(new Node(dbNodeIdDir11, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 1.1"), NodeTypeDirectory,
                                           OperationTypeNone, "r11", createdAt, lastmodified, size, rNode1));
    std::shared_ptr<Node> rNode2(new Node(dbNodeIdDir2, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 2"), NodeTypeDirectory,
                                          OperationTypeNone, "r2", createdAt, lastmodified, size, rrootNode));
    std::shared_ptr<Node> rNode3(new Node(dbNodeIdDir3, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("Dir 3"), NodeTypeDirectory,
                                          OperationTypeNone, "r3", createdAt, lastmodified, size, rrootNode));

    rootNode->deleteChildren(node1);
    node11->insertChildren(node1);
    node1->setParentNode(node11);
    node1->deleteChildren(node11);
    node1->insertChangeEvent(OperationTypeMove);
    node1->setMoveOrigin("Dir 1");
    node1->setMoveOriginParentDbId(_syncPal->syncDb()->rootNode().nodeId());
    rootNode->insertChildren(node11);
    node11->setParentNode(rootNode);
    node11->insertChangeEvent(OperationTypeMove);
    node11->setMoveOrigin("Dir 1/Dir 1.1");
    node11->setMoveOriginParentDbId(dbNodeIdDir1);

    SyncOpPtr op1 = std::make_shared<SyncOperation>();
    SyncOpPtr op2 = std::make_shared<SyncOperation>();
    SyncOpPtr op3 = std::make_shared<SyncOperation>();
    SyncOpPtr op4 = std::make_shared<SyncOperation>();
    op1->setAffectedNode(node11);
    op1->setTargetSide(node11->side());
    op1->setType(OperationTypeMove);
    op2->setAffectedNode(node2);
    op3->setAffectedNode(node1);
    op3->setTargetSide(node1->side());
    op3->setType(OperationTypeMove);
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

    time_t tLoc = std::time(0);
    time_t tDrive = std::time(0);
    DbNodeId dbNodeIdDirn;
    DbNodeId dbNodeIdDirt;
    DbNodeId dbNodeIdDirq;
    DbNodeId dbNodeIdDire;

    bool constraintError = false;
    DbNode nodeDirn(0, _syncPal->syncDb()->rootNode().nodeId(), Str("n"), Str("n"), "n", "re", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirn, dbNodeIdDirn, constraintError);
    DbNode nodeDirt(0, dbNodeIdDirn, Str("t"), Str("t"), "t", "rq", tLoc, tLoc, tDrive, NodeType::NodeTypeDirectory, 0,
                    std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirt, dbNodeIdDirt, constraintError);
    DbNode nodeDirq(0, dbNodeIdDirt, Str("q"), Str("q"), "q", "rt", tLoc, tLoc, tDrive, NodeType::NodeTypeDirectory, 0,
                    std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirq, dbNodeIdDirq, constraintError);
    DbNode nodeDire(0, dbNodeIdDirq, Str("e"), Str("e"), "e", "rn", tLoc, tLoc, tDrive, NodeType::NodeTypeDirectory, 0,
                    std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDire, dbNodeIdDire, constraintError);

    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;
    std::shared_ptr<Node> nodeN(new Node(dbNodeIdDirn, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("n"), NodeTypeDirectory,
                                         OperationTypeNone, "n", createdAt, lastmodified, size,
                                         _syncPal->updateTree(ReplicaSideLocal)->rootNode()));
    std::shared_ptr<Node> nodeT(new Node(dbNodeIdDirt, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("t"), NodeTypeDirectory,
                                         OperationTypeNone, "t", createdAt, lastmodified, size,
                                         _syncPal->updateTree(ReplicaSideLocal)->rootNode()));
    std::shared_ptr<Node> nodeQ(new Node(dbNodeIdDirq, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("q"), NodeTypeDirectory,
                                         OperationTypeNone, "q", createdAt, lastmodified, size, nodeT));
    std::shared_ptr<Node> nodeE(new Node(dbNodeIdDire, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("e"), NodeTypeDirectory,
                                         OperationTypeNone, "e", createdAt, lastmodified, size, nodeQ));
    std::shared_ptr<Node> rNodeN(new Node(dbNodeIdDirn, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("n"), NodeTypeDirectory,
                                          OperationTypeNone, "rn", createdAt, lastmodified, size,
                                          _syncPal->updateTree(ReplicaSideRemote)->rootNode()));
    std::shared_ptr<Node> rNodeT(new Node(dbNodeIdDirt, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("t"), NodeTypeDirectory,
                                          OperationTypeNone, "rt", createdAt, lastmodified, size,
                                          _syncPal->updateTree(ReplicaSideRemote)->rootNode()));
    std::shared_ptr<Node> rNodeQ(new Node(dbNodeIdDirq, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("q"), NodeTypeDirectory,
                                          OperationTypeNone, "rq", createdAt, lastmodified, size, rNodeT));
    std::shared_ptr<Node> rNodeE(new Node(dbNodeIdDire, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("e"), NodeTypeDirectory,
                                          OperationTypeNone, "re", createdAt, lastmodified, size, rNodeQ));

    _syncPal->updateTree(ReplicaSideLocal)->insertNode(nodeN);
    _syncPal->updateTree(ReplicaSideLocal)->insertNode(nodeT);
    _syncPal->updateTree(ReplicaSideLocal)->insertNode(nodeQ);
    _syncPal->updateTree(ReplicaSideLocal)->insertNode(nodeE);

    _syncPal->updateTree(ReplicaSideRemote)->insertNode(rNodeN);
    _syncPal->updateTree(ReplicaSideRemote)->insertNode(rNodeT);
    _syncPal->updateTree(ReplicaSideRemote)->insertNode(rNodeQ);
    _syncPal->updateTree(ReplicaSideRemote)->insertNode(rNodeE);

    // local changes
    nodeQ->deleteChildren(nodeE);
    nodeE->insertChangeEvent(OperationTypeMove);
    nodeE->setName(Str("n"));
    nodeE->setMoveOrigin("t/q/e");
    nodeE->setMoveOriginParentDbId(dbNodeIdDirq);
    _syncPal->updateTree(ReplicaSideLocal)->rootNode()->insertChildren(nodeE);
    _syncPal->updateTree(ReplicaSideLocal)->rootNode()->deleteChildren(nodeN);
    nodeE->setParentNode(_syncPal->updateTree(ReplicaSideLocal)->rootNode());
    nodeE->insertChildren(nodeN);
    nodeN->insertChangeEvent(OperationTypeMove);
    nodeN->setParentNode(nodeE);
    nodeN->setName(Str("g"));
    nodeN->setMoveOrigin("n");
    nodeN->setMoveOriginParentDbId(dbNodeIdDirn);

    // remote changes
    rNodeT->setParentNode(rNodeN);
    rNodeN->insertChildren(rNodeT);
    _syncPal->updateTree(ReplicaSideRemote)->rootNode()->deleteChildren(rNodeT);
    rNodeT->insertChangeEvent(OperationTypeMove);

    SyncOpPtr op1 = std::make_shared<SyncOperation>();
    SyncOpPtr op2 = std::make_shared<SyncOperation>();
    SyncOpPtr op3 = std::make_shared<SyncOperation>();
    op1->setType(OperationTypeMove);
    op1->setTargetSide(ReplicaSideLocal);
    op1->setAffectedNode(nodeN);
    op1->setNewName(Str("g"));
    op1->setNewParentNode(nodeE);
    op1->setType(OperationTypeMove);
    op2->setTargetSide(ReplicaSideLocal);
    op2->setAffectedNode(nodeE);
    op2->setNewName(Str("n"));
    op2->setNewParentNode(_syncPal->updateTree(ReplicaSideLocal)->rootNode());
    op3->setType(OperationTypeMove);
    op3->setTargetSide(ReplicaSideRemote);
    op3->setAffectedNode(rNodeT);
    op3->setNewName(Str("w"));

    _syncPal->_syncOps->setOpList({op1, op2, op3});
    std::optional<SyncOperationList> reshuffledOp = _syncPal->_operationsSorterWorker->fixImpossibleFirstMoveOp();
    CPPUNIT_ASSERT(reshuffledOp);
    CPPUNIT_ASSERT(reshuffledOp->_opSortedList.size() == 1);
    CPPUNIT_ASSERT(reshuffledOp->_opSortedList.back() == op3->id());
}

void TestOperationSorterWorker::testFindCompleteCycles() {
    time_t tLoc = std::time(0);
    time_t tDrive = std::time(0);
    DbNodeId dbNodeIdA;
    DbNodeId dbNodeIdB;
    DbNodeId dbNodeIdC;
    DbNodeId dbNodeIdD;

    bool constraintError = false;
    DbNode nodeDirn(0, _syncPal->syncDb()->rootNode().nodeId(), Str("A"), Str("A"), "la", "ra", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirn, dbNodeIdA, constraintError);
    DbNode nodeDirt(0, _syncPal->syncDb()->rootNode().nodeId(), Str("B"), Str("B"), "lb", "rb", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirt, dbNodeIdB, constraintError);
    DbNode nodeDirq(0, _syncPal->syncDb()->rootNode().nodeId(), Str("C"), Str("C"), "lc", "rc", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirq, dbNodeIdC, constraintError);
    DbNode nodeDire(0, _syncPal->syncDb()->rootNode().nodeId(), Str("D"), Str("D"), "ld", "rd", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDire, dbNodeIdD, constraintError);

    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;

    std::shared_ptr<Node> nodeA(
        new Node(dbNodeIdA, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("A"), NodeTypeDirectory, "a", createdAt, lastmodified, size));
    std::shared_ptr<Node> nodeB(
        new Node(dbNodeIdB, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("B"), NodeTypeDirectory, "b", createdAt, lastmodified, size));
    std::shared_ptr<Node> nodeC(
        new Node(dbNodeIdC, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("C"), NodeTypeDirectory, "c", createdAt, lastmodified, size));
    std::shared_ptr<Node> nodeD(
        new Node(dbNodeIdD, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("D"), NodeTypeDirectory, "d", createdAt, lastmodified, size));

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
    time_t tLoc = std::time(0);
    time_t tDrive = std::time(0);
    DbNodeId dbNodeIdDirA;

    bool constraintError = false;
    DbNode nodeDirA(0, _syncPal->syncDb()->rootNode().nodeId(), Str("A"), Str("A"), "A", "rA", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeFile, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirA, dbNodeIdDirA, constraintError);

    // initial situation

    //        S
    //        |
    //        A

    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;
    std::shared_ptr<Node> nodeA(new Node(dbNodeIdDirA, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("A"), NodeTypeFile,
                                         OperationTypeNone, "A", createdAt, lastmodified, size,
                                         _syncPal->updateTree(ReplicaSideLocal)->rootNode()));
    std::shared_ptr<Node> rNodeA(new Node(dbNodeIdDirA, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("A"), NodeTypeFile,
                                          OperationTypeNone, "rA", createdAt, lastmodified, size,
                                          _syncPal->updateTree(ReplicaSideRemote)->rootNode()));

    _syncPal->updateTree(ReplicaSideLocal)->insertNode(nodeA);
    _syncPal->updateTree(ReplicaSideRemote)->insertNode(rNodeA);

    std::shared_ptr<Node> nodeNewA(new Node(std::nullopt, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("A"), NodeTypeDirectory,
                                            OperationTypeNone, "newA", createdAt, lastmodified, size,
                                            _syncPal->updateTree(ReplicaSideLocal)->rootNode()));
    nodeNewA->insertChildren(nodeA);
    nodeA->setParentNode(nodeNewA);
    nodeA->setName(Str("subpath"));
    nodeA->setMoveOriginParentDbId(_syncPal->syncDb()->rootNode().nodeId());
    nodeA->setMoveOrigin("A");

    SyncOpPtr op1 = std::make_shared<SyncOperation>();
    SyncOpPtr op2 = std::make_shared<SyncOperation>();
    op1->setAffectedNode(nodeA);
    op1->setTargetSide(ReplicaSideLocal);
    op1->setType(OperationTypeMove);
    op1->setNewName(Str("subpath"));
    op1->setNewParentNode(nodeNewA);
    op2->setAffectedNode(nodeNewA);
    op2->setTargetSide(ReplicaSideLocal);
    op2->setType(OperationTypeCreate);

    SyncOpPtr resolutionOp = std::make_shared<SyncOperation>();
    SyncOperationList cycle;
    cycle.setOpList({op1, op2});
    _syncPal->_operationsSorterWorker->breakCycle(cycle, resolutionOp);
    CPPUNIT_ASSERT(resolutionOp->type() == OperationTypeMove);
    CPPUNIT_ASSERT(resolutionOp->targetSide() == ReplicaSideRemote);
    CPPUNIT_ASSERT(resolutionOp->affectedNode()->id() == "rA");
    CPPUNIT_ASSERT(resolutionOp->affectedNode()->idb() == op1->affectedNode()->idb());
}

void TestOperationSorterWorker::testBreakCycleEx2() {
    time_t tLoc = std::time(0);
    time_t tDrive = std::time(0);
    DbNodeId dbNodeIdDirA;
    DbNodeId dbNodeIdDirB;

    bool constraintError = false;
    DbNode nodeDirA(0, _syncPal->syncDb()->rootNode().nodeId(), Str("A"), Str("A"), "A", "rA", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirA, dbNodeIdDirA, constraintError);
    DbNode nodeDirB(0, dbNodeIdDirA, Str("B"), Str("B"), "B", "rB", tLoc, tLoc, tDrive, NodeType::NodeTypeDirectory, 0,
                    std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDirB, dbNodeIdDirB, constraintError);

    // initial situation

    //        S
    //        |
    //        A
    //        |
    //        B

    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;
    std::shared_ptr<Node> nodeA(new Node(dbNodeIdDirA, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("A"), NodeTypeDirectory,
                                         OperationTypeNone, "A", createdAt, lastmodified, size,
                                         _syncPal->updateTree(ReplicaSideLocal)->rootNode()));
    std::shared_ptr<Node> nodeB(new Node(dbNodeIdDirB, _syncPal->updateTree(ReplicaSideLocal)->side(), Str("B"), NodeTypeDirectory,
                                         OperationTypeNone, "B", createdAt, lastmodified, size, nodeA));
    std::shared_ptr<Node> rNodeA(new Node(dbNodeIdDirA, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("A"), NodeTypeDirectory,
                                          OperationTypeNone, "rA", createdAt, lastmodified, size,
                                          _syncPal->updateTree(ReplicaSideRemote)->rootNode()));
    std::shared_ptr<Node> rNodeB(new Node(dbNodeIdDirB, _syncPal->updateTree(ReplicaSideRemote)->side(), Str("B"), NodeTypeDirectory,
                                          OperationTypeNone, "rB", createdAt, lastmodified, size, rNodeA));

    _syncPal->updateTree(ReplicaSideLocal)->rootNode()->insertChildren(nodeA);
    nodeA->insertChildren(nodeB);
    _syncPal->updateTree(ReplicaSideLocal)->insertNode(nodeA);
    _syncPal->updateTree(ReplicaSideLocal)->insertNode(nodeB);
    _syncPal->updateTree(ReplicaSideRemote)->rootNode()->insertChildren(rNodeA);
    rNodeA->insertChildren(rNodeB);
    _syncPal->updateTree(ReplicaSideRemote)->insertNode(rNodeA);
    _syncPal->updateTree(ReplicaSideRemote)->insertNode(nodeB);

    nodeA->insertChangeEvent(OperationTypeDelete);
    nodeA->deleteChildren(nodeB);
    nodeB->insertChangeEvent(OperationTypeMove);
    nodeB->setName(Str("A"));
    nodeB->setParentNode(_syncPal->updateTree(ReplicaSideLocal)->rootNode());
    nodeB->setMoveOrigin("A/B");
    nodeB->setMoveOriginParentDbId(dbNodeIdDirA);

    SyncOpPtr op1 = std::make_shared<SyncOperation>();
    SyncOpPtr op2 = std::make_shared<SyncOperation>();
    op1->setAffectedNode(nodeA);
    op1->setType(OperationTypeDelete);
    op1->setTargetSide(op1->affectedNode()->side());
    op2->setAffectedNode(nodeB);
    op2->setType(OperationTypeMove);
    op2->setNewName(Str("A"));
    op2->setNewParentNode(_syncPal->updateTree(ReplicaSideLocal)->rootNode());
    op2->setTargetSide(op2->affectedNode()->side());

    SyncOpPtr resolutionOp = std::make_shared<SyncOperation>();
    SyncOperationList cycle;
    cycle.setOpList({op1, op2});
    _syncPal->_operationsSorterWorker->breakCycle(cycle, resolutionOp);
    CPPUNIT_ASSERT(resolutionOp->type() == OperationTypeMove);
    CPPUNIT_ASSERT(resolutionOp->targetSide() == ReplicaSideRemote);
    CPPUNIT_ASSERT(resolutionOp->affectedNode()->id() == "rA");
    CPPUNIT_ASSERT(resolutionOp->affectedNode()->type() == NodeTypeDirectory);
    CPPUNIT_ASSERT(resolutionOp->affectedNode()->idb() == op1->affectedNode()->idb());
}


}  // namespace KDC
