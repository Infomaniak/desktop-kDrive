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

#include "testoperationsorterworker.h"

#include "propagation/operation_sorter/cyclefinder.h"
#include "test_classes/testinitialsituationgenerator.h"
#include "test_utility/testhelpers.h"

using namespace CppUnit;

namespace KDC {

void TestOperationSorterWorker::setUp() {
    // Create SyncPal
    bool alreadyExists = false;
    const auto parmsDbPath = Db::makeDbName(alreadyExists, true);
    ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);

    SyncPath syncDbPath = Db::makeDbName(1, 1, 1, 1, alreadyExists);
    std::filesystem::remove(syncDbPath);
    _syncPal = std::make_shared<SyncPal>(std::make_shared<VfsOff>(VfsSetupParams(Log::instance()->getLogger())), syncDbPath,
                                         KDRIVE_VERSION_STRING, true);
    _syncPal->syncDb()->setAutoDelete(true);
    _syncPal->createSharedObjects();

    _syncPal->_operationsSorterWorker = std::make_shared<OperationSorterWorker>(_syncPal, "Operation Sorter", "OPSO");

    // Initial situation
    // .
    // ├── A
    // │   └── AA
    // │       ├── AAA
    // │       └── AAB
    // ├── B
    // ├── C
    // └── D
    _initialSituationGenerator.setSyncpal(_syncPal);
    _initialSituationGenerator.generateInitialSituation(R"({"a":{"aa":{"aaa":1,"aab":0}},"b":0,"c":0,"d":0})");
}

void TestOperationSorterWorker::tearDown() {
    ParmsDb::instance()->close();
    ParmsDb::reset();
    _syncPal->syncDb()->close();
}

void TestOperationSorterWorker::testMoveFirstAfterSecond() {
    const auto nodeA = _initialSituationGenerator.getNode(ReplicaSide::Local, "a");
    const auto nodeB = _initialSituationGenerator.getNode(ReplicaSide::Local, "b");
    const auto nodeC = _initialSituationGenerator.getNode(ReplicaSide::Local, "c");
    const auto opA = std::make_shared<SyncOperation>();
    const auto opB = std::make_shared<SyncOperation>();
    const auto opC = std::make_shared<SyncOperation>();
    opA->setAffectedNode(nodeA);
    opB->setAffectedNode(nodeB);
    opC->setAffectedNode(nodeC);
    _syncPal->_syncOps->pushOp(opA);
    _syncPal->_syncOps->pushOp(opB);
    _syncPal->_syncOps->pushOp(opC);

    // Move opB after opA -> nothing happens, opB is already after opA.
    _syncPal->_operationsSorterWorker->moveFirstAfterSecond(opB, opA);
    CPPUNIT_ASSERT_EQUAL(opA->id(), _syncPal->_syncOps->opSortedList().front());
    CPPUNIT_ASSERT_EQUAL(false, _syncPal->_operationsSorterWorker->hasOrderChanged());

    // Move opA after opB.
    _syncPal->_operationsSorterWorker->moveFirstAfterSecond(opA, opB);
    CPPUNIT_ASSERT_EQUAL(opB->id(), _syncPal->_syncOps->opSortedList().front());
    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    CPPUNIT_ASSERT_EQUAL(opC->id(), _syncPal->_syncOps->opSortedList().back());
}

// delete before move, e.g. user deletes an object at path "x" and moves another object "a" to "x".
void TestOperationSorterWorker::testFixDeleteBeforeMove() {
    const auto nodeA = _initialSituationGenerator.getNode(ReplicaSide::Local, "a");
    const auto nodeB = _initialSituationGenerator.getNode(ReplicaSide::Local, "b");

    // Delete node A
    nodeA->insertChangeEvent(OperationType::Delete);
    _initialSituationGenerator.removeFromUpdateTree(ReplicaSide::Local, "a");
    const auto deleteOp = generateSyncOperation(OperationType::Delete, nodeA);

    // Rename B into A
    nodeB->insertChangeEvent(OperationType::Move);
    nodeB->setName("A");
    const auto moveOp = generateSyncOperation(OperationType::Move, nodeB);

    _syncPal->_syncOps->pushOp(moveOp);
    _syncPal->_syncOps->pushOp(deleteOp);

    _syncPal->_operationsSorterWorker->fixDeleteBeforeMove();

    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    CPPUNIT_ASSERT_EQUAL(deleteOp->id(), _syncPal->_syncOps->opSortedList().front());
    CPPUNIT_ASSERT_EQUAL(moveOp->id(), _syncPal->_syncOps->opSortedList().back());
}

// move before create, e.g. user moves an object "a" to "b" and creates another object at "a".
void TestOperationSorterWorker::testFixMoveBeforeCreate() {
    const auto nodeA = _initialSituationGenerator.getNode(ReplicaSide::Local, "a");
    const auto nodeB = _initialSituationGenerator.getNode(ReplicaSide::Local, "b");

    // Move A to B/A
    _initialSituationGenerator.moveNode(ReplicaSide::Local, "a", "b");
    nodeA->insertChangeEvent(OperationType::Move);
    const auto moveOp = generateSyncOperation(OperationType::Move, nodeA);

    // Create A
    const auto nodeA2 = _initialSituationGenerator.insertInUpdateTree(ReplicaSide::Local, NodeType::File, "a2");
    nodeA2->insertChangeEvent(OperationType::Create);
    nodeA2->setName("A");
    const auto createOp = generateSyncOperation(OperationType::Create, nodeA2);

    _syncPal->_syncOps->pushOp(createOp);
    _syncPal->_syncOps->pushOp(moveOp);

    _syncPal->_operationsSorterWorker->fixMoveBeforeCreate();

    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    CPPUNIT_ASSERT_EQUAL(moveOp->id(), _syncPal->_syncOps->opSortedList().front());
    CPPUNIT_ASSERT_EQUAL(createOp->id(), _syncPal->_syncOps->opSortedList().back());
}

// move before delete, e.g. user moves object "X/y" outside of directory "X" (e.g. to "z") and then deletes "X".
void TestOperationSorterWorker::testFixMoveBeforeDelete() {
    const auto nodeA = _initialSituationGenerator.getNode(ReplicaSide::Local, "a");
    const auto nodeAAA = _initialSituationGenerator.getNode(ReplicaSide::Local, "aaa");

    // Move A/AA/AAA to AAA
    _initialSituationGenerator.moveNode(ReplicaSide::Local, "aaa", "aa");
    nodeAAA->insertChangeEvent(OperationType::Move);
    const auto moveOp = generateSyncOperation(OperationType::Move, nodeAAA);

    // Delete A
    nodeA->insertChangeEvent(OperationType::Delete);
    _initialSituationGenerator.removeFromUpdateTree(ReplicaSide::Local, "a");
    const auto deleteOp = generateSyncOperation(OperationType::Delete, nodeA);

    _syncPal->_syncOps->pushOp(deleteOp);
    _syncPal->_syncOps->pushOp(moveOp);

    _syncPal->_operationsSorterWorker->fixMoveBeforeDelete();

    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    CPPUNIT_ASSERT_EQUAL(moveOp->id(), _syncPal->_syncOps->opSortedList().front());
    CPPUNIT_ASSERT_EQUAL(deleteOp->id(), _syncPal->_syncOps->opSortedList().back());
}

// create before move, e.g. user creates directory "X" and moves object "y" into "X".
void TestOperationSorterWorker::testFixCreateBeforeMove() {
    const auto nodeA = _initialSituationGenerator.getNode(ReplicaSide::Local, "a");

    // Create D
    const auto nodeD = _initialSituationGenerator.insertInUpdateTree(ReplicaSide::Local, NodeType::Directory, "d");
    nodeD->insertChangeEvent(OperationType::Create);
    const auto createOp = generateSyncOperation(OperationType::Create, nodeD);

    // Move A to D/A
    _initialSituationGenerator.moveNode(ReplicaSide::Local, "a", "d");
    nodeA->insertChangeEvent(OperationType::Move);
    const auto moveOp = generateSyncOperation(OperationType::Move, nodeA);

    _syncPal->_syncOps->pushOp(moveOp);
    _syncPal->_syncOps->pushOp(createOp);

    _syncPal->_operationsSorterWorker->fixCreateBeforeMove();

    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    CPPUNIT_ASSERT_EQUAL(createOp->id(), _syncPal->_syncOps->opSortedList().front());
    CPPUNIT_ASSERT_EQUAL(moveOp->id(), _syncPal->_syncOps->opSortedList().back());
}

// delete before create, e.g. user deletes object "x" and then creates a new object at "x".
void TestOperationSorterWorker::testFixDeleteBeforeCreate() {
    const auto nodeAAA = _initialSituationGenerator.getNode(ReplicaSide::Local, "aaa");

    // Delete AAA
    nodeAAA->insertChangeEvent(OperationType::Delete);
    _initialSituationGenerator.removeFromUpdateTree(ReplicaSide::Local, "aaa");
    const auto deleteOp = generateSyncOperation(OperationType::Delete, nodeAAA);

    // Create AAA
    const auto nodeAAA2 = _initialSituationGenerator.insertInUpdateTree(ReplicaSide::Local, NodeType::File, "aaa2", "aa");
    nodeAAA2->insertChangeEvent(OperationType::Create);
    nodeAAA2->setName("AAA");
    const auto createOp = generateSyncOperation(OperationType::Create, nodeAAA2);

    _syncPal->_syncOps->pushOp(createOp);
    _syncPal->_syncOps->pushOp(deleteOp);

    _syncPal->_operationsSorterWorker->fixDeleteBeforeCreate();

    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    CPPUNIT_ASSERT_EQUAL(deleteOp->id(), _syncPal->_syncOps->opSortedList().front());
    CPPUNIT_ASSERT_EQUAL(createOp->id(), _syncPal->_syncOps->opSortedList().back());
}

// move before move (occupation), e.g. user moves file "a" to "temp" and then moves file "b" to "a".
void TestOperationSorterWorker::testFixMoveBeforeMoveOccupied() {
    const auto nodeAA = _initialSituationGenerator.getNode(ReplicaSide::Local, "aa");
    const auto nodeAAA = _initialSituationGenerator.getNode(ReplicaSide::Local, "aaa");
    const auto nodeC = _initialSituationGenerator.getNode(ReplicaSide::Local, "c");

    // Move A/AA/AAA to AAA
    _initialSituationGenerator.moveNode(ReplicaSide::Local, "aaa", {});
    nodeAAA->insertChangeEvent(OperationType::Move);
    const auto moveOp = generateSyncOperation(OperationType::Move, nodeAAA);

    // Move C to A/AA/AAA
    _initialSituationGenerator.moveNode(ReplicaSide::Local, "c", "aa");
    nodeC->insertChangeEvent(OperationType::Move);
    nodeC->setName("AAA");
    const auto moveOp2 = generateSyncOperation(OperationType::Move, nodeC);

    _syncPal->_syncOps->pushOp(moveOp2);
    _syncPal->_syncOps->pushOp(moveOp);

    _syncPal->_operationsSorterWorker->fixMoveBeforeMoveOccupied();

    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    CPPUNIT_ASSERT_EQUAL(moveOp->id(), _syncPal->_syncOps->opSortedList().front());
    CPPUNIT_ASSERT_EQUAL(moveOp2->id(), _syncPal->_syncOps->opSortedList().back());
}

bool isFirstBeforeSecond(const std::shared_ptr<SyncOperationList> &list, const SyncOpPtr &first, const SyncOpPtr &second) {
    for (const auto &opId: list->opSortedList()) {
        if (opId == first->id()) {
            return true;
        }
        if (opId == second->id()) {
            return false;
        }
    }
    return false;
}

// create before create, e.g. user creates directory "X" and then creates an object inside it.
void TestOperationSorterWorker::testFixCreateBeforeCreate() {
    // Insert the branch
    // .
    // └── D
    //     ├── DA
    //     │   ├── DAA
    //     │   └── DAB
    //     └── DB
    const auto nodeD = _initialSituationGenerator.insertInUpdateTree(ReplicaSide::Local, NodeType::Directory, "d");
    nodeD->insertChangeEvent(OperationType::Create);
    const auto opD = generateSyncOperation(OperationType::Create, nodeD);
    const auto nodeDA = _initialSituationGenerator.insertInUpdateTree(ReplicaSide::Local, NodeType::Directory, "da", nodeD);
    nodeDA->insertChangeEvent(OperationType::Create);
    const auto opDA = generateSyncOperation(OperationType::Create, nodeDA);
    const auto nodeDB = _initialSituationGenerator.insertInUpdateTree(ReplicaSide::Local, NodeType::Directory, "db", nodeD);
    nodeDB->insertChangeEvent(OperationType::Create);
    const auto opDB = generateSyncOperation(OperationType::Create, nodeDB);
    const auto nodeDAA = _initialSituationGenerator.insertInUpdateTree(ReplicaSide::Local, NodeType::File, "daa", nodeDA);
    nodeDAA->insertChangeEvent(OperationType::Create);
    const auto opDAA = generateSyncOperation(OperationType::Create, nodeDAA);
    const auto nodeDAB = _initialSituationGenerator.insertInUpdateTree(ReplicaSide::Local, NodeType::File, "dab", nodeDA);
    nodeDAB->insertChangeEvent(OperationType::Create);
    const auto opDAB = generateSyncOperation(OperationType::Create, nodeDAB);

    _syncPal->_syncOps->clear();
    {
        // Case : DAA DAB DA DB D
        _syncPal->_syncOps->pushOp(opDAA);
        _syncPal->_syncOps->pushOp(opDAB);
        _syncPal->_syncOps->pushOp(opDA);
        _syncPal->_syncOps->pushOp(opDB);
        _syncPal->_syncOps->pushOp(opD);

        // Test hasParentWithHigherIndex
        std::unordered_map<UniqueId, int32_t> opIdToIndexMap;
        _syncPal->_syncOps->getOpIdToIndexMap(opIdToIndexMap, OperationType::Create);
        SyncOpPtr ancestorOpWithHighestDistance;
        int32_t relativeDepth = 0;
        CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasParentWithHigherIndex(
                                           opIdToIndexMap, opDAA, ancestorOpWithHighestDistance, relativeDepth));
        CPPUNIT_ASSERT_EQUAL(opD->id(), ancestorOpWithHighestDistance->id());
        CPPUNIT_ASSERT_EQUAL(2, relativeDepth);
        CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasParentWithHigherIndex(
                                           opIdToIndexMap, opDA, ancestorOpWithHighestDistance, relativeDepth));
        CPPUNIT_ASSERT_EQUAL(opD->id(), ancestorOpWithHighestDistance->id());
        CPPUNIT_ASSERT_EQUAL(1, relativeDepth);
        CPPUNIT_ASSERT_EQUAL(false, _syncPal->_operationsSorterWorker->hasParentWithHigherIndex(
                                            opIdToIndexMap, opD, ancestorOpWithHighestDistance, relativeDepth));
        CPPUNIT_ASSERT(!ancestorOpWithHighestDistance);
        CPPUNIT_ASSERT_EQUAL(0, relativeDepth);

        _syncPal->_operationsSorterWorker->_hasOrderChanged = true;
        while (_syncPal->_operationsSorterWorker->_hasOrderChanged) {
            _syncPal->_operationsSorterWorker->_hasOrderChanged = false;
            _syncPal->_operationsSorterWorker->fixCreateBeforeCreate();
        }

        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->_syncOps, opD, opDA));
        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->_syncOps, opD, opDB));
        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->_syncOps, opDA, opDAA));
        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->_syncOps, opDA, opDAB));
    }

    _syncPal->_syncOps->clear();
    {
        // Case : DAA DAB D DA DB
        _syncPal->_syncOps->pushOp(opDAA);
        _syncPal->_syncOps->pushOp(opDAB);
        _syncPal->_syncOps->pushOp(opD);
        _syncPal->_syncOps->pushOp(opDA);
        _syncPal->_syncOps->pushOp(opDB);

        _syncPal->_operationsSorterWorker->_hasOrderChanged = true;
        while (_syncPal->_operationsSorterWorker->_hasOrderChanged) {
            _syncPal->_operationsSorterWorker->_hasOrderChanged = false;
            _syncPal->_operationsSorterWorker->fixCreateBeforeCreate();
        }

        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->_syncOps, opD, opDA));
        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->_syncOps, opD, opDB));
        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->_syncOps, opDA, opDAA));
        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->_syncOps, opDA, opDAB));
    }

    _syncPal->_syncOps->clear();
    {
        // Case : DA DAA DAB D DB
        _syncPal->_syncOps->pushOp(opDA);
        _syncPal->_syncOps->pushOp(opDAA);
        _syncPal->_syncOps->pushOp(opDAB);
        _syncPal->_syncOps->pushOp(opD);
        _syncPal->_syncOps->pushOp(opDB);

        _syncPal->_operationsSorterWorker->_hasOrderChanged = true;
        while (_syncPal->_operationsSorterWorker->_hasOrderChanged) {
            _syncPal->_operationsSorterWorker->_hasOrderChanged = false;
            _syncPal->_operationsSorterWorker->fixCreateBeforeCreate();
        }

        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->_syncOps, opD, opDA));
        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->_syncOps, opD, opDB));
        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->_syncOps, opDA, opDAA));
        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->_syncOps, opDA, opDAB));
    }

    _syncPal->_syncOps->clear();
    {
        // Case : D DA DB DAA DAB
        _syncPal->_syncOps->pushOp(opD);
        _syncPal->_syncOps->pushOp(opDA);
        _syncPal->_syncOps->pushOp(opDB);
        _syncPal->_syncOps->pushOp(opDAA);
        _syncPal->_syncOps->pushOp(opDAB);

        _syncPal->_operationsSorterWorker->_hasOrderChanged = true;
        while (_syncPal->_operationsSorterWorker->_hasOrderChanged) {
            _syncPal->_operationsSorterWorker->_hasOrderChanged = false;
            _syncPal->_operationsSorterWorker->fixCreateBeforeCreate();
        }

        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->_syncOps, opD, opDA));
        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->_syncOps, opD, opDB));
        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->_syncOps, opDA, opDAA));
        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->_syncOps, opDA, opDAB));
    }
}

// edit before move, e.g. user moves an object "a" to "b" and then edit it.
void TestOperationSorterWorker::testFixEditBeforeMove() {
    const auto nodeAAA = _initialSituationGenerator.getNode(ReplicaSide::Local, "aaa");

    // Move A/AA/AAA to AAA
    _initialSituationGenerator.moveNode(ReplicaSide::Local, *nodeAAA->id(), {});
    nodeAAA->insertChangeEvent(OperationType::Move);
    const auto moveOp = generateSyncOperation(OperationType::Move, nodeAAA);

    // Edit AAA
    _initialSituationGenerator.editNode(ReplicaSide::Local, *nodeAAA->id());
    const auto editOp = generateSyncOperation(OperationType::Edit, nodeAAA);

    _syncPal->_syncOps->pushOp(editOp);
    _syncPal->_syncOps->pushOp(moveOp);

    _syncPal->_operationsSorterWorker->fixEditBeforeMove();

    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    CPPUNIT_ASSERT_EQUAL(moveOp->id(), _syncPal->_syncOps->opSortedList().front());
    CPPUNIT_ASSERT_EQUAL(editOp->id(), _syncPal->_syncOps->opSortedList().back());
}

// move before move (parent-child flip), e.g. user moves directory "A/B" to "C", then moves directory "A" to "C/A" (parent-child
// relationships are now flipped).
void TestOperationSorterWorker::testFixMoveBeforeMoveParentChildFlip() {
    const auto nodeA = _initialSituationGenerator.getNode(ReplicaSide::Local, "a");
    const auto nodeAA = _initialSituationGenerator.getNode(ReplicaSide::Local, "aa");

    // Move A/AA to D
    _initialSituationGenerator.moveNode(ReplicaSide::Local, *nodeAA->id(), {});
    nodeAA->insertChangeEvent(OperationType::Move);
    nodeAA->setName("D");
    const auto moveOp1 = generateSyncOperation(OperationType::Move, nodeAA);

    // Move A to D
    _initialSituationGenerator.moveNode(ReplicaSide::Local, *nodeA->id(), *nodeAA->id());
    nodeA->insertChangeEvent(OperationType::Move);
    const auto moveOp2 = generateSyncOperation(OperationType::Move, nodeA);

    _syncPal->_syncOps->pushOp(moveOp2);
    _syncPal->_syncOps->pushOp(moveOp1);

    _syncPal->_operationsSorterWorker->fixMoveBeforeMoveHierarchyFlip();

    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    CPPUNIT_ASSERT_EQUAL(moveOp1->id(), _syncPal->_syncOps->opSortedList().front());
    CPPUNIT_ASSERT_EQUAL(moveOp2->id(), _syncPal->_syncOps->opSortedList().back());
}

void TestOperationSorterWorker::testFixImpossibleFirstMoveOp() {
    // Initial situation
    // .
    // ├── A(a)
    // │   └── AA(aa)
    // │       ├── AAA(aaa)
    // │       └── AAB(aab)
    // ├── B(b)
    // └── C(c)

    // Final situation local
    // .
    // ├── A(a)
    // │   └── AA(aa)
    // │       └── AAA(aaa)
    // ├── B(aab)
    // │   └── B*(b)
    // └── C(c)
    const auto lNodeAAB = _initialSituationGenerator.getNode(ReplicaSide::Local, "aab");
    const auto lNodeB = _initialSituationGenerator.getNode(ReplicaSide::Local, "b");

    // Move B to A/AA/AAB/B*
    _initialSituationGenerator.moveNode(ReplicaSide::Local, *lNodeB->id(), *lNodeAAB->id());
    lNodeB->insertChangeEvent(OperationType::Move);
    lNodeB->setName("B*");
    const auto lMoveOpB = generateSyncOperation(OperationType::Move, lNodeB);

    // Move A/AA/AAB to B
    _initialSituationGenerator.moveNode(ReplicaSide::Local, *lNodeAAB->id(), {});
    lNodeAAB->insertChangeEvent(OperationType::Move);
    lNodeAAB->setName("B");
    const auto lMoveOpAAB = generateSyncOperation(OperationType::Move, lNodeAAB);

    // Final situation remote
    // .
    // ├── B(b)
    // │   └── A*(a)
    // │       └── AA(aa)
    // │           ├── AAA*(aaa)
    // │           └── AAB(aab)
    // └── C*(c)
    const auto rNodeA = _initialSituationGenerator.getNode(ReplicaSide::Remote, "a");
    const auto rNodeAAA = _initialSituationGenerator.getNode(ReplicaSide::Remote, "aaa");
    const auto rNodeB = _initialSituationGenerator.getNode(ReplicaSide::Remote, "b");
    const auto rNodeC = _initialSituationGenerator.getNode(ReplicaSide::Remote, "c");

    // Move A to B/A*
    _initialSituationGenerator.moveNode(ReplicaSide::Remote, *rNodeA->id(), *rNodeB->id());
    rNodeA->insertChangeEvent(OperationType::Move);
    rNodeA->setName("A*");
    const auto rMoveOpA = generateSyncOperation(OperationType::Move, rNodeA);

    // Move C to C*
    rNodeC->insertChangeEvent(OperationType::Move);
    rNodeC->setName("C*");
    const auto rMoveOpC = generateSyncOperation(OperationType::Move, rNodeC);

    // Move A/AA/AAA to A/AA/AAA*
    rNodeAAA->insertChangeEvent(OperationType::Move);
    rNodeAAA->setName("AAA*");
    const auto rMoveOpAAA = generateSyncOperation(OperationType::Move, rNodeAAA);

    _syncPal->_syncOps->setOpList({lMoveOpB, lMoveOpAAB, rMoveOpC, rMoveOpA, rMoveOpAAA});
    const auto reshuffledOp = _syncPal->_operationsSorterWorker->fixImpossibleFirstMoveOp();
    CPPUNIT_ASSERT(reshuffledOp);
    CPPUNIT_ASSERT(reshuffledOp->_opSortedList.size() == 2);
    CPPUNIT_ASSERT(reshuffledOp->_opSortedList.back() == rMoveOpA->id());
}

void TestOperationSorterWorker::testFindCompleteCycles() {
    const auto nodeA = _initialSituationGenerator.getNode(ReplicaSide::Local, "a");
    nodeA->insertChangeEvent(OperationType::Move);
    nodeA->setName("A*");
    const auto opA = generateSyncOperation(OperationType::Move, nodeA);

    const auto nodeB = _initialSituationGenerator.getNode(ReplicaSide::Local, "b");
    nodeB->insertChangeEvent(OperationType::Move);
    nodeB->setName("B*");
    const auto opB = generateSyncOperation(OperationType::Move, nodeB);

    const auto nodeC = _initialSituationGenerator.getNode(ReplicaSide::Local, "c");
    nodeC->insertChangeEvent(OperationType::Move);
    nodeC->setName("C*");
    const auto opC = generateSyncOperation(OperationType::Move, nodeC);

    const auto nodeD = _initialSituationGenerator.getNode(ReplicaSide::Local, "d");
    nodeD->insertChangeEvent(OperationType::Move);
    nodeD->setName("D*");
    const auto opD = generateSyncOperation(OperationType::Move, nodeD);

    const auto nodeAA = _initialSituationGenerator.getNode(ReplicaSide::Local, "aa");
    nodeAA->insertChangeEvent(OperationType::Move);
    nodeAA->setName("AA*");
    const auto opAA = generateSyncOperation(OperationType::Move, nodeAA);

    const auto nodeAAA = _initialSituationGenerator.getNode(ReplicaSide::Local, "aaa");
    nodeAAA->insertChangeEvent(OperationType::Move);
    nodeAAA->setName("AAA*");
    const auto opAAA = generateSyncOperation(OperationType::Move, nodeAAA);

    auto start = std::chrono::steady_clock::now();
    for (int test = 1; test <= 10000; test++) {
        LOG_INFO(Log::instance()->getLogger(), "**************************************************");
        {
            // No cycle
            _syncPal->_operationsSorterWorker->_reorderings = {{opA, opB}, {opB, opC}, {opC, opD}};
            CycleFinder cycleFinder(_syncPal->_operationsSorterWorker->_reorderings);
            cycleFinder.findCompleteCycle();

            CPPUNIT_ASSERT_EQUAL(false, cycleFinder.hasCompleteCycle());
            CPPUNIT_ASSERT_EQUAL(0, static_cast<int>(cycleFinder.completeCycle().opSortedList().size()));
        }

        LOG_INFO(Log::instance()->getLogger(), "**************************************************");
        {
            // A cycle, the order in which reorderings are found follow the order of the operations in the cycle
            _syncPal->_operationsSorterWorker->_reorderings = {{opA, opB}, {opB, opC}, {opC, opD}, {opD, opA}};
            CycleFinder cycleFinder(_syncPal->_operationsSorterWorker->_reorderings);
            cycleFinder.findCompleteCycle();

            CPPUNIT_ASSERT_EQUAL(true, cycleFinder.hasCompleteCycle());
            CPPUNIT_ASSERT_EQUAL(4, static_cast<int>(cycleFinder.completeCycle().opSortedList().size()));
            const std::vector expectedOpIdsInCycle = {opA->id(), opB->id(), opC->id(), opD->id()};
            for (int i = 0; const auto &id: cycleFinder.completeCycle().opSortedList()) {
                CPPUNIT_ASSERT_EQUAL(expectedOpIdsInCycle[i++], id);
            }
        }

        LOG_INFO(Log::instance()->getLogger(), "**************************************************");
        {
            // A cycle, the order in which reorderings are found follow the order of the operations in the cycle but with other
            // operations that are not in the cycle chain.
            _syncPal->_operationsSorterWorker->_reorderings = {{opA, opB},   {opB, opAA},  {opB, opAAA}, {opB, opC},
                                                               {opC, opAA},  {opC, opAAA}, {opC, opD},   {opD, opAA},
                                                               {opA, opAAA}, {opD, opA},   {opA, opAA},  {opA, opAAA}};
            CycleFinder cycleFinder(_syncPal->_operationsSorterWorker->_reorderings);
            cycleFinder.findCompleteCycle();

            CPPUNIT_ASSERT_EQUAL(true, cycleFinder.hasCompleteCycle());
            CPPUNIT_ASSERT_EQUAL(4, static_cast<int>(cycleFinder.completeCycle().opSortedList().size()));
            const std::vector expectedOpIdsInCycle = {opA->id(), opB->id(), opC->id(), opD->id()};
            for (int i = 0; const auto &id: cycleFinder.completeCycle().opSortedList()) {
                CPPUNIT_ASSERT_EQUAL(expectedOpIdsInCycle[i++], id);
            }
        }

        LOG_INFO(Log::instance()->getLogger(), "**************************************************");
        {
            // Cycle, the order in which reorderings are found DO NOT follow the order of the operations in the cycle.
            _syncPal->_operationsSorterWorker->_reorderings = {{opB, opC}, {opD, opA}, {opA, opB}, {opC, opD}};
            CycleFinder cycleFinder(_syncPal->_operationsSorterWorker->_reorderings);
            cycleFinder.findCompleteCycle();

            CPPUNIT_ASSERT_EQUAL(true, cycleFinder.hasCompleteCycle());
            CPPUNIT_ASSERT_EQUAL(4, static_cast<int>(cycleFinder.completeCycle().opSortedList().size()));
            const std::vector expectedOpIdsInCycle = {opB->id(), opC->id(), opD->id(), opA->id()};
            for (int i = 0; const auto &id: cycleFinder.completeCycle().opSortedList()) {
                CPPUNIT_ASSERT_EQUAL(expectedOpIdsInCycle[i++], id);
            }
        }

        LOG_INFO(Log::instance()->getLogger(), "**************************************************");
        {
            // Cycle, the order in which reorderings are found DO NOT follow the order of the operations in the cycle and with
            // other operations that are not in the cycle chain.
            _syncPal->_operationsSorterWorker->_reorderings = {{opB, opC},   {opD, opA}, {opA, opAA},
                                                               {opA, opAAA}, {opA, opB}, {opC, opD}};
            CycleFinder cycleFinder(_syncPal->_operationsSorterWorker->_reorderings);
            cycleFinder.findCompleteCycle();

            CPPUNIT_ASSERT_EQUAL(true, cycleFinder.hasCompleteCycle());
            CPPUNIT_ASSERT_EQUAL(4, static_cast<int>(cycleFinder.completeCycle().opSortedList().size()));
            const std::vector expectedOpIdsInCycle = {opB->id(), opC->id(), opD->id(), opA->id()};
            for (int i = 0; const auto &id: cycleFinder.completeCycle().opSortedList()) {
                CPPUNIT_ASSERT_EQUAL(expectedOpIdsInCycle[i++], id);
            }
        }

        LOG_INFO(Log::instance()->getLogger(), "**************************************************");
        {
            // A cycle hidden within a chain
            _syncPal->_operationsSorterWorker->_reorderings = {{opB, opA}, {opA, opC}, {opC, opA}};
            CycleFinder cycleFinder(_syncPal->_operationsSorterWorker->_reorderings);
            cycleFinder.findCompleteCycle();

            CPPUNIT_ASSERT_EQUAL(true, cycleFinder.hasCompleteCycle());
            CPPUNIT_ASSERT_EQUAL(2, static_cast<int>(cycleFinder.completeCycle().opSortedList().size()));
            const std::vector expectedOpIdsInCycle = {opA->id(), opC->id()};
            for (int i = 0; const auto &id: cycleFinder.completeCycle().opSortedList()) {
                CPPUNIT_ASSERT_EQUAL(expectedOpIdsInCycle[i++], id);
            }
        }

        LOG_INFO(Log::instance()->getLogger(), "**************************************************");
        {
            // 2 cycles
            _syncPal->_operationsSorterWorker->_reorderings = {{opA, opB}, {opB, opA}, {opB, opC}, {opC, opD}, {opD, opA}};
            CycleFinder cycleFinder(_syncPal->_operationsSorterWorker->_reorderings);
            cycleFinder.findCompleteCycle();

            CPPUNIT_ASSERT_EQUAL(true, cycleFinder.hasCompleteCycle());
            CPPUNIT_ASSERT_EQUAL(2, static_cast<int>(cycleFinder.completeCycle().opSortedList().size()));
            const std::vector expectedOpIdsInCycle = {opA->id(), opB->id()};
            for (int i = 0; const auto &id: cycleFinder.completeCycle().opSortedList()) {
                CPPUNIT_ASSERT_EQUAL(expectedOpIdsInCycle[i++], id);
            }
        }
    }

    std::chrono::duration<double> elapsed_seconds = std::chrono::steady_clock::now() - start;
    LOG_INFO(Log::instance()->getLogger(), "Test took: " << elapsed_seconds.count() << "s");
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
    moveOp->setNewName(Str("A*"));
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

SyncOpPtr TestOperationSorterWorker::generateSyncOperation(const OperationType opType,
                                                           const std::shared_ptr<Node> &affectedNode) {
    const auto op = std::make_shared<SyncOperation>();
    op->setType(opType);
    op->setAffectedNode(affectedNode);
    const auto targetSide = otherSide(affectedNode->side());
    if (opType != OperationType::Create) {
        op->setCorrespondingNode(_initialSituationGenerator.getNode(targetSide, *affectedNode->id()));
    }
    op->setNewName(affectedNode->name());
    op->setTargetSide(targetSide);
    op->setNewParentNode(affectedNode->parentNode());
    return op;
}

} // namespace KDC
