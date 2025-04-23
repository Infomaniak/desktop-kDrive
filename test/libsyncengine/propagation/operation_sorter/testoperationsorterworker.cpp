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
#include "mocks/libcommonserver/db/mockdb.h"

#include "test_classes/testsituationgenerator.h"
#include "test_utility/testhelpers.h"
#include "utility/timerutility.h"

using namespace CppUnit;

namespace KDC {

void TestOperationSorterWorker::setUp() {
    TestBase::start();
    // Create SyncPal
    bool alreadyExists = false;
    const auto parmsDbPath = MockDb::makeDbName(alreadyExists);
    (void) ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);

    SyncPath syncDbPath = Db::makeDbName(1, 1, 1, 1, alreadyExists);
    (void) std::filesystem::remove(syncDbPath);
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
    (void) _syncPal->syncOps()->pushOp(opA);
    (void) _syncPal->syncOps()->pushOp(opB);
    (void) _syncPal->syncOps()->pushOp(opC);

    // Move opB after opA -> nothing happens, opB is already after opA.
    _syncPal->_operationsSorterWorker->moveFirstAfterSecond(opB, opA);
    CPPUNIT_ASSERT_EQUAL(opA->id(), _syncPal->syncOps()->opSortedList().front());
    CPPUNIT_ASSERT_EQUAL(false, _syncPal->_operationsSorterWorker->hasOrderChanged());

    // Move opA after opB.
    _syncPal->_operationsSorterWorker->moveFirstAfterSecond(opA, opB);
    CPPUNIT_ASSERT_EQUAL(opB->id(), _syncPal->syncOps()->opSortedList().front());
    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    CPPUNIT_ASSERT_EQUAL(opC->id(), _syncPal->syncOps()->opSortedList().back());
}

// delete before move, e.g. user deletes an object at path "x" and moves another object "a" to "x".
void TestOperationSorterWorker::testFixDeleteBeforeMove() {
    generateLotsOfDummySyncOperations(OperationType::Delete, OperationType::Move);

    // Delete node A
    const auto nodeA = _testSituationGenerator.deleteNode(ReplicaSide::Local, "a");
    const auto deleteOp = generateSyncOperation(OperationType::Delete, nodeA);

    // Rename B into A
    const auto nodeB = _testSituationGenerator.renameNode(ReplicaSide::Local, "b", Str("A"));
    const auto moveOp = generateSyncOperation(OperationType::Move, nodeB);

    (void) _syncPal->syncOps()->pushOp(moveOp);
    (void) _syncPal->syncOps()->pushOp(deleteOp);

    const TimerUtility timer;
    _syncPal->_operationsSorterWorker->fixDeleteBeforeMove();
    (void) timer.elapsed("Operation sorted in");

    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    std::unordered_map<UniqueId, uint32_t> mapIndex = {{moveOp->id(), 0}, {deleteOp->id(), 0}};
    findIndexesInOpList(mapIndex);
    CPPUNIT_ASSERT_LESS(mapIndex[moveOp->id()], mapIndex[deleteOp->id()]);
}

void TestOperationSorterWorker::testFixDeleteBeforeMoveOptimized() {
    generateLotsOfDummySyncOperations(OperationType::Delete, OperationType::Move);

    // Delete node A
    const auto nodeA = _testSituationGenerator.deleteNode(ReplicaSide::Local, "a");
    const auto deleteOp = generateSyncOperation(OperationType::Delete, nodeA);

    // Rename B into A
    const auto nodeB = _testSituationGenerator.renameNode(ReplicaSide::Local, "b", Str("A"));
    const auto moveOp = generateSyncOperation(OperationType::Move, nodeB);

    (void) _syncPal->syncOps()->pushOp(moveOp);
    (void) _syncPal->syncOps()->pushOp(deleteOp);

    const TimerUtility timer;
    _syncPal->_operationsSorterWorker->_filter.filterOperations();
    _syncPal->_operationsSorterWorker->fixDeleteBeforeMoveOptimized();
    (void) timer.elapsed("Operation sorted in");

    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    std::unordered_map<UniqueId, uint32_t> mapIndex = {{moveOp->id(), 0}, {deleteOp->id(), 0}};
    findIndexesInOpList(mapIndex);
    CPPUNIT_ASSERT_LESS(mapIndex[moveOp->id()], mapIndex[deleteOp->id()]);
}

// move before create, e.g. user moves an object "a" to "b" and creates another object at "a".
void TestOperationSorterWorker::testFixMoveBeforeCreate() {
    generateLotsOfDummySyncOperations(OperationType::Move, OperationType::Create);

    // Move A to B/A
    const auto nodeA = _testSituationGenerator.moveNode(ReplicaSide::Local, "a", "b");
    const auto moveOp = generateSyncOperation(OperationType::Move, nodeA);

    // Create A
    const auto nodeA2 = _testSituationGenerator.createNode(ReplicaSide::Local, NodeType::File, "a2", "");
    nodeA2->setName(Str("A"));
    const auto createOp = generateSyncOperation(OperationType::Create, nodeA2);

    (void) _syncPal->syncOps()->pushOp(createOp);
    (void) _syncPal->syncOps()->pushOp(moveOp);

    const TimerUtility timer;
    _syncPal->_operationsSorterWorker->fixMoveBeforeCreate();
    (void) timer.elapsed("Operation sorted in");

    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    std::unordered_map<UniqueId, uint32_t> mapIndex = {{moveOp->id(), 0}, {createOp->id(), 0}};
    findIndexesInOpList(mapIndex);
    CPPUNIT_ASSERT_LESS(mapIndex[createOp->id()], mapIndex[moveOp->id()]);
}

void TestOperationSorterWorker::testFixMoveBeforeCreateOptimized() {
    generateLotsOfDummySyncOperations(OperationType::Move, OperationType::Create);

    // Move A to B/A
    const auto nodeA = _testSituationGenerator.moveNode(ReplicaSide::Local, "a", "b");
    const auto moveOp = generateSyncOperation(OperationType::Move, nodeA);

    // Create A
    const auto nodeA2 = _testSituationGenerator.createNode(ReplicaSide::Local, NodeType::File, "a2", "");
    nodeA2->setName(Str("A"));
    const auto createOp = generateSyncOperation(OperationType::Create, nodeA2);

    (void) _syncPal->syncOps()->pushOp(createOp);
    (void) _syncPal->syncOps()->pushOp(moveOp);

    const TimerUtility timer;
    _syncPal->_operationsSorterWorker->_filter.filterOperations();
    _syncPal->_operationsSorterWorker->fixMoveBeforeCreateOptimized();
    (void) timer.elapsed("Operation sorted in");

    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    std::unordered_map<UniqueId, uint32_t> mapIndex = {{moveOp->id(), 0}, {createOp->id(), 0}};
    findIndexesInOpList(mapIndex);
    CPPUNIT_ASSERT_LESS(mapIndex[createOp->id()], mapIndex[moveOp->id()]);
}

// move before delete, e.g. user moves object "X/y" outside of directory "X" (e.g. to "z") and then deletes "X".
void TestOperationSorterWorker::testFixMoveBeforeDelete() {
    generateLotsOfDummySyncOperations(OperationType::Move, OperationType::Delete, NodeType::Directory);

    // Move A/AA/AAA to AAA
    const auto nodeAAA = _testSituationGenerator.moveNode(ReplicaSide::Local, "aaa", "");
    const auto moveOp = generateSyncOperation(OperationType::Move, nodeAAA);

    // Delete A
    const auto nodeA = _testSituationGenerator.deleteNode(ReplicaSide::Local, "a");
    const auto deleteOp = generateSyncOperation(OperationType::Delete, nodeA);

    (void) _syncPal->syncOps()->pushOp(deleteOp);
    (void) _syncPal->syncOps()->pushOp(moveOp);

    const TimerUtility timer;
    _syncPal->_operationsSorterWorker->fixMoveBeforeDelete();
    (void) timer.elapsed("Operation sorted in");

    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    std::unordered_map<UniqueId, uint32_t> mapIndex = {{deleteOp->id(), 0}, {moveOp->id(), 0}};
    findIndexesInOpList(mapIndex);
    CPPUNIT_ASSERT_LESS(mapIndex[deleteOp->id()], mapIndex[moveOp->id()]);
}

void TestOperationSorterWorker::testFixMoveBeforeDeleteOptimized() {
    generateLotsOfDummySyncOperations(OperationType::Move, OperationType::Delete, NodeType::Directory);

    // Move A/AA/AAA to AAA
    const auto nodeAAA = _testSituationGenerator.moveNode(ReplicaSide::Local, "aaa", "");
    const auto moveOp = generateSyncOperation(OperationType::Move, nodeAAA);

    // Delete A
    const auto nodeA = _testSituationGenerator.deleteNode(ReplicaSide::Local, "a");
    const auto deleteOp = generateSyncOperation(OperationType::Delete, nodeA);

    (void) _syncPal->syncOps()->pushOp(deleteOp);
    (void) _syncPal->syncOps()->pushOp(moveOp);

    const TimerUtility timer;
    _syncPal->_operationsSorterWorker->_filter.filterOperations();
    _syncPal->_operationsSorterWorker->fixMoveBeforeDeleteOptimized();
    (void) timer.elapsed("Operation sorted in");

    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    std::unordered_map<UniqueId, uint32_t> mapIndex = {{deleteOp->id(), 0}, {moveOp->id(), 0}};
    findIndexesInOpList(mapIndex);
    CPPUNIT_ASSERT_LESS(mapIndex[deleteOp->id()], mapIndex[moveOp->id()]);
}

// create before move, e.g. user creates directory "X" and moves object "y" into "X".
void TestOperationSorterWorker::testFixCreateBeforeMove() {
    generateLotsOfDummySyncOperations(OperationType::Create, OperationType::Move, NodeType::Directory);

    // Create E
    const auto nodeE = _testSituationGenerator.createNode(ReplicaSide::Local, NodeType::Directory, "e", "");
    const auto createOp = generateSyncOperation(OperationType::Create, nodeE);

    // Move A to E/A
    const auto nodeA = _testSituationGenerator.moveNode(ReplicaSide::Local, "a", *nodeE->id());
    const auto moveOp = generateSyncOperation(OperationType::Move, nodeA);

    (void) _syncPal->syncOps()->pushOp(moveOp);
    (void) _syncPal->syncOps()->pushOp(createOp);

    const TimerUtility timer;
    _syncPal->_operationsSorterWorker->fixCreateBeforeMove();
    (void) timer.elapsed("Operation sorted in");

    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    std::unordered_map<UniqueId, uint32_t> mapIndex = {{createOp->id(), 0}, {moveOp->id(), 0}};
    findIndexesInOpList(mapIndex);
    CPPUNIT_ASSERT_LESS(mapIndex[moveOp->id()], mapIndex[createOp->id()]);
}

void TestOperationSorterWorker::testFixCreateBeforeMoveOptimized() {
    generateLotsOfDummySyncOperations(OperationType::Create, OperationType::Move, NodeType::Directory);

    // Create E
    const auto nodeE = _testSituationGenerator.createNode(ReplicaSide::Local, NodeType::Directory, "e", "");
    const auto createOp = generateSyncOperation(OperationType::Create, nodeE);

    // Move A to E/A
    const auto nodeA = _testSituationGenerator.moveNode(ReplicaSide::Local, "a", *nodeE->id());
    const auto moveOp = generateSyncOperation(OperationType::Move, nodeA);

    (void) _syncPal->syncOps()->pushOp(moveOp);
    (void) _syncPal->syncOps()->pushOp(createOp);

    const TimerUtility timer;
    _syncPal->_operationsSorterWorker->_filter.filterOperations();
    _syncPal->_operationsSorterWorker->fixCreateBeforeMoveOptimized();
    (void) timer.elapsed("Operation sorted in");

    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    std::unordered_map<UniqueId, uint32_t> mapIndex = {{createOp->id(), 0}, {moveOp->id(), 0}};
    findIndexesInOpList(mapIndex);
    CPPUNIT_ASSERT_LESS(mapIndex[moveOp->id()], mapIndex[createOp->id()]);
}

// delete before create, e.g. user deletes object "x" and then creates a new object at "x".
void TestOperationSorterWorker::testFixDeleteBeforeCreate() {
    generateLotsOfDummySyncOperations(OperationType::Delete, OperationType::Create);

    // Delete AAA
    const auto nodeAAA = _testSituationGenerator.deleteNode(ReplicaSide::Local, "aaa");
    const auto deleteOp = generateSyncOperation(OperationType::Delete, nodeAAA);

    // Create AAA
    const auto nodeAAA2 = _testSituationGenerator.createNode(ReplicaSide::Local, NodeType::File, "aaa2", "aa");
    nodeAAA2->setName(Str("AAA"));
    const auto createOp = generateSyncOperation(OperationType::Create, nodeAAA2);

    (void) _syncPal->syncOps()->pushOp(createOp);
    (void) _syncPal->syncOps()->pushOp(deleteOp);

    const TimerUtility timer;
    _syncPal->_operationsSorterWorker->fixDeleteBeforeCreate();
    (void) timer.elapsed("Operation sorted in");

    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    std::unordered_map<UniqueId, uint32_t> mapIndex = {{createOp->id(), 0}, {deleteOp->id(), 0}};
    findIndexesInOpList(mapIndex);
    CPPUNIT_ASSERT_LESS(mapIndex[createOp->id()], mapIndex[deleteOp->id()]);
}

void TestOperationSorterWorker::testFixDeleteBeforeCreateOptimized() {
    generateLotsOfDummySyncOperations(OperationType::Delete, OperationType::Create);

    // Delete AAA
    const auto nodeAAA = _testSituationGenerator.deleteNode(ReplicaSide::Local, "aaa");
    const auto deleteOp = generateSyncOperation(OperationType::Delete, nodeAAA);

    // Create AAA
    const auto nodeAAA2 = _testSituationGenerator.createNode(ReplicaSide::Local, NodeType::File, "aaa2", "aa");
    nodeAAA2->setName(Str("AAA"));
    const auto createOp = generateSyncOperation(OperationType::Create, nodeAAA2);

    (void) _syncPal->syncOps()->pushOp(createOp);
    (void) _syncPal->syncOps()->pushOp(deleteOp);

    const TimerUtility timer;
    _syncPal->_operationsSorterWorker->_filter.filterOperations();
    _syncPal->_operationsSorterWorker->fixDeleteBeforeCreateOptimized();
    (void) timer.elapsed("Operation sorted in");

    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    std::unordered_map<UniqueId, uint32_t> mapIndex = {{createOp->id(), 0}, {deleteOp->id(), 0}};
    findIndexesInOpList(mapIndex);
    CPPUNIT_ASSERT_LESS(mapIndex[createOp->id()], mapIndex[deleteOp->id()]);
}

// move before move (occupation), e.g. user moves file "a" to "temp" and then moves file "b" to "a".
void TestOperationSorterWorker::testFixMoveBeforeMoveOccupied() {
    generateLotsOfDummySyncOperations(OperationType::Move, OperationType::Move);

    // Move A/AA/AAA to AAA
    const auto nodeAAA = _testSituationGenerator.moveNode(ReplicaSide::Local, "aaa", {});
    const auto moveOp = generateSyncOperation(OperationType::Move, nodeAAA);

    // Move C to A/AA/AAA
    const auto nodeC = _testSituationGenerator.moveNode(ReplicaSide::Local, "c", "aa", Str("AAA"));
    const auto moveOp2 = generateSyncOperation(OperationType::Move, nodeC);

    (void) _syncPal->syncOps()->pushOp(moveOp2);
    (void) _syncPal->syncOps()->pushOp(moveOp);

    const TimerUtility timer;
    _syncPal->_operationsSorterWorker->fixMoveBeforeMoveOccupied();
    (void) timer.elapsed("Operation sorted in");

    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    std::unordered_map<UniqueId, uint32_t> mapIndex = {{moveOp2->id(), 0}, {moveOp->id(), 0}};
    findIndexesInOpList(mapIndex);
    CPPUNIT_ASSERT_LESS(mapIndex[moveOp2->id()], mapIndex[moveOp->id()]);
}

void TestOperationSorterWorker::testFixMoveBeforeMoveOccupiedOptimized() {
    generateLotsOfDummySyncOperations(OperationType::Move, OperationType::Move);

    // Move A/AA/AAA to AAA
    const auto nodeAAA = _testSituationGenerator.moveNode(ReplicaSide::Local, "aaa", {});
    const auto moveOp = generateSyncOperation(OperationType::Move, nodeAAA);

    // Move C to A/AA/AAA
    const auto nodeC = _testSituationGenerator.moveNode(ReplicaSide::Local, "c", "aa", Str("AAA"));
    const auto moveOp2 = generateSyncOperation(OperationType::Move, nodeC);

    (void) _syncPal->syncOps()->pushOp(moveOp2);
    (void) _syncPal->syncOps()->pushOp(moveOp);

    const TimerUtility timer;
    _syncPal->_operationsSorterWorker->_filter.filterOperations();
    _syncPal->_operationsSorterWorker->fixMoveBeforeMoveOccupiedOptimized();
    (void) timer.elapsed("Operation sorted in");

    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    std::unordered_map<UniqueId, uint32_t> mapIndex = {{moveOp2->id(), 0}, {moveOp->id(), 0}};
    findIndexesInOpList(mapIndex);
    CPPUNIT_ASSERT_LESS(mapIndex[moveOp2->id()], mapIndex[moveOp->id()]);
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

    _syncPal->syncOps()->clear();
    {
        // Case : DAA DAB DA DB D
        (void) _syncPal->syncOps()->pushOp(opDAA);
        (void) _syncPal->syncOps()->pushOp(opDAB);
        (void) _syncPal->syncOps()->pushOp(opDA);
        (void) _syncPal->syncOps()->pushOp(opDB);
        (void) _syncPal->syncOps()->pushOp(opD);

        // Test hasParentWithHigherIndex
        std::unordered_map<UniqueId, int32_t> opIdToIndexMap;
        _syncPal->syncOps()->getOpIdToIndexMap(opIdToIndexMap, OperationType::Create);
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

        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->syncOps(), opD, opDA));
        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->syncOps(), opD, opDB));
        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->syncOps(), opDA, opDAA));
        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->syncOps(), opDA, opDAB));
    }

    _syncPal->syncOps()->clear();
    {
        // Case : DAA DAB D DA DB
        (void) _syncPal->syncOps()->pushOp(opDAA);
        (void) _syncPal->syncOps()->pushOp(opDAB);
        (void) _syncPal->syncOps()->pushOp(opD);
        (void) _syncPal->syncOps()->pushOp(opDA);
        (void) _syncPal->syncOps()->pushOp(opDB);

        do {
            _syncPal->_operationsSorterWorker->_hasOrderChanged = false;
            _syncPal->_operationsSorterWorker->fixCreateBeforeCreate();
        } while (_syncPal->_operationsSorterWorker->_hasOrderChanged);

        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->syncOps(), opD, opDA));
        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->syncOps(), opD, opDB));
        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->syncOps(), opDA, opDAA));
        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->syncOps(), opDA, opDAB));
    }

    _syncPal->syncOps()->clear();
    {
        // Case : DA DAA DAB D DB
        (void) _syncPal->syncOps()->pushOp(opDA);
        (void) _syncPal->syncOps()->pushOp(opDAA);
        (void) _syncPal->syncOps()->pushOp(opDAB);
        (void) _syncPal->syncOps()->pushOp(opD);
        (void) _syncPal->syncOps()->pushOp(opDB);

        do {
            _syncPal->_operationsSorterWorker->_hasOrderChanged = false;
            _syncPal->_operationsSorterWorker->fixCreateBeforeCreate();
        } while (_syncPal->_operationsSorterWorker->_hasOrderChanged);

        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->syncOps(), opD, opDA));
        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->syncOps(), opD, opDB));
        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->syncOps(), opDA, opDAA));
        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->syncOps(), opDA, opDAB));
    }

    _syncPal->syncOps()->clear();
    {
        // Case : D DA DB DAA DAB
        (void) _syncPal->syncOps()->pushOp(opD);
        (void) _syncPal->syncOps()->pushOp(opDA);
        (void) _syncPal->syncOps()->pushOp(opDB);
        (void) _syncPal->syncOps()->pushOp(opDAA);
        (void) _syncPal->syncOps()->pushOp(opDAB);

        do {
            _syncPal->_operationsSorterWorker->_hasOrderChanged = false;
            _syncPal->_operationsSorterWorker->fixCreateBeforeCreate();
        } while (_syncPal->_operationsSorterWorker->_hasOrderChanged);

        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->syncOps(), opD, opDA));
        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->syncOps(), opD, opDB));
        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->syncOps(), opDA, opDAA));
        CPPUNIT_ASSERT(isFirstBeforeSecond(_syncPal->syncOps(), opDA, opDAB));
    }
}

// edit before move, e.g. user moves an object "a" to "b" and then edit it.
void TestOperationSorterWorker::testFixEditBeforeMove() {
    generateLotsOfDummySyncOperations(OperationType::Move, OperationType::Edit);

    // Move A/AA/AAA to AAA
    const auto nodeAAA = _testSituationGenerator.moveNode(ReplicaSide::Local, "aaa", {});
    const auto moveOp = generateSyncOperation(OperationType::Move, nodeAAA);

    // Edit AAA
    (void) _testSituationGenerator.editNode(ReplicaSide::Local, *nodeAAA->id());
    const auto editOp = generateSyncOperation(OperationType::Edit, nodeAAA);

    (void) _syncPal->syncOps()->pushOp(editOp);
    (void) _syncPal->syncOps()->pushOp(moveOp);

    const TimerUtility timer;
    _syncPal->_operationsSorterWorker->fixEditBeforeMove();
    (void) timer.elapsed("Operation sorted in");

    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    std::unordered_map<UniqueId, uint32_t> mapIndex = {{editOp->id(), 0}, {moveOp->id(), 0}};
    findIndexesInOpList(mapIndex);
    CPPUNIT_ASSERT_LESS(mapIndex[editOp->id()], mapIndex[moveOp->id()]);
}

void TestOperationSorterWorker::testFixEditBeforeMoveOptimized() {
    generateLotsOfDummySyncOperations(OperationType::Move, OperationType::Edit);

    // Move A/AA/AAA to AAA
    const auto nodeAAA = _testSituationGenerator.moveNode(ReplicaSide::Local, "aaa", {});
    const auto moveOp = generateSyncOperation(OperationType::Move, nodeAAA);

    // Edit AAA
    (void) _testSituationGenerator.editNode(ReplicaSide::Local, *nodeAAA->id());
    const auto editOp = generateSyncOperation(OperationType::Edit, nodeAAA);

    (void) _syncPal->syncOps()->pushOp(editOp);
    (void) _syncPal->syncOps()->pushOp(moveOp);

    const TimerUtility timer;
    _syncPal->_operationsSorterWorker->_filter.filterOperations();
    _syncPal->_operationsSorterWorker->fixEditBeforeMoveOptimized();
    (void) timer.elapsed("Operation sorted in");

    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    std::unordered_map<UniqueId, uint32_t> mapIndex = {{editOp->id(), 0}, {moveOp->id(), 0}};
    findIndexesInOpList(mapIndex);
    CPPUNIT_ASSERT_LESS(mapIndex[editOp->id()], mapIndex[moveOp->id()]);
}

// move before move (parent-child flip), e.g. user moves directory "A/B" to "D", then moves directory "A" to "D/A" (parent-child
// relationships are now flipped).
void TestOperationSorterWorker::testFixMoveBeforeMoveParentChildFlip() {
    generateLotsOfDummySyncOperations(OperationType::Move, OperationType::Move, NodeType::Directory);

    // Move A/AA to D
    const auto nodeAA = _testSituationGenerator.moveNode(ReplicaSide::Local, "aa", {}, Str("D"));
    const auto moveOp1 = generateSyncOperation(OperationType::Move, nodeAA);

    // Move A to D
    const auto nodeA = _testSituationGenerator.moveNode(ReplicaSide::Local, "a", *nodeAA->id());
    const auto moveOp2 = generateSyncOperation(OperationType::Move, nodeA);

    (void) _syncPal->syncOps()->pushOp(moveOp2);
    (void) _syncPal->syncOps()->pushOp(moveOp1);

    const TimerUtility timer;
    _syncPal->_operationsSorterWorker->fixMoveBeforeMoveHierarchyFlip();
    (void) timer.elapsed("Operation sorted in");

    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    std::unordered_map<UniqueId, uint32_t> mapIndex = {{moveOp2->id(), 0}, {moveOp1->id(), 0}};
    findIndexesInOpList(mapIndex);
    CPPUNIT_ASSERT_LESS(mapIndex[moveOp2->id()], mapIndex[moveOp1->id()]);
}

void TestOperationSorterWorker::testFixMoveBeforeMoveParentChildFlipOptimized() {
    generateLotsOfDummySyncOperations(OperationType::Move, OperationType::Move, NodeType::Directory);

    // Move A/AA to D
    const auto nodeAA = _testSituationGenerator.moveNode(ReplicaSide::Local, "aa", {}, Str("D"));
    const auto moveOp1 = generateSyncOperation(OperationType::Move, nodeAA);

    // Move A to D
    const auto nodeA = _testSituationGenerator.moveNode(ReplicaSide::Local, "a", *nodeAA->id());
    const auto moveOp2 = generateSyncOperation(OperationType::Move, nodeA);

    (void) _syncPal->syncOps()->pushOp(moveOp2);
    (void) _syncPal->syncOps()->pushOp(moveOp1);

    const TimerUtility timer;
    _syncPal->_operationsSorterWorker->_filter.filterOperations();
    _syncPal->_operationsSorterWorker->fixMoveBeforeMoveHierarchyFlipOptimized();
    (void) timer.elapsed("Operation sorted in");

    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    std::unordered_map<UniqueId, uint32_t> mapIndex = {{moveOp2->id(), 0}, {moveOp1->id(), 0}};
    findIndexesInOpList(mapIndex);
    CPPUNIT_ASSERT_LESS(mapIndex[moveOp2->id()], mapIndex[moveOp1->id()]);
}

// move before move (parent-child flip), but it is not the direct parent that is moved
void TestOperationSorterWorker::testFixMoveBeforeMoveParentChildFlip2() {
    // Move A/AA/AAB to AAB
    const auto nodeAAB = _testSituationGenerator.moveNode(ReplicaSide::Local, "aab", {});
    const auto moveOp1 = generateSyncOperation(OperationType::Move, nodeAAB);

    // Move A to AAB/A
    const auto nodeA = _testSituationGenerator.moveNode(ReplicaSide::Local, "a", *nodeAAB->id());
    const auto moveOp2 = generateSyncOperation(OperationType::Move, nodeA);

    (void) _syncPal->syncOps()->pushOp(moveOp2);
    (void) _syncPal->syncOps()->pushOp(moveOp1);

    _syncPal->_operationsSorterWorker->fixMoveBeforeMoveHierarchyFlip();

    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    CPPUNIT_ASSERT_EQUAL(moveOp1->id(), _syncPal->syncOps()->opSortedList().front());
    CPPUNIT_ASSERT_EQUAL(moveOp2->id(), _syncPal->syncOps()->opSortedList().back());
}

void TestOperationSorterWorker::testFixMoveBeforeMoveParentChildFlip3() {
    // Move A/AA/AAB to AAB
    const auto nodeAAB = _testSituationGenerator.moveNode(ReplicaSide::Local, "aab", {});
    const auto moveOp1 = generateSyncOperation(OperationType::Move, nodeAAB);

    // Move A/AA to AAB/AA
    const auto nodeAA = _testSituationGenerator.moveNode(ReplicaSide::Local, "aa", *nodeAAB->id());
    const auto moveOp2 = generateSyncOperation(OperationType::Move, nodeAA);

    // Move A to AA/A
    const auto nodeA = _testSituationGenerator.moveNode(ReplicaSide::Local, "a", *nodeAA->id());
    const auto moveOp3 = generateSyncOperation(OperationType::Move, nodeA);

    (void) _syncPal->syncOps()->pushOp(moveOp3);
    (void) _syncPal->syncOps()->pushOp(moveOp2);
    (void) _syncPal->syncOps()->pushOp(moveOp1);

    _syncPal->_operationsSorterWorker->fixMoveBeforeMoveHierarchyFlip();

    CPPUNIT_ASSERT_EQUAL(true, _syncPal->_operationsSorterWorker->hasOrderChanged());
    CPPUNIT_ASSERT_EQUAL(moveOp1->id(), _syncPal->syncOps()->opSortedList().front());
    CPPUNIT_ASSERT_EQUAL(moveOp3->id(), _syncPal->syncOps()->opSortedList().back());
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
    (void) _testSituationGenerator.moveNode(ReplicaSide::Local, *lNodeB->id(), *lNodeAAB->id(), Str("B*"));
    const auto lMoveOpB = generateSyncOperation(OperationType::Move, lNodeB);

    // Move A/AA/AAB to B
    (void) _testSituationGenerator.moveNode(ReplicaSide::Local, *lNodeAAB->id(), {}, Str("B"));
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
    (void) _testSituationGenerator.moveNode(ReplicaSide::Remote, *rNodeA->id(), *rNodeB->id(), Str("A*"));
    const auto rMoveOpA = generateSyncOperation(OperationType::Move, rNodeA);

    // Rename C to C*
    _testSituationGenerator.renameNode(ReplicaSide::Remote, rNodeC->id().value(), Str("C*"));
    const auto rMoveOpC = generateSyncOperation(OperationType::Move, rNodeC);

    // Rename A/AA/AAA to A/AA/AAA*
    _testSituationGenerator.renameNode(ReplicaSide::Remote, rNodeAAA->id().value(), Str("AAA*"));
    const auto rMoveOpAAA = generateSyncOperation(OperationType::Move, rNodeAAA);

    _syncPal->syncOps()->setOpList({lMoveOpB, lMoveOpAAB, rMoveOpC, rMoveOpA, rMoveOpAAA});
    const auto reshuffledOp = _syncPal->_operationsSorterWorker->fixImpossibleFirstMoveOp();
    CPPUNIT_ASSERT(reshuffledOp);
    CPPUNIT_ASSERT(reshuffledOp->_opSortedList.size() == 2);
    CPPUNIT_ASSERT(reshuffledOp->_opSortedList.back() == rMoveOpA->id());
}

void TestOperationSorterWorker::testFindCompleteCycles() {
    const auto nodeA = _testSituationGenerator.renameNode(ReplicaSide::Local, "a", Str("A*"));
    const auto opA = generateSyncOperation(OperationType::Move, nodeA);

    const auto nodeB = _testSituationGenerator.renameNode(ReplicaSide::Local, "b", Str("B*"));
    const auto opB = generateSyncOperation(OperationType::Move, nodeB);

    const auto nodeC = _testSituationGenerator.renameNode(ReplicaSide::Local, "c", Str("C*"));
    const auto opC = generateSyncOperation(OperationType::Move, nodeC);

    const auto nodeD = _testSituationGenerator.renameNode(ReplicaSide::Local, "d", Str("D*"));
    const auto opD = generateSyncOperation(OperationType::Move, nodeD);

    const auto nodeAA = _testSituationGenerator.renameNode(ReplicaSide::Local, "aa", Str("AA*"));
    const auto opAA = generateSyncOperation(OperationType::Move, nodeAA);

    const auto nodeAAA = _testSituationGenerator.renameNode(ReplicaSide::Local, "aaa", Str("AAA*"));
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
    (void) _testSituationGenerator.moveNode(ReplicaSide::Local, *nodeA->id(), *nodeA2->id(), Str("A*"));
    const auto moveOp = generateSyncOperation(OperationType::Move, nodeA);

    SyncOperationList cycle;
    (void) cycle.pushOp(createOp);
    (void) cycle.pushOp(moveOp);

    const auto breakCycleOp = std::make_shared<SyncOperation>();
    (void) _syncPal->_operationsSorterWorker->breakCycle(cycle, breakCycleOp);

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
    (void) _testSituationGenerator.moveNode(ReplicaSide::Local, *nodeAA->id(), {}, Str("A"));
    const auto moveOp = generateSyncOperation(OperationType::Move, nodeAA);

    // Delete A(a)
    nodeA->setChangeEvents(OperationType::Delete);
    const auto deleteOp = generateSyncOperation(OperationType::Delete, nodeA);

    SyncOperationList cycle;
    (void) cycle.pushOp(deleteOp);
    (void) cycle.pushOp(moveOp);

    const auto breakCycleOp = std::make_shared<SyncOperation>();
    (void) _syncPal->_operationsSorterWorker->breakCycle(cycle, breakCycleOp);

    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Remote, breakCycleOp->targetSide());
    CPPUNIT_ASSERT_EQUAL(nodeA, breakCycleOp->affectedNode());
    CPPUNIT_ASSERT_EQUAL(correspondingNodeA, breakCycleOp->correspondingNode());
    CPPUNIT_ASSERT_EQUAL(true, breakCycleOp->isBreakingCycleOp());
    CPPUNIT_ASSERT(!breakCycleOp->newName().empty());
    CPPUNIT_ASSERT(breakCycleOp->newParentNode());
}

void TestOperationSorterWorker::testExtractOpsByType() {
    const auto dummyNode = _testSituationGenerator.createNode(ReplicaSide::Local, NodeType::File, "dummy", "", false);
    {
        const auto op1 = generateSyncOperation(OperationType::Create, dummyNode);
        const auto op2 = generateSyncOperation(OperationType::Delete, dummyNode);
        const auto [createOp, deleteOp] =
                _syncPal->_operationsSorterWorker->extractOpsByType(OperationType::Create, OperationType::Delete, op1, op2);
        CPPUNIT_ASSERT_EQUAL(OperationType::Create, createOp->type());
        CPPUNIT_ASSERT_EQUAL(OperationType::Delete, deleteOp->type());
        CPPUNIT_ASSERT(createOp != deleteOp);
    }
    {
        const auto op1 = generateSyncOperation(OperationType::Move, dummyNode);
        const auto op2 = generateSyncOperation(OperationType::Move, dummyNode);
        const auto [moveOp1, moveOp2] =
                _syncPal->_operationsSorterWorker->extractOpsByType(OperationType::Move, OperationType::Move, op1, op2);
        CPPUNIT_ASSERT_EQUAL(OperationType::Move, moveOp1->type());
        CPPUNIT_ASSERT_EQUAL(OperationType::Move, moveOp2->type());
        CPPUNIT_ASSERT(moveOp1 != moveOp2);
    }
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

void TestOperationSorterWorker::generateLotsOfDummySyncOperations(const OperationType opType1,
                                                                  const OperationType opType2 /*= OperationType::None*/,
                                                                  const NodeType nodeType /*= NodeType::File*/) const {
    const auto dummyNode = _testSituationGenerator.createNode(ReplicaSide::Local, nodeType, "dummy", "", false);
    dummyNode->setMoveOriginInfos({dummyNode->getPath(), "1"});
    for (const auto type: {opType1, opType2}) {
        if (type != OperationType::None) {
            // Generate 100 dummy operation
            for (uint32_t i = 0; i < 100; i++) {
                (void) _syncPal->syncOps()->pushOp(generateSyncOperation(type, dummyNode));
            }
        }
    }
}

void TestOperationSorterWorker::findIndexesInOpList(std::unordered_map<UniqueId, uint32_t> &mapIndex) const {
    uint32_t index = 0;
    uint32_t counter = 0;
    for (auto &id: _syncPal->syncOps()->opSortedList()) {
        if (mapIndex.contains(id)) {
            mapIndex[id] = index;
            counter++;
        }
        if (counter >= mapIndex.size()) {
            break;
        }
        ++index;
    }
}

} // namespace KDC
