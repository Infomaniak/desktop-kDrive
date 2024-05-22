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

#include "testconflictfinderworker.h"

using namespace CppUnit;

namespace KDC {

const std::string version = "3.4.0";

void TestConflictFinderWorker::setUp() {
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

    _syncPal->_conflictFinderWorker =
        std::shared_ptr<ConflictFinderWorker>(new ConflictFinderWorker(_syncPal, "Conflict Finder", "COFD"));

    _syncPal->_localUpdateTree->init();
    _syncPal->_remoteUpdateTree->init();
}

void TestConflictFinderWorker::tearDown() {
    ParmsDb::instance()->close();
    _syncPal->_syncDb->close();
}

void TestConflictFinderWorker::setUpTreesAndDb() {
    // db insertion
    time_t tLoc = std::time(0);
    time_t tDrive = std::time(0);
    DbNodeId dbNodeIdDirA;
    DbNodeId dbNodeIdDirB;
    DbNodeId dbNodeIdDir1;
    DbNodeId dbNodeIdDir11;
    DbNodeId dbNodeIdDir111;
    DbNodeId dbNodeIdFile1111;
    DbNodeId dbNodeId112;
    DbNodeId dbNodeIdDir2;
    DbNodeId dbNodeIdDir3;
    DbNodeId dbNodeIdDir31;
    DbNodeId dbnodeIdDir4;
    DbNodeId dbnodeIdDir41;
    DbNodeId dbnodeIdDir411;
    DbNodeId dbnodeIdfile4111;
    DbNodeId dbnodeIdfile4112;
    DbNodeId dbnodeIdDir5;
    DbNodeId dbnodeIdfile51;

    bool constraintError = false;
    DbNode nodeDbA(0, _syncPal->_syncDb->rootNode().nodeId(), Str("A"), Str("A"), "A", "rA", tLoc, tLoc, tDrive,
                   NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(nodeDbA, dbNodeIdDirA, constraintError);
    DbNode nodeDbB(0, _syncPal->_syncDb->rootNode().nodeId(), Str("B"), Str("B"), "B", "rB", tLoc, tLoc, tDrive,
                   NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(nodeDbB, dbNodeIdDirB, constraintError);
    DbNode nodeDir1(0, _syncPal->_syncDb->rootNode().nodeId(), Str("Dir 1"), Str("Dir 1"), "d1", "r1", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(nodeDir1, dbNodeIdDir1, constraintError);
    DbNode nodeFile112(0, dbNodeIdDir11, Str("File 1.1.2"), Str("File 1.1.2"), "112", "r112", tLoc, tLoc, tDrive,
                       NodeType::NodeTypeFile, 0, "cs 1.1");
    _syncPal->_syncDb->insertNode(nodeFile112, dbNodeId112, constraintError);
    DbNode nodeDir111(0, dbNodeIdDir111, Str("Dir 1.1.1"), Str("Dir 1.1.1"), "111", "r111", tLoc, tLoc, tDrive,
                      NodeType::NodeTypeDirectory, 0, "cs 1.1.1");
    _syncPal->_syncDb->insertNode(nodeDir111, dbNodeIdDir111, constraintError);
    DbNode nodeFile1111(0, dbNodeIdDir111, Str("File 1.1.1.1"), Str("File 1.1.1.1"), "1111", "r1111", tLoc, tLoc, tDrive,
                        NodeType::NodeTypeFile, 0, "cs 1.1.1");
    _syncPal->_syncDb->insertNode(nodeFile1111, dbNodeIdFile1111, constraintError);
    DbNode nodeDir2(0, _syncPal->_syncDb->rootNode().nodeId(), Str("Dir 2"), Str("Dir 2"), "2", "r2", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(nodeDir2, dbNodeIdDir2, constraintError);
    DbNode nodeDir3(0, _syncPal->_syncDb->rootNode().nodeId(), Str("Dir 3"), Str("Dir 3"), "3", "r3", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(nodeDir3, dbNodeIdDir3, constraintError);
    DbNode nodeDir31(0, dbNodeIdDir3, Str("Dir 3.1"), Str("Dir 3.1"), "31", "r31", tLoc, tLoc, tDrive,
                     NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(nodeDir31, dbNodeIdDir31, constraintError);
    DbNode nodeDir4(0, _syncPal->_syncDb->rootNode().nodeId(), Str("Dir 4"), Str("Dir 4"), "4", "r4", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(nodeDir4, dbnodeIdDir4, constraintError);
    DbNode nodedir41(0, dbnodeIdDir4, Str("Dir 4.1"), Str("Dir 4.1"), "41", "r41", tLoc, tLoc, tDrive,
                     NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(nodedir41, dbnodeIdDir41, constraintError);
    DbNode nodeDir411(0, dbnodeIdDir41, Str("Dir 4.1.1"), Str("Dir 4.1.1"), "411", "r411", tLoc, tLoc, tDrive,
                      NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(nodeDir411, dbnodeIdDir411, constraintError);
    DbNode nodeFile4111(0, dbnodeIdDir411, Str("File 4.1.1.1"), Str("File 4.1.1.1"), "4111", "r4111", tLoc, tLoc, tDrive,
                        NodeType::NodeTypeFile, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(nodeFile4111, dbnodeIdfile4111, constraintError);
    DbNode nodeFile4112(0, dbnodeIdDir411, Str("File 4.1.1.2"), Str("File 4.1.1.2"), "4112", "r4112", tLoc, tLoc, tDrive,
                        NodeType::NodeTypeFile, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(nodeFile4112, dbnodeIdfile4112, constraintError);
    DbNode nodeDir5(0, _syncPal->_syncDb->rootNode().nodeId(), Str("Dir 5"), Str("Dir 5"), "5", "r5", tLoc, tLoc, tDrive,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(nodeDir5, dbnodeIdDir5, constraintError);
    DbNode nodeFile51(0, dbnodeIdDir5, Str("File 5.1"), Str("File 5.1"), "51", "r51", tLoc, tLoc, tDrive, NodeType::NodeTypeFile,
                      0, std::nullopt);
    _syncPal->_syncDb->insertNode(nodeFile51, dbnodeIdfile51, constraintError);

    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;
    std::shared_ptr<Node> nodeA = std::shared_ptr<Node>(new Node(dbNodeIdDirA, _syncPal->_localUpdateTree->side(), Str("A"),
                                                                 NodeTypeDirectory, OperationTypeNone, "A", createdAt,
                                                                 lastmodified, size, _syncPal->_localUpdateTree->rootNode()));
    std::shared_ptr<Node> nodeB = std::shared_ptr<Node>(new Node(dbNodeIdDirB, _syncPal->_localUpdateTree->side(), Str("B"),
                                                                 NodeTypeDirectory, OperationTypeNone, "B", createdAt,
                                                                 lastmodified, size, _syncPal->_localUpdateTree->rootNode()));
    std::shared_ptr<Node> node1 = std::shared_ptr<Node>(new Node(dbNodeIdDir1, _syncPal->_localUpdateTree->side(), Str("Dir 1"),
                                                                 NodeTypeDirectory, OperationTypeNone, "1", createdAt,
                                                                 lastmodified, size, _syncPal->_localUpdateTree->rootNode()));
    std::shared_ptr<Node> node2 = std::shared_ptr<Node>(new Node(dbNodeIdDir2, _syncPal->_localUpdateTree->side(), Str("Dir 2"),
                                                                 NodeTypeDirectory, OperationTypeNone, "2", createdAt,
                                                                 lastmodified, size, _syncPal->_localUpdateTree->rootNode()));
    std::shared_ptr<Node> node3 = std::shared_ptr<Node>(new Node(dbNodeIdDir3, _syncPal->_localUpdateTree->side(), Str("Dir 3"),
                                                                 NodeTypeDirectory, OperationTypeNone, "3", createdAt,
                                                                 lastmodified, size, _syncPal->_localUpdateTree->rootNode()));
    std::shared_ptr<Node> node4 = std::shared_ptr<Node>(new Node(dbnodeIdDir4, _syncPal->_localUpdateTree->side(), Str("Dir 4"),
                                                                 NodeTypeDirectory, OperationTypeNone, "4", createdAt,
                                                                 lastmodified, size, _syncPal->_localUpdateTree->rootNode()));
    std::shared_ptr<Node> node11 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_localUpdateTree->side(), Str("Dir 1.1"), NodeTypeDirectory,
                                       OperationTypeNone, "11", createdAt, lastmodified, size, node1));
    std::shared_ptr<Node> node111 =
        std::shared_ptr<Node>(new Node(dbNodeIdDir111, _syncPal->_localUpdateTree->side(), Str("Dir 1.1.1"), NodeTypeDirectory,
                                       OperationTypeNone, "111", createdAt, lastmodified, size, node11));
    std::shared_ptr<Node> node1111 =
        std::shared_ptr<Node>(new Node(dbNodeIdFile1111, _syncPal->_localUpdateTree->side(), Str("File 1.1.1.1"), NodeTypeFile,
                                       OperationTypeNone, "1111", createdAt, lastmodified, size, node111));
    std::shared_ptr<Node> node31 =
        std::shared_ptr<Node>(new Node(dbNodeIdDir31, _syncPal->_localUpdateTree->side(), Str("Dir 3.1"), NodeTypeDirectory,
                                       OperationTypeNone, "31", createdAt, lastmodified, size, node3));
    std::shared_ptr<Node> node41 =
        std::shared_ptr<Node>(new Node(dbnodeIdDir41, _syncPal->_localUpdateTree->side(), Str("Dir 4.1"), NodeTypeDirectory,
                                       OperationTypeNone, "41", createdAt, lastmodified, size, node4));
    std::shared_ptr<Node> node411 =
        std::shared_ptr<Node>(new Node(dbnodeIdDir411, _syncPal->_localUpdateTree->side(), Str("Dir 4.1.1"), NodeTypeDirectory,
                                       OperationTypeNone, "411", createdAt, lastmodified, size, node41));
    std::shared_ptr<Node> node4111 =
        std::shared_ptr<Node>(new Node(dbnodeIdfile4111, _syncPal->_localUpdateTree->side(), Str("File 4.1.1.1"), NodeTypeFile,
                                       OperationTypeNone, "4111", createdAt, lastmodified, size, node411));

    std::shared_ptr<Node> rNodeA = std::shared_ptr<Node>(new Node(dbNodeIdDirA, _syncPal->_remoteUpdateTree->side(), Str("A"),
                                                                  NodeTypeDirectory, OperationTypeNone, "rA", createdAt,
                                                                  lastmodified, size, _syncPal->_remoteUpdateTree->rootNode()));
    std::shared_ptr<Node> rNodeB = std::shared_ptr<Node>(new Node(dbNodeIdDirB, _syncPal->_remoteUpdateTree->side(), Str("B"),
                                                                  NodeTypeDirectory, OperationTypeNone, "rB", createdAt,
                                                                  lastmodified, size, _syncPal->_remoteUpdateTree->rootNode()));
    std::shared_ptr<Node> rNode1 = std::shared_ptr<Node>(new Node(dbNodeIdDir1, _syncPal->_remoteUpdateTree->side(), Str("Dir 1"),
                                                                  NodeTypeDirectory, OperationTypeNone, "r1", createdAt,
                                                                  lastmodified, size, _syncPal->_remoteUpdateTree->rootNode()));
    std::shared_ptr<Node> rNode2 = std::shared_ptr<Node>(new Node(dbNodeIdDir2, _syncPal->_remoteUpdateTree->side(), Str("Dir 2"),
                                                                  NodeTypeDirectory, OperationTypeNone, "r2", createdAt,
                                                                  lastmodified, size, _syncPal->_remoteUpdateTree->rootNode()));
    std::shared_ptr<Node> rNode3 = std::shared_ptr<Node>(new Node(dbNodeIdDir3, _syncPal->_remoteUpdateTree->side(), Str("Dir 3"),
                                                                  NodeTypeDirectory, OperationTypeNone, "r3", createdAt,
                                                                  lastmodified, size, _syncPal->_remoteUpdateTree->rootNode()));
    std::shared_ptr<Node> rNode4 = std::shared_ptr<Node>(new Node(dbnodeIdDir4, _syncPal->_remoteUpdateTree->side(), Str("Dir 4"),
                                                                  NodeTypeDirectory, OperationTypeNone, "r4", createdAt,
                                                                  lastmodified, size, _syncPal->_remoteUpdateTree->rootNode()));
    std::shared_ptr<Node> rNode11 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("Dir 1.1"), NodeTypeDirectory,
                                       OperationTypeNone, "r11", createdAt, lastmodified, size, rNode1));
    std::shared_ptr<Node> rNode111 =
        std::shared_ptr<Node>(new Node(dbNodeIdDir111, _syncPal->_remoteUpdateTree->side(), Str("Dir 1.1.1"), NodeTypeDirectory,
                                       OperationTypeNone, "r111", createdAt, lastmodified, size, rNode11));
    std::shared_ptr<Node> rNode1111 =
        std::shared_ptr<Node>(new Node(dbNodeIdFile1111, _syncPal->_remoteUpdateTree->side(), Str("File 1.1.1.1"), NodeTypeFile,
                                       OperationTypeNone, "r1111", createdAt, lastmodified, size, rNode111));
    std::shared_ptr<Node> rNode31 =
        std::shared_ptr<Node>(new Node(dbNodeIdDir31, _syncPal->_remoteUpdateTree->side(), Str("Dir 3.1"), NodeTypeDirectory,
                                       OperationTypeNone, "r31", createdAt, lastmodified, size, rNode3));
    std::shared_ptr<Node> rNode41 =
        std::shared_ptr<Node>(new Node(dbnodeIdDir41, _syncPal->_remoteUpdateTree->side(), Str("Dir 4.1"), NodeTypeDirectory,
                                       OperationTypeNone, "r41", createdAt, lastmodified, size, rNode4));
    std::shared_ptr<Node> rNode411 =
        std::shared_ptr<Node>(new Node(dbnodeIdDir411, _syncPal->_remoteUpdateTree->side(), Str("Dir 4.1.1"), NodeTypeDirectory,
                                       OperationTypeNone, "r411", createdAt, lastmodified, size, rNode41));
    std::shared_ptr<Node> rNode4111 =
        std::shared_ptr<Node>(new Node(dbnodeIdfile4111, _syncPal->_remoteUpdateTree->side(), Str("File 4.1.1.1"), NodeTypeFile,
                                       OperationTypeNone, "r4111", createdAt, lastmodified, size, rNode411));

    _syncPal->_localUpdateTree->rootNode()->insertChildren(nodeA);
    _syncPal->_localUpdateTree->rootNode()->insertChildren(nodeB);
    _syncPal->_localUpdateTree->rootNode()->insertChildren(node1);
    _syncPal->_localUpdateTree->rootNode()->insertChildren(node2);
    _syncPal->_localUpdateTree->rootNode()->insertChildren(node3);
    _syncPal->_localUpdateTree->rootNode()->insertChildren(node4);
    node1->insertChildren(node11);
    node11->insertChildren(node111);
    node111->insertChildren(node1111);
    node3->insertChildren(node31);
    node4->insertChildren(node41);
    node41->insertChildren(node411);
    node411->insertChildren(node4111);

    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(rNodeA);
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(rNodeB);
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(rNode1);
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(rNode2);
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(rNode3);
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(rNode4);
    rNode1->insertChildren(rNode11);
    rNode11->insertChildren(rNode111);
    rNode111->insertChildren(rNode1111);
    rNode3->insertChildren(rNode31);
    rNode4->insertChildren(rNode41);
    rNode41->insertChildren(rNode411);
    rNode411->insertChildren(rNode4111);

    _syncPal->_localUpdateTree->insertNode(node1111);
    _syncPal->_localUpdateTree->insertNode(node111);
    _syncPal->_localUpdateTree->insertNode(node11);
    _syncPal->_localUpdateTree->insertNode(nodeA);
    _syncPal->_localUpdateTree->insertNode(nodeB);
    _syncPal->_localUpdateTree->insertNode(node1);
    _syncPal->_localUpdateTree->insertNode(node2);
    _syncPal->_localUpdateTree->insertNode(node3);
    _syncPal->_localUpdateTree->insertNode(node4);
    _syncPal->_localUpdateTree->insertNode(node31);
    _syncPal->_localUpdateTree->insertNode(node41);
    _syncPal->_localUpdateTree->insertNode(node411);
    _syncPal->_localUpdateTree->insertNode(node4111);

    _syncPal->_remoteUpdateTree->insertNode(rNode1111);
    _syncPal->_remoteUpdateTree->insertNode(rNode111);
    _syncPal->_remoteUpdateTree->insertNode(rNode11);
    _syncPal->_remoteUpdateTree->insertNode(rNodeA);
    _syncPal->_remoteUpdateTree->insertNode(rNodeB);
    _syncPal->_remoteUpdateTree->insertNode(rNode1);
    _syncPal->_remoteUpdateTree->insertNode(rNode2);
    _syncPal->_remoteUpdateTree->insertNode(rNode3);
    _syncPal->_remoteUpdateTree->insertNode(rNode4);
    _syncPal->_remoteUpdateTree->insertNode(rNode31);
    _syncPal->_remoteUpdateTree->insertNode(rNode41);
    _syncPal->_remoteUpdateTree->insertNode(rNode411);
    _syncPal->_remoteUpdateTree->insertNode(rNode4111);
}

void TestConflictFinderWorker::testCreateCreate() {
    setUpTreesAndDb();
    // edit conflict
    std::string rootId = _syncPal->_localSnapshot->rootFolderId();
    std::string rRootId = _syncPal->_remoteSnapshot->rootFolderId();
    _syncPal->_localSnapshot->updateItem(SnapshotItem("4", rootId, Str("Dir 4"), 222, 222, NodeTypeDirectory, 123, true));
    _syncPal->_localSnapshot->updateItem(SnapshotItem("41", "4", Str("Dir 4.1"), 222, 222, NodeTypeDirectory, 123, true));
    _syncPal->_localSnapshot->updateItem(SnapshotItem("411", "41", Str("Dir 4.1.1"), 222, 222, NodeTypeDirectory, 123, true));
    _syncPal->_localSnapshot->updateItem(
        SnapshotItem("4111", "411", Str("File 4.1.1.1"), 222, 222, NodeTypeFile, 123, true, "1", "1"));

    _syncPal->_remoteSnapshot->updateItem(SnapshotItem("r4", rRootId, Str("Dir 4"), 222, 222, NodeTypeDirectory, 123, true));
    _syncPal->_remoteSnapshot->updateItem(SnapshotItem("r41", "r4", Str("Dir 4.1"), 222, 222, NodeTypeDirectory, 123, true));
    _syncPal->_remoteSnapshot->updateItem(SnapshotItem("r411", "r41", Str("Dir 4.1.1"), 222, 222, NodeTypeDirectory, 123, true));
    _syncPal->_remoteSnapshot->updateItem(
        SnapshotItem("r4111", "r411", Str("File 4.1.1.1"), 221, 221, NodeTypeFile, 123, true, "0", "0"));


    _syncPal->_localUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1")->setChangeEvents(OperationTypeCreate);
    _syncPal->_remoteUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1")->setChangeEvents(OperationTypeCreate);

    std::optional<Conflict> confTest = _syncPal->_conflictFinderWorker->checkCreateCreateConflict(
        _syncPal->_localUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1"));
    CPPUNIT_ASSERT(confTest);
    CPPUNIT_ASSERT(confTest->node() == _syncPal->_localUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1"));
    CPPUNIT_ASSERT(confTest->correspondingNode() ==
                   _syncPal->_remoteUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1"));
    CPPUNIT_ASSERT(confTest->type() == ConflictType::ConflictTypeCreateCreate);
}

void TestConflictFinderWorker::testEditEdit() {
    setUpTreesAndDb();
    // edit conflict
    std::string rootId = _syncPal->_localSnapshot->rootFolderId();
    std::string rRootId = _syncPal->_remoteSnapshot->rootFolderId();
    _syncPal->_localSnapshot->updateItem(SnapshotItem("1", rootId, Str("Dir 1"), 222, 222, NodeTypeDirectory, 123, true, "1"));
    _syncPal->_localSnapshot->updateItem(SnapshotItem("11", "1", Str("Dir 1.1"), 222, 222, NodeTypeDirectory, 123, true, "11"));
    _syncPal->_localSnapshot->updateItem(
        SnapshotItem("111", "11", Str("Dir 1.1.1"), 222, 222, NodeTypeDirectory, 123, true, "111"));
    _syncPal->_localSnapshot->updateItem(
        SnapshotItem("1111", "111", Str("File 1.1.1.1"), 222, 222, NodeTypeFile, 123, true, "36"));
    _syncPal->_remoteSnapshot->updateItem(SnapshotItem("r1", rRootId, Str("Dir 1"), 222, 222, NodeTypeDirectory, 123, true, "1"));
    _syncPal->_remoteSnapshot->updateItem(
        SnapshotItem("r11", "r1", Str("Dir 1.1"), 222, 222, NodeTypeDirectory, 123, true, "11"));
    _syncPal->_remoteSnapshot->updateItem(
        SnapshotItem("r111", "r11", Str("Dir 1.1.1"), 222, 222, NodeTypeDirectory, 123, true, "111"));
    _syncPal->_remoteSnapshot->updateItem(
        SnapshotItem("r1111", "r111", Str("File 1.1.1.1"), 222, 222, NodeTypeFile, 123, true, "63"));

    _syncPal->_localUpdateTree->getNodeByPath("Dir 1/Dir 1.1/Dir 1.1.1/File 1.1.1.1")->setChangeEvents(OperationTypeEdit);
    _syncPal->_remoteUpdateTree->getNodeByPath("Dir 1/Dir 1.1/Dir 1.1.1/File 1.1.1.1")->setChangeEvents(OperationTypeEdit);
    std::optional<Conflict> confTest = _syncPal->_conflictFinderWorker->checkEditEditConflict(
        _syncPal->_localUpdateTree->getNodeByPath("Dir 1/Dir 1.1/Dir 1.1.1/File 1.1.1.1"));
    CPPUNIT_ASSERT(confTest);
    CPPUNIT_ASSERT(confTest->node() == _syncPal->_localUpdateTree->getNodeByPath("Dir 1/Dir 1.1/Dir 1.1.1/File 1.1.1.1"));
    CPPUNIT_ASSERT(confTest->correspondingNode() ==
                   _syncPal->_remoteUpdateTree->getNodeByPath("Dir 1/Dir 1.1/Dir 1.1.1/File 1.1.1.1"));
    CPPUNIT_ASSERT(confTest->type() == ConflictType::ConflictTypeEditEdit);
}

void TestConflictFinderWorker::testMoveCreate() {
    setUpTreesAndDb();
    _syncPal->_localUpdateTree->getNodeByPath("Dir 3/Dir 3.1")->setChangeEvents(OperationTypeCreate);
    _syncPal->_remoteUpdateTree->getNodeByPath("Dir 3/Dir 3.1")->setChangeEvents(OperationTypeMove);
    std::optional<Conflict> confTest =
        _syncPal->_conflictFinderWorker->checkMoveCreateConflict(_syncPal->_remoteUpdateTree->getNodeByPath("Dir 3/Dir 3.1"));
    CPPUNIT_ASSERT(confTest);
    CPPUNIT_ASSERT(confTest->node() == _syncPal->_remoteUpdateTree->getNodeByPath("Dir 3/Dir 3.1"));
    CPPUNIT_ASSERT(confTest->correspondingNode() == _syncPal->_localUpdateTree->getNodeByPath("Dir 3/Dir 3.1"));
    CPPUNIT_ASSERT(confTest->type() == ConflictType::ConflictTypeMoveCreate);
}

void TestConflictFinderWorker::testEditDelete() {
    setUpTreesAndDb();
    _syncPal->_localUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1")->setChangeEvents(OperationTypeEdit);
    _syncPal->_remoteUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1")->setChangeEvents(OperationTypeDelete);
    std::optional<Conflict> confTest = _syncPal->_conflictFinderWorker->checkEditDeleteConflict(
        _syncPal->_remoteUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1"));
    CPPUNIT_ASSERT(confTest);
    CPPUNIT_ASSERT(confTest->node() == _syncPal->_remoteUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1"));
    CPPUNIT_ASSERT(confTest->correspondingNode() ==
                   _syncPal->_localUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1"));
    CPPUNIT_ASSERT(confTest->type() == ConflictType::ConflictTypeEditDelete);
}

void TestConflictFinderWorker::testMoveDeleteFile() {
    setUpTreesAndDb();
    _syncPal->_localUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1")->setChangeEvents(OperationTypeMove);
    _syncPal->_remoteUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1")->setChangeEvents(OperationTypeDelete);
    std::optional<Conflict> confTest = _syncPal->_conflictFinderWorker->checkMoveDeleteConflict(
        _syncPal->_remoteUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1"));
    CPPUNIT_ASSERT(confTest);
    CPPUNIT_ASSERT(confTest->node() == _syncPal->_remoteUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1"));
    CPPUNIT_ASSERT(confTest->correspondingNode() ==
                   _syncPal->_localUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1"));
    CPPUNIT_ASSERT(confTest->type() == ConflictType::ConflictTypeMoveDelete);
}

void TestConflictFinderWorker::testMoveDeleteDir() {
    // TODO : to be implemented but this class needs to be refactored first.
    // We shouldn't have to setup a SyncPal completely to test one worker.
}

void TestConflictFinderWorker::testMoveParentDelete() {
    setUpTreesAndDb();
    _syncPal->_localUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1")->setChangeEvents(OperationTypeDelete);
    _syncPal->_remoteUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1")->setChangeEvents(OperationTypeMove);
    std::optional<std::vector<Conflict>> confTest = _syncPal->_conflictFinderWorker->checkMoveParentDeleteConflicts(
        _syncPal->_localUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1"));
    CPPUNIT_ASSERT(confTest);
    CPPUNIT_ASSERT(confTest->back().node() == _syncPal->_localUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1"));
    CPPUNIT_ASSERT(confTest->back().correspondingNode() ==
                   _syncPal->_remoteUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1"));
    CPPUNIT_ASSERT(confTest->back().type() == ConflictType::ConflictTypeMoveParentDelete);
}

void TestConflictFinderWorker::testCreateParentDelete() {
    setUpTreesAndDb();
    _syncPal->_localUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1")->setChangeEvents(OperationTypeDelete);
    _syncPal->_remoteUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1")->setChangeEvents(OperationTypeCreate);
    std::optional<std::vector<Conflict>> confTest = _syncPal->_conflictFinderWorker->checkCreateParentDeleteConflicts(
        _syncPal->_localUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1"));
    CPPUNIT_ASSERT(confTest);
    CPPUNIT_ASSERT(confTest->back().node() == _syncPal->_localUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1"));
    CPPUNIT_ASSERT(confTest->back().correspondingNode() ==
                   _syncPal->_remoteUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1"));
    CPPUNIT_ASSERT(confTest->back().type() == ConflictType::ConflictTypeCreateParentDelete);
}

void TestConflictFinderWorker::testMoveMoveSrc() {
    setUpTreesAndDb();
    std::shared_ptr<Node> localDir2 = _syncPal->_localUpdateTree->getNodeByPath("Dir 2");
    std::shared_ptr<Node> remoteDir2 = _syncPal->_remoteUpdateTree->getNodeByPath("Dir 2");
    localDir2->setChangeEvents(OperationTypeMove);
    remoteDir2->setChangeEvents(OperationTypeMove);
    _syncPal->_localUpdateTree->getNodeByPath("Dir 1")->insertChildren(localDir2);
    _syncPal->_remoteUpdateTree->getNodeByPath("Dir 3")->insertChildren(remoteDir2);
    localDir2->setParentNode(_syncPal->_localUpdateTree->getNodeByPath("Dir 1"));
    remoteDir2->setParentNode(_syncPal->_remoteUpdateTree->getNodeByPath("Dir 3"));
    _syncPal->_localUpdateTree->rootNode()->deleteChildren(localDir2);
    _syncPal->_remoteUpdateTree->rootNode()->deleteChildren(remoteDir2);

    std::optional<Conflict> confTest = _syncPal->_conflictFinderWorker->checkMoveMoveSourceConflict(localDir2);
    CPPUNIT_ASSERT(confTest);
    CPPUNIT_ASSERT(confTest->node() == localDir2);
    CPPUNIT_ASSERT(confTest->correspondingNode() == remoteDir2);
    CPPUNIT_ASSERT(confTest->type() == ConflictType::ConflictTypeMoveMoveSource);
}

void TestConflictFinderWorker::testMoveMoveDest() {
    setUpTreesAndDb();
    std::shared_ptr<Node> localDir2 = _syncPal->_localUpdateTree->getNodeByPath("Dir 2");
    std::shared_ptr<Node> remoteDir1 = _syncPal->_remoteUpdateTree->getNodeByPath("Dir 1");
    localDir2->setChangeEvents(OperationTypeMove);
    remoteDir1->setChangeEvents(OperationTypeMove);
    _syncPal->_localUpdateTree->getNodeByPath("Dir 3")->insertChildren(localDir2);
    _syncPal->_remoteUpdateTree->getNodeByPath("Dir 3")->insertChildren(remoteDir1);
    localDir2->setParentNode(_syncPal->_localUpdateTree->getNodeByPath("Dir 3"));
    remoteDir1->setParentNode(_syncPal->_remoteUpdateTree->getNodeByPath("Dir 3"));
    localDir2->setName(Str("Dir 3.2"));
    remoteDir1->setName(Str("Dir 3.2"));
    _syncPal->_localUpdateTree->rootNode()->deleteChildren(localDir2);
    _syncPal->_remoteUpdateTree->rootNode()->deleteChildren(remoteDir1);

    std::optional<Conflict> confTest = _syncPal->_conflictFinderWorker->checkMoveMoveDestConflict(localDir2);
    CPPUNIT_ASSERT(confTest);
    CPPUNIT_ASSERT(confTest->node() == localDir2);
    CPPUNIT_ASSERT(confTest->correspondingNode() == remoteDir1);
    CPPUNIT_ASSERT(confTest->type() == ConflictType::ConflictTypeMoveMoveDest);
}

void TestConflictFinderWorker::testMoveMoveCycle() {
    setUpTreesAndDb();
    std::shared_ptr<Node> A = _syncPal->_localUpdateTree->getNodeByPath("A");
    std::shared_ptr<Node> B = _syncPal->_remoteUpdateTree->getNodeByPath("B");

    // A moved in B/A
    A->setChangeEvents(OperationTypeMove);
    A->setParentNode(_syncPal->_localUpdateTree->getNodeByPath("B"));
    _syncPal->_localUpdateTree->getNodeByPath("B")->insertChildren(A);
    _syncPal->_localUpdateTree->rootNode()->deleteChildren(A);
    // B moved in A/B
    B->setChangeEvents(OperationTypeMove);
    B->setParentNode(_syncPal->_remoteUpdateTree->getNodeByPath("A"));
    _syncPal->_remoteUpdateTree->getNodeByPath("A")->insertChildren(B);
    _syncPal->_remoteUpdateTree->rootNode()->deleteChildren(B);

    std::optional<std::vector<Conflict>> confTestList = _syncPal->_conflictFinderWorker->determineMoveMoveCycleConflicts(
        {A, _syncPal->_localUpdateTree->getNodeByPath("Dir 1")}, {B, _syncPal->_remoteUpdateTree->getNodeByPath("Dir 1")});
    CPPUNIT_ASSERT(confTestList);
    CPPUNIT_ASSERT(confTestList->size() == 1);
    CPPUNIT_ASSERT(confTestList->back().node() == A);
    CPPUNIT_ASSERT(confTestList->back().correspondingNode() == B);
    CPPUNIT_ASSERT(confTestList->back().type() == ConflictType::ConflictTypeMoveMoveCycle);
}

/* Edit-Edit + Move-Create */
void TestConflictFinderWorker::testCase55b() {
    // cf p93 figure 5.5 (b)
    time_t tLoc = std::time(0);
    time_t tDrive = std::time(0);
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;
    DbNodeId dbNodeIdFileA;

    bool constraintError = false;
    DbNode nodeDbA(0, _syncPal->_syncDb->rootNode().nodeId(), Str("A"), Str("A"), "A", "rA", tLoc, tLoc, tDrive,
                   NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(nodeDbA, dbNodeIdFileA, constraintError);
    _syncPal->_localSnapshot->updateItem(SnapshotItem("A", _syncPal->_syncDb->rootNode().nodeIdLocal().value(), Str("A"), 222,
                                                      222, NodeTypeFile, 123, true, "36"));
    _syncPal->_remoteSnapshot->updateItem(SnapshotItem("rA", _syncPal->_syncDb->rootNode().nodeIdRemote().value(), Str("A"), 222,
                                                       222, NodeTypeFile, 123, true, "63"));

    // Start situation
    std::shared_ptr<Node> nodeA = std::shared_ptr<Node>(new Node(dbNodeIdFileA, _syncPal->_localUpdateTree->side(), Str("A"),
                                                                 NodeTypeFile, OperationTypeNone, "A", createdAt, lastmodified,
                                                                 size, _syncPal->_localUpdateTree->rootNode()));
    std::shared_ptr<Node> rNodeA = std::shared_ptr<Node>(new Node(dbNodeIdFileA, _syncPal->_remoteUpdateTree->side(), Str("A"),
                                                                  NodeTypeFile, OperationTypeNone, "rA", createdAt, lastmodified,
                                                                  size, _syncPal->_remoteUpdateTree->rootNode()));
    _syncPal->_localUpdateTree->rootNode()->insertChildren(nodeA);
    _syncPal->_localUpdateTree->insertNode(nodeA);
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(rNodeA);
    _syncPal->_remoteUpdateTree->insertNode(rNodeA);

    // Conflict Situation
    nodeA->setChangeEvents(OperationTypeMove | OperationTypeEdit);
    nodeA->setMoveOrigin("A");
    nodeA->setName(Str("B"));
    rNodeA->setChangeEvents(OperationTypeEdit);

    std::shared_ptr<Node> rNodeB = std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("B"),
                                                                  NodeTypeFile, OperationTypeCreate, "rB", createdAt,
                                                                  lastmodified, size, _syncPal->_remoteUpdateTree->rootNode()));
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(rNodeB);
    _syncPal->_remoteUpdateTree->insertNode(rNodeB);

    _syncPal->_conflictFinderWorker->findConflicts();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->size() == 2);
    auto conflict = _syncPal->_conflictQueue->top();
    _syncPal->_conflictQueue->pop();
    auto conflict1 = _syncPal->_conflictQueue->top();

    CPPUNIT_ASSERT(conflict.type() == ConflictTypeMoveCreate);
    CPPUNIT_ASSERT(conflict.node() == nodeA);
    CPPUNIT_ASSERT(conflict.correspondingNode() == rNodeB);
    CPPUNIT_ASSERT(conflict1.type() == ConflictTypeEditEdit);
    CPPUNIT_ASSERT(conflict1.node() == nodeA);
    CPPUNIT_ASSERT(conflict1.correspondingNode() == rNodeA);
}
/* Move-Move (Source) + Move-Create + Move-Create */
void TestConflictFinderWorker::testCase55c() {
    // cf p93 figure 5.5 (c)
    time_t tLoc = std::time(0);
    time_t tDrive = std::time(0);
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;
    DbNodeId dbNodeIdFileA;

    bool constraintError = false;
    DbNode nodeDbA(0, _syncPal->_syncDb->rootNode().nodeId(), Str("A"), Str("A"), "A", "rA", tLoc, tLoc, tDrive,
                   NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(nodeDbA, dbNodeIdFileA, constraintError);

    // Start situation
    std::shared_ptr<Node> nodeA = std::shared_ptr<Node>(new Node(dbNodeIdFileA, _syncPal->_localUpdateTree->side(), Str("A"),
                                                                 NodeTypeFile, OperationTypeNone, "A", createdAt, lastmodified,
                                                                 size, _syncPal->_localUpdateTree->rootNode()));
    std::shared_ptr<Node> rNodeA = std::shared_ptr<Node>(new Node(dbNodeIdFileA, _syncPal->_remoteUpdateTree->side(), Str("A"),
                                                                  NodeTypeFile, OperationTypeNone, "rA", createdAt, lastmodified,
                                                                  size, _syncPal->_remoteUpdateTree->rootNode()));
    _syncPal->_localUpdateTree->rootNode()->insertChildren(nodeA);
    _syncPal->_localUpdateTree->insertNode(nodeA);
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(rNodeA);
    _syncPal->_remoteUpdateTree->insertNode(rNodeA);

    // Conflict Situation
    nodeA->setChangeEvents(OperationTypeMove);
    nodeA->setMoveOrigin("A");
    nodeA->setName(Str("B"));
    rNodeA->setChangeEvents(OperationTypeMove);
    rNodeA->setMoveOrigin("A");
    rNodeA->setName(Str("C"));

    std::shared_ptr<Node> rNodeB = std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("B"),
                                                                  NodeTypeFile, OperationTypeCreate, "rB", createdAt,
                                                                  lastmodified, size, _syncPal->_remoteUpdateTree->rootNode()));
    _syncPal->_remoteUpdateTree->insertNode(rNodeB);
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(rNodeB);

    std::shared_ptr<Node> nodeC = std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_localUpdateTree->side(), Str("C"),
                                                                 NodeTypeFile, OperationTypeCreate, "C", createdAt, lastmodified,
                                                                 size, _syncPal->_localUpdateTree->rootNode()));
    _syncPal->_localUpdateTree->insertNode(nodeC);
    _syncPal->_localUpdateTree->rootNode()->insertChildren(nodeC);

    _syncPal->_conflictFinderWorker->findConflicts();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->size() == 3);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictTypeMoveMoveSource);
    _syncPal->_conflictQueue->pop();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictTypeMoveCreate);
    _syncPal->_conflictQueue->pop();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictTypeMoveCreate);
}

/* Move-ParentDelete > Move-Move (Source) */
void TestConflictFinderWorker::testCase57() {
    // cf p96 figure 5.7 (b)
    time_t tLoc = std::time(0);
    time_t tDrive = std::time(0);
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;
    DbNodeId dbNodeIdDirA;
    DbNodeId dbNodeIdDirB;
    DbNodeId dbNodeIdFileC;

    bool constraintError = false;
    DbNode nodeDbA(0, _syncPal->_syncDb->rootNode().nodeId(), Str("A"), Str("A"), "A", "rA", tLoc, tLoc, tDrive,
                   NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(nodeDbA, dbNodeIdDirA, constraintError);
    DbNode nodeDbB(0, _syncPal->_syncDb->rootNode().nodeId(), Str("B"), Str("B"), "B", "rB", tLoc, tLoc, tDrive,
                   NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(nodeDbB, dbNodeIdDirB, constraintError);
    DbNode nodeDbC(0, dbNodeIdDirA, Str("c"), Str("c"), "c", "rc", tLoc, tLoc, tDrive, NodeType::NodeTypeFile, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(nodeDbC, dbNodeIdFileC, constraintError);

    // Start situation
    std::shared_ptr<Node> nodeA = std::shared_ptr<Node>(new Node(dbNodeIdDirA, _syncPal->_localUpdateTree->side(), Str("A"),
                                                                 NodeTypeDirectory, OperationTypeNone, "A", createdAt,
                                                                 lastmodified, size, _syncPal->_localUpdateTree->rootNode()));
    std::shared_ptr<Node> nodeB = std::shared_ptr<Node>(new Node(dbNodeIdDirB, _syncPal->_localUpdateTree->side(), Str("B"),
                                                                 NodeTypeDirectory, OperationTypeNone, "B", createdAt,
                                                                 lastmodified, size, _syncPal->_localUpdateTree->rootNode()));
    std::shared_ptr<Node> nodeC =
        std::shared_ptr<Node>(new Node(dbNodeIdFileC, _syncPal->_localUpdateTree->side(), Str("c"), NodeTypeFile,
                                       OperationTypeNone, "c", createdAt, lastmodified, size, nodeA));
    std::shared_ptr<Node> rNodeA = std::shared_ptr<Node>(new Node(dbNodeIdDirA, _syncPal->_remoteUpdateTree->side(), Str("A"),
                                                                  NodeTypeDirectory, OperationTypeNone, "rA", createdAt,
                                                                  lastmodified, size, _syncPal->_remoteUpdateTree->rootNode()));
    std::shared_ptr<Node> rNodeB = std::shared_ptr<Node>(new Node(dbNodeIdDirB, _syncPal->_remoteUpdateTree->side(), Str("B"),
                                                                  NodeTypeDirectory, OperationTypeNone, "rB", createdAt,
                                                                  lastmodified, size, _syncPal->_remoteUpdateTree->rootNode()));
    std::shared_ptr<Node> rNodeC =
        std::shared_ptr<Node>(new Node(dbNodeIdFileC, _syncPal->_remoteUpdateTree->side(), Str("c"), NodeTypeFile,
                                       OperationTypeNone, "rc", createdAt, lastmodified, size, rNodeA));
    _syncPal->_localUpdateTree->rootNode()->insertChildren(nodeA);
    _syncPal->_localUpdateTree->rootNode()->insertChildren(nodeB);
    nodeA->insertChildren(nodeC);
    _syncPal->_localUpdateTree->insertNode(nodeA);
    _syncPal->_localUpdateTree->insertNode(nodeB);
    _syncPal->_localUpdateTree->insertNode(nodeC);
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(rNodeA);
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(rNodeB);
    rNodeA->insertChildren(rNodeC);
    _syncPal->_remoteUpdateTree->insertNode(rNodeA);
    _syncPal->_remoteUpdateTree->insertNode(rNodeB);
    _syncPal->_remoteUpdateTree->insertNode(rNodeC);

    // Conflict Situation
    nodeC->setParentNode(nodeB);
    nodeC->insertChangeEvent(OperationTypeMove);
    nodeB->insertChildren(nodeC);
    CPPUNIT_ASSERT(nodeA->deleteChildren(nodeC));
    rNodeC->insertChangeEvent(OperationTypeMove);
    rNodeC->setParentNode(_syncPal->_remoteUpdateTree->rootNode());
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(rNodeC);
    rNodeB->insertChangeEvent(OperationTypeDelete);
    CPPUNIT_ASSERT(rNodeA->deleteChildren(rNodeC));

    _syncPal->_conflictFinderWorker->findConflicts();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->size() == 2);
    auto conf1 = _syncPal->_conflictQueue->top();
    CPPUNIT_ASSERT(conf1.type() == ConflictTypeMoveParentDelete);
    CPPUNIT_ASSERT(conf1.node() == rNodeB);
    CPPUNIT_ASSERT(conf1.node()->parentNode() == _syncPal->_remoteUpdateTree->rootNode());
    CPPUNIT_ASSERT(conf1.correspondingNode() == nodeC);
    CPPUNIT_ASSERT(conf1.correspondingNode()->parentNode() == nodeB);
    _syncPal->_conflictQueue->pop();
    auto conf2 = _syncPal->_conflictQueue->top();
    CPPUNIT_ASSERT(conf2.type() == ConflictTypeMoveMoveSource);
    CPPUNIT_ASSERT(conf2.node() == rNodeC);
    CPPUNIT_ASSERT(conf2.node()->parentNode() == _syncPal->_remoteUpdateTree->rootNode());
    CPPUNIT_ASSERT(conf2.correspondingNode() == nodeC);
    CPPUNIT_ASSERT(conf2.correspondingNode()->parentNode() == nodeB);
}
/* Move-Delete > Move-Move (Dest) */
void TestConflictFinderWorker::testCase59() {
    // cf p98 figure 5.9 (b)
    time_t tLoc = std::time(0);
    time_t tDrive = std::time(0);
    DbNodeId dbNodeIdDirA;
    DbNodeId dbNodeIdDirB;

    bool constraintError = false;
    DbNode nodeDbA(0, _syncPal->_syncDb->rootNode().nodeId(), Str("A"), Str("A"), "A", "rA", tLoc, tLoc, tDrive,
                   NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(nodeDbA, dbNodeIdDirA, constraintError);
    DbNode nodeDbB(0, _syncPal->_syncDb->rootNode().nodeId(), Str("B"), Str("B"), "B", "rB", tLoc, tLoc, tDrive,
                   NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(nodeDbB, dbNodeIdDirB, constraintError);

    // Start situation
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;
    std::shared_ptr<Node> nodeA = std::shared_ptr<Node>(new Node(dbNodeIdDirA, _syncPal->_localUpdateTree->side(), Str("A"),
                                                                 NodeTypeDirectory, OperationTypeNone, "A", createdAt,
                                                                 lastmodified, size, _syncPal->_localUpdateTree->rootNode()));
    std::shared_ptr<Node> nodeB = std::shared_ptr<Node>(new Node(dbNodeIdDirB, _syncPal->_localUpdateTree->side(), Str("B"),
                                                                 NodeTypeDirectory, OperationTypeNone, "B", createdAt,
                                                                 lastmodified, size, _syncPal->_localUpdateTree->rootNode()));
    std::shared_ptr<Node> rNodeA = std::shared_ptr<Node>(new Node(dbNodeIdDirA, _syncPal->_remoteUpdateTree->side(), Str("A"),
                                                                  NodeTypeDirectory, OperationTypeNone, "rA", createdAt,
                                                                  lastmodified, size, _syncPal->_remoteUpdateTree->rootNode()));
    std::shared_ptr<Node> rNodeB = std::shared_ptr<Node>(new Node(dbNodeIdDirB, _syncPal->_remoteUpdateTree->side(), Str("B"),
                                                                  NodeTypeDirectory, OperationTypeNone, "rB", createdAt,
                                                                  lastmodified, size, _syncPal->_remoteUpdateTree->rootNode()));
    _syncPal->_localUpdateTree->rootNode()->insertChildren(nodeA);
    _syncPal->_localUpdateTree->rootNode()->insertChildren(nodeB);
    _syncPal->_localUpdateTree->insertNode(nodeA);
    _syncPal->_localUpdateTree->insertNode(nodeB);
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(rNodeA);
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(rNodeB);
    _syncPal->_remoteUpdateTree->insertNode(rNodeA);
    _syncPal->_remoteUpdateTree->insertNode(rNodeB);

    // Conflict situation
    nodeB->setName(Str("x"));
    nodeB->insertChangeEvent(OperationTypeMove);
    rNodeA->setName(Str("x"));
    rNodeA->insertChangeEvent(OperationTypeMove);
    rNodeB->insertChangeEvent(OperationTypeDelete);

    _syncPal->_conflictFinderWorker->findConflicts();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->size() == 2);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictTypeMoveDelete);
    _syncPal->_conflictQueue->pop();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictTypeMoveMoveDest);
}
/* Move-Delete | Create-ParentDelete */
void TestConflictFinderWorker::testCase510() {
    // cf p99 figure 5.10
    time_t tLoc = std::time(0);
    time_t tDrive = std::time(0);
    DbNodeId dbNodeIdDirA;
    DbNodeId dbNodeIdFileX;

    bool constraintError = false;
    DbNode nodeDbA(0, _syncPal->_syncDb->rootNode().nodeId(), Str("A"), Str("A"), "A", "rA", tLoc, tLoc, tDrive,
                   NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(nodeDbA, dbNodeIdDirA, constraintError);
    DbNode nodeDbX(0, _syncPal->_syncDb->rootNode().nodeId(), Str("x"), Str("x"), "X", "rX", tLoc, tLoc, tDrive,
                   NodeType::NodeTypeFile, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(nodeDbX, dbNodeIdFileX, constraintError);

    // Start situation
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;
    std::shared_ptr<Node> nodeA = std::shared_ptr<Node>(new Node(dbNodeIdDirA, _syncPal->_localUpdateTree->side(), Str("A"),
                                                                 NodeTypeDirectory, OperationTypeNone, "A", createdAt,
                                                                 lastmodified, size, _syncPal->_localUpdateTree->rootNode()));
    std::shared_ptr<Node> nodeX =
        std::shared_ptr<Node>(new Node(dbNodeIdFileX, _syncPal->_localUpdateTree->side(), Str("x"), NodeTypeFile,
                                       OperationTypeNone, "X", createdAt, lastmodified, size, nodeA));
    std::shared_ptr<Node> rNodeA = std::shared_ptr<Node>(new Node(dbNodeIdDirA, _syncPal->_remoteUpdateTree->side(), Str("A"),
                                                                  NodeTypeDirectory, OperationTypeNone, "rA", createdAt,
                                                                  lastmodified, size, _syncPal->_remoteUpdateTree->rootNode()));
    std::shared_ptr<Node> rNodeX =
        std::shared_ptr<Node>(new Node(dbNodeIdFileX, _syncPal->_remoteUpdateTree->side(), Str("x"), NodeTypeFile,
                                       OperationTypeNone, "rX", createdAt, lastmodified, size, rNodeA));

    _syncPal->_localUpdateTree->rootNode()->insertChildren(nodeA);
    nodeA->insertChildren(nodeX);
    _syncPal->_localUpdateTree->insertNode(nodeA);
    _syncPal->_localUpdateTree->insertNode(nodeX);
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(rNodeA);
    rNodeA->insertChildren(rNodeX);
    _syncPal->_remoteUpdateTree->insertNode(rNodeA);
    _syncPal->_remoteUpdateTree->insertNode(rNodeX);

    // Conflict situation
    nodeX->insertChangeEvent(OperationTypeMove);
    nodeX->setParentNode(_syncPal->_localUpdateTree->rootNode());
    _syncPal->_localUpdateTree->rootNode()->insertChildren(nodeX);
    CPPUNIT_ASSERT(nodeA->deleteChildren(nodeX));
    std::shared_ptr<Node> nodeX2 =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_localUpdateTree->side(), Str("x"), NodeTypeFile,
                                       OperationTypeCreate, "X2", createdAt, lastmodified, size, nodeA));
    _syncPal->_localUpdateTree->insertNode(nodeX2);
    nodeA->insertChildren(nodeX2);
    rNodeA->insertChangeEvent(OperationTypeDelete);
    rNodeX->insertChangeEvent(OperationTypeDelete);

    _syncPal->_conflictFinderWorker->findConflicts();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->size() == 2);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictTypeMoveDelete);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().node() == rNodeX);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().correspondingNode() == nodeX);
    _syncPal->_conflictQueue->pop();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictTypeCreateParentDelete);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().node() == rNodeA);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().correspondingNode() == nodeX2);
}
/* Move-Delete > Create-ParentDelete */
void TestConflictFinderWorker::testCase511() {
    // cf p100 figure 5.11
    time_t tLoc = std::time(0);
    time_t tDrive = std::time(0);
    DbNodeId dbNodeIdDirA;
    DbNodeId dbNodeIdDirB;

    bool constraintError = false;
    DbNode nodeDbA(0, _syncPal->_syncDb->rootNode().nodeId(), Str("A"), Str("A"), "A", "rA", tLoc, tLoc, tDrive,
                   NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(nodeDbA, dbNodeIdDirA, constraintError);
    DbNode nodeDbB(0, _syncPal->_syncDb->rootNode().nodeId(), Str("B"), Str("B"), "B", "rB", tLoc, tLoc, tDrive,
                   NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(nodeDbB, dbNodeIdDirB, constraintError);

    // Start situation
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;
    std::shared_ptr<Node> nodeA = std::shared_ptr<Node>(new Node(dbNodeIdDirA, _syncPal->_localUpdateTree->side(), Str("A"),
                                                                 NodeTypeDirectory, OperationTypeNone, "A", createdAt,
                                                                 lastmodified, size, _syncPal->_localUpdateTree->rootNode()));
    std::shared_ptr<Node> nodeB =
        std::shared_ptr<Node>(new Node(dbNodeIdDirB, _syncPal->_localUpdateTree->side(), Str("B"), NodeTypeDirectory,
                                       OperationTypeNone, "B", createdAt, lastmodified, size, nodeA));
    std::shared_ptr<Node> rNodeA = std::shared_ptr<Node>(new Node(dbNodeIdDirA, _syncPal->_remoteUpdateTree->side(), Str("A"),
                                                                  NodeTypeDirectory, OperationTypeNone, "rA", createdAt,
                                                                  lastmodified, size, _syncPal->_remoteUpdateTree->rootNode()));
    std::shared_ptr<Node> rNodeB =
        std::shared_ptr<Node>(new Node(dbNodeIdDirB, _syncPal->_remoteUpdateTree->side(), Str("B"), NodeTypeDirectory,
                                       OperationTypeNone, "rB", createdAt, lastmodified, size, rNodeA));

    _syncPal->_localUpdateTree->rootNode()->insertChildren(nodeA);
    nodeA->insertChildren(nodeB);
    _syncPal->_localUpdateTree->insertNode(nodeA);
    _syncPal->_localUpdateTree->insertNode(nodeB);
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(rNodeA);
    rNodeA->insertChildren(rNodeB);
    _syncPal->_remoteUpdateTree->insertNode(rNodeA);
    _syncPal->_remoteUpdateTree->insertNode(rNodeB);

    // Conflict situation
    nodeB->insertChangeEvent(OperationTypeDelete);
    nodeA->insertChangeEvent(OperationTypeDelete);
    CPPUNIT_ASSERT(rNodeA->deleteChildren(rNodeB));
    rNodeB->setParentNode(_syncPal->_remoteUpdateTree->rootNode());
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(rNodeB);
    rNodeB->setName(Str("B_moved"));
    rNodeB->insertChangeEvent(OperationTypeMove);
    std::shared_ptr<Node> nodeNewFile =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("new.txt"), NodeTypeFile,
                                       OperationTypeCreate, "new", createdAt, lastmodified, size, rNodeB));
    rNodeB->insertChildren(nodeNewFile);
    _syncPal->_remoteUpdateTree->insertNode(nodeNewFile);

    _syncPal->_conflictFinderWorker->findConflicts();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->size() == 2);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictTypeMoveDelete);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().node() == nodeB);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().correspondingNode() == rNodeB);
    _syncPal->_conflictQueue->pop();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictTypeCreateParentDelete);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().node() == nodeB);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().correspondingNode() == nodeNewFile);
}
/* Move_Move_Cycle */
void TestConflictFinderWorker::testCase513() {
    // cf p103 figure 5.13
    time_t tLoc = std::time(0);
    time_t tDrive = std::time(0);
    DbNodeId dbNodeIdDirQ;
    DbNodeId dbNodeIdDirR;
    DbNodeId dbNodeIdDirN;

    bool constraintError = false;
    DbNode nodeDbQ(0, _syncPal->_syncDb->rootNode().nodeId(), Str("q"), Str("q"), "q", "rq", tLoc, tLoc, tDrive,
                   NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(nodeDbQ, dbNodeIdDirQ, constraintError);
    DbNode nodeDbR(0, _syncPal->_syncDb->rootNode().nodeId(), Str("r"), Str("r"), "r", "rr", tLoc, tLoc, tDrive,
                   NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(nodeDbR, dbNodeIdDirR, constraintError);
    DbNode nodeDbN(0, dbNodeIdDirQ, Str("n"), Str("n"), "n", "rn", tLoc, tLoc, tDrive, NodeType::NodeTypeDirectory, 0,
                   std::nullopt);
    _syncPal->_syncDb->insertNode(nodeDbN, dbNodeIdDirN, constraintError);

    // Start situation
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;
    std::shared_ptr<Node> nodeQ = std::shared_ptr<Node>(new Node(dbNodeIdDirQ, _syncPal->_localUpdateTree->side(), Str("q"),
                                                                 NodeTypeDirectory, OperationTypeNone, "q", createdAt,
                                                                 lastmodified, size, _syncPal->_localUpdateTree->rootNode()));
    std::shared_ptr<Node> nodeR = std::shared_ptr<Node>(new Node(dbNodeIdDirR, _syncPal->_localUpdateTree->side(), Str("r"),
                                                                 NodeTypeDirectory, OperationTypeNone, "r", createdAt,
                                                                 lastmodified, size, _syncPal->_localUpdateTree->rootNode()));
    std::shared_ptr<Node> nodeN =
        std::shared_ptr<Node>(new Node(dbNodeIdDirN, _syncPal->_localUpdateTree->side(), Str("n"), NodeTypeDirectory,
                                       OperationTypeNone, "n", createdAt, lastmodified, size, nodeQ));
    std::shared_ptr<Node> rNodeQ = std::shared_ptr<Node>(new Node(dbNodeIdDirQ, _syncPal->_remoteUpdateTree->side(), Str("q"),
                                                                  NodeTypeDirectory, OperationTypeNone, "rq", createdAt,
                                                                  lastmodified, size, _syncPal->_remoteUpdateTree->rootNode()));
    std::shared_ptr<Node> rNodeR = std::shared_ptr<Node>(new Node(dbNodeIdDirR, _syncPal->_remoteUpdateTree->side(), Str("r"),
                                                                  NodeTypeDirectory, OperationTypeNone, "rr", createdAt,
                                                                  lastmodified, size, _syncPal->_remoteUpdateTree->rootNode()));
    std::shared_ptr<Node> rNodeN =
        std::shared_ptr<Node>(new Node(dbNodeIdDirN, _syncPal->_remoteUpdateTree->side(), Str("n"), NodeTypeDirectory,
                                       OperationTypeNone, "rn", createdAt, lastmodified, size, rNodeQ));

    _syncPal->_localUpdateTree->rootNode()->insertChildren(nodeQ);
    _syncPal->_localUpdateTree->rootNode()->insertChildren(nodeR);
    nodeQ->insertChildren(nodeN);
    _syncPal->_localUpdateTree->insertNode(nodeQ);
    _syncPal->_localUpdateTree->insertNode(nodeR);
    _syncPal->_localUpdateTree->insertNode(nodeN);
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(rNodeQ);
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(rNodeR);
    rNodeQ->insertChildren(rNodeN);
    _syncPal->_remoteUpdateTree->insertNode(rNodeQ);
    _syncPal->_remoteUpdateTree->insertNode(rNodeR);
    _syncPal->_remoteUpdateTree->insertNode(rNodeN);

    // Conflict situation
    nodeR->insertChangeEvent(OperationTypeMove);
    nodeR->setParentNode(nodeN);
    nodeN->insertChildren(nodeR);
    CPPUNIT_ASSERT(_syncPal->_localUpdateTree->rootNode()->deleteChildren(nodeR));
    rNodeQ->insertChangeEvent(OperationTypeMove);
    rNodeQ->setParentNode(rNodeR);
    rNodeR->insertChildren(rNodeQ);
    CPPUNIT_ASSERT(_syncPal->_remoteUpdateTree->rootNode()->deleteChildren(rNodeQ));
    rNodeN->insertChangeEvent(OperationTypeMove);
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(rNodeN);
    // update the node's name in the parent's children map
    rNodeN->setParentNode(_syncPal->_remoteUpdateTree->rootNode());
    CPPUNIT_ASSERT(rNodeQ->deleteChildren(rNodeN));
    rNodeN->setName(Str("n_moved2"));

    _syncPal->_conflictFinderWorker->findConflicts();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->size() == 1);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictTypeMoveMoveCycle);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().node() == nodeR);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().correspondingNode() == rNodeQ);
}
/* Move_Move_Cycle & 2 Move-Move-Source */
void TestConflictFinderWorker::testCase516() {
    // cf p106 figure 5.16
    time_t tLoc = std::time(0);
    time_t tDrive = std::time(0);
    DbNodeId dbNodeIdDirQ;
    DbNodeId dbNodeIdDirR;
    DbNodeId dbNodeIdDirN;
    DbNodeId dbNodeIdDirM;

    bool constraintError = false;
    DbNode nodeDbQ(0, _syncPal->_syncDb->rootNode().nodeId(), Str("q"), Str("q"), "q", "rq", tLoc, tLoc, tDrive,
                   NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(nodeDbQ, dbNodeIdDirQ, constraintError);
    DbNode nodeDbR(0, _syncPal->_syncDb->rootNode().nodeId(), Str("r"), Str("r"), "r", "rr", tLoc, tLoc, tDrive,
                   NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(nodeDbR, dbNodeIdDirR, constraintError);
    DbNode nodeDbN(0, dbNodeIdDirR, Str("n"), Str("n"), "n", "rn", tLoc, tLoc, tDrive, NodeType::NodeTypeDirectory, 0,
                   std::nullopt);
    _syncPal->_syncDb->insertNode(nodeDbN, dbNodeIdDirN, constraintError);
    DbNode nodeDbM(0, dbNodeIdDirQ, Str("m"), Str("m"), "m", "rm", tLoc, tLoc, tDrive, NodeType::NodeTypeDirectory, 0,
                   std::nullopt);
    _syncPal->_syncDb->insertNode(nodeDbM, dbNodeIdDirM, constraintError);

    // Start situation
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;
    std::shared_ptr<Node> nodeQ = std::shared_ptr<Node>(new Node(dbNodeIdDirQ, _syncPal->_localUpdateTree->side(), Str("q"),
                                                                 NodeTypeDirectory, OperationTypeNone, "q", createdAt,
                                                                 lastmodified, size, _syncPal->_localUpdateTree->rootNode()));
    std::shared_ptr<Node> nodeM =
        std::shared_ptr<Node>(new Node(dbNodeIdDirM, _syncPal->_localUpdateTree->side(), Str("m"), NodeTypeDirectory,
                                       OperationTypeNone, "m", createdAt, lastmodified, size, nodeQ));
    std::shared_ptr<Node> nodeR = std::shared_ptr<Node>(new Node(dbNodeIdDirR, _syncPal->_localUpdateTree->side(), Str("r"),
                                                                 NodeTypeDirectory, OperationTypeNone, "r", createdAt,
                                                                 lastmodified, size, _syncPal->_localUpdateTree->rootNode()));
    std::shared_ptr<Node> nodeN =
        std::shared_ptr<Node>(new Node(dbNodeIdDirN, _syncPal->_localUpdateTree->side(), Str("n"), NodeTypeDirectory,
                                       OperationTypeNone, "n", createdAt, lastmodified, size, nodeR));
    std::shared_ptr<Node> rNodeQ = std::shared_ptr<Node>(new Node(dbNodeIdDirQ, _syncPal->_remoteUpdateTree->side(), Str("q"),
                                                                  NodeTypeDirectory, OperationTypeNone, "rq", createdAt,
                                                                  lastmodified, size, _syncPal->_remoteUpdateTree->rootNode()));
    std::shared_ptr<Node> rNodeM =
        std::shared_ptr<Node>(new Node(dbNodeIdDirM, _syncPal->_remoteUpdateTree->side(), Str("m"), NodeTypeDirectory,
                                       OperationTypeNone, "rm", createdAt, lastmodified, size, rNodeQ));
    std::shared_ptr<Node> rNodeR = std::shared_ptr<Node>(new Node(dbNodeIdDirR, _syncPal->_remoteUpdateTree->side(), Str("r"),
                                                                  NodeTypeDirectory, OperationTypeNone, "rr", createdAt,
                                                                  lastmodified, size, _syncPal->_remoteUpdateTree->rootNode()));
    std::shared_ptr<Node> rNodeN =
        std::shared_ptr<Node>(new Node(dbNodeIdDirN, _syncPal->_remoteUpdateTree->side(), Str("n"), NodeTypeDirectory,
                                       OperationTypeNone, "rn", createdAt, lastmodified, size, rNodeR));

    _syncPal->_localUpdateTree->rootNode()->insertChildren(nodeQ);
    nodeQ->insertChildren(nodeM);
    _syncPal->_localUpdateTree->rootNode()->insertChildren(nodeR);
    nodeR->insertChildren(nodeN);
    _syncPal->_localUpdateTree->insertNode(nodeQ);
    _syncPal->_localUpdateTree->insertNode(nodeR);
    _syncPal->_localUpdateTree->insertNode(nodeN);
    _syncPal->_localUpdateTree->insertNode(nodeM);
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(rNodeQ);
    rNodeQ->insertChildren(rNodeM);
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(rNodeR);
    rNodeR->insertChildren(rNodeN);
    _syncPal->_remoteUpdateTree->insertNode(rNodeQ);
    _syncPal->_remoteUpdateTree->insertNode(rNodeR);
    _syncPal->_remoteUpdateTree->insertNode(rNodeN);
    _syncPal->_remoteUpdateTree->insertNode(rNodeM);

    // Conflict situation
    CPPUNIT_ASSERT(nodeR->deleteChildren(nodeN));
    CPPUNIT_ASSERT(nodeQ->deleteChildren(nodeM));
    nodeM->insertChangeEvent(OperationTypeMove);
    nodeM->setParentNode(_syncPal->_localUpdateTree->rootNode());
    _syncPal->_localUpdateTree->rootNode()->insertChildren(nodeM);
    nodeM->insertChildren(nodeN);
    nodeN->insertChangeEvent(OperationTypeMove);
    nodeN->setParentNode(nodeM);
    nodeN->insertChildren(nodeQ);
    nodeQ->insertChangeEvent(OperationTypeMove);
    nodeQ->setParentNode(nodeN);

    CPPUNIT_ASSERT(_syncPal->_remoteUpdateTree->rootNode()->deleteChildren(rNodeR));
    rNodeR->insertChangeEvent(OperationTypeMove);
    rNodeR->setParentNode(rNodeQ);
    rNodeQ->insertChildren(rNodeR);
    CPPUNIT_ASSERT(rNodeR->deleteChildren(rNodeN));
    rNodeN->insertChangeEvent(OperationTypeMove);
    rNodeN->setParentNode(_syncPal->_remoteUpdateTree->rootNode());
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(rNodeN);
    CPPUNIT_ASSERT(rNodeQ->deleteChildren(rNodeM));
    rNodeM->insertChangeEvent(OperationTypeMove);
    rNodeM->setParentNode(rNodeN);
    rNodeN->insertChildren(rNodeM);

    _syncPal->_conflictFinderWorker->findConflicts();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->size() == 3);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictTypeMoveMoveSource);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().node() == nodeM);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().node()->side() == nodeM->side());
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().correspondingNode() == rNodeM);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().correspondingNode()->side() == rNodeM->side());
    _syncPal->_conflictQueue->pop();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictTypeMoveMoveSource);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().node() == rNodeN);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().node()->side() == rNodeN->side());
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().correspondingNode() == nodeN);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().correspondingNode()->side() == nodeN->side());
    _syncPal->_conflictQueue->pop();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictTypeMoveMoveCycle);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().node() == nodeN);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().node()->side() == nodeN->side());
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().correspondingNode() == rNodeM);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().correspondingNode()->side() == rNodeM->side());
}

}  // namespace KDC
