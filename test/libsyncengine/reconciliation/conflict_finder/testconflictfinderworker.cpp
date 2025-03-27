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

#include "testconflictfinderworker.h"
#include "mocks/libcommonserver/db/mockdb.h"

#include "test_utility/testhelpers.h"

using namespace CppUnit;

namespace KDC {

void TestConflictFinderWorker::setUp() {
    TestBase::start();
    // Create SyncPal
    bool alreadyExists;
    std::filesystem::path parmsDbPath = MockDb::makeDbName(alreadyExists);
    ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);

    SyncPath syncDbPath = Db::makeDbName(1, 1, 1, 1, alreadyExists);
    std::filesystem::remove(syncDbPath);
    _syncPal = std::make_shared<SyncPal>(std::make_shared<VfsOff>(VfsSetupParams(Log::instance()->getLogger())), syncDbPath,
                                         KDRIVE_VERSION_STRING, true);
    _syncPal->syncDb()->setAutoDelete(true);
    _syncPal->createSharedObjects();

    _syncPal->_conflictFinderWorker =
            std::shared_ptr<ConflictFinderWorker>(new ConflictFinderWorker(_syncPal, "Conflict Finder", "COFD"));

    _syncPal->updateTree(ReplicaSide::Local)->init();
    _syncPal->updateTree(ReplicaSide::Remote)->init();
}

void TestConflictFinderWorker::tearDown() {
    ParmsDb::instance()->close();
    ParmsDb::reset();
    _syncPal->syncDb()->close();
    TestBase::stop();
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
    DbNode nodeDbA(0, _syncPal->syncDb()->rootNode().nodeId(), Str("A"), Str("A"), "A", "rA", tLoc, tLoc, tDrive,
                   NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDbA, dbNodeIdDirA, constraintError);
    DbNode nodeDbB(0, _syncPal->syncDb()->rootNode().nodeId(), Str("B"), Str("B"), "B", "rB", tLoc, tLoc, tDrive,
                   NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDbB, dbNodeIdDirB, constraintError);
    DbNode nodeDir1(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 1"), Str("Dir 1"), "d1", "r1", tLoc, tLoc, tDrive,
                    NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir1, dbNodeIdDir1, constraintError);
    DbNode nodeFile112(0, dbNodeIdDir11, Str("File 1.1.2"), Str("File 1.1.2"), "l112", "r112", tLoc, tLoc, tDrive, NodeType::File,
                       0, "cs 1.1");
    _syncPal->syncDb()->insertNode(nodeFile112, dbNodeId112, constraintError);
    DbNode nodeDir111(0, dbNodeIdDir111, Str("Dir 1.1.1"), Str("Dir 1.1.1"), "l111", "r111", tLoc, tLoc, tDrive,
                      NodeType::Directory, 0, "cs 1.1.1");
    _syncPal->syncDb()->insertNode(nodeDir111, dbNodeIdDir111, constraintError);
    DbNode nodeFile1111(0, dbNodeIdDir111, Str("File 1.1.1.1"), Str("File 1.1.1.1"), "l1111", "r1111", tLoc, tLoc, tDrive,
                        NodeType::File, 0, "cs 1.1.1");
    _syncPal->syncDb()->insertNode(nodeFile1111, dbNodeIdFile1111, constraintError);
    DbNode nodeDir2(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 2"), Str("Dir 2"), "2", "r2", tLoc, tLoc, tDrive,
                    NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir2, dbNodeIdDir2, constraintError);
    DbNode nodeDir3(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 3"), Str("Dir 3"), "3", "r3", tLoc, tLoc, tDrive,
                    NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir3, dbNodeIdDir3, constraintError);
    DbNode nodeDir31(0, dbNodeIdDir3, Str("Dir 3.1"), Str("Dir 3.1"), "31", "r31", tLoc, tLoc, tDrive, NodeType::Directory, 0,
                     std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir31, dbNodeIdDir31, constraintError);
    DbNode nodeDir4(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 4"), Str("Dir 4"), "4", "r4", tLoc, tLoc, tDrive,
                    NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir4, dbnodeIdDir4, constraintError);
    DbNode nodedir41(0, dbnodeIdDir4, Str("Dir 4.1"), Str("Dir 4.1"), "41", "r41", tLoc, tLoc, tDrive, NodeType::Directory, 0,
                     std::nullopt);
    _syncPal->syncDb()->insertNode(nodedir41, dbnodeIdDir41, constraintError);
    DbNode nodeDir411(0, dbnodeIdDir41, Str("Dir 4.1.1"), Str("Dir 4.1.1"), "411", "r411", tLoc, tLoc, tDrive,
                      NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir411, dbnodeIdDir411, constraintError);
    DbNode nodeFile4111(0, dbnodeIdDir411, Str("File 4.1.1.1"), Str("File 4.1.1.1"), "4111", "r4111", tLoc, tLoc, tDrive,
                        NodeType::File, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeFile4111, dbnodeIdfile4111, constraintError);
    DbNode nodeFile4112(0, dbnodeIdDir411, Str("File 4.1.1.2"), Str("File 4.1.1.2"), "4112", "r4112", tLoc, tLoc, tDrive,
                        NodeType::File, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeFile4112, dbnodeIdfile4112, constraintError);
    DbNode nodeDir5(0, _syncPal->syncDb()->rootNode().nodeId(), Str("Dir 5"), Str("Dir 5"), "5", "r5", tLoc, tLoc, tDrive,
                    NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDir5, dbnodeIdDir5, constraintError);
    DbNode nodeFile51(0, dbnodeIdDir5, Str("File 5.1"), Str("File 5.1"), "51", "r51", tLoc, tLoc, tDrive, NodeType::File, 0,
                      std::nullopt);
    _syncPal->syncDb()->insertNode(nodeFile51, dbnodeIdfile51, constraintError);

    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;
    const auto nodeA =
            std::make_shared<Node>(dbNodeIdDirA, ReplicaSide::Local, Str("A"), NodeType::Directory, OperationType::None, "A",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Local)->rootNode());
    const auto nodeB =
            std::make_shared<Node>(dbNodeIdDirB, ReplicaSide::Local, Str("B"), NodeType::Directory, OperationType::None, "B",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Local)->rootNode());
    const auto node1 =
            std::make_shared<Node>(dbNodeIdDir1, ReplicaSide::Local, Str("Dir 1"), NodeType::Directory, OperationType::None, "l1",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Local)->rootNode());
    const auto node2 =
            std::make_shared<Node>(dbNodeIdDir2, ReplicaSide::Local, Str("Dir 2"), NodeType::Directory, OperationType::None, "2",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Local)->rootNode());
    const auto node3 =
            std::make_shared<Node>(dbNodeIdDir3, ReplicaSide::Local, Str("Dir 3"), NodeType::Directory, OperationType::None, "3",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Local)->rootNode());
    const auto node4 =
            std::make_shared<Node>(dbnodeIdDir4, ReplicaSide::Local, Str("Dir 4"), NodeType::Directory, OperationType::None, "4",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Local)->rootNode());
    const auto node11 = std::make_shared<Node>(std::nullopt, ReplicaSide::Local, Str("Dir 1.1"), NodeType::Directory,
                                               OperationType::None, "l11", createdAt, lastmodified, size, node1);
    const auto node111 = std::make_shared<Node>(dbNodeIdDir111, ReplicaSide::Local, Str("Dir 1.1.1"), NodeType::Directory,
                                                OperationType::None, "l111", createdAt, lastmodified, size, node11);
    const auto node1111 = std::make_shared<Node>(dbNodeIdFile1111, ReplicaSide::Local, Str("File 1.1.1.1"), NodeType::File,
                                                 OperationType::None, "l1111", createdAt, lastmodified, size, node111);
    const auto node31 = std::make_shared<Node>(dbNodeIdDir31, ReplicaSide::Local, Str("Dir 3.1"), NodeType::Directory,
                                               OperationType::None, "31", createdAt, lastmodified, size, node3);
    const auto node41 = std::make_shared<Node>(dbnodeIdDir41, ReplicaSide::Local, Str("Dir 4.1"), NodeType::Directory,
                                               OperationType::None, "41", createdAt, lastmodified, size, node4);
    const auto node411 = std::make_shared<Node>(dbnodeIdDir411, ReplicaSide::Local, Str("Dir 4.1.1"), NodeType::Directory,
                                                OperationType::None, "411", createdAt, lastmodified, size, node41);
    const auto node4111 = std::make_shared<Node>(dbnodeIdfile4111, ReplicaSide::Local, Str("File 4.1.1.1"), NodeType::File,
                                                 OperationType::None, "4111", createdAt, lastmodified, size, node411);

    const auto rNodeA =
            std::make_shared<Node>(dbNodeIdDirA, ReplicaSide::Remote, Str("A"), NodeType::Directory, OperationType::None, "rA",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());
    const auto rNodeB =
            std::make_shared<Node>(dbNodeIdDirB, ReplicaSide::Remote, Str("B"), NodeType::Directory, OperationType::None, "rB",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());
    const auto rNode1 =
            std::make_shared<Node>(dbNodeIdDir1, ReplicaSide::Remote, Str("Dir 1"), NodeType::Directory, OperationType::None,
                                   "r1", createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());
    const auto rNode2 =
            std::make_shared<Node>(dbNodeIdDir2, ReplicaSide::Remote, Str("Dir 2"), NodeType::Directory, OperationType::None,
                                   "r2", createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());
    const auto rNode3 =
            std::make_shared<Node>(dbNodeIdDir3, ReplicaSide::Remote, Str("Dir 3"), NodeType::Directory, OperationType::None,
                                   "r3", createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());
    const auto rNode4 =
            std::make_shared<Node>(dbnodeIdDir4, ReplicaSide::Remote, Str("Dir 4"), NodeType::Directory, OperationType::None,
                                   "r4", createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());
    const auto rNode11 = std::make_shared<Node>(std::nullopt, ReplicaSide::Remote, Str("Dir 1.1"), NodeType::Directory,
                                                OperationType::None, "r11", createdAt, lastmodified, size, rNode1);
    const auto rNode111 = std::make_shared<Node>(dbNodeIdDir111, ReplicaSide::Remote, Str("Dir 1.1.1"), NodeType::Directory,
                                                 OperationType::None, "r111", createdAt, lastmodified, size, rNode11);
    const auto rNode1111 = std::make_shared<Node>(dbNodeIdFile1111, ReplicaSide::Remote, Str("File 1.1.1.1"), NodeType::File,
                                                  OperationType::None, "r1111", createdAt, lastmodified, size, rNode111);
    const auto rNode31 = std::make_shared<Node>(dbNodeIdDir31, ReplicaSide::Remote, Str("Dir 3.1"), NodeType::Directory,
                                                OperationType::None, "r31", createdAt, lastmodified, size, rNode3);
    const auto rNode41 = std::make_shared<Node>(dbnodeIdDir41, ReplicaSide::Remote, Str("Dir 4.1"), NodeType::Directory,
                                                OperationType::None, "r41", createdAt, lastmodified, size, rNode4);
    const auto rNode411 = std::make_shared<Node>(dbnodeIdDir411, ReplicaSide::Remote, Str("Dir 4.1.1"), NodeType::Directory,
                                                 OperationType::None, "r411", createdAt, lastmodified, size, rNode41);
    const auto rNode4111 = std::make_shared<Node>(dbnodeIdfile4111, ReplicaSide::Remote, Str("File 4.1.1.1"), NodeType::File,
                                                  OperationType::None, "r4111", createdAt, lastmodified, size, rNode411);

    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->insertChildren(nodeA));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->insertChildren(nodeB));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->insertChildren(node1));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->insertChildren(node2));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->insertChildren(node3));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->insertChildren(node4));
    CPPUNIT_ASSERT(node1->insertChildren(node11));
    CPPUNIT_ASSERT(node11->insertChildren(node111));
    CPPUNIT_ASSERT(node111->insertChildren(node1111));
    CPPUNIT_ASSERT(node3->insertChildren(node31));
    CPPUNIT_ASSERT(node4->insertChildren(node41));
    CPPUNIT_ASSERT(node41->insertChildren(node411));
    CPPUNIT_ASSERT(node411->insertChildren(node4111));

    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNodeA));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNodeB));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNode1));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNode2));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNode3));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNode4));
    CPPUNIT_ASSERT(rNode1->insertChildren(rNode11));
    CPPUNIT_ASSERT(rNode11->insertChildren(rNode111));
    CPPUNIT_ASSERT(rNode111->insertChildren(rNode1111));
    CPPUNIT_ASSERT(rNode3->insertChildren(rNode31));
    CPPUNIT_ASSERT(rNode4->insertChildren(rNode41));
    CPPUNIT_ASSERT(rNode41->insertChildren(rNode411));
    CPPUNIT_ASSERT(rNode411->insertChildren(rNode4111));

    _syncPal->updateTree(ReplicaSide::Local)->insertNode(node1111);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(node111);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(node11);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeA);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeB);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(node1);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(node2);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(node3);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(node4);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(node31);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(node41);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(node411);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(node4111);

    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNode1111);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNode111);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNode11);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeA);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeB);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNode1);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNode2);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNode3);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNode4);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNode31);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNode41);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNode411);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNode4111);
}

void TestConflictFinderWorker::testCreateCreate() {
    setUpTreesAndDb();
    // edit conflict
    auto localNode = _syncPal->updateTree(ReplicaSide::Local)->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1");
    localNode->setChangeEvents(OperationType::Create);
    localNode->setSize(222);

    auto remoteNode = _syncPal->updateTree(ReplicaSide::Remote)->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1");
    remoteNode->setChangeEvents(OperationType::Create);
    remoteNode->setSize(223);

    std::optional<Conflict> confTest = _syncPal->_conflictFinderWorker->checkCreateCreateConflict(localNode);
    CPPUNIT_ASSERT(confTest);
    CPPUNIT_ASSERT(confTest->node() ==
                   _syncPal->updateTree(ReplicaSide::Local)->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1"));
    CPPUNIT_ASSERT(confTest->correspondingNode() ==
                   _syncPal->updateTree(ReplicaSide::Remote)->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1"));
    CPPUNIT_ASSERT(confTest->type() == ConflictType::CreateCreate);
}

void TestConflictFinderWorker::testEditEdit() {
    setUpTreesAndDb();
    // edit conflict
    std::string rootId = _syncPal->_localSnapshot->rootFolderId();
    std::string rRootId = _syncPal->_remoteSnapshot->rootFolderId();
    _syncPal->_localSnapshot->updateItem(
            SnapshotItem("l1", rootId, Str("Dir 1"), 222, 222, NodeType::Directory, 123, false, true, true));
    _syncPal->_localSnapshot->updateItem(
            SnapshotItem("l11", "l1", Str("Dir 1.1"), 222, 222, NodeType::Directory, 123, false, true, true));
    _syncPal->_localSnapshot->updateItem(
            SnapshotItem("l111", "l11", Str("Dir 1.1.1"), 222, 222, NodeType::Directory, 123, false, true, true));
    _syncPal->_localSnapshot->updateItem(
            SnapshotItem("l1111", "l111", Str("File 1.1.1.1"), 222, 222, NodeType::File, 123, false, true, true));
    _syncPal->_remoteSnapshot->updateItem(
            SnapshotItem("r1", rRootId, Str("Dir 1"), 222, 222, NodeType::Directory, 123, false, true, true));
    _syncPal->_remoteSnapshot->updateItem(
            SnapshotItem("r11", "r1", Str("Dir 1.1"), 222, 222, NodeType::Directory, 123, false, true, true));
    _syncPal->_remoteSnapshot->updateItem(
            SnapshotItem("r111", "r11", Str("Dir 1.1.1"), 222, 222, NodeType::Directory, 123, false, true, true));
    _syncPal->_remoteSnapshot->updateItem(
            SnapshotItem("r1111", "r111", Str("File 1.1.1.1"), 222, 222, NodeType::File, 123, false, true, true));

    auto localNode = _syncPal->updateTree(ReplicaSide::Local)->getNodeByPath("Dir 1/Dir 1.1/Dir 1.1.1/File 1.1.1.1");
    localNode->setChangeEvents(OperationType::Edit);
    localNode->setSize(222);

    auto remoteNode = _syncPal->updateTree(ReplicaSide::Remote)->getNodeByPath("Dir 1/Dir 1.1/Dir 1.1.1/File 1.1.1.1");
    remoteNode->setChangeEvents(OperationType::Edit);
    remoteNode->setSize(223);

    std::optional<Conflict> confTest = _syncPal->_conflictFinderWorker->checkEditEditConflict(localNode);
    CPPUNIT_ASSERT(confTest);
    CPPUNIT_ASSERT(confTest->node() ==
                   _syncPal->updateTree(ReplicaSide::Local)->getNodeByPath("Dir 1/Dir 1.1/Dir 1.1.1/File 1.1.1.1"));
    CPPUNIT_ASSERT(confTest->correspondingNode() ==
                   _syncPal->updateTree(ReplicaSide::Remote)->getNodeByPath("Dir 1/Dir 1.1/Dir 1.1.1/File 1.1.1.1"));
    CPPUNIT_ASSERT(confTest->type() == ConflictType::EditEdit);
}

void TestConflictFinderWorker::testMoveCreate() {
    setUpTreesAndDb();
    _syncPal->updateTree(ReplicaSide::Local)->getNodeByPath("Dir 3/Dir 3.1")->setChangeEvents(OperationType::Create);
    _syncPal->updateTree(ReplicaSide::Remote)->getNodeByPath("Dir 3/Dir 3.1")->setChangeEvents(OperationType::Move);
    std::optional<Conflict> confTest = _syncPal->_conflictFinderWorker->checkMoveCreateConflict(
            _syncPal->updateTree(ReplicaSide::Remote)->getNodeByPath("Dir 3/Dir 3.1"));
    CPPUNIT_ASSERT(confTest);
    CPPUNIT_ASSERT(confTest->node() == _syncPal->updateTree(ReplicaSide::Remote)->getNodeByPath("Dir 3/Dir 3.1"));
    CPPUNIT_ASSERT(confTest->correspondingNode() == _syncPal->updateTree(ReplicaSide::Local)->getNodeByPath("Dir 3/Dir 3.1"));
    CPPUNIT_ASSERT(confTest->type() == ConflictType::MoveCreate);
}

void TestConflictFinderWorker::testEditDelete() {
    setUpTreesAndDb();
    _syncPal->updateTree(ReplicaSide::Local)
            ->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1")
            ->setChangeEvents(OperationType::Edit);
    _syncPal->updateTree(ReplicaSide::Remote)
            ->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1")
            ->setChangeEvents(OperationType::Delete);
    std::optional<Conflict> confTest = _syncPal->_conflictFinderWorker->checkEditDeleteConflict(
            _syncPal->updateTree(ReplicaSide::Remote)->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1"));
    CPPUNIT_ASSERT(confTest);
    CPPUNIT_ASSERT(confTest->node() ==
                   _syncPal->updateTree(ReplicaSide::Remote)->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1"));
    CPPUNIT_ASSERT(confTest->correspondingNode() ==
                   _syncPal->updateTree(ReplicaSide::Local)->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1"));
    CPPUNIT_ASSERT(confTest->type() == ConflictType::EditDelete);
}

void TestConflictFinderWorker::testMoveDeleteFile() {
    setUpTreesAndDb();
    _syncPal->updateTree(ReplicaSide::Local)
            ->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1")
            ->setChangeEvents(OperationType::Move);
    _syncPal->updateTree(ReplicaSide::Remote)
            ->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1")
            ->setChangeEvents(OperationType::Delete);
    std::optional<Conflict> confTest = _syncPal->_conflictFinderWorker->checkMoveDeleteConflict(
            _syncPal->updateTree(ReplicaSide::Remote)->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1"));
    CPPUNIT_ASSERT(confTest);
    CPPUNIT_ASSERT(confTest->node() ==
                   _syncPal->updateTree(ReplicaSide::Remote)->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1"));
    CPPUNIT_ASSERT(confTest->correspondingNode() ==
                   _syncPal->updateTree(ReplicaSide::Local)->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1"));
    CPPUNIT_ASSERT(confTest->type() == ConflictType::MoveDelete);
}

void TestConflictFinderWorker::testMoveDeleteDir() {
    // TODO : to be implemented but this class needs to be refactored first.
    // We shouldn't have to setup a SyncPal completely to test one worker.
}

void TestConflictFinderWorker::testMoveParentDelete() {
    setUpTreesAndDb();
    _syncPal->updateTree(ReplicaSide::Local)->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1")->setChangeEvents(OperationType::Delete);
    _syncPal->updateTree(ReplicaSide::Remote)
            ->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1")
            ->setChangeEvents(OperationType::Move);
    std::optional<std::vector<Conflict>> confTest = _syncPal->_conflictFinderWorker->checkMoveParentDeleteConflicts(
            _syncPal->updateTree(ReplicaSide::Local)->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1"));
    CPPUNIT_ASSERT(confTest);
    CPPUNIT_ASSERT(confTest->back().node() == _syncPal->updateTree(ReplicaSide::Local)->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1"));
    CPPUNIT_ASSERT(confTest->back().correspondingNode() ==
                   _syncPal->updateTree(ReplicaSide::Remote)->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1"));
    CPPUNIT_ASSERT(confTest->back().type() == ConflictType::MoveParentDelete);
}

void TestConflictFinderWorker::testCreateParentDelete() {
    setUpTreesAndDb();
    _syncPal->updateTree(ReplicaSide::Local)->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1")->setChangeEvents(OperationType::Delete);
    _syncPal->updateTree(ReplicaSide::Remote)
            ->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1")
            ->setChangeEvents(OperationType::Create);
    std::optional<std::vector<Conflict>> confTest = _syncPal->_conflictFinderWorker->checkCreateParentDeleteConflicts(
            _syncPal->updateTree(ReplicaSide::Local)->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1"));
    CPPUNIT_ASSERT(confTest);
    CPPUNIT_ASSERT(confTest->back().node() == _syncPal->updateTree(ReplicaSide::Local)->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1"));
    CPPUNIT_ASSERT(confTest->back().correspondingNode() ==
                   _syncPal->updateTree(ReplicaSide::Remote)->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1"));
    CPPUNIT_ASSERT(confTest->back().type() == ConflictType::CreateParentDelete);
}

void TestConflictFinderWorker::testMoveMoveSrc() {
    setUpTreesAndDb();
    const auto localDir2 = _syncPal->updateTree(ReplicaSide::Local)->getNodeByPath("Dir 2");
    const auto remoteDir2 = _syncPal->updateTree(ReplicaSide::Remote)->getNodeByPath("Dir 2");
    localDir2->setChangeEvents(OperationType::Move);
    remoteDir2->setChangeEvents(OperationType::Move);
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->getNodeByPath("Dir 1")->insertChildren(localDir2));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->getNodeByPath("Dir 3")->insertChildren(remoteDir2));
    CPPUNIT_ASSERT(localDir2->setParentNode(_syncPal->updateTree(ReplicaSide::Local)->getNodeByPath("Dir 1")));
    CPPUNIT_ASSERT(remoteDir2->setParentNode(_syncPal->updateTree(ReplicaSide::Remote)->getNodeByPath("Dir 3")));
    _syncPal->updateTree(ReplicaSide::Local)->rootNode()->deleteChildren(localDir2);
    _syncPal->updateTree(ReplicaSide::Remote)->rootNode()->deleteChildren(remoteDir2);

    std::optional<Conflict> confTest = _syncPal->_conflictFinderWorker->checkMoveMoveSourceConflict(localDir2);
    CPPUNIT_ASSERT(confTest);
    CPPUNIT_ASSERT(confTest->node() == localDir2);
    CPPUNIT_ASSERT(confTest->correspondingNode() == remoteDir2);
    CPPUNIT_ASSERT(confTest->type() == ConflictType::MoveMoveSource);
}

void TestConflictFinderWorker::testMoveMoveDest() {
    setUpTreesAndDb();
    const auto localDir2 = _syncPal->updateTree(ReplicaSide::Local)->getNodeByPath("Dir 2");
    const auto remoteDir1 = _syncPal->updateTree(ReplicaSide::Remote)->getNodeByPath("Dir 1");
    localDir2->setChangeEvents(OperationType::Move);
    remoteDir1->setChangeEvents(OperationType::Move);
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->getNodeByPath("Dir 3")->insertChildren(localDir2));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->getNodeByPath("Dir 3")->insertChildren(remoteDir1));
    CPPUNIT_ASSERT(localDir2->setParentNode(_syncPal->updateTree(ReplicaSide::Local)->getNodeByPath("Dir 3")));
    CPPUNIT_ASSERT(remoteDir1->setParentNode(_syncPal->updateTree(ReplicaSide::Remote)->getNodeByPath("Dir 3")));
    localDir2->setName(Str("Dir 3.2"));
    remoteDir1->setName(Str("Dir 3.2"));
    _syncPal->updateTree(ReplicaSide::Local)->rootNode()->deleteChildren(localDir2);
    _syncPal->updateTree(ReplicaSide::Remote)->rootNode()->deleteChildren(remoteDir1);

    std::optional<Conflict> confTest = _syncPal->_conflictFinderWorker->checkMoveMoveDestConflict(localDir2);
    CPPUNIT_ASSERT(confTest);
    CPPUNIT_ASSERT(confTest->node() == localDir2);
    CPPUNIT_ASSERT(confTest->correspondingNode() == remoteDir1);
    CPPUNIT_ASSERT(confTest->type() == ConflictType::MoveMoveDest);
}

void TestConflictFinderWorker::testMoveMoveCycle() {
    setUpTreesAndDb();
    const auto A = _syncPal->updateTree(ReplicaSide::Local)->getNodeByPath("A");
    const auto B = _syncPal->updateTree(ReplicaSide::Remote)->getNodeByPath("B");

    // A moved in B/A
    A->setChangeEvents(OperationType::Move);
    CPPUNIT_ASSERT(A->setParentNode(_syncPal->updateTree(ReplicaSide::Local)->getNodeByPath("B")));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->getNodeByPath("B")->insertChildren(A));
    _syncPal->updateTree(ReplicaSide::Local)->rootNode()->deleteChildren(A);
    // B moved in A/B
    B->setChangeEvents(OperationType::Move);
    CPPUNIT_ASSERT(B->setParentNode(_syncPal->updateTree(ReplicaSide::Remote)->getNodeByPath("A")));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->getNodeByPath("A")->insertChildren(B));
    _syncPal->updateTree(ReplicaSide::Remote)->rootNode()->deleteChildren(B);

    std::optional<std::vector<Conflict>> confTestList = _syncPal->_conflictFinderWorker->determineMoveMoveCycleConflicts(
            {A, _syncPal->updateTree(ReplicaSide::Local)->getNodeByPath("Dir 1")},
            {B, _syncPal->updateTree(ReplicaSide::Remote)->getNodeByPath("Dir 1")});
    CPPUNIT_ASSERT(confTestList);
    CPPUNIT_ASSERT(confTestList->size() == 1);
    CPPUNIT_ASSERT(confTestList->back().node() == A);
    CPPUNIT_ASSERT(confTestList->back().correspondingNode() == B);
    CPPUNIT_ASSERT(confTestList->back().type() == ConflictType::MoveMoveCycle);
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
    DbNode nodeDbA(0, _syncPal->syncDb()->rootNode().nodeId(), Str("A"), Str("A"), "A", "rA", tLoc, tLoc, tDrive,
                   NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDbA, dbNodeIdFileA, constraintError);
    _syncPal->_localSnapshot->updateItem(SnapshotItem("A", _syncPal->syncDb()->rootNode().nodeIdLocal().value(), Str("A"), 222,
                                                      222, NodeType::File, 123, false, true, true));
    _syncPal->_remoteSnapshot->updateItem(SnapshotItem("rA", _syncPal->syncDb()->rootNode().nodeIdRemote().value(), Str("A"), 222,
                                                       222, NodeType::File, 123, false, true, true));

    // Start situation
    const auto nodeA =
            std::make_shared<Node>(dbNodeIdFileA, ReplicaSide::Local, Str("A"), NodeType::File, OperationType::None, "A",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Local)->rootNode());
    const auto rNodeA =
            std::make_shared<Node>(dbNodeIdFileA, ReplicaSide::Remote, Str("A"), NodeType::File, OperationType::None, "rA",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->insertChildren(nodeA));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeA);
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNodeA));
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeA);

    // Conflict Situation
    nodeA->setChangeEvents(OperationType::Edit);
    nodeA->setSize(size + 1);
    nodeA->setMoveOriginInfos({rNodeA->getPath(), _syncPal->updateTree(ReplicaSide::Local)->rootNode()->id().value()});
    nodeA->setName(Str("B"));
    nodeA->insertChangeEvent(OperationType::Move);

    rNodeA->setChangeEvents(OperationType::Edit);
    rNodeA->setSize(size - 1);

    const auto rNodeB =
            std::make_shared<Node>(std::nullopt, ReplicaSide::Remote, Str("B"), NodeType::File, OperationType::Create, "rB",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNodeB));
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeB);

    _syncPal->_conflictFinderWorker->findConflicts();
    CPPUNIT_ASSERT_EQUAL(size_t(2), _syncPal->_conflictQueue->size());
    auto conflict = _syncPal->_conflictQueue->top();
    _syncPal->_conflictQueue->pop();
    auto conflict1 = _syncPal->_conflictQueue->top();

    CPPUNIT_ASSERT(conflict.type() == ConflictType::MoveCreate);
    CPPUNIT_ASSERT(conflict.node() == nodeA);
    CPPUNIT_ASSERT(conflict.correspondingNode() == rNodeB);
    CPPUNIT_ASSERT(conflict1.type() == ConflictType::EditEdit);
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
    DbNode nodeDbA(0, _syncPal->syncDb()->rootNode().nodeId(), Str("A"), Str("A"), "A", "rA", tLoc, tLoc, tDrive,
                   NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDbA, dbNodeIdFileA, constraintError);

    // Start situation
    const auto nodeA =
            std::make_shared<Node>(dbNodeIdFileA, ReplicaSide::Local, Str("A"), NodeType::File, OperationType::None, "A",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Local)->rootNode());
    const auto rNodeA =
            std::make_shared<Node>(dbNodeIdFileA, ReplicaSide::Remote, Str("A"), NodeType::File, OperationType::None, "rA",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->insertChildren(nodeA));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeA);
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNodeA));
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeA);

    // Conflict Situation
    nodeA->setMoveOriginInfos({"A", nodeA->parentNode()->id().value()});
    nodeA->setChangeEvents(OperationType::Move);
    nodeA->setName(Str("B"));

    rNodeA->setMoveOriginInfos({"A", rNodeA->parentNode()->id().value()});
    rNodeA->setChangeEvents(OperationType::Move);
    rNodeA->setName(Str("C"));

    const auto rNodeB =
            std::make_shared<Node>(std::nullopt, ReplicaSide::Remote, Str("B"), NodeType::File, OperationType::Create, "rB",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeB);
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNodeB));

    const auto nodeC =
            std::make_shared<Node>(std::nullopt, ReplicaSide::Local, Str("C"), NodeType::File, OperationType::Create, "C",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Local)->rootNode());
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeC);
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->insertChildren(nodeC));

    _syncPal->_conflictFinderWorker->findConflicts();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->size() == 3);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictType::MoveMoveSource);
    _syncPal->_conflictQueue->pop();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictType::MoveCreate);
    _syncPal->_conflictQueue->pop();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictType::MoveCreate);
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
    DbNode nodeDbA(0, _syncPal->syncDb()->rootNode().nodeId(), Str("A"), Str("A"), "A", "rA", tLoc, tLoc, tDrive,
                   NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDbA, dbNodeIdDirA, constraintError);
    DbNode nodeDbB(0, _syncPal->syncDb()->rootNode().nodeId(), Str("B"), Str("B"), "B", "rB", tLoc, tLoc, tDrive,
                   NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDbB, dbNodeIdDirB, constraintError);
    DbNode nodeDbC(0, dbNodeIdDirA, Str("c"), Str("c"), "c", "rc", tLoc, tLoc, tDrive, NodeType::File, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDbC, dbNodeIdFileC, constraintError);

    // Start situation
    const auto nodeA =
            std::make_shared<Node>(dbNodeIdDirA, ReplicaSide::Local, Str("A"), NodeType::Directory, OperationType::None, "A",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Local)->rootNode());
    const auto nodeB =
            std::make_shared<Node>(dbNodeIdDirB, ReplicaSide::Local, Str("B"), NodeType::Directory, OperationType::None, "B",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Local)->rootNode());
    const auto nodeC = std::make_shared<Node>(dbNodeIdFileC, ReplicaSide::Local, Str("c"), NodeType::File, OperationType::None,
                                              "c", createdAt, lastmodified, size, nodeA);
    const auto rNodeA =
            std::make_shared<Node>(dbNodeIdDirA, ReplicaSide::Remote, Str("A"), NodeType::Directory, OperationType::None, "rA",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());
    const auto rNodeB =
            std::make_shared<Node>(dbNodeIdDirB, ReplicaSide::Remote, Str("B"), NodeType::Directory, OperationType::None, "rB",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());
    const auto rNodeC = std::make_shared<Node>(dbNodeIdFileC, ReplicaSide::Remote, Str("c"), NodeType::File, OperationType::None,
                                               "rc", createdAt, lastmodified, size, rNodeA);
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->insertChildren(nodeA));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->insertChildren(nodeB));
    CPPUNIT_ASSERT(nodeA->insertChildren(nodeC));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeA);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeB);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeC);
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNodeA));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNodeB));
    CPPUNIT_ASSERT(rNodeA->insertChildren(rNodeC));
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeA);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeB);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeC);

    // Conflict Situation
    CPPUNIT_ASSERT(nodeC->setParentNode(nodeB));
    nodeC->insertChangeEvent(OperationType::Move);
    CPPUNIT_ASSERT(nodeB->insertChildren(nodeC));
    CPPUNIT_ASSERT(nodeA->deleteChildren(nodeC));
    rNodeC->insertChangeEvent(OperationType::Move);
    CPPUNIT_ASSERT(rNodeC->setParentNode(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNodeC));
    rNodeB->insertChangeEvent(OperationType::Delete);
    CPPUNIT_ASSERT(rNodeA->deleteChildren(rNodeC));

    _syncPal->_conflictFinderWorker->findConflicts();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->size() == 2);
    auto conf1 = _syncPal->_conflictQueue->top();
    CPPUNIT_ASSERT(conf1.type() == ConflictType::MoveParentDelete);
    CPPUNIT_ASSERT(conf1.node() == rNodeB);
    CPPUNIT_ASSERT(conf1.node()->parentNode() == _syncPal->updateTree(ReplicaSide::Remote)->rootNode());
    CPPUNIT_ASSERT(conf1.correspondingNode() == nodeC);
    CPPUNIT_ASSERT(conf1.correspondingNode()->parentNode() == nodeB);
    _syncPal->_conflictQueue->pop();
    auto conf2 = _syncPal->_conflictQueue->top();
    CPPUNIT_ASSERT(conf2.type() == ConflictType::MoveMoveSource);
    CPPUNIT_ASSERT(conf2.node() == rNodeC);
    CPPUNIT_ASSERT(conf2.node()->parentNode() == _syncPal->updateTree(ReplicaSide::Remote)->rootNode());
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
    DbNode nodeDbA(0, _syncPal->syncDb()->rootNode().nodeId(), Str("A"), Str("A"), "A", "rA", tLoc, tLoc, tDrive,
                   NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDbA, dbNodeIdDirA, constraintError);
    DbNode nodeDbB(0, _syncPal->syncDb()->rootNode().nodeId(), Str("B"), Str("B"), "B", "rB", tLoc, tLoc, tDrive,
                   NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDbB, dbNodeIdDirB, constraintError);

    // Start situation
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;
    const auto nodeA =
            std::make_shared<Node>(dbNodeIdDirA, ReplicaSide::Local, Str("A"), NodeType::Directory, OperationType::None, "A",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Local)->rootNode());
    const auto nodeB =
            std::make_shared<Node>(dbNodeIdDirB, ReplicaSide::Local, Str("B"), NodeType::Directory, OperationType::None, "B",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Local)->rootNode());
    const auto rNodeA =
            std::make_shared<Node>(dbNodeIdDirA, ReplicaSide::Remote, Str("A"), NodeType::Directory, OperationType::None, "rA",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());
    const auto rNodeB =
            std::make_shared<Node>(dbNodeIdDirB, ReplicaSide::Remote, Str("B"), NodeType::Directory, OperationType::None, "rB",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->insertChildren(nodeA));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->insertChildren(nodeB));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeA);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeB);
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNodeA));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNodeB));
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeA);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeB);

    // Conflict situation
    nodeB->setName(Str("x"));
    nodeB->insertChangeEvent(OperationType::Move);
    rNodeA->setName(Str("x"));
    rNodeA->insertChangeEvent(OperationType::Move);
    rNodeB->insertChangeEvent(OperationType::Delete);

    _syncPal->_conflictFinderWorker->findConflicts();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->size() == 2);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictType::MoveDelete);
    _syncPal->_conflictQueue->pop();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictType::MoveMoveDest);
}
/* Move-Delete | Create-ParentDelete */
void TestConflictFinderWorker::testCase510() {
    // cf p99 figure 5.10
    time_t tLoc = std::time(0);
    time_t tDrive = std::time(0);
    DbNodeId dbNodeIdDirA;
    DbNodeId dbNodeIdFileX;

    bool constraintError = false;
    DbNode nodeDbA(0, _syncPal->syncDb()->rootNode().nodeId(), Str("A"), Str("A"), "A", "rA", tLoc, tLoc, tDrive,
                   NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDbA, dbNodeIdDirA, constraintError);
    DbNode nodeDbX(0, _syncPal->syncDb()->rootNode().nodeId(), Str("x"), Str("x"), "X", "rX", tLoc, tLoc, tDrive, NodeType::File,
                   0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDbX, dbNodeIdFileX, constraintError);

    // Start situation
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;
    const auto nodeA =
            std::make_shared<Node>(dbNodeIdDirA, ReplicaSide::Local, Str("A"), NodeType::Directory, OperationType::None, "A",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Local)->rootNode());
    const auto nodeX = std::make_shared<Node>(dbNodeIdFileX, ReplicaSide::Local, Str("x"), NodeType::File, OperationType::None,
                                              "X", createdAt, lastmodified, size, nodeA);
    const auto rNodeA =
            std::make_shared<Node>(dbNodeIdDirA, ReplicaSide::Remote, Str("A"), NodeType::Directory, OperationType::None, "rA",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());
    const auto rNodeX = std::make_shared<Node>(dbNodeIdFileX, ReplicaSide::Remote, Str("x"), NodeType::File, OperationType::None,
                                               "rX", createdAt, lastmodified, size, rNodeA);

    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->insertChildren(nodeA));
    CPPUNIT_ASSERT(nodeA->insertChildren(nodeX));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeA);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeX);
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNodeA));
    CPPUNIT_ASSERT(rNodeA->insertChildren(rNodeX));
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeA);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeX);

    // Conflict situation
    nodeX->insertChangeEvent(OperationType::Move);
    CPPUNIT_ASSERT(nodeX->setParentNode(_syncPal->updateTree(ReplicaSide::Local)->rootNode()));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->insertChildren(nodeX));
    CPPUNIT_ASSERT(nodeA->deleteChildren(nodeX));
    const auto nodeX2 = std::make_shared<Node>(std::nullopt, ReplicaSide::Local, Str("x"), NodeType::File, OperationType::Create,
                                               "X2", createdAt, lastmodified, size, nodeA);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeX2);
    CPPUNIT_ASSERT(nodeA->insertChildren(nodeX2));
    rNodeA->insertChangeEvent(OperationType::Delete);
    rNodeX->insertChangeEvent(OperationType::Delete);

    _syncPal->_conflictFinderWorker->findConflicts();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->size() == 2);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictType::MoveDelete);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().node() == rNodeX);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().correspondingNode() == nodeX);
    _syncPal->_conflictQueue->pop();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictType::CreateParentDelete);
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
    DbNode nodeDbA(0, _syncPal->syncDb()->rootNode().nodeId(), Str("A"), Str("A"), "A", "rA", tLoc, tLoc, tDrive,
                   NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDbA, dbNodeIdDirA, constraintError);
    DbNode nodeDbB(0, _syncPal->syncDb()->rootNode().nodeId(), Str("B"), Str("B"), "B", "rB", tLoc, tLoc, tDrive,
                   NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDbB, dbNodeIdDirB, constraintError);

    // Start situation
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;
    const auto nodeA =
            std::make_shared<Node>(dbNodeIdDirA, ReplicaSide::Local, Str("A"), NodeType::Directory, OperationType::None, "A",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Local)->rootNode());
    const auto nodeB = std::make_shared<Node>(dbNodeIdDirB, ReplicaSide::Local, Str("B"), NodeType::Directory,
                                              OperationType::None, "B", createdAt, lastmodified, size, nodeA);
    const auto rNodeA =
            std::make_shared<Node>(dbNodeIdDirA, ReplicaSide::Remote, Str("A"), NodeType::Directory, OperationType::None, "rA",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());
    const auto rNodeB = std::make_shared<Node>(dbNodeIdDirB, ReplicaSide::Remote, Str("B"), NodeType::Directory,
                                               OperationType::None, "rB", createdAt, lastmodified, size, rNodeA);

    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->insertChildren(nodeA));
    CPPUNIT_ASSERT(nodeA->insertChildren(nodeB));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeA);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeB);
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNodeA));
    CPPUNIT_ASSERT(rNodeA->insertChildren(rNodeB));
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeA);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeB);

    // Conflict situation
    nodeB->insertChangeEvent(OperationType::Delete);
    nodeA->insertChangeEvent(OperationType::Delete);
    CPPUNIT_ASSERT(rNodeA->deleteChildren(rNodeB));
    CPPUNIT_ASSERT(rNodeB->setParentNode(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNodeB));
    rNodeB->setName(Str("B_moved"));
    rNodeB->insertChangeEvent(OperationType::Move);
    const auto nodeNewFile = std::make_shared<Node>(std::nullopt, ReplicaSide::Remote, Str("new.txt"), NodeType::File,
                                                    OperationType::Create, "new", createdAt, lastmodified, size, rNodeB);
    CPPUNIT_ASSERT(rNodeB->insertChildren(nodeNewFile));
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(nodeNewFile);

    _syncPal->_conflictFinderWorker->findConflicts();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->size() == 2);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictType::MoveDelete);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().node() == nodeB);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().correspondingNode() == rNodeB);
    _syncPal->_conflictQueue->pop();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictType::CreateParentDelete);
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
    DbNode nodeDbQ(0, _syncPal->syncDb()->rootNode().nodeId(), Str("q"), Str("q"), "q", "rq", tLoc, tLoc, tDrive,
                   NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDbQ, dbNodeIdDirQ, constraintError);
    DbNode nodeDbR(0, _syncPal->syncDb()->rootNode().nodeId(), Str("r"), Str("r"), "r", "rr", tLoc, tLoc, tDrive,
                   NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDbR, dbNodeIdDirR, constraintError);
    DbNode nodeDbN(0, dbNodeIdDirQ, Str("n"), Str("n"), "n", "rn", tLoc, tLoc, tDrive, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDbN, dbNodeIdDirN, constraintError);

    // Start situation
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;
    const auto nodeQ =
            std::make_shared<Node>(dbNodeIdDirQ, ReplicaSide::Local, Str("q"), NodeType::Directory, OperationType::None, "q",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Local)->rootNode());
    const auto nodeR =
            std::make_shared<Node>(dbNodeIdDirR, ReplicaSide::Local, Str("r"), NodeType::Directory, OperationType::None, "r",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Local)->rootNode());
    const auto nodeN = std::make_shared<Node>(dbNodeIdDirN, ReplicaSide::Local, Str("n"), NodeType::Directory,
                                              OperationType::None, "n", createdAt, lastmodified, size, nodeQ);
    const auto rNodeQ =
            std::make_shared<Node>(dbNodeIdDirQ, ReplicaSide::Remote, Str("q"), NodeType::Directory, OperationType::None, "rq",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());
    const auto rNodeR =
            std::make_shared<Node>(dbNodeIdDirR, ReplicaSide::Remote, Str("r"), NodeType::Directory, OperationType::None, "rr",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());
    const auto rNodeN = std::make_shared<Node>(dbNodeIdDirN, ReplicaSide::Remote, Str("n"), NodeType::Directory,
                                               OperationType::None, "rn", createdAt, lastmodified, size, rNodeQ);

    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->insertChildren(nodeQ));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->insertChildren(nodeR));
    CPPUNIT_ASSERT(nodeQ->insertChildren(nodeN));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeQ);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeR);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeN);
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNodeQ));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNodeR));
    CPPUNIT_ASSERT(rNodeQ->insertChildren(rNodeN));
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeQ);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeR);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeN);

    // Conflict situation
    nodeR->insertChangeEvent(OperationType::Move);
    CPPUNIT_ASSERT(nodeR->setParentNode(nodeN));
    CPPUNIT_ASSERT(nodeN->insertChildren(nodeR));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->deleteChildren(nodeR));
    rNodeQ->insertChangeEvent(OperationType::Move);
    CPPUNIT_ASSERT(rNodeQ->setParentNode(rNodeR));
    CPPUNIT_ASSERT(rNodeR->insertChildren(rNodeQ));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->deleteChildren(rNodeQ));
    rNodeN->insertChangeEvent(OperationType::Move);
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNodeN));
    // update the node's name in the parent's children map
    CPPUNIT_ASSERT(rNodeN->setParentNode(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()));
    CPPUNIT_ASSERT(rNodeQ->deleteChildren(rNodeN));
    rNodeN->setName(Str("n_moved2"));

    _syncPal->_conflictFinderWorker->findConflicts();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->size() == 1);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictType::MoveMoveCycle);
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
    DbNode nodeDbQ(0, _syncPal->syncDb()->rootNode().nodeId(), Str("q"), Str("q"), "q", "rq", tLoc, tLoc, tDrive,
                   NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDbQ, dbNodeIdDirQ, constraintError);
    DbNode nodeDbR(0, _syncPal->syncDb()->rootNode().nodeId(), Str("r"), Str("r"), "r", "rr", tLoc, tLoc, tDrive,
                   NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDbR, dbNodeIdDirR, constraintError);
    DbNode nodeDbN(0, dbNodeIdDirR, Str("n"), Str("n"), "n", "rn", tLoc, tLoc, tDrive, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDbN, dbNodeIdDirN, constraintError);
    DbNode nodeDbM(0, dbNodeIdDirQ, Str("m"), Str("m"), "m", "rm", tLoc, tLoc, tDrive, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(nodeDbM, dbNodeIdDirM, constraintError);

    // Start situation
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;
    const auto nodeQ =
            std::make_shared<Node>(dbNodeIdDirQ, ReplicaSide::Local, Str("q"), NodeType::Directory, OperationType::None, "q",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Local)->rootNode());

    const auto nodeM = std::make_shared<Node>(dbNodeIdDirM, ReplicaSide::Local, Str("m"), NodeType::Directory,
                                              OperationType::None, "m", createdAt, lastmodified, size, nodeQ);
    const auto nodeR =
            std::make_shared<Node>(dbNodeIdDirR, ReplicaSide::Local, Str("r"), NodeType::Directory, OperationType::None, "r",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Local)->rootNode());
    const auto nodeN = std::make_shared<Node>(dbNodeIdDirN, ReplicaSide::Local, Str("n"), NodeType::Directory,
                                              OperationType::None, "n", createdAt, lastmodified, size, nodeR);
    const auto rNodeQ =
            std::make_shared<Node>(dbNodeIdDirQ, ReplicaSide::Remote, Str("q"), NodeType::Directory, OperationType::None, "rq",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());
    const auto rNodeM = std::make_shared<Node>(dbNodeIdDirM, ReplicaSide::Remote, Str("m"), NodeType::Directory,
                                               OperationType::None, "rm", createdAt, lastmodified, size, rNodeQ);
    const auto rNodeR =
            std::make_shared<Node>(dbNodeIdDirR, ReplicaSide::Remote, Str("r"), NodeType::Directory, OperationType::None, "rr",
                                   createdAt, lastmodified, size, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());
    const auto rNodeN = std::make_shared<Node>(dbNodeIdDirN, ReplicaSide::Remote, Str("n"), NodeType::Directory,
                                               OperationType::None, "rn", createdAt, lastmodified, size, rNodeR);

    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->insertChildren(nodeQ));
    CPPUNIT_ASSERT(nodeQ->insertChildren(nodeM));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->insertChildren(nodeR));
    CPPUNIT_ASSERT(nodeR->insertChildren(nodeN));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeQ);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeR);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeN);
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(nodeM);
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNodeQ));
    CPPUNIT_ASSERT(rNodeQ->insertChildren(rNodeM));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNodeR));
    CPPUNIT_ASSERT(rNodeR->insertChildren(rNodeN));
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeQ);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeR);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeN);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeM);

    // Conflict situation
    CPPUNIT_ASSERT(nodeR->deleteChildren(nodeN));
    CPPUNIT_ASSERT(nodeQ->deleteChildren(nodeM));
    nodeM->insertChangeEvent(OperationType::Move);
    CPPUNIT_ASSERT(nodeM->setParentNode(_syncPal->updateTree(ReplicaSide::Local)->rootNode()));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->insertChildren(nodeM));
    CPPUNIT_ASSERT(nodeM->insertChildren(nodeN));
    nodeN->insertChangeEvent(OperationType::Move);
    CPPUNIT_ASSERT(nodeN->setParentNode(nodeM));
    CPPUNIT_ASSERT(nodeN->insertChildren(nodeQ));
    nodeQ->insertChangeEvent(OperationType::Move);
    CPPUNIT_ASSERT(nodeQ->setParentNode(nodeN));

    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->deleteChildren(rNodeR));
    rNodeR->insertChangeEvent(OperationType::Move);
    CPPUNIT_ASSERT(rNodeR->setParentNode(rNodeQ));
    CPPUNIT_ASSERT(rNodeQ->insertChildren(rNodeR));
    CPPUNIT_ASSERT(rNodeR->deleteChildren(rNodeN));
    rNodeN->insertChangeEvent(OperationType::Move);
    CPPUNIT_ASSERT(rNodeN->setParentNode(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()));
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNodeN));
    CPPUNIT_ASSERT(rNodeQ->deleteChildren(rNodeM));
    rNodeM->insertChangeEvent(OperationType::Move);
    CPPUNIT_ASSERT(rNodeM->setParentNode(rNodeN));
    CPPUNIT_ASSERT(rNodeN->insertChildren(rNodeM));

    _syncPal->_conflictFinderWorker->findConflicts();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->size() == 3);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictType::MoveMoveSource);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().node() == nodeM);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().node()->side() == nodeM->side());
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().correspondingNode() == rNodeM);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().correspondingNode()->side() == rNodeM->side());
    _syncPal->_conflictQueue->pop();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictType::MoveMoveSource);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().node() == rNodeN);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().node()->side() == rNodeN->side());
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().correspondingNode() == nodeN);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().correspondingNode()->side() == nodeN->side());
    _syncPal->_conflictQueue->pop();
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().type() == ConflictType::MoveMoveCycle);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().node() == nodeN);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().node()->side() == nodeN->side());
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().correspondingNode() == rNodeM);
    CPPUNIT_ASSERT(_syncPal->_conflictQueue->top().correspondingNode()->side() == rNodeM->side());
}

void TestConflictFinderWorker::testConflictCmp() {
    const ConflictCmp cmp(_syncPal->updateTree(ReplicaSide::Local), _syncPal->updateTree(ReplicaSide::Remote));
    const std::array<ConflictType, 10> conflictTypes = {ConflictType::MoveParentDelete, ConflictType::CreateParentDelete,
                                                        ConflictType::MoveDelete,       ConflictType::EditDelete,
                                                        ConflictType::MoveMoveSource,   ConflictType::MoveMoveDest,
                                                        ConflictType::MoveMoveCycle,    ConflictType::CreateCreate,
                                                        ConflictType::EditEdit,         ConflictType::MoveCreate};
    const size_t nbConflictType = conflictTypes.size();

    const auto localNodeA = std::make_shared<Node>(0, ReplicaSide::Local, Str("A"), NodeType::Directory, OperationType::None, "A",
                                                   0, 0, 0, _syncPal->updateTree(ReplicaSide::Local)->rootNode());

    const auto remoteNodeA = std::make_shared<Node>(0, ReplicaSide::Remote, Str("A"), NodeType::Directory, OperationType::None,
                                                    "A", 0, 0, 0, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());

    OperationType allOp = OperationType::Create | OperationType::Edit | OperationType::Delete | OperationType::Move;
    const auto localNodeAA =
            std::make_shared<Node>(0, ReplicaSide::Local, Str("AA"), NodeType::Directory, allOp, "AA", 0, 0, 0, localNodeA);
    localNodeAA->setMoveOriginInfos({"AA1", localNodeA->parentNode()->id().value()});

    const auto remoteNodeAA =
            std::make_shared<Node>(0, ReplicaSide::Remote, Str("AA"), NodeType::Directory, allOp, "AA", 0, 0, 0, remoteNodeA);
    remoteNodeAA->setMoveOriginInfos({"AA2", remoteNodeA->parentNode()->id().value()});


    ConflictQueue queue(_syncPal->updateTree(ReplicaSide::Local), _syncPal->updateTree(ReplicaSide::Remote));
    for (size_t i = 0; i < 1000; i++) {
        size_t index = (size_t) rand() % nbConflictType;
        Conflict c1(localNodeAA, remoteNodeAA, conflictTypes[index]);
        queue.push(c1);
    }

    // Ensure that all the operations are grouped by type
    ConflictType lastType = ConflictType::None;
    while (!queue.empty()) {
        const ConflictType currentType = queue.top().type();
        CPPUNIT_ASSERT(currentType >= lastType);
        lastType = currentType;
        queue.pop();
    }
}
} // namespace KDC
