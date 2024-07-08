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

using namespace CppUnit;

namespace KDC {

static const SyncTime defaultTime = 1654788079;
static const int64_t defaultSize = 1654788079;

void TestExecutorWorker::setUp() {
    // Create SyncPal
    bool alreadyExists = false;
    std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExists, true);
    ParmsDb::instance(parmsDbPath, "3.4.0", true, true);

    SyncPath syncDbPath = Db::makeDbName(1, 1, 1, 1, alreadyExists);
    std::filesystem::remove(syncDbPath);
    _syncPal = std::make_shared<SyncPal>(syncDbPath, "3.4.0", true);
    _testObj = std::make_shared<ExecutorWorker>(_syncPal, "name", "short name");

    _syncPal->_syncDb->setAutoDelete(true);

    /**
     * Initial FS state:
     *
     *            root
     *             |
     *             A
     *        _____|______
     *       |           |
     *      AA          AB
     *      |
     *     AAA
     */

    // Setup DB
    DbNodeId dbNodeIdA;
    DbNodeId dbNodeIdAA;
    DbNodeId dbNodeIdAB;
    DbNodeId dbNodeIdAAA;

    bool constraintError = false;
    DbNode dbNodeA(0, _syncPal->_syncDb->rootNode().nodeId(), Str("A"), Str("A"), "lA", "rA", defaultTime, defaultTime,
                   defaultTime, NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(dbNodeA, dbNodeIdA, constraintError);
    DbNode dbNodeAA(0, dbNodeIdA, Str("AA"), Str("AA"), "lAA", "rAA", defaultTime, defaultTime, defaultTime,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(dbNodeAA, dbNodeIdAA, constraintError);
    DbNode dbNodeAB(0, dbNodeIdA, Str("AB"), Str("AB"), "lAB", "rAB", defaultTime, defaultTime, defaultTime,
                    NodeType::NodeTypeDirectory, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(dbNodeAB, dbNodeIdAB, constraintError);
    DbNode dbNodeAAA(0, dbNodeIdAA, Str("AAA"), Str("AAA"), "lAAA", "rAAA", defaultTime, defaultTime, defaultTime,
                     NodeType::NodeTypeFile, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(dbNodeAAA, dbNodeIdAAA, constraintError);

    // Build update trees
    std::shared_ptr<Node> lNodeA =
        std::make_shared<Node>(dbNodeIdA, _syncPal->_localUpdateTree->side(), Str("A"), NodeTypeDirectory, OperationTypeNone,
                               "lA", defaultTime, defaultTime, defaultSize, _syncPal->_localUpdateTree->rootNode());
    _syncPal->_localUpdateTree->rootNode()->insertChildren(lNodeA);
    _syncPal->_localUpdateTree->insertNode(lNodeA);
    std::shared_ptr<Node> lNodeAA =
        std::make_shared<Node>(dbNodeIdAA, _syncPal->_localUpdateTree->side(), Str("AA"), NodeTypeDirectory, OperationTypeNone,
                               "lAA", defaultTime, defaultTime, defaultSize, lNodeA);
    lNodeA->insertChildren(lNodeAA);
    _syncPal->_localUpdateTree->insertNode(lNodeAA);
    std::shared_ptr<Node> lNodeAB =
        std::make_shared<Node>(dbNodeIdAB, _syncPal->_localUpdateTree->side(), Str("AB"), NodeTypeDirectory, OperationTypeNone,
                               "lAB", defaultTime, defaultTime, defaultSize, lNodeA);
    lNodeA->insertChildren(lNodeAB);
    _syncPal->_localUpdateTree->insertNode(lNodeAB);
    std::shared_ptr<Node> lNodeAAA =
        std::make_shared<Node>(dbNodeIdAAA, _syncPal->_localUpdateTree->side(), Str("AAA"), NodeTypeFile, OperationTypeNone,
                               "lAAA", defaultTime, defaultTime, defaultSize, lNodeAA);
    lNodeAA->insertChildren(lNodeAAA);
    _syncPal->_localUpdateTree->insertNode(lNodeAAA);

    std::shared_ptr<Node> rNodeA =
        std::make_shared<Node>(dbNodeIdA, _syncPal->_remoteUpdateTree->side(), Str("A"), NodeTypeDirectory, OperationTypeNone,
                               "rA", defaultTime, defaultTime, defaultSize, _syncPal->_remoteUpdateTree->rootNode());
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(rNodeA);
    _syncPal->_remoteUpdateTree->insertNode(rNodeA);
    std::shared_ptr<Node> rNodeAA =
        std::make_shared<Node>(dbNodeIdAA, _syncPal->_remoteUpdateTree->side(), Str("AA"), NodeTypeDirectory, OperationTypeNone,
                               "rAA", defaultTime, defaultTime, defaultSize, rNodeA);
    rNodeA->insertChildren(rNodeAA);
    _syncPal->_remoteUpdateTree->insertNode(rNodeAA);
    std::shared_ptr<Node> rNodeAB =
        std::make_shared<Node>(dbNodeIdAB, _syncPal->_remoteUpdateTree->side(), Str("AB"), NodeTypeDirectory, OperationTypeNone,
                               "rAB", defaultTime, defaultTime, defaultSize, rNodeA);
    rNodeA->insertChildren(rNodeAB);
    _syncPal->_remoteUpdateTree->insertNode(rNodeAB);
    std::shared_ptr<Node> rNodeAAA =
        std::make_shared<Node>(dbNodeIdAAA, _syncPal->_remoteUpdateTree->side(), Str("AAA"), NodeTypeFile, OperationTypeNone,
                               "rAAA", defaultTime, defaultTime, defaultSize, rNodeAA);
    rNodeAA->insertChildren(rNodeAAA);
    _syncPal->_remoteUpdateTree->insertNode(rNodeAAA);
}

void TestExecutorWorker::tearDown() {
    _testObj->cancelAllOngoingJobs();
    _syncPal->stop(false, true, false);
    _testObj.reset();
    _syncPal.reset();
    ParmsDb::reset();
}

void TestExecutorWorker::testAffectedUpdateTree() {
    // Normal cases
    auto syncOp = std::make_shared<SyncOperation>();
    syncOp->setTargetSide(ReplicaSideLocal);
    CPPUNIT_ASSERT_EQUAL(_syncPal->_remoteUpdateTree, _testObj->affectedUpdateTree(syncOp));

    syncOp->setTargetSide(ReplicaSideRemote);
    CPPUNIT_ASSERT_EQUAL(_syncPal->_localUpdateTree, _testObj->affectedUpdateTree(syncOp));

    // ReplicaSideUnknown case
    syncOp->setTargetSide(ReplicaSideUnknown);
    CPPUNIT_ASSERT_EQUAL(std::shared_ptr<UpdateTree>(nullptr), _testObj->affectedUpdateTree(syncOp));
}

void TestExecutorWorker::testTargetUpdateTree() {
    // Normal cases
    auto syncOp = std::make_shared<SyncOperation>();
    syncOp->setTargetSide(ReplicaSideLocal);
    CPPUNIT_ASSERT_EQUAL(_syncPal->_localUpdateTree, _testObj->targetUpdateTree(syncOp));

    syncOp->setTargetSide(ReplicaSideRemote);
    CPPUNIT_ASSERT_EQUAL(_syncPal->_remoteUpdateTree, _testObj->targetUpdateTree(syncOp));

    // ReplicaSideUnknown case
    syncOp->setTargetSide(ReplicaSideUnknown);
    CPPUNIT_ASSERT_EQUAL(std::shared_ptr<UpdateTree>(nullptr), _testObj->targetUpdateTree(syncOp));
}
}  // namespace KDC
