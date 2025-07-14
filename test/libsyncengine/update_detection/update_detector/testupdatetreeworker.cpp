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

#include "testupdatetreeworker.h"
#include "requests/parameterscache.h"
#include "mocks/libcommonserver/db/mockdb.h"

#include "test_utility/testhelpers.h"

#include <memory>

using namespace CppUnit;

namespace KDC {

void TestUpdateTreeWorker::setUp() {
    TestBase::start();
    // Create parmsDb
    bool alreadyExists = false;
    std::filesystem::path parmsDbPath = MockDb::makeDbName(alreadyExists);
    ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);
    ParametersCache::instance()->parameters().setExtendedLog(true);

    // Create DB
    const std::filesystem::path syncDbPath = MockDb::makeDbName(1, 1, 1, 1, alreadyExists);
    _syncDb = std::make_shared<SyncDb>(syncDbPath.string(), "3.6.1");
    _syncDb->init("3.6.1");
    _syncDb->setAutoDelete(true);
    _operationSet = std::make_shared<FSOperationSet>(ReplicaSide::Unknown);

    _localUpdateTree = std::make_shared<UpdateTree>(ReplicaSide::Local, SyncDb::driveRootNode());
    _remoteUpdateTree = std::make_shared<UpdateTree>(ReplicaSide::Remote, SyncDb::driveRootNode());

    _localUpdateTreeWorker = std::make_shared<UpdateTreeWorker>(_syncDb->cache(), _operationSet, _localUpdateTree,
                                                                "Test Tree Updater", "LTRU", ReplicaSide::Local);
    _remoteUpdateTreeWorker = std::make_shared<UpdateTreeWorker>(_syncDb->cache(), _operationSet, _remoteUpdateTree,
                                                                 "Test Tree Updater", "RTRU", ReplicaSide::Remote);

    setUpDbTree();

    _localUpdateTree->init();
    _remoteUpdateTree->init();
}

void TestUpdateTreeWorker::tearDown() {
    ParmsDb::instance()->close();
    // The singleton ParmsDb calls KDC::Log()->instance() in its destructor.
    // As the two singletons are instantiated in different translation units, the order of their destruction is unknown.
    ParmsDb::reset();
    ParametersCache::reset();
    TestBase::stop();
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
        ├── 6a
        └── 7
     */

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
    const DbNode nodeDir1(0, _syncDb->rootNode().nodeId(), Str("Dir 1"), Str("Dir 1"), "id1", "id drive 1",
                          testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory,
                          testhelpers::defaultFileSize, std::nullopt);
    _syncDb->insertNode(nodeDir1, dbNodeIdDir1, constraintError);
    const DbNode nodeDir11(0, dbNodeIdDir1, Str("Dir 1.1"), Str("Dir 1.1"), "id11", "id drive 11", testhelpers::defaultTime,
                           testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, testhelpers::defaultFileSize,
                           std::nullopt);
    _syncDb->insertNode(nodeDir11, dbNodeIdDir11, constraintError);
    const DbNode nodeDir111(0, dbNodeIdDir11, Str("Dir 1.1.1"), Str("Dir 1.1.1"), "id111", "id drive 111",
                            testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory,
                            testhelpers::defaultFileSize, std::nullopt);
    _syncDb->insertNode(nodeDir111, dbNodeIdDir111, constraintError);
    const DbNode nodeFile112(0, dbNodeIdDir11, Str("File 1.1.2"), Str("File 1.1.2"), "id112", "id drive 112",
                             testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File,
                             testhelpers::defaultFileSize, "cs 1.1");
    _syncDb->insertNode(nodeFile112, dbNodeId112, constraintError);
    const DbNode nodeFile1111(0, dbNodeIdDir111, Str("File 1.1.1.1"), Str("File 1.1.1.1"), "id1111", "id drive 1111",
                              testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File,
                              testhelpers::defaultFileSize, "cs 1.1");
    _syncDb->insertNode(nodeFile1111, dbNodeIdFile1111, constraintError);
    const DbNode nodeDir2(0, _syncDb->rootNode().nodeId(), Str("Dir 2"), Str("Dir 2"), "id2", "id drive 2",
                          testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory,
                          testhelpers::defaultFileSize, std::nullopt);
    _syncDb->insertNode(nodeDir2, dbNodeIdDir2, constraintError);
    const DbNode nodeDir3(0, _syncDb->rootNode().nodeId(), Str("Dir 3"), Str("Dir 3"), "id3", "id drive 3",
                          testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory,
                          testhelpers::defaultFileSize, std::nullopt);
    _syncDb->insertNode(nodeDir3, dbNodeIdDir3, constraintError);
    const DbNode nodeDir31(0, dbNodeIdDir3, Str("Dir 3.1"), Str("Dir 3.1"), "id31", "id drive 31", testhelpers::defaultTime,
                           testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, testhelpers::defaultFileSize,
                           std::nullopt);
    _syncDb->insertNode(nodeDir31, dbNodeIdDir31, constraintError);
    const DbNode nodeDir4(0, _syncDb->rootNode().nodeId(), Str("Dir 4"), Str("Dir 4"), "id4", "id drive 4",
                          testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory,
                          testhelpers::defaultFileSize, std::nullopt);
    _syncDb->insertNode(nodeDir4, dbnodeIdDir4, constraintError);
    const DbNode nodedir41(0, dbnodeIdDir4, Str("Dir 4.1"), Str("Dir 4.1"), "id41", "id drive 41", testhelpers::defaultTime,
                           testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, testhelpers::defaultFileSize,
                           std::nullopt);
    _syncDb->insertNode(nodedir41, _dbnodeIdDir41, constraintError);
    const DbNode nodeDir411(0, _dbnodeIdDir41, Str("Dir 4.1.1"), Str("Dir 4.1.1"), "id411", "id drive 411",
                            testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory,
                            testhelpers::defaultFileSize, std::nullopt);
    _syncDb->insertNode(nodeDir411, _dbnodeIdDir411, constraintError);
    const DbNode nodeFile4111(0, _dbnodeIdDir411, Str("File 4.1.1.1"), Str("File 4.1.1.1"), "id4111", "id drive 4111",
                              testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File,
                              testhelpers::defaultFileSize, std::nullopt);
    _syncDb->insertNode(nodeFile4111, dbnodeIdfile4111, constraintError);
    const DbNode nodeFile4112(0, _dbnodeIdDir411, Str("File 4.1.1.2"), Str("File 4.1.1.2"), "id4112", "id drive 4112",
                              testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File,
                              testhelpers::defaultFileSize, std::nullopt);
    _syncDb->insertNode(nodeFile4112, dbnodeIdfile4112, constraintError);
    const DbNode nodeDir5(0, _syncDb->rootNode().nodeId(), Str("Dir 5"), Str("Dir 5"), "id5", "id drive 5",
                          testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory,
                          testhelpers::defaultFileSize, std::nullopt);
    _syncDb->insertNode(nodeDir5, dbnodeIdDir5, constraintError);
    const DbNode nodeFile51(0, dbnodeIdDir5, Str("File 5.1"), Str("File 5.1"), "id51", "id drive 51", testhelpers::defaultTime,
                            testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File, testhelpers::defaultFileSize,
                            std::nullopt);
    _syncDb->insertNode(nodeFile51, dbnodeIdfile51, constraintError);
    const DbNode nodeFile6(0, _syncDb->rootNode().nodeId(), Str("File 6"), Str("File 6"), "id6", "id drive 6",
                           testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File,
                           testhelpers::defaultFileSize, std::nullopt);
    _syncDb->insertNode(nodeFile6, dbnodeIdfile6, constraintError);
    const DbNode nodeFile6a(0, _syncDb->rootNode().nodeId(), Str("File 6a"), Str("File 6a"), "id6a", "id drive 6a",
                            testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File,
                            testhelpers::defaultFileSize, std::nullopt);
    _syncDb->insertNode(nodeFile6a, dbnodeIdfile6a, constraintError);

    // Node with name encoded differently on remote (NFC) and on local (NFD) side
    DbNodeId dbnodeIdfile7;
    const DbNode nodeFile7(0, _syncDb->rootNode().nodeId(), testhelpers::makeNfdSyncName(), testhelpers::makeNfdSyncName(),
                           "id7l", "id7r", testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime,
                           NodeType::File, testhelpers::defaultFileSize, std::nullopt);
    _syncDb->insertNode(nodeFile7, dbnodeIdfile7, constraintError);
    _syncDb->cache().reloadIfNeeded();
}

void TestUpdateTreeWorker::setUpUpdateTree(ReplicaSide side) {
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
        ├── 6a
        └── 7
     */

    std::shared_ptr<UpdateTree> updateTree = side == ReplicaSide::Local ? _localUpdateTree : _remoteUpdateTree;

    bool found = false;
    DbNodeId dbNodeIdDir;
    _syncDb->dbId(ReplicaSide::Local, NodeId("id1"), dbNodeIdDir, found);
    const auto node1 = std::make_shared<Node>(dbNodeIdDir, updateTree->side(), Str("Dir 1"), NodeType::Directory,
                                              OperationType::None, "id1", testhelpers::defaultTime, testhelpers::defaultTime,
                                              testhelpers::defaultFileSize, updateTree->rootNode());
    _syncDb->dbId(ReplicaSide::Local, NodeId("id2"), dbNodeIdDir, found);
    const auto node2 = std::make_shared<Node>(dbNodeIdDir, updateTree->side(), Str("Dir 2"), NodeType::Directory,
                                              OperationType::None, "id2", testhelpers::defaultTime, testhelpers::defaultTime,
                                              testhelpers::defaultFileSize, updateTree->rootNode());
    _syncDb->dbId(ReplicaSide::Local, NodeId("id3"), dbNodeIdDir, found);
    const auto node3 = std::make_shared<Node>(dbNodeIdDir, updateTree->side(), Str("Dir 3"), NodeType::Directory,
                                              OperationType::None, "id3", testhelpers::defaultTime, testhelpers::defaultTime,
                                              testhelpers::defaultFileSize, updateTree->rootNode());
    _syncDb->dbId(ReplicaSide::Local, NodeId("id4"), dbNodeIdDir, found);
    auto node4 = std::make_shared<Node>(dbNodeIdDir, updateTree->side(), Str("Dir 4"), NodeType::Directory, OperationType::None,
                                        "id4", testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize,
                                        updateTree->rootNode());
    _syncDb->dbId(ReplicaSide::Local, NodeId("id11"), dbNodeIdDir, found);
    auto node11 = std::make_shared<Node>(dbNodeIdDir, updateTree->side(), Str("Dir 1.1"), NodeType::Directory,
                                         OperationType::None, "id11", testhelpers::defaultTime, testhelpers::defaultTime,
                                         testhelpers::defaultFileSize, node1);
    _syncDb->dbId(ReplicaSide::Local, NodeId("id111"), dbNodeIdDir, found);
    auto node111 = std::make_shared<Node>(dbNodeIdDir, updateTree->side(), Str("Dir 1.1.1"), NodeType::Directory,
                                          OperationType::None, "id111", testhelpers::defaultTime, testhelpers::defaultTime,
                                          testhelpers::defaultFileSize, node11);
    _syncDb->dbId(ReplicaSide::Local, NodeId("id1111"), dbNodeIdDir, found);
    const auto node1111 = std::make_shared<Node>(dbNodeIdDir, updateTree->side(), Str("File 1.1.1.1"), NodeType::File,
                                                 OperationType::None, "id1111", testhelpers::defaultTime,
                                                 testhelpers::defaultTime, testhelpers::defaultFileSize, node111);
    _syncDb->dbId(ReplicaSide::Local, NodeId("id31"), dbNodeIdDir, found);
    const auto node31 = std::make_shared<Node>(dbNodeIdDir, updateTree->side(), Str("Dir 3.1"), NodeType::Directory,
                                               OperationType::None, "id31", testhelpers::defaultTime, testhelpers::defaultTime,
                                               testhelpers::defaultFileSize, node3);
    const auto node41 = std::make_shared<Node>(_dbnodeIdDir41, updateTree->side(), Str("Dir 4.1"), NodeType::Directory,
                                               OperationType::None, "id41", testhelpers::defaultTime, testhelpers::defaultTime,
                                               testhelpers::defaultFileSize, node4);
    const auto node411 = std::make_shared<Node>(_dbnodeIdDir411, updateTree->side(), Str("Dir 4.1.1"), NodeType::Directory,
                                                OperationType::None, "id411", testhelpers::defaultTime, testhelpers::defaultTime,
                                                testhelpers::defaultFileSize, node41);
    _syncDb->dbId(ReplicaSide::Local, NodeId("id4111"), dbNodeIdDir, found);
    const auto node4111 = std::make_shared<Node>(dbNodeIdDir, updateTree->side(), Str("File 4.1.1.1"), NodeType::File,
                                                 OperationType::None, "id4111", testhelpers::defaultTime,
                                                 testhelpers::defaultTime, testhelpers::defaultFileSize, node411);
    _syncDb->dbId(ReplicaSide::Local, NodeId("id6"), dbNodeIdDir, found);
    const auto node6 = std::make_shared<Node>(dbNodeIdDir, updateTree->side(), Str("File 6"), NodeType::File, OperationType::None,
                                              "id6", testhelpers::defaultTime, testhelpers::defaultTime,
                                              testhelpers::defaultFileSize, updateTree->rootNode());
    _syncDb->dbId(ReplicaSide::Local, NodeId("id6a"), dbNodeIdDir, found);
    const auto node6a = std::make_shared<Node>(dbNodeIdDir, updateTree->side(), Str("File 6a"), NodeType::File,
                                               OperationType::None, "id6a", testhelpers::defaultTime, testhelpers::defaultTime,
                                               testhelpers::defaultFileSize, updateTree->rootNode());

    std::shared_ptr<Node> node7 = nullptr;
    if (side == ReplicaSide::Local) {
        // Name encoded NFD on local side
        _syncDb->dbId(ReplicaSide::Local, NodeId("id7l"), dbNodeIdDir, found);
        node7 = std::make_shared<Node>(dbNodeIdDir, updateTree->side(), testhelpers::makeNfdSyncName(), NodeType::File,
                                       OperationType::None, "id7l", testhelpers::defaultTime, testhelpers::defaultTime,
                                       testhelpers::defaultFileSize, updateTree->rootNode());
    } else {
        // Name encoded NFC on remote side
        _syncDb->dbId(ReplicaSide::Local, NodeId("id7r"), dbNodeIdDir, found);
        node7 = std::make_shared<Node>(dbNodeIdDir, updateTree->side(), testhelpers::makeNfdSyncName(), NodeType::File,
                                       OperationType::None, "id7r", testhelpers::defaultTime, testhelpers::defaultTime,
                                       testhelpers::defaultFileSize, updateTree->rootNode());
    }

    updateTree->init();

    CPPUNIT_ASSERT(updateTree->rootNode()->insertChildren(node1));
    CPPUNIT_ASSERT(updateTree->rootNode()->insertChildren(node2));
    CPPUNIT_ASSERT(updateTree->rootNode()->insertChildren(node3));
    CPPUNIT_ASSERT(updateTree->rootNode()->insertChildren(node4));
    CPPUNIT_ASSERT(updateTree->rootNode()->insertChildren(node6));
    CPPUNIT_ASSERT(updateTree->rootNode()->insertChildren(node6a));
    CPPUNIT_ASSERT(updateTree->rootNode()->insertChildren(node7));

    CPPUNIT_ASSERT(node1->insertChildren(node11));
    CPPUNIT_ASSERT(node11->insertChildren(node111));
    CPPUNIT_ASSERT(node111->insertChildren(node1111));
    CPPUNIT_ASSERT(node3->insertChildren(node31));
    CPPUNIT_ASSERT(node4->insertChildren(node41));
    CPPUNIT_ASSERT(node41->insertChildren(node411));
    CPPUNIT_ASSERT(node411->insertChildren(node4111));

    updateTree->insertNode(node1111);
    updateTree->insertNode(node111);
    updateTree->insertNode(node11);
    updateTree->insertNode(node1);
    updateTree->insertNode(node2);
    updateTree->insertNode(node3);
    updateTree->insertNode(node4);
    updateTree->insertNode(node31);
    updateTree->insertNode(node41);
    updateTree->insertNode(node411);
    updateTree->insertNode(node4111);
    updateTree->insertNode(node6);
    updateTree->insertNode(node6a);
    updateTree->insertNode(node7);
}

void TestUpdateTreeWorker::testUtilsFunctions() {
    setUpUpdateTree(ReplicaSide::Local);

    // test _localUpdateTree->getNodeByPath
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("")->id() == _syncDb->rootNode().nodeIdLocal());
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 1/Dir 1.1/Dir 1.1.1")->id() == "id111");

    // test getNewPathAfterMove
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id3", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 3", "Dir 3bis"));
    _localUpdateTree->getNodeByPath("Dir 3")->setName(Str("Dir 3bis"));

    SyncPath newPath;
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->getNewPathAfterMove("Dir 3/Dir 3.1", newPath));
    CPPUNIT_ASSERT(newPath == "Dir 3bis/Dir 3.1");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath(newPath)->id() == "id31");

    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id41", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 4/Dir 4.1", "Dir 4/Dir 4.2"));
    _localUpdateTree->getNodeByPath("Dir 4/Dir 4.1")->setName(Str("Dir 4.2"));
    CPPUNIT_ASSERT(_localUpdateTreeWorker->getNewPathAfterMove("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1", newPath) == ExitCode::Ok);
    CPPUNIT_ASSERT(newPath == "Dir 4/Dir 4.2/Dir 4.1.1/File 4.1.1.1");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath(newPath)->id() == "id4111");

    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPathNormalized(newPath) == _localUpdateTree->getNodeByPath(newPath));
    _localUpdateTree->getNodeByPath("Dir 4/Dir 4.2")->setName(testhelpers::makeNfdSyncName());

    CPPUNIT_ASSERT(nullptr == _localUpdateTree->getNodeByPath(SyncPath("Dir 4") / testhelpers::makeNfcSyncName()));
    CPPUNIT_ASSERT(nullptr != _localUpdateTree->getNodeByPathNormalized(SyncPath("Dir 4") / testhelpers::makeNfcSyncName()));
}

void TestUpdateTreeWorker::testUpdateTmpFileNode() {
    const auto createOp = std::make_shared<FSOperation>(OperationType::Create, "id51", NodeType::File, testhelpers::defaultTime,
                                                        testhelpers::defaultTime, testhelpers::defaultFileSize, "Dir 5/File 5.1");
    const auto deleteOp =
            std::make_shared<FSOperation>(OperationType::Delete, "id51bis", NodeType::File, testhelpers::defaultTime,
                                          testhelpers::defaultTime, testhelpers::defaultFileSize, "Dir 5/File 5.1");
    {
        std::shared_ptr<Node> newNode;
        CPPUNIT_ASSERT(_localUpdateTreeWorker->getOrCreateNodeFromExistingPath("Dir 5/File 5.1", newNode) == ExitCode::Ok);
        CPPUNIT_ASSERT(newNode->id()->substr(0, 4) == "tmp_");
        CPPUNIT_ASSERT(newNode->isTmp());

        CPPUNIT_ASSERT(_localUpdateTreeWorker->updateTmpFileNode(newNode, createOp, deleteOp, OperationType::Edit));
        CPPUNIT_ASSERT_EQUAL(NodeId("id51"), *newNode->id());
        CPPUNIT_ASSERT_EQUAL(*newNode->createdAt(), createOp->createdAt());
        CPPUNIT_ASSERT_EQUAL(*newNode->modificationTime(), createOp->lastModified());
        CPPUNIT_ASSERT_EQUAL(newNode->size(), createOp->size());
        CPPUNIT_ASSERT(newNode->hasChangeEvent(OperationType::Edit));
        CPPUNIT_ASSERT_EQUAL(deleteOp->nodeId(), *newNode->previousId());
        CPPUNIT_ASSERT(!newNode->isTmp());
        CPPUNIT_ASSERT(_localUpdateTree->nodes()[createOp->nodeId()] == newNode);
    }

    CPPUNIT_ASSERT(_localUpdateTree->deleteNode(NodeId("id51")));

    {
        std::shared_ptr<Node> newNode;
        CPPUNIT_ASSERT(_localUpdateTreeWorker->getOrCreateNodeFromExistingPath("Dir 5/File 5.1", newNode) == ExitCode::Ok);
        CPPUNIT_ASSERT(newNode->id()->substr(0, 4) == "tmp_");
        CPPUNIT_ASSERT(newNode->isTmp());

        CPPUNIT_ASSERT(_localUpdateTreeWorker->updateTmpFileNode(newNode, deleteOp, deleteOp, OperationType::Delete));
        CPPUNIT_ASSERT_EQUAL(NodeId("id51bis"), *newNode->id());
        CPPUNIT_ASSERT_EQUAL(*newNode->createdAt(), deleteOp->createdAt());
        CPPUNIT_ASSERT_EQUAL(*newNode->modificationTime(), deleteOp->lastModified());
        CPPUNIT_ASSERT_EQUAL(newNode->size(), createOp->size());
        CPPUNIT_ASSERT(newNode->hasChangeEvent(OperationType::Delete));
        CPPUNIT_ASSERT(!newNode->isTmp());
        CPPUNIT_ASSERT(_localUpdateTree->nodes()[deleteOp->nodeId()] == newNode);
    }
}

void TestUpdateTreeWorker::testHandleCreateOperationsWithSamePath() {
    setUpUpdateTree(ReplicaSide::Local);

    { // Regular case: success
        _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id51", NodeType::File,
                                                              testhelpers::defaultTime, testhelpers::defaultTime,
                                                              testhelpers::defaultFileSize, "Dir 5/File 5.1"));
        CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->handleCreateOperationsWithSamePath());


        // Duplicate paths imply failure
        _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id51bis", NodeType::File,
                                                              testhelpers::defaultTime, testhelpers::defaultTime,
                                                              testhelpers::defaultFileSize, "Dir 5/File 5.1"));
        CPPUNIT_ASSERT_EQUAL(ExitCode::DataError, _localUpdateTreeWorker->handleCreateOperationsWithSamePath());
    }

    {
        _operationSet->clear();

        // Regular case: success
        _operationSet->insertOp(std::make_shared<FSOperation>(
                OperationType::Create, "id6nfc", NodeType::File, testhelpers::defaultTime, testhelpers::defaultTime,
                testhelpers::defaultFileSize, SyncPath("Dir 6") / testhelpers::makeNfcSyncName()));
        CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->handleCreateOperationsWithSamePath());


        // Duplicate paths but distinct name encodings are not supported and should never reach the updateTree.
        _operationSet->insertOp(std::make_shared<FSOperation>(
                OperationType::Create, "id6nfd", NodeType::File, testhelpers::defaultTime, testhelpers::defaultTime,
                testhelpers::defaultFileSize, SyncPath("Dir 6") / testhelpers::makeNfdSyncName()));
        CPPUNIT_ASSERT_EQUAL(ExitCode::DataError, _localUpdateTreeWorker->handleCreateOperationsWithSamePath());
    }
}


void TestUpdateTreeWorker::testSearchForParentNode() {
    setUpUpdateTree(ReplicaSide::Local);

    std::shared_ptr<Node> parentNode;

    // A parent is found.
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->searchForParentNode("Dir 4/Dir 4.1/Dir 4.1.1", parentNode));
    CPPUNIT_ASSERT(parentNode);
    CPPUNIT_ASSERT_EQUAL(NodeId("id41"), *parentNode->id());

    // No such parent exists.
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->searchForParentNode("Dir 4/Dir 5.1/Dir 4.1.1", parentNode));
    CPPUNIT_ASSERT(!parentNode);
}

void TestUpdateTreeWorker::testGetNewPathAfterMove() {
    // Simulate a rename of "Dir 1.1" to "Dir 1.1*"
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id11", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 1/Dir 1.1", "Dir 1/Dir 1.1*"));

    const SyncPath initPath = "Dir 1/Dir 1.1/Dir 1.1.1";
    SyncPath finalPath;
    _localUpdateTreeWorker->getNewPathAfterMove(initPath, finalPath);
    CPPUNIT_ASSERT_EQUAL(SyncPath("Dir 1/Dir 1.1*/Dir 1.1.1"), finalPath);
}

void TestUpdateTreeWorker::testStep1() {
    setUpUpdateTree(ReplicaSide::Local);

    // Step 1 : move into non-existing & existing folder
    _operationSet->insertOp(std::make_shared<FSOperation>(
            OperationType::Move, "id111", NodeType::Directory, testhelpers::defaultTime, testhelpers::defaultTime,
            testhelpers::defaultFileSize, "Dir 1/Dir 1.1/Dir 1.1.1", "Dir 1/Dir 1.2/Dir 1.2.1/Dir 1.1.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id3", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 3", "Dir 1/Dir 1.2/Dir 3"));
    // rename dir
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id11", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 1/Dir 1.1", "Dir 1/Dir 1.2"));

    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->step1MoveDirectory());
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 1.2.1/Dir 1.1.1")->id() == "id111");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 3")->id() == "id3");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 1/Dir 1.2")->id() == "id11");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 1/Dir 1.1") == nullptr);
}

void TestUpdateTreeWorker::testStep2() {
    setUpUpdateTree(ReplicaSide::Local);

    // Step 2 :Move files
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id1111", NodeType::File, testhelpers::defaultTime,
                                                          testhelpers::defaultTime, testhelpers::defaultFileSize,
                                                          "Dir 1/Dir 1.1/Dir 1.1.1/File 1.1.1.1", "Dir 1/File 1.1"));

    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->step2MoveFile());
    std::shared_ptr<Node> node = _localUpdateTree->getNodeByPath("Dir 1/File 1.1");
    CPPUNIT_ASSERT(node);
    CPPUNIT_ASSERT(node->hasChangeEvent(OperationType::Move));
    CPPUNIT_ASSERT(node->id() == "id1111");
    CPPUNIT_ASSERT(node->parentNode()->id() == "id1");
    CPPUNIT_ASSERT(node->moveOriginInfos().path() == "Dir 1/Dir 1.1/Dir 1.1.1/File 1.1.1.1");
}

void TestUpdateTreeWorker::testStep3() {
    setUpUpdateTree(ReplicaSide::Local);

    // Step 3 : special delete case with move parent & delete child
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id3", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 3", "Dir 1/Dir 1.2/Dir 3"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id11", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 1/Dir 1.1", "Dir 1/Dir 1.2"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id3", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 1/Dir 1.2/Dir 3"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id2", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 2")); // existing node
    // make move dir to test special case
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->step1MoveDirectory());
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->step3DeleteDirectory());
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 3")->hasChangeEvent(OperationType::Move));
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 3")->hasChangeEvent(OperationType::Delete));
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 3")->id() == "id3");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 3")->parentNode()->id() == "id11");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 2")->id() == "id2");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 2")->parentNode()->id() == _syncDb->rootNode().nodeIdLocal());
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 2")->hasChangeEvent(OperationType::Delete));
}

void TestUpdateTreeWorker::testStep3b() {
    // Step 3 : delete then recreate folder when sync is stopped (update tree is empty)
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id1111", NodeType::File,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 1/Dir 1.1/Dir 1.1.1/File 1.1.1.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id111", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 1/Dir 1.1/Dir 1.1.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id11", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 1/Dir 1.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id11b", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 1/Dir 1.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id111b", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 1/Dir 1.1/Dir 1.1.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id1111b", NodeType::File,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 1/Dir 1.1/Dir 1.1.1/File 1.1.1.1"));

    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->step3DeleteDirectory());
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->step4DeleteFile());
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->step5CreateDirectory());
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->step6CreateFile());
    CPPUNIT_ASSERT(_localUpdateTree->getNodeById("id11")->hasChangeEvent(OperationType::Delete));
    CPPUNIT_ASSERT(_localUpdateTree->getNodeById("id111")->hasChangeEvent(OperationType::Delete));
    CPPUNIT_ASSERT(_localUpdateTree->getNodeById("id11b")->hasChangeEvent(OperationType::Create));
    CPPUNIT_ASSERT(_localUpdateTree->getNodeById("id111b")->hasChangeEvent(OperationType::Create));
    CPPUNIT_ASSERT(_localUpdateTree->getNodeById("id1111b")->hasChangeEvent(OperationType::Edit)); // Delete + Create file => Edit
}

void TestUpdateTreeWorker::testStep4() {
    setUpUpdateTree(ReplicaSide::Local);

    // Step 4 :
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id4111", NodeType::File,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id41", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 4/Dir 4.1", "Dir 4/Dir 4.1*"));
    // Special delete create file
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id51", NodeType::File, testhelpers::defaultTime,
                                                          testhelpers::defaultTime, testhelpers::defaultFileSize,
                                                          "Dir 5/File 5.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id511", NodeType::File,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 5/File 5.1"));
    CPPUNIT_ASSERT_EQUAL(
            ExitCode::Ok,
            _localUpdateTreeWorker->step1MoveDirectory()); // Since we test with a renamed parent, step 1 has to be run.
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->step4DeleteFile());
    CPPUNIT_ASSERT(
            _localUpdateTree->getNodeByPath("Dir 4/Dir 4.1*/Dir 4.1.1/File 4.1.1.1")->hasChangeEvent(OperationType::Delete));
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 4/Dir 4.1*/Dir 4.1.1/File 4.1.1.1")->id() == "id4111");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 4/Dir 4.1*/Dir 4.1.1/File 4.1.1.1")->parentNode()->id() == "id411");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 4/Dir 4.1*/Dir 4.1.1/File 4.1.1.1")->modificationTime() ==
                   testhelpers::defaultTime);
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 5/File 5.1")->hasChangeEvent(OperationType::Edit));
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 5/File 5.1")->id() == "id511");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 5/File 5.1")->parentNode()->isTmp());
}

void TestUpdateTreeWorker::testStep5() {
    setUpUpdateTree(ReplicaSide::Local);

    // Step 5 :create for Dir
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id121", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 1/Dir 1.2/Dir 1.2.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "idX", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 1/Dir x"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id7", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 7"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id5", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 5"));
    // test step5CreateDirectory
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->step5CreateDirectory());
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 1.2.1")->id() == "id121");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 1.2.1")->hasChangeEvent(OperationType::Create));
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 1/Dir x")->id() == "idX");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 1.2.1")->hasChangeEvent(OperationType::Create));
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 1/Dir x")->parentNode()->children().size() >= 2);
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 7")->id() == "id7");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 7")->parentNode()->id() == _syncDb->rootNode().nodeIdLocal());
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 7")->children().empty());
}

void TestUpdateTreeWorker::testStep6() {
    setUpUpdateTree(ReplicaSide::Local);

    // Step 4 : delete files
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id4111", NodeType::File,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id52", NodeType::File, testhelpers::defaultTime,
                                                          testhelpers::defaultTime, testhelpers::defaultFileSize,
                                                          "Dir 5/File 5.2"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id15", NodeType::File, testhelpers::defaultTime,
                                                          testhelpers::defaultTime, testhelpers::defaultFileSize,
                                                          "Dir 7/File 1.5"));
    // special delete create file
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id51", NodeType::File, testhelpers::defaultTime,
                                                          testhelpers::defaultTime, testhelpers::defaultFileSize,
                                                          "Dir 5/File 5.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id51bis", NodeType::File,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 5/File 5.1"));
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->step4DeleteFile());
    // Step 6 : create files
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->step6CreateFile());
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 5/File 5.2")->id() == "id52");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 5/File 5.2")->parentNode()->isTmp());
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 5/File 5.2")->hasChangeEvent(OperationType::Create));
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 7/File 1.5")->id() == "id15");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 7/File 1.5")->parentNode()->isTmp());
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 7/File 1.5")->hasChangeEvent(OperationType::Create));
}

void TestUpdateTreeWorker::testStep7() {
    setUpUpdateTree(ReplicaSide::Local);

    // Step 7 : Edit
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Edit, "id4112", NodeType::File, testhelpers::defaultTime,
                                                          testhelpers::defaultTime, testhelpers::defaultFileSize,
                                                          "Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.2"));
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->step7EditFile());
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.2")->id() == "id4112");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.2")->parentNode()->id() == "id411");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.2")->hasChangeEvent(OperationType::Edit));
}

void TestUpdateTreeWorker::testStep8() {
    setUpUpdateTree(ReplicaSide::Local);

    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->step8CompleteUpdateTree());
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 5")->id() == "id5");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 5/File 5.1")->id() == "id51");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 1/Dir 1.1/File 1.1.2")->id() == "id112");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.2")->id() == "id4112");

    // Make sure we retrieve the NFD encoded version of the name
    CPPUNIT_ASSERT_EQUAL(std::string("id7l"), *_localUpdateTree->getNodeByPath(testhelpers::makeNfdSyncName())->id());
}

void TestUpdateTreeWorker::testStep8b() {
    // Moving a directory to a Dir1 will create tmpNode for Dir1
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id41", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 4/Dir 4.1", "Dir 1/Dir 4.1"));

    // Renaming Dir1 to Dir1b will create real node for Dir1
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id1", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 1", "Dir 1b"));

    // Ensure we have a duplicated Dir1 node in the tree (normal case), one is tmp, the other is not
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->step1MoveDirectory());
    CPPUNIT_ASSERT(_localUpdateTree->getNodeById("id41")->parentNode()->isTmp());
    CPPUNIT_ASSERT_EQUAL(std::string("Dir 1"), SyncName2Str(_localUpdateTree->getNodeById("id41")->parentNode()->name()));
    CPPUNIT_ASSERT(_localUpdateTree->nodes().contains("id1"));

    // Ensure the real Node has the origin node informations and the excpected name
    CPPUNIT_ASSERT(_localUpdateTree->getNodeById("id1")->moveOriginInfos().isValid());
    CPPUNIT_ASSERT_EQUAL(SyncPath("Dir 1"), _localUpdateTree->getNodeById("id1")->moveOriginInfos().path());
    CPPUNIT_ASSERT_EQUAL(std::string("Dir 1b"), SyncName2Str(_localUpdateTree->getNodeById("id1")->name()));

    // Ensure origin node info and name are not lost after step8
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->step8CompleteUpdateTree());

    CPPUNIT_ASSERT(!_localUpdateTree->getNodeById("id41")->parentNode()->isTmp());
    CPPUNIT_ASSERT_EQUAL(_localUpdateTree->getNodeById("id41")->parentNode(), _localUpdateTree->nodes().at("id1"));

    CPPUNIT_ASSERT_EQUAL(SyncPath("Dir 1"), _localUpdateTree->getNodeById("id1")->moveOriginInfos().path());
    CPPUNIT_ASSERT_EQUAL(std::string("Dir 1b"), SyncName2Str(_localUpdateTree->getNodeById("id1")->name()));
}

void TestUpdateTreeWorker::testClearTreeStep1() {
    // Step 1 : move into non-existing & existing folder
    _operationSet->insertOp(std::make_shared<FSOperation>(
            OperationType::Move, "id111", NodeType::Directory, testhelpers::defaultTime, testhelpers::defaultTime,
            testhelpers::defaultFileSize, "Dir 1/Dir 1.1/Dir 1.1.1", "Dir 1/Dir 1.2/Dir 1.2.1/Dir 1.1.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id3", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 3", "Dir 1/Dir 1.2/Dir 3"));
    // rename dir
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id11", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 1/Dir 1.1", "Dir 1/Dir 1.2"));

    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->step1MoveDirectory());
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 1.2.1/Dir 1.1.1")->id() == "id111");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 3")->id() == "id3");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 1/Dir 1.2")->id() == "id11");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 1/Dir 1.1") == nullptr);
}

void TestUpdateTreeWorker::testClearTreeStep2() {
    // Step 2 :Move files
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id1111", NodeType::File, testhelpers::defaultTime,
                                                          testhelpers::defaultTime, testhelpers::defaultFileSize,
                                                          "Dir 1/Dir 1.1/Dir 1.1.1/File 1.1.1.1", "Dir 1/File 1.1"));

    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->step2MoveFile());
    std::shared_ptr<Node> node = _localUpdateTree->getNodeByPath("Dir 1/File 1.1");
    CPPUNIT_ASSERT(node);
    CPPUNIT_ASSERT(node->hasChangeEvent(OperationType::Move));
    CPPUNIT_ASSERT(node->id() == "id1111");
    // tree has been cleared so parent node has temp data
    CPPUNIT_ASSERT(node->parentNode()->name() == Str("Dir 1"));
    CPPUNIT_ASSERT(node->parentNode()->isTmp());
    CPPUNIT_ASSERT(node->moveOriginInfos().path() == "Dir 1/Dir 1.1/Dir 1.1.1/File 1.1.1.1");
}

void TestUpdateTreeWorker::testClearTreeStep3() {
    // Step 3 : special delete case with move parent & delete child
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id3", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 3", "Dir 1/Dir 1.2/Dir 3"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id11", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 1/Dir 1.1", "Dir 1/Dir 1.2"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id3", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 1/Dir 1.2/Dir 3"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id2", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 2"));
    // make move dir to test special case
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->step1MoveDirectory());
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->step3DeleteDirectory());
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 3")->hasChangeEvent(OperationType::Move));
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 3")->hasChangeEvent(OperationType::Delete));
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 3")->id() == "id3");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 3")->parentNode()->id() == "id11");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 2")->id() == "id2");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 2")->hasChangeEvent(OperationType::Delete));
}

void TestUpdateTreeWorker::testClearTreeStep4() {
    // Step 4 :
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id4111", NodeType::File,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id41", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 4/Dir 4.1", "Dir 4/Dir 4.1*"));
    // special delete create file
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id51", NodeType::File, testhelpers::defaultTime,
                                                          testhelpers::defaultTime, testhelpers::defaultFileSize,
                                                          "Dir 5/File 5.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id51bis", NodeType::File,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 5/File 5.1"));
    CPPUNIT_ASSERT_EQUAL(
            ExitCode::Ok,
            _localUpdateTreeWorker->step1MoveDirectory()); // Since we test with a renamed parent, step 1 has to be run.
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->step4DeleteFile());
    CPPUNIT_ASSERT(
            _localUpdateTree->getNodeByPath("Dir 4/Dir 4.1*/Dir 4.1.1/File 4.1.1.1")->hasChangeEvent(OperationType::Delete));
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 4/Dir 4.1*/Dir 4.1.1/File 4.1.1.1")->id() == "id4111");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 4/Dir 4.1*/Dir 4.1.1/File 4.1.1.1")->parentNode()->isTmp());
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 4/Dir 4.1*/Dir 4.1.1/File 4.1.1.1")->modificationTime() ==
                   testhelpers::defaultTime);
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 5/File 5.1")->hasChangeEvent(OperationType::Edit));
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 5/File 5.1")->id() == "id51bis");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 5/File 5.1")->parentNode()->isTmp());
}

void TestUpdateTreeWorker::testClearTreeStep5() {
    // Step 5 :create for Dir
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id121", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 1/Dir 1.2/Dir 1.2.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "idX", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 1/Dir x"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id7", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 7"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id5", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 5"));
    // test step5CreateDirectory
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->step5CreateDirectory());
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 1.2.1")->id() == "id121");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 1.2.1")->hasChangeEvent(OperationType::Create));
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 1/Dir x")->id() == "idX");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 1/Dir 1.2/Dir 1.2.1")->hasChangeEvent(OperationType::Create));
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 1/Dir x")->parentNode()->children().size() >= 2);
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 7")->id() == "id7");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 7")->children().size() == 0);
}

void TestUpdateTreeWorker::testClearTreeStep6() {
    // Step 4 : delete files
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id4111", NodeType::File,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id52", NodeType::File, testhelpers::defaultTime,
                                                          testhelpers::defaultTime, testhelpers::defaultFileSize,
                                                          "Dir 5/File 5.2"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id15", NodeType::File, testhelpers::defaultTime,
                                                          testhelpers::defaultTime, testhelpers::defaultFileSize,
                                                          "Dir 7/File 1.5"));
    // special delete create file
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id51", NodeType::File, testhelpers::defaultTime,
                                                          testhelpers::defaultTime, testhelpers::defaultFileSize,
                                                          "Dir 5/File 5.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id51bis", NodeType::File,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 5/File 5.1"));

    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->step4DeleteFile());
    // Step 6 : create files
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->step6CreateFile());
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 5/File 5.2")->id() == "id52");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 5/File 5.2")->parentNode()->isTmp());
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 5/File 5.2")->hasChangeEvent(OperationType::Create));
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 7/File 1.5")->id() == "id15");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 7/File 1.5")->parentNode()->isTmp());
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 7/File 1.5")->hasChangeEvent(OperationType::Create));
}

void TestUpdateTreeWorker::testClearTreeStep7() {
    // Step 7 :
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Edit, "id4112", NodeType::File, testhelpers::defaultTime,
                                                          testhelpers::defaultTime, testhelpers::defaultFileSize,
                                                          "Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.2"));

    // test step7EditFile
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->step7EditFile());
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.2")->id() == "id4112");
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.2")->parentNode()->isTmp());
    CPPUNIT_ASSERT(_localUpdateTree->getNodeByPath("Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.2")->hasChangeEvent(OperationType::Edit));
}

void TestUpdateTreeWorker::testClearTreeStep8() {
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->step8CompleteUpdateTree());
    CPPUNIT_ASSERT(_localUpdateTreeWorker->_updateTree->nodes().size() == 19);
}

void TestUpdateTreeWorker::testGetOriginPath() {
    setUpUpdateTree(ReplicaSide::Local);

    // Test without move operation
    std::shared_ptr<Node> node = _localUpdateTree->getNodeById("id4111");
    SyncPath path;
    CPPUNIT_ASSERT(_localUpdateTreeWorker->getOriginPath(node, path) == ExitCode::Ok);
    CPPUNIT_ASSERT(path == "Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1");
    CPPUNIT_ASSERT(node->getPath() == "Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1");

    // Test with move operation on the child
    node->setMoveOriginInfos({"Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1", "id411"});
    node->insertChangeEvent(OperationType::Move);
    CPPUNIT_ASSERT(node->setParentNode(_localUpdateTree->getNodeById("id4"))); // Move node 4111 under parent 4
    node->setName(Str("File 4.1.1.1 renamed")); // Rename node
    CPPUNIT_ASSERT(_localUpdateTreeWorker->getOriginPath(node, path) == ExitCode::Ok);
    CPPUNIT_ASSERT(path == "Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1");
    CPPUNIT_ASSERT(node->getPath() == "Dir 4/File 4.1.1.1 renamed");
}

void TestUpdateTreeWorker::testGetOriginPath2() {
    setUpUpdateTree(ReplicaSide::Local);

    // Test with move operation on some parents
    std::shared_ptr<Node> node = _localUpdateTree->getNodeById("id411");
    node->setMoveOriginInfos({"Dir 4/Dir 4.1/Dir 4.1.1", "id41"});
    node->insertChangeEvent(OperationType::Move);
    CPPUNIT_ASSERT(node->setParentNode(_localUpdateTree->getNodeById("id4"))); // Move node 411 under parent 4
    node->setName(Str("Dir 4.1.1 renamed")); // Rename node

    SyncPath path;
    CPPUNIT_ASSERT(_localUpdateTreeWorker->getOriginPath(node, path) == ExitCode::Ok);
    CPPUNIT_ASSERT(path == "Dir 4/Dir 4.1/Dir 4.1.1");
    CPPUNIT_ASSERT(node->getPath() == "Dir 4/Dir 4.1.1 renamed");

    node = _localUpdateTree->getNodeById("id4111");
    CPPUNIT_ASSERT(_localUpdateTreeWorker->getOriginPath(node, path) == ExitCode::Ok);
    CPPUNIT_ASSERT(path == "Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1");
    CPPUNIT_ASSERT(node->getPath() == "Dir 4/Dir 4.1.1 renamed/File 4.1.1.1");
}

void TestUpdateTreeWorker::testGetOriginPath3() {
    setUpUpdateTree(ReplicaSide::Local);

    // Test with move operation on parent AND child (rename children THEN move parent)
    std::shared_ptr<Node> node4111 = _localUpdateTree->getNodeById("id4111");
    std::shared_ptr<Node> node411 = _localUpdateTree->getNodeById("id411");

    node4111->setMoveOriginInfos({"Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1", "id411"});
    node4111->insertChangeEvent(OperationType::Move);
    node4111->setName(Str("File 4.1.1.1 renamed")); // Rename node

    node411->setMoveOriginInfos({"Dir 4/Dir 4.1/Dir 4.1.1", "id41"});
    node411->insertChangeEvent(OperationType::Move);
    CPPUNIT_ASSERT(node411->setParentNode(_localUpdateTree->getNodeById("id4"))); // Move node 411 under parent 4

    SyncPath path;
    CPPUNIT_ASSERT(_localUpdateTreeWorker->getOriginPath(node4111, path) == ExitCode::Ok);
    CPPUNIT_ASSERT(path == "Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1");
    CPPUNIT_ASSERT(node4111->getPath() == "Dir 4/Dir 4.1.1/File 4.1.1.1 renamed");
}

void TestUpdateTreeWorker::testGetOriginPath4() {
    setUpUpdateTree(ReplicaSide::Local);

    // Test with move operation on parent AND child (move parent THEN rename children)
    std::shared_ptr<Node> node4111 = _localUpdateTree->getNodeById("id4111");
    std::shared_ptr<Node> node411 = _localUpdateTree->getNodeById("id411");

    node411->setMoveOriginInfos({"Dir 4/Dir 4.1/Dir 4.1.1", "id41"});
    node411->insertChangeEvent(OperationType::Move);
    CPPUNIT_ASSERT(node411->setParentNode(_localUpdateTree->getNodeById("id4"))); // Move node 411 under parent 4

    node4111->setMoveOriginInfos({"Dir 4/Dir 4.1.1/File 4.1.1.1", "id411"});
    node4111->insertChangeEvent(OperationType::Move);
    node4111->setName(Str("File 4.1.1.1 renamed")); // Rename node

    SyncPath path;
    CPPUNIT_ASSERT(_localUpdateTreeWorker->getOriginPath(node4111, path) == ExitCode::Ok);
    CPPUNIT_ASSERT(path == "Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1");
    CPPUNIT_ASSERT(node4111->getPath() == "Dir 4/Dir 4.1.1/File 4.1.1.1 renamed");
}

void TestUpdateTreeWorker::testGetOriginPath5() {
    setUpUpdateTree(ReplicaSide::Local);
    setUpUpdateTree(ReplicaSide::Remote);

    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, _localUpdateTreeWorker->_side);
    std::shared_ptr<Node> nodeFile7l = _localUpdateTree->getNodeById("id7l");
    SyncPath path;
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _localUpdateTreeWorker->getOriginPath(nodeFile7l, path));
    CPPUNIT_ASSERT_EQUAL(SyncPath(nodeFile7l->name()), path);

    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Remote, _remoteUpdateTreeWorker->_side);
    std::shared_ptr<Node> nodeFile7r = _remoteUpdateTree->getNodeById("id7r");
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, _remoteUpdateTreeWorker->getOriginPath(nodeFile7r, path));
    CPPUNIT_ASSERT_EQUAL(SyncPath(nodeFile7r->name()), path);
}

void TestUpdateTreeWorker::testDeleteMove() {
    setUpUpdateTree(ReplicaSide::Local);

    /**
     *  - Delete 6a
     *  - Rename 6 into 6a
     *  - Create 6
     */
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id6a", NodeType::File, testhelpers::defaultTime,
                                                          testhelpers::defaultTime, testhelpers::defaultFileSize));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Move, "id6", NodeType::File, testhelpers::defaultTime,
                                                          testhelpers::defaultTime, testhelpers::defaultFileSize, "File 6",
                                                          "File 6a"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id6b", NodeType::File, testhelpers::defaultTime,
                                                          testhelpers::defaultTime, testhelpers::defaultFileSize, "File 6"));

    _localUpdateTreeWorker->execute();

    auto node6 = _localUpdateTree->getNodeById("id6");
    auto node6a = _localUpdateTree->getNodeById("id6a");
    auto node6b = _localUpdateTree->getNodeById("id6b");
    CPPUNIT_ASSERT(node6->parentNode() == _localUpdateTree->rootNode());
    CPPUNIT_ASSERT(_localUpdateTree->rootNode()->children().find("id6") != _localUpdateTree->rootNode()->children().end());
    CPPUNIT_ASSERT(node6->name() == Str("File 6a"));

    CPPUNIT_ASSERT(node6a->parentNode() == _localUpdateTree->rootNode());
    CPPUNIT_ASSERT(_localUpdateTree->rootNode()->children().find("id6a") != _localUpdateTree->rootNode()->children().end());
    CPPUNIT_ASSERT(node6a->name() == Str("File 6a"));

    CPPUNIT_ASSERT(node6b->parentNode() == _localUpdateTree->rootNode());
    CPPUNIT_ASSERT(_localUpdateTree->rootNode()->children().find("id6b") != _localUpdateTree->rootNode()->children().end());
    CPPUNIT_ASSERT(node6b->name() == Str("File 6"));
}

void TestUpdateTreeWorker::testDeleteRecreateBranch() {
    setUpUpdateTree(ReplicaSide::Local);

    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id1", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id11", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id111", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Delete, "id1111", NodeType::File,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize));

    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id1_new", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id11_new", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 1/Dir 1.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id111_new", NodeType::Directory,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 1/Dir 1.1/Dir 1.1.1"));
    _operationSet->insertOp(std::make_shared<FSOperation>(OperationType::Create, "id1111_new", NodeType::File,
                                                          testhelpers::defaultTime, testhelpers::defaultTime,
                                                          testhelpers::defaultFileSize, "Dir 1/Dir 1.1/Dir 1.1.1/File 1.1.1.1"));

    _localUpdateTreeWorker->execute();

    auto node1 = _localUpdateTree->getNodeById("id1");
    CPPUNIT_ASSERT(node1->hasChangeEvent(OperationType::Delete));
    CPPUNIT_ASSERT(!node1->hasChangeEvent(OperationType::Create));
    auto node11 = _localUpdateTree->getNodeById("id11");
    CPPUNIT_ASSERT(node11->hasChangeEvent(OperationType::Delete));
    CPPUNIT_ASSERT(!node11->hasChangeEvent(OperationType::Create));
    auto node111 = _localUpdateTree->getNodeById("id111");
    CPPUNIT_ASSERT(node111->hasChangeEvent(OperationType::Delete));
    CPPUNIT_ASSERT(!node111->hasChangeEvent(OperationType::Create));
    auto node1111 = _localUpdateTree->getNodeById("id1111");
    CPPUNIT_ASSERT(node1111->hasChangeEvent(OperationType::Delete));
    CPPUNIT_ASSERT(!node1111->hasChangeEvent(OperationType::Create));
    CPPUNIT_ASSERT(node1111->parentNode() == node111);

    auto node1new = _localUpdateTree->getNodeById("id1_new");
    CPPUNIT_ASSERT(!node1new->hasChangeEvent(OperationType::Delete));
    CPPUNIT_ASSERT(node1new->hasChangeEvent(OperationType::Create));
    auto node11new = _localUpdateTree->getNodeById("id11_new");
    CPPUNIT_ASSERT(!node11new->hasChangeEvent(OperationType::Delete));
    CPPUNIT_ASSERT(node11new->hasChangeEvent(OperationType::Create));
    auto node111new = _localUpdateTree->getNodeById("id111_new");
    CPPUNIT_ASSERT(!node111new->hasChangeEvent(OperationType::Delete));
    CPPUNIT_ASSERT(node111new->hasChangeEvent(OperationType::Create));
    auto node1111new = _localUpdateTree->getNodeById("id1111_new");
    CPPUNIT_ASSERT(!node1111new->hasChangeEvent(OperationType::Delete));
    CPPUNIT_ASSERT(node1111new->hasChangeEvent(OperationType::Create));
    CPPUNIT_ASSERT(node1111new->parentNode() == node111new);
}

} // namespace KDC
