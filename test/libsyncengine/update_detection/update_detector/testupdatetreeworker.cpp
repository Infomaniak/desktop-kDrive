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

#include "testupdatetreeworker.h"

#include <requests/parameterscache.h>

using namespace CppUnit;

namespace KDC {

void TestUpdateTreeWorker::setUp() {
    // Create parmsDb
    bool alreadyExists = false;
    std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExists, true);
    ParmsDb::instance(parmsDbPath, "3.4.0", true, true);
    ParmsDb::instance()->setAutoDelete(true);
    ParametersCache::instance()->parameters().setExtendedLog(true);

    // Create DB
    std::filesystem::path syncDbPath = Db::makeDbName(1, 1, 1, 1, alreadyExists, true);
    _syncDb = std::shared_ptr<SyncDb>(new SyncDb(syncDbPath.string(), "3.6.1"));
    _syncDb->setAutoDelete(true);
    _operationSet = std::shared_ptr<FSOperationSet>(new FSOperationSet(ReplicaSide::Unknown));

    _updateTree = std::shared_ptr<UpdateTree>(new UpdateTree(ReplicaSide::Local, SyncDb::driveRootNode()));

    _updateTreeWorker = std::shared_ptr<UpdateTreeWorker>(
        new UpdateTreeWorker(_syncDb, _operationSet, _updateTree, "Test Tree Updater", "LTRU", ReplicaSide::Local));

    setUpDbTree();
    _updateTree->init();
}

void TestUpdateTreeWorker::tearDown() {
    ParmsDb::instance()->close();
    // The singleton ParmsDb calls KDC::Log()->instance() in its destructor.
    // As the two singletons are instantiated in different translation units, the order of their destructions is unknown.
    ParmsDb::reset();
}

void TestUpdateTreeWorker::setUpDbTree() {
    // Init DB tree
    /**
        Root
        ├── 1
        │   └── 1.1
        │       └── 1.1.1
        │           └── 1.1.1.1
        ├── 2
        ├── 3
        │   └── 3.1
        ├── 4
        │   └── 4.1
        │       └── 4.1.1
        │           ├── 4.1.1.1
        │           └── 4.1.1.2
        ├── 5
        │   └── 5.1
        ├── 6
        └── 6a
     */

    time_t tLoc = std::time(0);
    time_t tDrive = std::time(0);
    DbNodeId dbNodeIdDir1;
    DbNodeId dbNodeIdDir11;
    DbNodeId dbNodeIdDir111;
    DbNodeId dbNodeId112;
    DbNodeId dbNodeIdFile1111;
    DbNodeId dbNodeIdDir2;
    DbNodeId dbNodeIdDir3;
    DbNodeId dbNodeIdDir31;
    DbNodeId dbnodeIdDir4;
    DbNodeId dbnodeIdfile4111;
    DbNodeId dbnodeIdfile4112;
    DbNodeId dbnodeIdDir5;
    DbNodeId dbnodeIdfile51;
    DbNodeId dbnodeIdfile6;
    DbNodeId dbnodeIdfile6a;

    bool constraintError = false;
    DbNode nodeDir1(0, _syncDb->rootNode().nodeId(), Str("Dir 1"), Str("Dir 1"), "id1", "id drive 1", tLoc, tLoc, tDrive,
                    NodeType::Directory, 0, std::nullopt);
    _syncDb->insertNode(nodeDir1, dbNodeIdDir1, constraintError);
    DbNode nodeDir11(0, dbNodeIdDir1, Str("Dir 1.1"), Str("Dir 1.1"), "id11", "id drive 11", tLoc, tLoc, tDrive,
                     NodeType::Directory, 0, std::nullopt);
    _syncDb->insertNode(nodeDir11, dbNodeIdDir11, constraintError);
    DbNode nodeDir111(0, dbNodeIdDir11, Str("Dir 1.1.1"), Str("Dir 1.1.1"), "id111", "id drive 111", tLoc, tLoc, tDrive,
                      NodeType::Directory, 0, std::nullopt);
    _syncDb->insertNode(nodeDir111, dbNodeIdDir111, constraintError);
    DbNode nodeFile112(0, dbNodeIdDir11, Str("File 1.1.2"), Str("File 1.1.2"), "id112", "id drive 112", tLoc, tLoc, tDrive,
                       NodeType::File, 0, "cs 1.1");
    _syncDb->insertNode(nodeFile112, dbNodeId112, constraintError);
    DbNode nodeFile1111(0, dbNodeIdDir111, Str("File 1.1.1.1"), Str("File 1.1.1.1"), "id1111", "id drive 1111", tLoc, tLoc,
                        tDrive, NodeType::File, 0, "cs 1.1");
    _syncDb->insertNode(nodeFile1111, dbNodeIdFile1111, constraintError);
    DbNode nodeDir2(0, _syncDb->rootNode().nodeId(), Str("Dir 2"), Str("Dir 2"), "id2", "id drive 2", tLoc, tLoc, tDrive,
                    NodeType::Directory, 0, std::nullopt);
    _syncDb->insertNode(nodeDir2, dbNodeIdDir2, constraintError);
    DbNode nodeDir3(0, _syncDb->rootNode().nodeId(), Str("Dir 3"), Str("Dir 3"), "id3", "id drive 3", tLoc, tLoc, tDrive,
                    NodeType::Directory, 0, std::nullopt);
    _syncDb->insertNode(nodeDir3, dbNodeIdDir3, constraintError);
    DbNode nodeDir31(0, dbNodeIdDir3, Str("Dir 3.1"), Str("Dir 3.1"), "id31", "id drive 31", tLoc, tLoc, tDrive,
                     NodeType::Directory, 0, std::nullopt);
    _syncDb->insertNode(nodeDir31, dbNodeIdDir31, constraintError);
    DbNode nodeDir4(0, _syncDb->rootNode().nodeId(), Str("Dir 4"), Str("Dir 4"), "id4", "id drive 4", tLoc, tLoc, tDrive,
                    NodeType::Directory, 0, std::nullopt);
    _syncDb->insertNode(nodeDir4, dbnodeIdDir4, constraintError);
    DbNode nodedir41(0, dbnodeIdDir4, Str("Dir 4.1"), Str("Dir 4.1"), "id41", "id drive 41", tLoc, tLoc, tDrive,
                     NodeType::Directory, 0, std::nullopt);
    _syncDb->insertNode(nodedir41, _dbnodeIdDir41, constraintError);
    DbNode nodeDir411(0, _dbnodeIdDir41, Str("Dir 4.1.1"), Str("Dir 4.1.1"), "id411", "id drive 411", tLoc, tLoc, tDrive,
                      NodeType::Directory, 0, std::nullopt);
    _syncDb->insertNode(nodeDir411, _dbnodeIdDir411, constraintError);
    DbNode nodeFile4111(0, _dbnodeIdDir411, Str("File 4.1.1.1"), Str("File 4.1.1.1"), "id4111", "id drive 4111", tLoc, tLoc,
                        tDrive, NodeType::File, 0, std::nullopt);
    _syncDb->insertNode(nodeFile4111, dbnodeIdfile4111, constraintError);
    DbNode nodeFile4112(0, _dbnodeIdDir411, Str("File 4.1.1.2"), Str("File 4.1.1.2"), "id4112", "id drive 4112", tLoc, tLoc,
                        tDrive, NodeType::File, 0, std::nullopt);
    _syncDb->insertNode(nodeFile4112, dbnodeIdfile4112, constraintError);
    DbNode nodeDir5(0, _syncDb->rootNode().nodeId(), Str("Dir 5"), Str("Dir 5"), "id5", "id drive 5", tLoc, tLoc, tDrive,
                    NodeType::Directory, 0, std::nullopt);
    _syncDb->insertNode(nodeDir5, dbnodeIdDir5, constraintError);
    DbNode nodeFile51(0, dbnodeIdDir5, Str("File 5.1"), Str("File 5.1"), "id51", "id drive 51", tLoc, tLoc, tDrive,
                      NodeType::File, 0, std::nullopt);
    _syncDb->insertNode(nodeFile51, dbnodeIdfile51, constraintError);
    DbNode nodeFile6(0, _syncDb->rootNode().nodeId(), Str("File 6"), Str("File 6"), "id6", "id drive 6", tLoc, tLoc, tDrive,
                     NodeType::File, 0, std::nullopt);
    _syncDb->insertNode(nodeFile6, dbnodeIdfile6, constraintError);
    DbNode nodeFile6a(0, _syncDb->rootNode().nodeId(), Str("File 6a"), Str("File 6a"), "id6a", "id drive 6a", tLoc, tLoc, tDrive,
                      NodeType::File, 0, std::nullopt);
    _syncDb->insertNode(nodeFile6a, dbnodeIdfile6a, constraintError);
}

void TestUpdateTreeWorker::setUpUpdateTree() {
    // Init Update Tree
    /**
        Root
        ├── 1
        │   └── 1.1
        │       └── 1.1.1
        │           └── 1.1.1.1
        ├── 2
        ├── 3
        │   └── 3.1
        ├── 4
        │   └── 4.1
        │       └── 4.1.1
        │           └── 4.1.1.1
        ├── 6
        └── 6a
     */
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 12345;

    bool found = false;
    DbNodeId dbNodeIdDir;
    _syncDb->dbId(ReplicaSide::Local, NodeId("id1"), dbNodeIdDir, found);
    std::shared_ptr<Node> node1 =
        std::shared_ptr<Node>(new Node(dbNodeIdDir, _updateTree->side(), Str("Dir 1"), NodeType::Directory, OperationType::None,
                                       "id1", createdAt, lastmodified, size, _updateTree->rootNode()));
    _syncDb->dbId(ReplicaSide::Local, NodeId("id2"), dbNodeIdDir, found);
    std::shared_ptr<Node> node2 =
        std::shared_ptr<Node>(new Node(dbNodeIdDir, _updateTree->side(), Str("Dir 2"), NodeType::Directory, OperationType::None,
                                       "id2", createdAt, lastmodified, size, _updateTree->rootNode()));
    _syncDb->dbId(ReplicaSide::Local, NodeId("id3"), dbNodeIdDir, found);
    std::shared_ptr<Node> node3 =
        std::shared_ptr<Node>(new Node(dbNodeIdDir, _updateTree->side(), Str("Dir 3"), NodeType::Directory, OperationType::None,
                                       "id3", createdAt, lastmodified, size, _updateTree->rootNode()));
    _syncDb->dbId(ReplicaSide::Local, NodeId("id4"), dbNodeIdDir, found);
    std::shared_ptr<Node> node4 =
        std::shared_ptr<Node>(new Node(dbNodeIdDir, _updateTree->side(), Str("Dir 4"), NodeType::Directory, OperationType::None,
                                       "id4", createdAt, lastmodified, size, _updateTree->rootNode()));
    _syncDb->dbId(ReplicaSide::Local, NodeId("id11"), dbNodeIdDir, found);
    std::shared_ptr<Node> node11 =
        std::shared_ptr<Node>(new Node(dbNodeIdDir, _updateTree->side(), Str("Dir 1.1"), NodeType::Directory, OperationType::None,
                                       "id11", createdAt, lastmodified, size, node1));
    _syncDb->dbId(ReplicaSide::Local, NodeId("id111"), dbNodeIdDir, found);
    std::shared_ptr<Node> node111 =
        std::shared_ptr<Node>(new Node(dbNodeIdDir, _updateTree->side(), Str("Dir 1.1.1"), NodeType::Directory,
                                       OperationType::None, "id111", createdAt, lastmodified, size, node11));
    _syncDb->dbId(ReplicaSide::Local, NodeId("id1111"), dbNodeIdDir, found);
    std::shared_ptr<Node> node1111 =
        std::shared_ptr<Node>(new Node(dbNodeIdDir, _updateTree->side(), Str("File 1.1.1.1"), NodeType::File, OperationType::None,
                                       "id1111", createdAt, lastmodified, size, node111));
    _syncDb->dbId(ReplicaSide::Local, NodeId("id31"), dbNodeIdDir, found);
    std::shared_ptr<Node> node31 =
        std::shared_ptr<Node>(new Node(dbNodeIdDir, _updateTree->side(), Str("Dir 3.1"), NodeType::Directory, OperationType::None,
                                       "id31", createdAt, lastmodified, size, node3));
    std::shared_ptr<Node> node41 =
        std::shared_ptr<Node>(new Node(_dbnodeIdDir41, _updateTree->side(), Str("Dir 4.1"), NodeType::Directory,
                                       OperationType::None, "id41", createdAt, lastmodified, size, node4));
    std::shared_ptr<Node> node411 =
        std::shared_ptr<Node>(new Node(_dbnodeIdDir411, _updateTree->side(), Str("Dir 4.1.1"), NodeType::Directory,
                                       OperationType::None, "id411", createdAt, lastmodified, size, node41));
    _syncDb->dbId(ReplicaSide::Local, NodeId("id4111"), dbNodeIdDir, found);
    std::shared_ptr<Node> node4111 =
        std::shared_ptr<Node>(new Node(dbNodeIdDir, _updateTree->side(), Str("File 4.1.1.1"), NodeType::File, OperationType::None,
                                       "id4111", createdAt, lastmodified, size, node411));
    _syncDb->dbId(ReplicaSide::Local, NodeId("id6"), dbNodeIdDir, found);
    std::shared_ptr<Node> node6 =
        std::shared_ptr<Node>(new Node(dbNodeIdDir, _updateTree->side(), Str("File 6"), NodeType::File, OperationType::None,
                                       "id6", createdAt, lastmodified, size, _updateTree->rootNode()));
    _syncDb->dbId(ReplicaSide::Local, NodeId("id6a"), dbNodeIdDir, found);
    std::shared_ptr<Node> node6a =
        std::shared_ptr<Node>(new Node(dbNodeIdDir, _updateTree->side(), Str("File 6a"), NodeType::File, OperationType::None,
                                       "id6a", createdAt, lastmodified, size, _updateTree->rootNode()));

    _updateTree->init();

    CPPUNIT_ASSERT(_updateTree->rootNode()->insertChildren(node1));
    CPPUNIT_ASSERT(_updateTree->rootNode()->insertChildren(node2));
    CPPUNIT_ASSERT(_updateTree->rootNode()->insertChildren(node3));
    CPPUNIT_ASSERT(_updateTree->rootNode()->insertChildren(node4));
    CPPUNIT_ASSERT(_updateTree->rootNode()->insertChildren(node6));
    CPPUNIT_ASSERT(_updateTree->rootNode()->insertChildren(node6a));

    CPPUNIT_ASSERT(node1->insertChildren(node11));
    CPPUNIT_ASSERT(node11->insertChildren(node111));
    CPPUNIT_ASSERT(node111->insertChildren(node1111));
    CPPUNIT_ASSERT(node3->insertChildren(node31));
    CPPUNIT_ASSERT(node4->insertChildren(node41));
    CPPUNIT_ASSERT(node41->insertChildren(node411));
    CPPUNIT_ASSERT(node411->insertChildren(node4111));

    _updateTree->insertNode(node1111);
    _updateTree->insertNode(node111);
    _updateTree->insertNode(node11);
    _updateTree->insertNode(node1);
    _updateTree->insertNode(node2);
    _updateTree->insertNode(node3);
    _updateTree->insertNode(node4);
    _updateTree->insertNode(node31);
    _updateTree->insertNode(node41);
    _updateTree->insertNode(node411);
    _updateTree->insertNode(node4111);
    _updateTree->insertNode(node6);
    _updateTree->insertNode(node6a);
}

void TestUpdateTreeWorker::testUtilsFunctions() {
    setUpUpdateTree();
    // test _updateTree->getNodeByPath
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("")->id() == _syncDb->rootNode().nodeIdLocal());
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 1/Dir 1.1/Dir 1.1.1")->id() == "id111");

    // test getNewPathAfterMove
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id3", NodeType::Directory, 1654788256, 1654788256,
                                                          12345, "Dir 3", "Dir 3bis"));
    _updateTree->getNodeByPath("Dir 3")->setName(Str("Dir 3bis"));

    SyncPath newPath;
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _updateTreeWorker->getNewPathAfterMove("Dir 3/Dir 3.1", newPath));
    CPPUNIT_ASSERT(newPath == "Dir 3bis/Dir 3.1");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath(newPath)->id() == "id31");

    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id41", NodeType::Directory, 1654788256,
                                                          1654788256, 12345, "Dir 4/Dir 4.1", "Dir 4/Dir 4.2"));
    _updateTree->getNodeByPath("Dir 4/Dir 4.1")->setName(Str("Dir 4.2"));
    CPPUNIT_ASSERT(_updateTreeWorker->getNewPathAfterMove("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1", newPath) == ExitCode::Ok);
    CPPUNIT_ASSERT(newPath == "Dir 4/Dir 4.2/Dir 4.1.1/File 4.1.1.1");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath(newPath)->id() == "id4111");
}

void TestUpdateTreeWorker::testUpdateTmpFileNode() {
    const auto createOp = std::make_shared<FSOperation>(OperationType::Create, "id51", NodeType::File, 1654798336, 1654798336,
                                                        12345, "Dir 5/File 5.1");
    const auto deleteOp = std::make_shared<FSOperation>(OperationType::Delete, "id51bis", NodeType::File, 1654788552, 1654788552,
                                                        12345, "Dir 5/File 5.1");
    {
        auto newNode = _updateTreeWorker->getOrCreateNodeFromExistingPath("Dir 5/File 5.1");
        CPPUNIT_ASSERT(newNode->id()->substr(0, 4) == "tmp_");
        CPPUNIT_ASSERT(newNode->isTmp());

        CPPUNIT_ASSERT(_updateTreeWorker->updateTmpFileNode(newNode, createOp, deleteOp, OperationType::Edit));
        CPPUNIT_ASSERT_EQUAL(NodeId("id51"), *newNode->id());
        CPPUNIT_ASSERT_EQUAL(*newNode->createdAt(), createOp->createdAt());
        CPPUNIT_ASSERT_EQUAL(*newNode->lastmodified(), createOp->lastModified());
        CPPUNIT_ASSERT_EQUAL(newNode->size(), createOp->size());
        CPPUNIT_ASSERT(newNode->hasChangeEvent(OperationType::Edit));
        CPPUNIT_ASSERT_EQUAL(deleteOp->nodeId(), *newNode->previousId());
        CPPUNIT_ASSERT(!newNode->isTmp());
        CPPUNIT_ASSERT(_updateTree->nodes()[createOp->nodeId()] == newNode);
    }

    CPPUNIT_ASSERT(_updateTree->deleteNode(NodeId("id51")));

    {
        auto newNode = _updateTreeWorker->getOrCreateNodeFromExistingPath("Dir 5/File 5.1");
        CPPUNIT_ASSERT(newNode->id()->substr(0, 4) == "tmp_");
        CPPUNIT_ASSERT(newNode->isTmp());

        CPPUNIT_ASSERT(_updateTreeWorker->updateTmpFileNode(newNode, deleteOp, deleteOp, OperationType::Delete));
        CPPUNIT_ASSERT_EQUAL(NodeId("id51bis"), *newNode->id());
        CPPUNIT_ASSERT_EQUAL(*newNode->createdAt(), deleteOp->createdAt());
        CPPUNIT_ASSERT_EQUAL(*newNode->lastmodified(), deleteOp->lastModified());
        CPPUNIT_ASSERT_EQUAL(newNode->size(), createOp->size());
        CPPUNIT_ASSERT(newNode->hasChangeEvent(OperationType::Delete));
        CPPUNIT_ASSERT(!newNode->isTmp());
        CPPUNIT_ASSERT(_updateTree->nodes()[deleteOp->nodeId()] == newNode);
    }
}

void TestUpdateTreeWorker::testHandleCreateOperationsWithSamePath() {
    setUpUpdateTree();

    // Regular case: success
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id51", NodeType::File, 1654798336, 1654798336,
                                                          12345, "Dir 5/File 5.1"));
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _updateTreeWorker->handleCreateOperationsWithSamePath());


    // Duplicate paths imply failure
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id51bis", NodeType::File, 1654798336,
                                                          1654798336, 12345, "Dir 5/File 5.1"));
    CPPUNIT_ASSERT_EQUAL(ExitCode::DataError, _updateTreeWorker->handleCreateOperationsWithSamePath());
}


void TestUpdateTreeWorker::testSearchForParentNode() {
    setUpUpdateTree();
    std::shared_ptr<Node> parentNode;

    // A parent is found.
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _updateTreeWorker->searchForParentNode("Dir 4/Dir 4.1/Dir 4.1.1", parentNode));
    CPPUNIT_ASSERT(parentNode);
    CPPUNIT_ASSERT_EQUAL(NodeId("id41"), *parentNode->id());

    // No such parent exists.
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _updateTreeWorker->searchForParentNode("Dir 4/Dir 5.1/Dir 4.1.1", parentNode));
    CPPUNIT_ASSERT(!parentNode);
}

void TestUpdateTreeWorker::testStep1() {
    setUpUpdateTree();

    // Step 1 : move into non-existing & existing folder
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id111", NodeType::Directory, 1654788110,
                                                          1654788110, 12345, "Dir 1/Dir 1.1/Dir 1.1.1",
                                                          "Dir 1/Dir 1.2/Dir 1.2.1/Dir 1.1.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id3", NodeType::Directory, 1654788252, 1654788252,
                                                          12345, "Dir 3", "Dir 1/Dir 1.2/Dir 3"));
    // rename dir
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id11", NodeType::Directory, 1654788252,
                                                          1654788252, 12345, "Dir 1/Dir 1.1", "Dir 1/Dir 1.2"));

    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _updateTreeWorker->step1MoveDirectory());
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 1.2.1/Dir 1.1.1")->id() == "id111");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 3")->id() == "id3");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 1/Dir 1.2")->id() == "id11");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 1/Dir 1.1") == nullptr);
}

void TestUpdateTreeWorker::testStep2() {
    setUpUpdateTree();

    // Step 2 :Move files
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id1111", NodeType::File, 1654788256, 1654788256,
                                                          12345, "Dir 1/Dir 1.1/Dir 1.1.1/File 1.1.1.1", "Dir 1/File 1.1"));

    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _updateTreeWorker->step2MoveFile());
    std::shared_ptr<Node> node = _updateTree->getNodeByPath("Dir 1/File 1.1");
    CPPUNIT_ASSERT(node);
    CPPUNIT_ASSERT(node->hasChangeEvent(OperationType::Move));
    CPPUNIT_ASSERT(node->id() == "id1111");
    CPPUNIT_ASSERT(node->parentNode()->id() == "id1");
    CPPUNIT_ASSERT(node->moveOrigin() == "Dir 1/Dir 1.1/Dir 1.1.1/File 1.1.1.1");
}

void TestUpdateTreeWorker::testStep3() {
    setUpUpdateTree();

    // Step 3 : special delete case with move parent & delete child
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id3", NodeType::Directory, 1654788256, 1654788256,
                                                          12345, "Dir 3", "Dir 1/Dir 1.2/Dir 3"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id11", NodeType::Directory, 1654788252,
                                                          1654788252, 12345, "Dir 1/Dir 1.1", "Dir 1/Dir 1.2"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id3", NodeType::Directory, 1654788256,
                                                          1654788256, 12345, "Dir 1/Dir 1.2/Dir 3"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id2", NodeType::Directory, 1654788256,
                                                          1654788256, 12345, "Dir 2"));  // existing node
    // make move dir to test special case
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _updateTreeWorker->step1MoveDirectory());
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _updateTreeWorker->step3DeleteDirectory());
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 3")->hasChangeEvent(OperationType::Move));
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 3")->hasChangeEvent(OperationType::Delete));
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 3")->id() == "id3");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 3")->parentNode()->id() == "id11");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 2")->id() == "id2");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 2")->parentNode()->id() == _syncDb->rootNode().nodeIdLocal());
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 2")->hasChangeEvent(OperationType::Delete));
}

void TestUpdateTreeWorker::testStep4() {
    setUpUpdateTree();

    // Step 4 :
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id4111", NodeType::File, 1654798667, 1654798667,
                                                          12345, "Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1"));
    // Special delete create file
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id51", NodeType::File, 1654788552, 1654788552,
                                                          12345, "Dir 5/File 5.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id511", NodeType::File, 1654798336, 1654798336,
                                                          12345, "Dir 5/File 5.1"));
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _updateTreeWorker->step4DeleteFile());
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1")->hasChangeEvent(OperationType::Delete));
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1")->id() == "id4111");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1")->parentNode()->id() == "id411");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1")->lastmodified() == 1654798667);
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 5/File 5.1")->hasChangeEvent(OperationType::Edit));
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 5/File 5.1")->id() == "id511");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 5/File 5.1")->parentNode()->isTmp());
}

void TestUpdateTreeWorker::testStep5() {
    setUpUpdateTree();

    // Step 5 :create for Dir
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id121", NodeType::Directory, 1654725635,
                                                          1654725635, 12345, "Dir 1/Dir 1.2/Dir 1.2.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "idX", NodeType::Directory, 1654725632,
                                                          1654725632, 12345, "Dir 1/Dir x"));
    _operationSet->insertOp(
        std::make_shared<FSOperation>(OperationType::Create, "id7", NodeType::Directory, 1654725632, 1654725632, 12345, "Dir 7"));
    _operationSet->insertOp(
        std::make_shared<FSOperation>(OperationType::Create, "id5", NodeType::Directory, 1654725632, 1654725632, 12345, "Dir 5"));
    // test step5CreateDirectory
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _updateTreeWorker->step5CreateDirectory());
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 1.2.1")->id() == "id121");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 1.2.1")->hasChangeEvent(OperationType::Create));
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 1/Dir x")->id() == "idX");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 1.2.1")->hasChangeEvent(OperationType::Create));
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 1/Dir x")->parentNode()->children().size() >= 2);
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 7")->id() == "id7");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 7")->parentNode()->id() == _syncDb->rootNode().nodeIdLocal());
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 7")->children().empty());
}

void TestUpdateTreeWorker::testStep6() {
    setUpUpdateTree();

    // Step 4 : delete files
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id4111", NodeType::File, 1654798667, 1654798667,
                                                          12345, "Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id52", NodeType::File, 1654725632, 1654725632,
                                                          12345, "Dir 5/File 5.2"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id15", NodeType::File, 1654725632, 1654725632,
                                                          12345, "Dir 7/File 1.5"));
    // special delete create file
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id51", NodeType::File, 1654788552, 1654788552,
                                                          12345, "Dir 5/File 5.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id51bis", NodeType::File, 1654798336,
                                                          1654798336, 12345, "Dir 5/File 5.1"));
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _updateTreeWorker->step4DeleteFile());
    // Step 6 : create files
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _updateTreeWorker->step6CreateFile());
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 5/File 5.2")->id() == "id52");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 5/File 5.2")->parentNode()->isTmp());
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 5/File 5.2")->hasChangeEvent(OperationType::Create));
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 7/File 1.5")->id() == "id15");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 7/File 1.5")->parentNode()->isTmp());
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 7/File 1.5")->hasChangeEvent(OperationType::Create));
}

void TestUpdateTreeWorker::testStep7() {
    setUpUpdateTree();

    // Step 7 : Edit
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Edit, "id4112", NodeType::File, 1654999667, 1654999667,
                                                          12345, "Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.2"));
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _updateTreeWorker->step7EditFile());
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.2")->id() == "id4112");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.2")->parentNode()->id() == "id411");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.2")->hasChangeEvent(OperationType::Edit));
}

void TestUpdateTreeWorker::testStep8() {
    setUpUpdateTree();

    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _updateTreeWorker->step8CompleteUpdateTree());
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 5")->id() == "id5");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 5/File 5.1")->id() == "id51");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 1/Dir 1.1/File 1.1.2")->id() == "id112");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.2")->id() == "id4112");
}

void TestUpdateTreeWorker::testClearTreeStep1() {
    // Step 1 : move into non-existing & existing folder
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id111", NodeType::Directory, 1654788110,
                                                          1654788110, 12345, "Dir 1/Dir 1.1/Dir 1.1.1",
                                                          "Dir 1/Dir 1.2/Dir 1.2.1/Dir 1.1.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id3", NodeType::Directory, 1654788252, 1654788252,
                                                          12345, "Dir 3", "Dir 1/Dir 1.2/Dir 3"));
    // rename dir
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id11", NodeType::Directory, 1654788252,
                                                          1654788252, 12345, "Dir 1/Dir 1.1", "Dir 1/Dir 1.2"));

    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _updateTreeWorker->step1MoveDirectory());
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 1.2.1/Dir 1.1.1")->id() == "id111");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 3")->id() == "id3");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 1/Dir 1.2")->id() == "id11");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 1/Dir 1.1") == nullptr);
}

void TestUpdateTreeWorker::testClearTreeStep2() {
    // Step 2 :Move files
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id1111", NodeType::File, 1654788256, 1654788256,
                                                          12345, "Dir 1/Dir 1.1/Dir 1.1.1/File 1.1.1.1", "Dir 1/File 1.1"));

    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _updateTreeWorker->step2MoveFile());
    std::shared_ptr<Node> node = _updateTree->getNodeByPath("Dir 1/File 1.1");
    CPPUNIT_ASSERT(node);
    CPPUNIT_ASSERT(node->hasChangeEvent(OperationType::Move));
    CPPUNIT_ASSERT(node->id() == "id1111");
    // tree has been cleared so parent node has temp data
    CPPUNIT_ASSERT(node->parentNode()->name() == Str("Dir 1"));
    CPPUNIT_ASSERT(node->parentNode()->isTmp());
    CPPUNIT_ASSERT(node->moveOrigin() == "Dir 1/Dir 1.1/Dir 1.1.1/File 1.1.1.1");
}

void TestUpdateTreeWorker::testClearTreeStep3() {
    // Step 3 : special delete case with move parent & delete child
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id3", NodeType::Directory, 1654788256, 1654788256,
                                                          12345, "Dir 3", "Dir 1/Dir 1.2/Dir 3"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id11", NodeType::Directory, 1654788252,
                                                          1654788252, 12345, "Dir 1/Dir 1.1", "Dir 1/Dir 1.2"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id3", NodeType::Directory, 1654788256,
                                                          1654788256, 12345, "Dir 1/Dir 1.2/Dir 3"));
    _operationSet->insertOp(
        std::make_shared<FSOperation>(OperationType::Delete, "id2", NodeType::Directory, 1654788256, 1654788256, 12345, "Dir 2"));
    // make move dir to test special case
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _updateTreeWorker->step1MoveDirectory());
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _updateTreeWorker->step3DeleteDirectory());
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 3")->hasChangeEvent(OperationType::Move));
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 3")->hasChangeEvent(OperationType::Delete));
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 3")->id() == "id3");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 3")->parentNode()->id() == "id11");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 2")->id() == "id2");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 2")->hasChangeEvent(OperationType::Delete));
}

void TestUpdateTreeWorker::testClearTreeStep4() {
    // Step 4 :
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id4111", NodeType::File, 1654798667, 1654798667,
                                                          12345, "Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1"));
    // special delete create file
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id51", NodeType::File, 1654788552, 1654788552,
                                                          12345, "Dir 5/File 5.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id51bis", NodeType::File, 1654798336,
                                                          1654798336, 12345, "Dir 5/File 5.1"));
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _updateTreeWorker->step4DeleteFile());
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1")->hasChangeEvent(OperationType::Delete));
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1")->id() == "id4111");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1")->parentNode()->isTmp());
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1")->lastmodified() == 1654798667);
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 5/File 5.1")->hasChangeEvent(OperationType::Edit));
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 5/File 5.1")->id() == "id51bis");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 5/File 5.1")->parentNode()->isTmp());
}

void TestUpdateTreeWorker::testClearTreeStep5() {
    // Step 5 :create for Dir
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id121", NodeType::Directory, 1654725635,
                                                          1654725635, 12345, "Dir 1/Dir 1.2/Dir 1.2.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "idX", NodeType::Directory, 1654725632,
                                                          1654725632, 12345, "Dir 1/Dir x"));
    _operationSet->insertOp(
        std::make_shared<FSOperation>(OperationType::Create, "id7", NodeType::Directory, 1654725632, 1654725632, 12345, "Dir 7"));
    _operationSet->insertOp(
        std::make_shared<FSOperation>(OperationType::Create, "id5", NodeType::Directory, 1654725632, 1654725632, 12345, "Dir 5"));
    // test step5CreateDirectory
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _updateTreeWorker->step5CreateDirectory());
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 1.2.1")->id() == "id121");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 1.2.1")->hasChangeEvent(OperationType::Create));
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 1/Dir x")->id() == "idX");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 1.2.1")->hasChangeEvent(OperationType::Create));
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 1/Dir x")->parentNode()->children().size() >= 2);
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 7")->id() == "id7");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 7")->children().size() == 0);
}

void TestUpdateTreeWorker::testClearTreeStep6() {
    // Step 4 : delete files
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id4111", NodeType::File, 1654798667, 1654798667,
                                                          12345, "Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id52", NodeType::File, 1654725632, 1654725632,
                                                          12345, "Dir 5/File 5.2"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id15", NodeType::File, 1654725632, 1654725632,
                                                          12345, "Dir 7/File 1.5"));
    // special delete create file
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id51", NodeType::File, 1654788552, 1654788552,
                                                          12345, "Dir 5/File 5.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id51bis", NodeType::File, 1654798336,
                                                          1654798336, 12345, "Dir 5/File 5.1"));

    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _updateTreeWorker->step4DeleteFile());
    // Step 6 : create files
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _updateTreeWorker->step6CreateFile());
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 5/File 5.2")->id() == "id52");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 5/File 5.2")->parentNode()->isTmp());
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 5/File 5.2")->hasChangeEvent(OperationType::Create));
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 7/File 1.5")->id() == "id15");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 7/File 1.5")->parentNode()->isTmp());
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 7/File 1.5")->hasChangeEvent(OperationType::Create));
}

void TestUpdateTreeWorker::testClearTreeStep7() {
    // Step 7 :
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Edit, "id4112", NodeType::File, 1654999667, 1654999667,
                                                          12345, "Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.2"));

    // test step7EditFile
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _updateTreeWorker->step7EditFile());
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.2")->id() == "id4112");
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.2")->parentNode()->isTmp());
    CPPUNIT_ASSERT(_updateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.2")->hasChangeEvent(OperationType::Edit));
}

void TestUpdateTreeWorker::testClearTreeStep8() {
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _updateTreeWorker->step8CompleteUpdateTree());
    CPPUNIT_ASSERT(_updateTreeWorker->_updateTree->nodes().size() == 18);
}

void TestUpdateTreeWorker::testGetOriginPath() {
    setUpUpdateTree();

    // Test without move operation
    std::shared_ptr<Node> node = _updateTree->getNodeById("id4111");
    SyncPath path;
    CPPUNIT_ASSERT(_updateTreeWorker->getOriginPath(node, path) == ExitCode::Ok);
    CPPUNIT_ASSERT(path == "Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1");
    CPPUNIT_ASSERT(node->getPath() == "Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1");

    // Test with move operation on the child
    node->insertChangeEvent(OperationType::Move);
    CPPUNIT_ASSERT(node->setParentNode(_updateTree->getNodeById("id4")));  // Move node 4111 under parent 4
    node->setName(Str("File 4.1.1.1 renamed"));                            // Rename node
    node->setMoveOrigin("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1");
    node->setMoveOriginParentDbId(_dbnodeIdDir411);
    CPPUNIT_ASSERT(_updateTreeWorker->getOriginPath(node, path) == ExitCode::Ok);
    CPPUNIT_ASSERT(path == "Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1");
    CPPUNIT_ASSERT(node->getPath() == "Dir 4/File 4.1.1.1 renamed");
}

void TestUpdateTreeWorker::testGetOriginPath2() {
    setUpUpdateTree();

    // Test with move operation on some parents
    std::shared_ptr<Node> node = _updateTree->getNodeById("id411");
    node->insertChangeEvent(OperationType::Move);
    CPPUNIT_ASSERT(node->setParentNode(_updateTree->getNodeById("id4")));  // Move node 411 under parent 4
    node->setName(Str("Dir 4.1.1 renamed"));                               // Rename node
    node->setMoveOrigin("Dir 4/Dir 4.1/Dir 4.1.1");
    node->setMoveOriginParentDbId(_dbnodeIdDir41);

    SyncPath path;
    CPPUNIT_ASSERT(_updateTreeWorker->getOriginPath(node, path) == ExitCode::Ok);
    CPPUNIT_ASSERT(path == "Dir 4/Dir 4.1/Dir 4.1.1");
    CPPUNIT_ASSERT(node->getPath() == "Dir 4/Dir 4.1.1 renamed");

    node = _updateTree->getNodeById("id4111");
    CPPUNIT_ASSERT(_updateTreeWorker->getOriginPath(node, path) == ExitCode::Ok);
    CPPUNIT_ASSERT(path == "Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1");
    CPPUNIT_ASSERT(node->getPath() == "Dir 4/Dir 4.1.1 renamed/File 4.1.1.1");
}

void TestUpdateTreeWorker::testGetOriginPath3() {
    setUpUpdateTree();

    // Test with move operation on parent AND child (rename children THEN move parent)
    std::shared_ptr<Node> node4111 = _updateTree->getNodeById("id4111");
    node4111->insertChangeEvent(OperationType::Move);
    node4111->setName(Str("File 4.1.1.1 renamed"));  // Rename node
    node4111->setMoveOrigin("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1");
    node4111->setMoveOriginParentDbId(_dbnodeIdDir411);

    std::shared_ptr<Node> node411 = _updateTree->getNodeById("id411");
    node411->insertChangeEvent(OperationType::Move);
    CPPUNIT_ASSERT(node411->setParentNode(_updateTree->getNodeById("id4")));  // Move node 411 under parent 4
    node411->setMoveOrigin("Dir 4/Dir 4.1/Dir 4.1.1");
    node411->setMoveOriginParentDbId(_dbnodeIdDir41);

    SyncPath path;
    CPPUNIT_ASSERT(_updateTreeWorker->getOriginPath(node4111, path) == ExitCode::Ok);
    CPPUNIT_ASSERT(path == "Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1");
    CPPUNIT_ASSERT(node4111->getPath() == "Dir 4/Dir 4.1.1/File 4.1.1.1 renamed");
}

void TestUpdateTreeWorker::testGetOriginPath4() {
    setUpUpdateTree();

    // Test with move operation on parent AND child (move parent THEN rename children)
    std::shared_ptr<Node> node411 = _updateTree->getNodeById("id411");
    node411->insertChangeEvent(OperationType::Move);
    CPPUNIT_ASSERT(node411->setParentNode(_updateTree->getNodeById("id4")));  // Move node 411 under parent 4
    node411->setMoveOrigin("Dir 4/Dir 4.1/Dir 4.1.1");
    node411->setMoveOriginParentDbId(_dbnodeIdDir41);

    std::shared_ptr<Node> node4111 = _updateTree->getNodeById("id4111");
    node4111->insertChangeEvent(OperationType::Move);
    node4111->setName(Str("File 4.1.1.1 renamed"));  // Rename node
    node4111->setMoveOrigin("Dir 4/Dir 4.1.1/File 4.1.1.1");
    node4111->setMoveOriginParentDbId(_dbnodeIdDir411);

    SyncPath path;
    CPPUNIT_ASSERT(_updateTreeWorker->getOriginPath(node4111, path) == ExitCode::Ok);
    CPPUNIT_ASSERT(path == "Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1");
    CPPUNIT_ASSERT(node4111->getPath() == "Dir 4/Dir 4.1.1/File 4.1.1.1 renamed");
}

void TestUpdateTreeWorker::testDeleteMove() {
    setUpUpdateTree();

    /**
     *  - Delete 6a
     *  - Rename 6 into 6a
     *  - Create 6
     */
    _operationSet->insertOp(
        std::make_shared<FSOperation>(OperationType::Delete, "id6a", NodeType::File, 1654798667, 1654798667, 12345));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id6", NodeType::File, 1654725632, 1654725632,
                                                          12345, "File 6", "File 6a"));
    _operationSet->insertOp(
        std::make_shared<FSOperation>(OperationType::Create, "id6b", NodeType::File, 1654725632, 1654725632, 12345, "File 6"));

    _updateTreeWorker->execute();

    auto node6 = _updateTree->getNodeById("id6");
    auto node6a = _updateTree->getNodeById("id6a");
    auto node6b = _updateTree->getNodeById("id6b");
    CPPUNIT_ASSERT(node6->parentNode() == _updateTree->rootNode());
    CPPUNIT_ASSERT(_updateTree->rootNode()->children().find("id6") != _updateTree->rootNode()->children().end());
    CPPUNIT_ASSERT(node6->name() == Str("File 6a"));

    CPPUNIT_ASSERT(node6a->parentNode() == _updateTree->rootNode());
    CPPUNIT_ASSERT(_updateTree->rootNode()->children().find("id6a") != _updateTree->rootNode()->children().end());
    CPPUNIT_ASSERT(node6a->name() == Str("File 6a"));

    CPPUNIT_ASSERT(node6b->parentNode() == _updateTree->rootNode());
    CPPUNIT_ASSERT(_updateTree->rootNode()->children().find("id6b") != _updateTree->rootNode()->children().end());
    CPPUNIT_ASSERT(node6b->name() == Str("File 6"));
}

void TestUpdateTreeWorker::testDeleteRecreateBranch() {
    setUpUpdateTree();

    _operationSet->insertOp(
        std::make_shared<FSOperation>(OperationType::Delete, "id1", NodeType::Directory, 1654798667, 1654798667, 12345));
    _operationSet->insertOp(
        std::make_shared<FSOperation>(OperationType::Delete, "id11", NodeType::Directory, 1654798667, 1654798667, 12345));
    _operationSet->insertOp(
        std::make_shared<FSOperation>(OperationType::Delete, "id111", NodeType::Directory, 1654798667, 1654798667, 12345));
    _operationSet->insertOp(
        std::make_shared<FSOperation>(OperationType::Delete, "id1111", NodeType::File, 1654798667, 1654798667, 12345));

    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id1_new", NodeType::Directory, 1654798667,
                                                          1654798667, 12345, "Dir 1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id11_new", NodeType::Directory, 1654798667,
                                                          1654798667, 12345, "Dir 1/Dir 1.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id111_new", NodeType::Directory, 1654798667,
                                                          1654798667, 12345, "Dir 1/Dir 1.1/Dir 1.1.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id1111_new", NodeType::File, 1654798667,
                                                          1654798667, 12345, "Dir 1/Dir 1.1/Dir 1.1.1/File 1.1.1.1"));

    _updateTreeWorker->execute();

    auto node1 = _updateTree->getNodeById("id1");
    CPPUNIT_ASSERT(node1->hasChangeEvent(OperationType::Delete));
    CPPUNIT_ASSERT(!node1->hasChangeEvent(OperationType::Create));
    auto node11 = _updateTree->getNodeById("id11");
    CPPUNIT_ASSERT(node11->hasChangeEvent(OperationType::Delete));
    CPPUNIT_ASSERT(!node11->hasChangeEvent(OperationType::Create));
    auto node111 = _updateTree->getNodeById("id111");
    CPPUNIT_ASSERT(node111->hasChangeEvent(OperationType::Delete));
    CPPUNIT_ASSERT(!node111->hasChangeEvent(OperationType::Create));
    auto node1111 = _updateTree->getNodeById("id1111");
    CPPUNIT_ASSERT(node1111->hasChangeEvent(OperationType::Delete));
    CPPUNIT_ASSERT(!node1111->hasChangeEvent(OperationType::Create));
    CPPUNIT_ASSERT(node1111->parentNode() == node111);

    auto node1new = _updateTree->getNodeById("id1_new");
    CPPUNIT_ASSERT(!node1new->hasChangeEvent(OperationType::Delete));
    CPPUNIT_ASSERT(node1new->hasChangeEvent(OperationType::Create));
    auto node11new = _updateTree->getNodeById("id11_new");
    CPPUNIT_ASSERT(!node11new->hasChangeEvent(OperationType::Delete));
    CPPUNIT_ASSERT(node11new->hasChangeEvent(OperationType::Create));
    auto node111new = _updateTree->getNodeById("id111_new");
    CPPUNIT_ASSERT(!node111new->hasChangeEvent(OperationType::Delete));
    CPPUNIT_ASSERT(node111new->hasChangeEvent(OperationType::Create));
    auto node1111new = _updateTree->getNodeById("id1111_new");
    CPPUNIT_ASSERT(!node1111new->hasChangeEvent(OperationType::Delete));
    CPPUNIT_ASSERT(node1111new->hasChangeEvent(OperationType::Create));
    CPPUNIT_ASSERT(node1111new->parentNode() == node111new);
}

}  // namespace KDC
