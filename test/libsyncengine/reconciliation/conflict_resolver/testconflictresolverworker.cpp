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

#include "testconflictresolverworker.h"
#include "requests/parameterscache.h"
#include "reconciliation/platform_inconsistency_checker/platforminconsistencycheckerutility.h"

#include <memory>

namespace KDC {

void TestConflictResolverWorker::setUp() {
    // Create SyncPal
    bool alreadyExists = false;
    std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExists, true);
    ParmsDb::instance(parmsDbPath, "3.4.0", true, true);

    SyncPath syncDbPath = Db::makeDbName(1, 1, 1, 1, alreadyExists);
    std::filesystem::remove(syncDbPath);
    _syncPal = std::make_shared<SyncPal>(syncDbPath, "3.4.0", true);
    _syncPal->_syncDb->setAutoDelete(true);

    _syncPal->_conflictResolverWorker = std::make_shared<ConflictResolverWorker>(_syncPal, "Conflict Resolver", "CORE");

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

void TestConflictResolverWorker::tearDown() {
    ParmsDb::instance()->close();
    ParmsDb::reset();
    if (_syncPal && _syncPal->_syncDb) {
        _syncPal->_syncDb->close();
    }
}

void TestConflictResolverWorker::testCreateCreate() {
    // Simulate file creation on both replica
    std::shared_ptr<Node> lNodeAA = _syncPal->_localUpdateTree->getNodeById("lAA");
    std::shared_ptr<Node> lNodeAAB =
        std::make_shared<Node>(std::nullopt, _syncPal->_localUpdateTree->side(), Str("AAB.txt"), NodeTypeFile,
                               OperationTypeCreate, "lAAB", defaultTime, defaultTime, defaultSize, lNodeAA);
    lNodeAA->insertChildren(lNodeAAB);
    _syncPal->_localUpdateTree->insertNode(lNodeAAB);
    _syncPal->_localSnapshot->updateItem(
        SnapshotItem("lAAB", "lAA", Str("AAB.txt"), defaultTime, defaultTime, NodeTypeFile, 123, "lcs1"));

    std::shared_ptr<Node> rNodeAA = _syncPal->_remoteUpdateTree->getNodeById("rAA");
    std::shared_ptr<Node> rNodeAAB =
        std::make_shared<Node>(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("AAB.txt"), NodeTypeFile,
                               OperationTypeCreate, "rAAB", defaultTime, defaultTime, defaultSize, rNodeAA);
    rNodeAA->insertChildren(rNodeAAB);
    _syncPal->_remoteUpdateTree->insertNode(rNodeAAB);
    _syncPal->_remoteSnapshot->updateItem(
        SnapshotItem("rAAB", "rAA", Str("AAB.txt"), defaultTime, defaultTime, NodeTypeFile, 123, "rcs1"));

    Conflict conflict(lNodeAAB, rNodeAA, ConflictTypeCreateCreate);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() == 1);
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(!op->newName().empty());
    CPPUNIT_ASSERT(op->targetSide() == ReplicaSideLocal);
    CPPUNIT_ASSERT(op->type() == OperationTypeMove);
}

void TestConflictResolverWorker::testEditEdit() {
    // Simulate edit conflict of file AAA on both replica
    std::shared_ptr<Node> lNodeAAA = _syncPal->_localUpdateTree->getNodeById("lAAA");
    lNodeAAA->setChangeEvents(OperationTypeEdit);
    std::shared_ptr<Node> rNodeAAA = _syncPal->_remoteUpdateTree->getNodeById("rAAA");
    rNodeAAA->setChangeEvents(OperationTypeEdit);
    Conflict conflict(lNodeAAA, rNodeAAA, ConflictTypeEditEdit);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() == 1);
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(!op->newName().empty());
    CPPUNIT_ASSERT(op->targetSide() == ReplicaSideLocal);
    CPPUNIT_ASSERT(op->type() == OperationTypeMove);
}

void TestConflictResolverWorker::testMoveCreate() {
    // Simulate create file ABA in AB on local replica
    std::shared_ptr<Node> lNodeAB = _syncPal->_localUpdateTree->getNodeById("lAB");
    std::shared_ptr<Node> lNodeABA =
        std::make_shared<Node>(std::nullopt, _syncPal->_localUpdateTree->side(), Str("ABA"), NodeTypeFile, OperationTypeCreate,
                               "lABA", defaultTime, defaultTime, defaultSize, lNodeAB);
    lNodeAB->insertChildren(lNodeABA);
    _syncPal->_localUpdateTree->insertNode(lNodeABA);

    // Simulate move of file AAA in AA to ABA in AB on remote replica
    std::shared_ptr<Node> rNodeAA = _syncPal->_remoteUpdateTree->getNodeById("rAA");
    std::shared_ptr<Node> rNodeAAA = _syncPal->_remoteUpdateTree->getNodeById("rAAA");
    std::shared_ptr<Node> rNodeAB = _syncPal->_remoteUpdateTree->getNodeById("rAB");
    std::shared_ptr<Node> rNodeABA = rNodeAAA;

    rNodeABA->setChangeEvents(OperationTypeMove);
    rNodeABA->setMoveOrigin(rNodeABA->getPath());
    rNodeABA->setMoveOriginParentDbId(rNodeAA->idb());
    rNodeABA->setParentNode(rNodeAB);
    rNodeABA->setName(Str("ABA"));

    Conflict conflict(lNodeABA, rNodeABA, ConflictTypeMoveCreate);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() == 1);
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(!op->newName().empty());
    CPPUNIT_ASSERT(op->targetSide() == ReplicaSideLocal);
    CPPUNIT_ASSERT(op->type() == OperationTypeMove);
}

void TestConflictResolverWorker::testEditDelete1() {
    // Simulate edit of file AAA on local replica
    std::shared_ptr<Node> lNodeAAA = _syncPal->_localUpdateTree->getNodeById("lAAA");
    lNodeAAA->setChangeEvents(OperationTypeEdit);

    // and delete of file AAA on remote replica
    std::shared_ptr<Node> rNodeAA = _syncPal->_remoteUpdateTree->getNodeById("rAA");
    std::shared_ptr<Node> rNodeAAA = _syncPal->_remoteUpdateTree->getNodeById("rAAA");
    rNodeAAA->setChangeEvents(OperationTypeDelete);
    rNodeAA->deleteChildren(rNodeAAA);
    _syncPal->_remoteUpdateTree->insertNode(rNodeAAA);

    Conflict conflict(lNodeAAA, rNodeAAA, ConflictTypeEditDelete);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() == 1);
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(op->newName().empty());
    CPPUNIT_ASSERT(op->omit());
    CPPUNIT_ASSERT(op->type() == OperationTypeDelete);
}

void TestConflictResolverWorker::testEditDelete2() {
    // Simulate edit of file AAA on local replica
    std::shared_ptr<Node> lNodeAAA = _syncPal->_localUpdateTree->getNodeById("lAAA");
    lNodeAAA->setChangeEvents(OperationTypeEdit);

    // and delete of dir AA (and all children) on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->_remoteUpdateTree->getNodeById("rA");
    std::shared_ptr<Node> rNodeAA = _syncPal->_remoteUpdateTree->getNodeById("rAA");
    std::shared_ptr<Node> rNodeAAA = _syncPal->_remoteUpdateTree->getNodeById("rAAA");
    rNodeAA->setChangeEvents(OperationTypeDelete);
    rNodeAAA->setChangeEvents(OperationTypeDelete);
    rNodeA->deleteChildren(rNodeAA);
    rNodeAA->deleteChildren(rNodeAAA);

    Conflict conflict(lNodeAAA, rNodeAAA, ConflictTypeEditDelete);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() == 2);
    for (const auto &opId : _syncPal->_syncOps->opSortedList()) {
        SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
        if (op->type() == OperationTypeMove) {
            CPPUNIT_ASSERT(!op->newName().empty());
            CPPUNIT_ASSERT(op->newParentNode() == _syncPal->_remoteUpdateTree->rootNode());
            CPPUNIT_ASSERT(op->omit() == false);
            CPPUNIT_ASSERT(op->affectedNode() == rNodeAAA);
        } else if (op->type() == OperationTypeDelete) {
            CPPUNIT_ASSERT(op->omit() == true);
            CPPUNIT_ASSERT(op->affectedNode() == rNodeAAA);
        } else {
            CPPUNIT_ASSERT(false);  // Should not happen
        }
    }
}

void TestConflictResolverWorker::testMoveDelete1() {
    // Simulate rename of node A to B on local replica
    std::shared_ptr<Node> lNodeA = _syncPal->_localUpdateTree->getNodeById("lA");
    lNodeA->setMoveOrigin(lNodeA->getPath());
    lNodeA->setMoveOriginParentDbId(_syncPal->_localUpdateTree->rootNode()->idb());
    lNodeA->setName(Str("B"));
    lNodeA->setChangeEvents(OperationTypeMove);

    // Simulate a delete of node AB on local replica
    std::shared_ptr<Node> lNodeAB = _syncPal->_localUpdateTree->getNodeById("lAB");
    lNodeAB->setChangeEvents(OperationTypeDelete);
    lNodeA->deleteChildren(lNodeA);

    // Simulate a delete of node A on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->_remoteUpdateTree->getNodeById("rA");
    rNodeA->setChangeEvents(OperationTypeDelete);
    _syncPal->_remoteUpdateTree->rootNode()->deleteChildren(rNodeA);

    Conflict conflict(lNodeA, rNodeA, ConflictTypeMoveDelete);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    /**
     * Desired final FS state:
     *
     *     root
     *      |
     *      B
     *      |
     *     AA
     *      |
     *    AAA
     */

    // Only 1 delete operation should be generated only,
    // changes to be done in db only
    // and on the remote replica only
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(_syncPal->_syncOps->size() == 1);
    CPPUNIT_ASSERT(op->omit() == true);
    CPPUNIT_ASSERT(op->type() == OperationTypeDelete);
}

void TestConflictResolverWorker::testMoveDelete2() {
    // Simulate rename of node A to B on local replica
    std::shared_ptr<Node> lNodeA = _syncPal->_localUpdateTree->getNodeById("lA");
    lNodeA->setMoveOrigin(lNodeA->getPath());
    lNodeA->setMoveOriginParentDbId(_syncPal->_localUpdateTree->rootNode()->idb());
    lNodeA->setName(Str("B"));
    lNodeA->setChangeEvents(OperationTypeMove);

    // Simulate edit of node AAA on local replica
    std::shared_ptr<Node> lNodeAAA = _syncPal->_localUpdateTree->getNodeById("lAAA");
    lNodeAAA->setChangeEvents(OperationTypeEdit);

    // Simulate create of node ABA on local replica
    std::shared_ptr<Node> lNodeAB = _syncPal->_localUpdateTree->getNodeById("lAB");
    std::shared_ptr<Node> lNodeABA =
        std::make_shared<Node>(std::nullopt, _syncPal->_localUpdateTree->side(), Str("ABA"), NodeTypeFile, OperationTypeCreate,
                               "lABA", defaultTime, defaultTime, defaultSize, lNodeAB);
    lNodeAB->insertChildren(lNodeABA);
    _syncPal->_localUpdateTree->insertNode(lNodeABA);

    // Simulate a delete of node A on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->_remoteUpdateTree->getNodeById("rA");
    rNodeA->setChangeEvents(OperationTypeDelete);
    _syncPal->_remoteUpdateTree->rootNode()->deleteChildren(rNodeA);

    Conflict conflict(lNodeA, rNodeA, ConflictTypeMoveDelete);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    /**
     * Desired final FS state:
     *
     *            root
     *             |
     *             B
     *        _____|______
     *       |           |
     *      AA          AB
     *      |           |
     *     AAA'        ABA
     */

    // Only 1 delete operation should be generated only,
    // changes to be done in db only
    // and on the remote replica only
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(_syncPal->_syncOps->size() == 1);
    CPPUNIT_ASSERT(op->omit() == true);
    CPPUNIT_ASSERT(op->type() == OperationTypeDelete);
}

void TestConflictResolverWorker::testMoveDelete3() {
    // Simulate rename of node A to B on local replica
    std::shared_ptr<Node> lNodeA = _syncPal->_localUpdateTree->getNodeById("lA");
    lNodeA->setMoveOrigin(lNodeA->getPath());
    lNodeA->setMoveOriginParentDbId(_syncPal->_localUpdateTree->rootNode()->idb());
    lNodeA->setName(Str("B"));
    lNodeA->setChangeEvents(OperationTypeMove);

    // Simulate move of node AB under root on local replica
    std::shared_ptr<Node> lNodeAB = _syncPal->_localUpdateTree->getNodeById("lAB");
    lNodeAB->setMoveOrigin(lNodeAB->getPath());
    lNodeAB->setMoveOriginParentDbId(lNodeA->idb());
    lNodeAB->setChangeEvents(OperationTypeMove);
    lNodeAB->setParentNode(_syncPal->_localUpdateTree->rootNode());
    lNodeA->deleteChildren(lNodeAB);
    _syncPal->_localUpdateTree->rootNode()->insertChildren(lNodeAB);

    // Simulate a delete of node A on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->_remoteUpdateTree->getNodeById("rA");
    rNodeA->setChangeEvents(OperationTypeDelete);
    _syncPal->_remoteUpdateTree->rootNode()->deleteChildren(rNodeA);

    Conflict conflict(lNodeA, rNodeA, ConflictTypeMoveDelete);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    /**
     * Desired final FS state:
     *
     *         root
     *      ____|____
     *     |        |
     *     B       AB'
     *     |
     *    AA
     *    |
     *   AAA
     */

    // Only 1 delete operation should be generated only,
    // changes to be done in db only
    // and on the remote replica only
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(_syncPal->_syncOps->size() == 1);
    CPPUNIT_ASSERT(op->omit() == true);
    CPPUNIT_ASSERT(op->type() == OperationTypeDelete);
}

void TestConflictResolverWorker::testMoveDelete4() {
    // Simulate rename of node A to B on local replica
    std::shared_ptr<Node> lNodeA = _syncPal->_localUpdateTree->getNodeById("lA");
    lNodeA->setMoveOrigin(lNodeA->getPath());
    lNodeA->setMoveOriginParentDbId(_syncPal->_localUpdateTree->rootNode()->idb());
    lNodeA->setName(Str("B"));
    lNodeA->setChangeEvents(OperationTypeMove);

    // Simulate move of node AB under root on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->_remoteUpdateTree->getNodeById("rA");
    std::shared_ptr<Node> rNodeAB = _syncPal->_remoteUpdateTree->getNodeById("rAB");
    rNodeAB->setChangeEvents(OperationTypeMove);
    rNodeAB->setMoveOrigin(rNodeAB->getPath());
    rNodeAB->setMoveOriginParentDbId(rNodeA->idb());
    rNodeAB->setParentNode(_syncPal->_remoteUpdateTree->rootNode());
    rNodeA->deleteChildren(rNodeAB);
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(rNodeAB);

    // Simulate a delete of node A on remote replica
    rNodeA->setChangeEvents(OperationTypeDelete);
    _syncPal->_remoteUpdateTree->rootNode()->deleteChildren(rNodeA);

    Conflict conflict(lNodeA, rNodeA, ConflictTypeMoveDelete);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    /**
     * Desired final FS state:
     *
     *          root
     *      _____|_____
     *     |          |
     *     B         AB'
     *     |
     *     AA
     *     |
     *    AAA
     */

    // Should have 1 move (orphan node) and 1 delete operation,
    // changes to be done in db only for both op
    // and on the remote replica only
    CPPUNIT_ASSERT_EQUAL((size_t)2, _syncPal->_syncOps->size());
    CPPUNIT_ASSERT(!_syncPal->_conflictResolverWorker->registeredOrphans().empty());
    for (const auto &opId : _syncPal->_syncOps->opSortedList()) {
        SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
        CPPUNIT_ASSERT(op->omit());

        if (op->type() == OperationTypeMove) {
            CPPUNIT_ASSERT(!op->newName().empty());
            CPPUNIT_ASSERT(op->newParentNode() == _syncPal->_remoteUpdateTree->rootNode());
            CPPUNIT_ASSERT(op->affectedNode() == rNodeAB);
        } else if (op->type() == OperationTypeDelete) {
            CPPUNIT_ASSERT(op->affectedNode() == rNodeA);
        } else {
            CPPUNIT_ASSERT(false);  // Should not happen
        }
    }
}

void TestConflictResolverWorker::testMoveDelete5() {
    // Simulate rename of node AA to AA' on local replica
    std::shared_ptr<Node> lNodeA = _syncPal->_localUpdateTree->getNodeById("lA");
    std::shared_ptr<Node> lNodeAA = _syncPal->_localUpdateTree->getNodeById("lAA");
    lNodeAA->setMoveOrigin(lNodeAA->getPath());
    lNodeAA->setMoveOriginParentDbId(lNodeA->idb());
    lNodeAA->setName(Str("AA'"));
    lNodeAA->setChangeEvents(OperationTypeMove);

    // Simulate a delete of node A on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->_remoteUpdateTree->getNodeById("rA");
    rNodeA->setChangeEvents(OperationTypeDelete);
    _syncPal->_remoteUpdateTree->rootNode()->deleteChildren(rNodeA);

    std::shared_ptr<Node> rNodeAA = _syncPal->_remoteUpdateTree->getNodeById("rAA");

    Conflict conflict1(lNodeAA, rNodeAA, ConflictTypeMoveDelete);
    _syncPal->_conflictQueue->push(conflict1);
    Conflict conflict2(lNodeAA, rNodeA, ConflictTypeMoveParentDelete);
    _syncPal->_conflictQueue->push(conflict2);

    // This should be treated as a Move-ParentDelete conflict, the Move-Delete conflict must be ignored.
    // For this test, we only make sure that the Move-Delete conflict is ignored.
    // In real situation, a Move-ParentDelete conflict should have been detected as well.
    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT(_syncPal->_syncOps->opSortedList().size() == 1);
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    auto syncOp = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(syncOp->conflict().type() == ConflictTypeMoveParentDelete);
}

void TestConflictResolverWorker::testMoveParentDelete() {
    // Simulate a move of node AAA from AA to AB on local replica
    std::shared_ptr<Node> lNodeAA = _syncPal->_localUpdateTree->getNodeById("lAA");
    std::shared_ptr<Node> lNodeAB = _syncPal->_localUpdateTree->getNodeById("lAB");
    std::shared_ptr<Node> lNodeAAA = _syncPal->_localUpdateTree->getNodeById("lAAA");
    lNodeAAA->setChangeEvents(OperationTypeMove);
    lNodeAAA->setMoveOrigin(lNodeAAA->getPath());
    lNodeAAA->setMoveOriginParentDbId(lNodeAA->idb());
    lNodeAA->deleteChildren(lNodeAAA);
    lNodeAB->insertChildren(lNodeAAA);

    // Simulate a delete of node A on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->_remoteUpdateTree->getNodeById("rA");
    rNodeA->setChangeEvents(OperationTypeDelete);
    _syncPal->_remoteUpdateTree->rootNode()->deleteChildren(rNodeA);

    Conflict conflict(lNodeAAA, rNodeA, ConflictTypeMoveParentDelete);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    // We should only undo the move operation on the move replica
    CPPUNIT_ASSERT(_syncPal->_syncOps->size() == 1);
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(op->omit() == false);
    CPPUNIT_ASSERT(op->targetSide() == ReplicaSideLocal);
    CPPUNIT_ASSERT(op->type() == OperationTypeMove);
}

void TestConflictResolverWorker::testCreateParentDelete() {
    // Simulate file creation on local replica
    std::shared_ptr<Node> lNodeAA = _syncPal->_localUpdateTree->getNodeById("lAA");
    std::shared_ptr<Node> lNodeAAB =
        std::make_shared<Node>(std::nullopt, _syncPal->_localUpdateTree->side(), Str("AAB.txt"), NodeTypeFile,
                               OperationTypeCreate, "lAAB", defaultTime, defaultTime, defaultSize, lNodeAA);
    lNodeAA->insertChildren(lNodeAAB);
    _syncPal->_localUpdateTree->insertNode(lNodeAAB);
    _syncPal->_localSnapshot->updateItem(
        SnapshotItem("lAAB", "lAA", Str("AAB.txt"), defaultTime, defaultTime, NodeTypeFile, 123, "lcs1"));

    // Simulate a delete of node AA on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->_remoteUpdateTree->getNodeById("rA");
    std::shared_ptr<Node> rNodeAA = _syncPal->_remoteUpdateTree->getNodeById("rAA");
    rNodeAA->setChangeEvents(OperationTypeDelete);
    rNodeA->deleteChildren(rNodeAA);

    Conflict conflict(lNodeAAB, rNodeAA, ConflictTypeCreateParentDelete);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT_EQUAL((size_t)1, _syncPal->_syncOps->size());
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT_EQUAL(ReplicaSideLocal, op->targetSide());
    CPPUNIT_ASSERT_EQUAL(OperationTypeDelete, op->type());
}

void TestConflictResolverWorker::testMoveMoveSource() {
    // Simulate move of node AAA to AB on local replica
    std::shared_ptr<Node> lNodeAA = _syncPal->_localUpdateTree->getNodeById("lAA");
    std::shared_ptr<Node> lNodeAB = _syncPal->_localUpdateTree->getNodeById("lAB");
    std::shared_ptr<Node> lNodeAAA = _syncPal->_localUpdateTree->getNodeById("lAAA");
    lNodeAAA->setChangeEvents(OperationTypeMove);
    lNodeAAA->setMoveOriginParentDbId(lNodeAA->idb());
    lNodeAAA->setMoveOrigin("A/AA/AAA");
    lNodeAA->deleteChildren(lNodeAAA);
    lNodeAB->insertChildren(lNodeAAA);

    // Simulate move of node AAA to A on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->_remoteUpdateTree->getNodeById("rA");
    std::shared_ptr<Node> rNodeAA = _syncPal->_remoteUpdateTree->getNodeById("rAA");
    std::shared_ptr<Node> rNodeAAA = _syncPal->_remoteUpdateTree->getNodeById("rAAA");
    rNodeAAA->setChangeEvents(OperationTypeMove);
    rNodeAAA->setMoveOriginParentDbId(lNodeAA->idb());
    rNodeAAA->setMoveOrigin("A/AA/AAA");
    rNodeAA->deleteChildren(lNodeAAA);
    rNodeA->insertChildren(lNodeAAA);

    Conflict conflict(lNodeAAA, rNodeAAA, ConflictTypeMoveMoveSource);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() == 1);
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(!op->newName().empty());
    CPPUNIT_ASSERT(op->targetSide() == ReplicaSideLocal);
    CPPUNIT_ASSERT(op->type() == OperationTypeMove);
}

void TestConflictResolverWorker::testMoveMoveSourceWithOrphanNodes() {
    // Initial state : Node AAA is orphan.
    const SyncName orphanName = PlatformInconsistencyCheckerUtility::instance()->generateNewValidName(
        "AAA", PlatformInconsistencyCheckerUtility::SuffixTypeOrphan);

    std::shared_ptr<Node> lNodeAAA = _syncPal->_localUpdateTree->getNodeById("lAAA");
    lNodeAAA->setName(orphanName);
    lNodeAAA->parentNode()->deleteChildren(lNodeAAA);
    _syncPal->_localUpdateTree->rootNode()->insertChildren(lNodeAAA);
    lNodeAAA->setParentNode(_syncPal->_localUpdateTree->rootNode());

    std::shared_ptr<Node> rNodeAAA = _syncPal->_remoteUpdateTree->getNodeById("rAAA");
    rNodeAAA->setName(orphanName);
    rNodeAAA->parentNode()->deleteChildren(rNodeAAA);
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(rNodeAAA);
    rNodeAAA->setParentNode(_syncPal->_remoteUpdateTree->rootNode());

    _syncPal->_conflictResolverWorker->_registeredOrphans.insert({*rNodeAAA->idb(), ReplicaSideRemote});

    // Simulate move of node AAA to AB on local replica
    std::shared_ptr<Node> lNodeAB = _syncPal->_localUpdateTree->getNodeById("lAB");

    lNodeAAA->setChangeEvents(OperationTypeMove);
    lNodeAAA->setMoveOriginParentDbId(_syncPal->_localUpdateTree->rootNode()->idb());
    lNodeAAA->setMoveOrigin(lNodeAAA->getPath());
    lNodeAAA->setParentNode(lNodeAB);
    lNodeAAA->setName(Str("AAA"));
    _syncPal->_localUpdateTree->rootNode()->deleteChildren(lNodeAAA);
    lNodeAB->insertChildren(lNodeAAA);

    // Simulate move of node AAA to A on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->_remoteUpdateTree->getNodeById("rA");

    rNodeAAA->setChangeEvents(OperationTypeMove);
    rNodeAAA->setMoveOriginParentDbId(_syncPal->_remoteUpdateTree->rootNode()->idb());
    rNodeAAA->setMoveOrigin(rNodeAAA->getPath());
    rNodeAAA->setParentNode(rNodeA);
    rNodeAAA->setName(Str("AAA"));
    _syncPal->_remoteUpdateTree->rootNode()->deleteChildren(rNodeAAA);
    rNodeA->insertChildren(rNodeAAA);

    Conflict conflict(lNodeAAA, rNodeAAA, ConflictTypeMoveMoveSource);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() == 1);
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(orphanName), SyncName2Str(op->newName()));
    CPPUNIT_ASSERT_EQUAL(ReplicaSideLocal, op->targetSide());
    CPPUNIT_ASSERT_EQUAL(_syncPal->_localUpdateTree->rootNode(), op->newParentNode());
    CPPUNIT_ASSERT_EQUAL(OperationTypeMove, op->type());
}

void TestConflictResolverWorker::testMoveMoveDest() {
    // Simulate move of node AAA to AB on local replica
    std::shared_ptr<Node> lNodeAA = _syncPal->_localUpdateTree->getNodeById("lAA");
    std::shared_ptr<Node> lNodeAB = _syncPal->_localUpdateTree->getNodeById("lAB");
    std::shared_ptr<Node> lNodeAAA = _syncPal->_localUpdateTree->getNodeById("lAAA");
    lNodeAAA->setChangeEvents(OperationTypeMove);
    lNodeAAA->setMoveOriginParentDbId(lNodeAA->idb());
    lNodeAAA->setMoveOrigin("A/AA/AAA");
    lNodeAAA->setParentNode(lNodeAB);
    lNodeAA->deleteChildren(lNodeAAA);
    lNodeAB->insertChildren(lNodeAAA);

    // Simulate move of node AA to AB, and rename AA to AAA, on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->_remoteUpdateTree->getNodeById("rA");
    std::shared_ptr<Node> rNodeAA = _syncPal->_remoteUpdateTree->getNodeById("rAA");
    std::shared_ptr<Node> rNodeAB = _syncPal->_remoteUpdateTree->getNodeById("rAB");
    rNodeAA->setChangeEvents(OperationTypeMove);
    rNodeAA->setMoveOriginParentDbId(rNodeA->idb());
    rNodeAA->setMoveOrigin("A/AA");
    rNodeAA->setParentNode(rNodeAB);
    rNodeAA->setName(Str("AAA"));
    rNodeA->deleteChildren(rNodeAA);
    rNodeAB->insertChildren(rNodeAA);

    Conflict conflict(lNodeAAA, rNodeAA, ConflictTypeMoveMoveDest);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() == 1);
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(!op->newName().empty());
    CPPUNIT_ASSERT(op->targetSide() == ReplicaSideLocal);
    CPPUNIT_ASSERT(op->type() == OperationTypeMove);
}

void TestConflictResolverWorker::testMoveMoveCycle() {
    // Simulate move of node AA to AB on local replica
    std::shared_ptr<Node> lNodeA = _syncPal->_localUpdateTree->getNodeById("lA");
    std::shared_ptr<Node> lNodeAA = _syncPal->_localUpdateTree->getNodeById("lAA");
    std::shared_ptr<Node> lNodeAB = _syncPal->_localUpdateTree->getNodeById("lAB");
    lNodeAA->setChangeEvents(OperationTypeMove);
    lNodeAA->setMoveOriginParentDbId(lNodeA->idb());
    lNodeAA->setMoveOrigin("A/AA");
    lNodeAA->setParentNode(lNodeAB);
    lNodeA->deleteChildren(lNodeAA);
    lNodeAB->insertChildren(lNodeAA);

    // Simulate move of node AB to AA, on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->_remoteUpdateTree->getNodeById("rA");
    std::shared_ptr<Node> rNodeAA = _syncPal->_remoteUpdateTree->getNodeById("rAA");
    std::shared_ptr<Node> rNodeAB = _syncPal->_remoteUpdateTree->getNodeById("rAB");
    rNodeAB->setChangeEvents(OperationTypeMove);
    rNodeAB->setMoveOriginParentDbId(rNodeA->idb());
    rNodeAB->setMoveOrigin("A/AB");
    rNodeAB->setParentNode(rNodeAA);
    rNodeA->deleteChildren(rNodeAB);
    rNodeAA->insertChildren(rNodeAB);

    Conflict conflict(lNodeAA, rNodeAA, ConflictTypeMoveMoveCycle);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() == 1);
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(op->newParentNode() == lNodeA);
    CPPUNIT_ASSERT(op->targetSide() == ReplicaSideLocal);
    CPPUNIT_ASSERT(op->type() == OperationTypeMove);
}

}  // namespace KDC
