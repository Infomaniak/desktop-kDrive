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
#include "test_classes/testsituationgenerator.h"
#include "test_utility/testhelpers.h"

using namespace CppUnit;

namespace KDC {

void TestOperationSorterWorker::setUp() {
    TestBase::start();
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
    _testSituationGenerator.setSyncpal(_syncPal);
    _testSituationGenerator.generateInitialSituation(R"({"a":{"aa":{"aaa":1,"aab":{}}},"b":{},"c":{},"d":{}})");
}

void TestOperationSorterWorker::tearDown() {
    ParmsDb::instance()->close();
    ParmsDb::reset();
    _syncPal->syncDb()->close();
    TestBase::stop();
}

void TestOperationSorterWorker::testMoveFirstAfterSecond() {
    const auto nodeA = _testSituationGenerator.getNode(ReplicaSide::Local, "a");
    const auto nodeB = _testSituationGenerator.getNode(ReplicaSide::Local, "b");
    const auto nodeC = _testSituationGenerator.getNode(ReplicaSide::Local, "c");
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
    const auto nodeA = _testSituationGenerator.getNode(ReplicaSide::Local, "a");
    const auto nodeB = _testSituationGenerator.getNode(ReplicaSide::Local, "b");

    // Delete node A
    nodeA->insertChangeEvent(OperationType::Delete);
    const auto deleteOp = generateSyncOperation(OperationType::Delete, nodeA);

    // Rename B into A
    nodeB->insertChangeEvent(OperationType::Move);
    nodeB->setName(Str("A"));
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
    const auto nodeA = _testSituationGenerator.getNode(ReplicaSide::Local, "a");
    const auto nodeB = _testSituationGenerator.getNode(ReplicaSide::Local, "b");

    // Move A to B/A
    _testSituationGenerator.moveNode(ReplicaSide::Local, *nodeA->id(), *nodeB->id());
    const auto moveOp = generateSyncOperation(OperationType::Move, nodeA);

    // Create A
    const auto nodeA2 = _testSituationGenerator.createNode(ReplicaSide::Local, NodeType::File, "a2", "");
    nodeA2->setName(Str("A"));
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
    const auto nodeA = _testSituationGenerator.getNode(ReplicaSide::Local, "a");
    const auto nodeAAA = _testSituationGenerator.getNode(ReplicaSide::Local, "aaa");

    // Move A/AA/AAA to AAA
    _testSituationGenerator.moveNode(ReplicaSide::Local, *nodeAAA->id(), "");
    const auto moveOp = generateSyncOperation(OperationType::Move, nodeAAA);

    // Delete A
    nodeA->insertChangeEvent(OperationType::Delete);
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
    const auto nodeA = _testSituationGenerator.getNode(ReplicaSide::Local, "a");

    // Create E
    const auto nodeE = _testSituationGenerator.createNode(ReplicaSide::Local, NodeType::Directory, "e", "");
    const auto createOp = generateSyncOperation(OperationType::Create, nodeE);

    // Move A to E/A
    _testSituationGenerator.moveNode(ReplicaSide::Local, *nodeA->id(), *nodeE->id());
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
    const auto nodeAA = _testSituationGenerator.getNode(ReplicaSide::Local, "aa");
    const auto nodeAAA = _testSituationGenerator.getNode(ReplicaSide::Local, "aaa");

    // Delete AAA
    nodeAAA->insertChangeEvent(OperationType::Delete);
    const auto deleteOp = generateSyncOperation(OperationType::Delete, nodeAAA);

    // Create AAA
    const auto nodeAAA2 = _testSituationGenerator.createNode(ReplicaSide::Local, NodeType::File, "aaa2", *nodeAA->id());
    nodeAAA2->setName(Str("AAA"));
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
    const auto nodeAA = _testSituationGenerator.getNode(ReplicaSide::Local, "aa");
    const auto nodeAAA = _testSituationGenerator.getNode(ReplicaSide::Local, "aaa");
    const auto nodeC = _testSituationGenerator.getNode(ReplicaSide::Local, "c");

    // Move A/AA/AAA to AAA
    _testSituationGenerator.moveNode(ReplicaSide::Local, *nodeAAA->id(), {});
    const auto moveOp = generateSyncOperation(OperationType::Move, nodeAAA);

    // Move C to A/AA/AAA
    _testSituationGenerator.moveNode(ReplicaSide::Local, *nodeC->id(), *nodeAA->id());
    nodeC->setName(Str("AAA"));
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
    const auto nodeD = _testSituationGenerator.createNode(ReplicaSide::Local, NodeType::Directory, "d", "");
    const auto opD = generateSyncOperation(OperationType::Create, nodeD);
    const auto nodeDA = _testSituationGenerator.createNode(ReplicaSide::Local, NodeType::Directory, "da", nodeD);
    const auto opDA = generateSyncOperation(OperationType::Create, nodeDA);
    const auto nodeDB = _testSituationGenerator.createNode(ReplicaSide::Local, NodeType::Directory, "db", nodeD);
    const auto opDB = generateSyncOperation(OperationType::Create, nodeDB);
    const auto nodeDAA = _testSituationGenerator.createNode(ReplicaSide::Local, NodeType::File, "daa", nodeDA);
    const auto opDAA = generateSyncOperation(OperationType::Create, nodeDAA);
    const auto nodeDAB = _testSituationGenerator.createNode(ReplicaSide::Local, NodeType::File, "dab", nodeDA);
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

        do {
            _syncPal->_operationsSorterWorker->_hasOrderChanged = false;
            _syncPal->_operationsSorterWorker->fixCreateBeforeCreate();
        } while (_syncPal->_operationsSorterWorker->_hasOrderChanged);

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

        do {
            _syncPal->_operationsSorterWorker->_hasOrderChanged = false;
            _syncPal->_operationsSorterWorker->fixCreateBeforeCreate();
        } while (_syncPal->_operationsSorterWorker->_hasOrderChanged);

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

        do {
            _syncPal->_operationsSorterWorker->_hasOrderChanged = false;
            _syncPal->_operationsSorterWorker->fixCreateBeforeCreate();
        } while (_syncPal->_operationsSorterWorker->_hasOrderChanged);

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

        do {
            _syncPal->_operationsSorterWorker->_hasOrderChanged = false;
            _syncPal->_operationsSorterWorker->fixCreateBeforeCreate();
        } while (_syncPal->_operationsSorterWorker->_hasOrderChanged);

        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->_syncOps, opD, opDA));
        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->_syncOps, opD, opDB));
        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->_syncOps, opDA, opDAA));
        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->_syncOps, opDA, opDAB));
    }
}

// edit before move, e.g. user moves an object "a" to "b" and then edit it.
void TestOperationSorterWorker::testFixEditBeforeMove() {
    const auto nodeAAA = _testSituationGenerator.getNode(ReplicaSide::Local, "aaa");

    // Move A/AA/AAA to AAA
    _testSituationGenerator.moveNode(ReplicaSide::Local, *nodeAAA->id(), {});
    const auto moveOp = generateSyncOperation(OperationType::Move, nodeAAA);

    // Edit AAA
    _testSituationGenerator.editNode(ReplicaSide::Local, *nodeAAA->id());
    const auto editOp = generateSyncOperation(OperationType::Edit, nodeAAA);

    _syncPal->_syncOps->pushOp(editOp);
    _syncPal->_syncOps->pushOp(moveOp);

    _syncPal->_operationsSorterWorker->fixEditBeforeMove();

    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    CPPUNIT_ASSERT_EQUAL(moveOp->id(), _syncPal->_syncOps->opSortedList().front());
    CPPUNIT_ASSERT_EQUAL(editOp->id(), _syncPal->_syncOps->opSortedList().back());
}

// move before move (parent-child flip), e.g. user moves directory "A/B" to "D", then moves directory "A" to "D/A" (parent-child
// relationships are now flipped).
void TestOperationSorterWorker::testFixMoveBeforeMoveParentChildFlip() {
    const auto nodeA = _testSituationGenerator.getNode(ReplicaSide::Local, "a");
    const auto nodeAA = _testSituationGenerator.getNode(ReplicaSide::Local, "aa");

    // Move A/AA to D
    _testSituationGenerator.moveNode(ReplicaSide::Local, *nodeAA->id(), {});
    nodeAA->setName(Str("D"));
    const auto moveOp1 = generateSyncOperation(OperationType::Move, nodeAA);

    // Move A to D
    _testSituationGenerator.moveNode(ReplicaSide::Local, *nodeA->id(), *nodeAA->id());
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
    const auto lNodeAAB = _testSituationGenerator.getNode(ReplicaSide::Local, "aab");
    const auto lNodeB = _testSituationGenerator.getNode(ReplicaSide::Local, "b");

    // Move B to A/AA/AAB/B*
    _testSituationGenerator.moveNode(ReplicaSide::Local, *lNodeB->id(), *lNodeAAB->id());
    lNodeB->setName(Str("B*"));
    const auto lMoveOpB = generateSyncOperation(OperationType::Move, lNodeB);

    // Move A/AA/AAB to B
    _testSituationGenerator.moveNode(ReplicaSide::Local, *lNodeAAB->id(), {});
    lNodeAAB->setName(Str("B"));
    const auto lMoveOpAAB = generateSyncOperation(OperationType::Move, lNodeAAB);

    // Final situation remote
    // .
    // ├── B(b)
    // │   └── A*(a)
    // │       └── AA(aa)
    // │           ├── AAA*(aaa)
    // │           └── AAB(aab)
    // └── C*(c)
    const auto rNodeA = _testSituationGenerator.getNode(ReplicaSide::Remote, "a");
    const auto rNodeAAA = _testSituationGenerator.getNode(ReplicaSide::Remote, "aaa");
    const auto rNodeB = _testSituationGenerator.getNode(ReplicaSide::Remote, "b");
    const auto rNodeC = _testSituationGenerator.getNode(ReplicaSide::Remote, "c");

    // Move A to B/A*
    _testSituationGenerator.moveNode(ReplicaSide::Remote, *rNodeA->id(), *rNodeB->id());
    rNodeA->setName(Str("A*"));
    const auto rMoveOpA = generateSyncOperation(OperationType::Move, rNodeA);

    // Move C to C*
    rNodeC->setName(Str("C*"));
    const auto rMoveOpC = generateSyncOperation(OperationType::Move, rNodeC);

    // Move A/AA/AAA to A/AA/AAA*
    rNodeAAA->setName(Str("AAA*"));
    const auto rMoveOpAAA = generateSyncOperation(OperationType::Move, rNodeAAA);

    _syncPal->_syncOps->setOpList({lMoveOpB, lMoveOpAAB, rMoveOpC, rMoveOpA, rMoveOpAAA});
    const auto reshuffledOp = _syncPal->_operationsSorterWorker->fixImpossibleFirstMoveOp();
    CPPUNIT_ASSERT(reshuffledOp);
    CPPUNIT_ASSERT(reshuffledOp->_opSortedList.size() == 2);
    CPPUNIT_ASSERT(reshuffledOp->_opSortedList.back() == rMoveOpA->id());
}

void TestOperationSorterWorker::testFindCompleteCycles() {
    const auto nodeA = _testSituationGenerator.getNode(ReplicaSide::Local, "a");
    nodeA->setName(Str("A*"));
    const auto opA = generateSyncOperation(OperationType::Move, nodeA);

    const auto nodeB = _testSituationGenerator.getNode(ReplicaSide::Local, "b");
    nodeB->setName(Str("B*"));
    const auto opB = generateSyncOperation(OperationType::Move, nodeB);

    const auto nodeC = _testSituationGenerator.getNode(ReplicaSide::Local, "c");
    nodeC->setName(Str("C*"));
    const auto opC = generateSyncOperation(OperationType::Move, nodeC);

    const auto nodeD = _testSituationGenerator.getNode(ReplicaSide::Local, "d");
    nodeD->setName(Str("D*"));
    const auto opD = generateSyncOperation(OperationType::Move, nodeD);

    const auto nodeAA = _testSituationGenerator.getNode(ReplicaSide::Local, "aa");
    nodeAA->setName(Str("AA*"));
    const auto opAA = generateSyncOperation(OperationType::Move, nodeAA);

    const auto nodeAAA = _testSituationGenerator.getNode(ReplicaSide::Local, "aaa");
    nodeAAA->setName(Str("AAA*"));
    const auto opAAA = generateSyncOperation(OperationType::Move, nodeAAA);

    {
        // No cycle
        _syncPal->_operationsSorterWorker->_reorderings = {{opA, opB}, {opB, opC}, {opC, opD}};
        CycleFinder cycleFinder(_syncPal->_operationsSorterWorker->_reorderings);
        cycleFinder.findCompleteCycle();

        CPPUNIT_ASSERT_EQUAL(false, cycleFinder.hasCompleteCycle());
        CPPUNIT_ASSERT_EQUAL(0, static_cast<int>(cycleFinder.completeCycle().opSortedList().size()));
    }

    {
        // A cycle, the order in which reorderings are found follow the order of the operations in the cycle
        _syncPal->_operationsSorterWorker->_reorderings = {{opA, opB}, {opB, opC}, {opC, opD}, {opD, opA}};
        CycleFinder cycleFinder(_syncPal->_operationsSorterWorker->_reorderings);
        cycleFinder.findCompleteCycle();

        CPPUNIT_ASSERT_EQUAL(true, cycleFinder.hasCompleteCycle());
        CPPUNIT_ASSERT_EQUAL(4, static_cast<int>(cycleFinder.completeCycle().opSortedList().size()));
        const std::vector expectedOpIdsInCycle = {opA->id(), opB->id(), opC->id(), opD->id()};
        for (size_t i = 0; const auto &id: cycleFinder.completeCycle().opSortedList()) {
            CPPUNIT_ASSERT_EQUAL(expectedOpIdsInCycle[i++], id);
        }
    }

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
        for (size_t i = 0; const auto &id: cycleFinder.completeCycle().opSortedList()) {
            CPPUNIT_ASSERT_EQUAL(expectedOpIdsInCycle[i++], id);
        }
    }

    {
        // Cycle, the order in which reorderings are found DO NOT follow the order of the operations in the cycle.
        _syncPal->_operationsSorterWorker->_reorderings = {{opB, opC}, {opD, opA}, {opA, opB}, {opC, opD}};
        CycleFinder cycleFinder(_syncPal->_operationsSorterWorker->_reorderings);
        cycleFinder.findCompleteCycle();

        CPPUNIT_ASSERT_EQUAL(true, cycleFinder.hasCompleteCycle());
        CPPUNIT_ASSERT_EQUAL(4, static_cast<int>(cycleFinder.completeCycle().opSortedList().size()));
        const std::vector expectedOpIdsInCycle = {opB->id(), opC->id(), opD->id(), opA->id()};
        for (size_t i = 0; const auto &id: cycleFinder.completeCycle().opSortedList()) {
            CPPUNIT_ASSERT_EQUAL(expectedOpIdsInCycle[i++], id);
        }
    }

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
        for (size_t i = 0; const auto &id: cycleFinder.completeCycle().opSortedList()) {
            CPPUNIT_ASSERT_EQUAL(expectedOpIdsInCycle[i++], id);
        }
    }

    {
        // A cycle hidden within a chain
        _syncPal->_operationsSorterWorker->_reorderings = {{opB, opA}, {opA, opC}, {opC, opA}};
        CycleFinder cycleFinder(_syncPal->_operationsSorterWorker->_reorderings);
        cycleFinder.findCompleteCycle();

        CPPUNIT_ASSERT_EQUAL(true, cycleFinder.hasCompleteCycle());
        CPPUNIT_ASSERT_EQUAL(2, static_cast<int>(cycleFinder.completeCycle().opSortedList().size()));
        const std::vector expectedOpIdsInCycle = {opA->id(), opC->id()};
        for (size_t i = 0; const auto &id: cycleFinder.completeCycle().opSortedList()) {
            CPPUNIT_ASSERT_EQUAL(expectedOpIdsInCycle[i++], id);
        }
    }

    {
        // 2 cycles
        _syncPal->_operationsSorterWorker->_reorderings = {{opA, opB}, {opB, opA}, {opB, opC}, {opC, opD}, {opD, opA}};
        CycleFinder cycleFinder(_syncPal->_operationsSorterWorker->_reorderings);
        cycleFinder.findCompleteCycle();

        CPPUNIT_ASSERT_EQUAL(true, cycleFinder.hasCompleteCycle());
        CPPUNIT_ASSERT_EQUAL(2, static_cast<int>(cycleFinder.completeCycle().opSortedList().size()));
        const std::vector expectedOpIdsInCycle = {opA->id(), opB->id()};
        for (size_t i = 0; const auto &id: cycleFinder.completeCycle().opSortedList()) {
            CPPUNIT_ASSERT_EQUAL(expectedOpIdsInCycle[i++], id);
        }
    }
}

void TestOperationSorterWorker::testBreakCycle() {
    // Initial situation
    // .
    // └── A(a)
    const auto nodeA = _testSituationGenerator.getNode(ReplicaSide::Local, "a");
    const auto correspondingNodeA = _testSituationGenerator.getNode(ReplicaSide::Remote, "a");

    // Final situation
    // .
    // └── A(a2)
    //     └── A*(a)

    // Create a new node A(a2)
    const auto nodeA2 = _testSituationGenerator.createNode(ReplicaSide::Local, NodeType::Directory, "a_new", "");
    nodeA2->setName(Str("A"));
    const auto createOp = generateSyncOperation(OperationType::Create, nodeA2);

    // Move A(a) to A(a2)/A*(a)
    _testSituationGenerator.moveNode(ReplicaSide::Local, *nodeA->id(), *nodeA2->id());
    nodeA->setName(Str("A*"));
    const auto moveOp = generateSyncOperation(OperationType::Move, nodeA);

    SyncOperationList cycle;
    cycle.pushOp(createOp);
    cycle.pushOp(moveOp);

    const auto breakCycleOp = std::make_shared<SyncOperation>();
    _syncPal->_operationsSorterWorker->breakCycle(cycle, breakCycleOp);

    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Remote, breakCycleOp->targetSide());
    CPPUNIT_ASSERT_EQUAL(nodeA, breakCycleOp->affectedNode());
    CPPUNIT_ASSERT_EQUAL(correspondingNodeA, breakCycleOp->correspondingNode());
    CPPUNIT_ASSERT_EQUAL(true, breakCycleOp->isBreakingCycleOp());
    CPPUNIT_ASSERT(!breakCycleOp->newName().empty());
    CPPUNIT_ASSERT(breakCycleOp->newParentNode());
}

void TestOperationSorterWorker::testBreakCycle2() {
    // Initial situation
    // .
    // └── A(a)
    //     └── AA(aa)
    const auto nodeA = _testSituationGenerator.getNode(ReplicaSide::Local, "a");
    const auto nodeAA = _testSituationGenerator.getNode(ReplicaSide::Local, "aa");
    const auto correspondingNodeA = _testSituationGenerator.getNode(ReplicaSide::Remote, "a");

    // Final situation
    // .
    // └── A(aa)

    // Move A(a)/AA(aa) to A(aa)
    _testSituationGenerator.moveNode(ReplicaSide::Local, *nodeAA->id(), {});
    nodeAA->setName(Str("A"));
    const auto moveOp = generateSyncOperation(OperationType::Move, nodeAA);

    // Delete A(a)
    nodeA->setChangeEvents(OperationType::Delete);
    const auto deleteOp = generateSyncOperation(OperationType::Delete, nodeA);

    SyncOperationList cycle;
    cycle.pushOp(deleteOp);
    cycle.pushOp(moveOp);

    const auto breakCycleOp = std::make_shared<SyncOperation>();
    _syncPal->_operationsSorterWorker->breakCycle(cycle, breakCycleOp);

    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Remote, breakCycleOp->targetSide());
    CPPUNIT_ASSERT_EQUAL(nodeA, breakCycleOp->affectedNode());
    CPPUNIT_ASSERT_EQUAL(correspondingNodeA, breakCycleOp->correspondingNode());
    CPPUNIT_ASSERT_EQUAL(true, breakCycleOp->isBreakingCycleOp());
    CPPUNIT_ASSERT(!breakCycleOp->newName().empty());
    CPPUNIT_ASSERT(breakCycleOp->newParentNode());
}

SyncOpPtr TestOperationSorterWorker::generateSyncOperation(const OperationType opType,
                                                           const std::shared_ptr<Node> &affectedNode) const {
    const auto op = std::make_shared<SyncOperation>();
    op->setType(opType);
    op->setAffectedNode(affectedNode);
    const auto targetSide = otherSide(affectedNode->side());
    if (opType != OperationType::Create) {
        op->setCorrespondingNode(_testSituationGenerator.getNode(targetSide, *affectedNode->id()));
    }
    op->setNewName(affectedNode->name());
    op->setTargetSide(targetSide);
    op->setNewParentNode(affectedNode->parentNode());
    return op;
}

} // namespace KDC
