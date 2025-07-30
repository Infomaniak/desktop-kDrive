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

#include "testconflictresolverworker.h"
#include "reconciliation/platform_inconsistency_checker/platforminconsistencycheckerutility.h"
#include "mocks/libcommonserver/db/mockdb.h"
#include "mocks/libsyncengine/vfs/mockvfs.h"

#include "test_utility/testhelpers.h"
#if defined(KD_MACOS)
#include "vfs/mac/vfs_mac.h"
#endif

#include <memory>

namespace KDC {

void TestConflictResolverWorker::setUp() {
    TestBase::start();
    // Create SyncPal
    bool alreadyExists = false;
    std::filesystem::path parmsDbPath = MockDb::makeDbName(alreadyExists);
    (void) ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);

    SyncPath syncDbPath = MockDb::makeDbName(1, 1, 1, 1, alreadyExists);
    (void) std::filesystem::remove(syncDbPath);
    _mockVfs = std::make_shared<MockVfs<VfsOff>>(VfsSetupParams(Log::instance()->getLogger()));
    _syncPal = std::make_shared<SyncPal>(_mockVfs, syncDbPath, KDRIVE_VERSION_STRING, true);
    _syncPal->syncDb()->setAutoDelete(true);
    _syncPal->createSharedObjects();
    _syncPal->createWorkers();

    _syncPal->_conflictResolverWorker = std::make_shared<ConflictResolverWorker>(_syncPal, "Conflict Resolver", "CORE");

    // Initial state:
    // .
    // └── A
    //     ├── AA
    //     │   └── AAA
    //     └── AB
    _testSituationGenerator.setSyncpal(_syncPal);
    _testSituationGenerator.generateInitialSituation(R"({"a":{"aa":{"aaa":1},"ab":{}}})");
}

void TestConflictResolverWorker::tearDown() {
    ParmsDb::instance()->close();
    ParmsDb::reset();
    if (_syncPal && _syncPal->syncDb()) {
        _syncPal->syncDb()->close();
    }
    TestBase::stop();
}

void TestConflictResolverWorker::testCreateCreate() {
    // Simulate file creation on both replica
    const auto lNodeAAB = _testSituationGenerator.createNode(ReplicaSide::Local, NodeType::File, "aab", "aa");
    const auto rNodeAAB = _testSituationGenerator.createNode(ReplicaSide::Remote, NodeType::File, "aab", "aa");

    const Conflict conflict(lNodeAAB, rNodeAAB, ConflictType::CreateCreate);
    _syncPal->_conflictQueue->push(conflict);
    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), _syncPal->_syncOps->size());
    const auto opId = _syncPal->_syncOps->opSortedList().front();
    const auto op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(!op->newName().empty());
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, op->targetSide());
    CPPUNIT_ASSERT_EQUAL(OperationType::Move, op->type());
}

void TestConflictResolverWorker::testEditEdit() {
    // Simulate edit conflict of file A/AA/AAA on both replica
    const auto lNodeAAA = _testSituationGenerator.editNode(ReplicaSide::Local, "aaa");
    const auto rNodeAAA = _testSituationGenerator.editNode(ReplicaSide::Remote, "aaa");

    const Conflict conflict(lNodeAAA, rNodeAAA, ConflictType::EditEdit);
    _syncPal->_conflictQueue->push(conflict);
    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), _syncPal->_syncOps->size());
    const auto opId = _syncPal->_syncOps->opSortedList().front();
    const auto op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(!op->newName().empty());
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, op->targetSide());
    CPPUNIT_ASSERT_EQUAL(OperationType::Move, op->type());
    CPPUNIT_ASSERT(op->affectedNode()->hasChangeEvent(OperationType::Move));
    CPPUNIT_ASSERT(op->affectedNode()->moveOriginInfos().parentNodeId() != defaultInvalidNodeId);
    CPPUNIT_ASSERT(op->affectedNode()->moveOriginInfos().path() != defaultInvalidPath);
}


void TestConflictResolverWorker::testOmitEditEdit() {
    // Simulate edit conflict of file A/AA/AAA on both replica
    const auto lNodeAAA = _testSituationGenerator.getNode(ReplicaSide::Local, "aaa");
    const auto rNodeAAA = _testSituationGenerator.editNode(ReplicaSide::Remote, "aaa");
    lNodeAAA->setCreatedAt(testhelpers::defaultTime + 1); // Editing only the creation time is considered an omit edit.
    lNodeAAA->setChangeEvents(OperationType::Edit);
    rNodeAAA->setChangeEvents(OperationType::Edit);

    const Conflict conflict(lNodeAAA, rNodeAAA, ConflictType::EditEdit);
    _syncPal->_conflictQueue->push(conflict);
    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), _syncPal->_syncOps->size());
    const auto opId = _syncPal->_syncOps->opSortedList().front();
    const auto op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Remote, op->targetSide());
    CPPUNIT_ASSERT_EQUAL(OperationType::Edit, op->type());
    CPPUNIT_ASSERT(op->omit());
    CPPUNIT_ASSERT_EQUAL(lNodeAAA, op->affectedNode());
    CPPUNIT_ASSERT_EQUAL(rNodeAAA, op->correspondingNode());
}

void TestConflictResolverWorker::testMoveCreate1() {
    // Simulate create file A/AB/ABA on local replica
    const auto lNodeABA = _testSituationGenerator.createNode(ReplicaSide::Local, NodeType::File, "aba", "ab");

    // Simulate move of file A/AA/AAA to A/AB/ABA on remote replica
    const auto rNodeAAA = _testSituationGenerator.moveNode(ReplicaSide::Remote, "aaa", "ab", Str("ABA"));

    const Conflict conflict(lNodeABA, rNodeAAA, ConflictType::MoveCreate);
    _syncPal->_conflictQueue->push(conflict);
    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), _syncPal->_syncOps->size());
    const auto opId = _syncPal->_syncOps->opSortedList().front();
    const auto op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, op->targetSide());
    CPPUNIT_ASSERT_EQUAL(OperationType::Move, op->type());
    CPPUNIT_ASSERT(op->relativeDestinationPath().filename().string().find("conflict") != 0);
}

void TestConflictResolverWorker::testMoveCreate2() {
    // Simulate create file A/AB/ABA on remote replica
    const auto lNodeABA = _testSituationGenerator.createNode(ReplicaSide::Remote, NodeType::File, "aba", "ab");

    // Simulate move of file A/AA/AAA to A/AB/ABA on local replica
    const auto rNodeAAA = _testSituationGenerator.moveNode(ReplicaSide::Local, "aaa", "ab", Str("ABA"));

    const Conflict conflict(rNodeAAA, lNodeABA, ConflictType::MoveCreate);
    _syncPal->_conflictQueue->push(conflict);
    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), _syncPal->_syncOps->size());
    const auto opId = _syncPal->_syncOps->opSortedList().front();
    const auto op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, op->targetSide());
    CPPUNIT_ASSERT_EQUAL(OperationType::Move, op->type());
    CPPUNIT_ASSERT_EQUAL(NodeId("l_aa"), op->newParentNode()->id().value());
}

void TestConflictResolverWorker::testMoveCreateDehydratedPlaceholder() {
    // Simulate move of A/AA/AAA to AAA on local replica
    const auto lNodeAAA = _testSituationGenerator.moveNode(ReplicaSide::Local, "aaa", "");

    // Simulate create of AAA on remote replica
    const auto rNodeAAA2 = _testSituationGenerator.createNode(ReplicaSide::Remote, NodeType::File, "aaa2", "");
    rNodeAAA2->setName(Str("AAA"));

    // Since the methods needed for tests are mocked, we can put any VirtualFileMode type. It just needs to be different from
    // VirtualFileMode::off
    _syncPal->setVfsMode(VirtualFileMode::Mac);
    // Simulate a local dehydrate placeholder
    auto mockStatus = []([[maybe_unused]] const SyncPath &absolutePath, VfsStatus &vfsStatus) -> ExitInfo {
        vfsStatus.isPlaceholder = true;
        vfsStatus.isHydrated = false;
        vfsStatus.isSyncing = false;
        vfsStatus.progress = 0;
        return ExitCode::Ok;
    };
    _mockVfs->setMockStatus(mockStatus);

    const Conflict conflict(lNodeAAA, rNodeAAA2, ConflictType::MoveCreate);
    _syncPal->_conflictQueue->push(conflict);
    _syncPal->_conflictResolverWorker->execute();

    const auto opId = _syncPal->_syncOps->opSortedList().front();
    const auto op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), _syncPal->_syncOps->size());
    CPPUNIT_ASSERT_EQUAL(false, op->omit());
    CPPUNIT_ASSERT_EQUAL(OperationType::Delete, op->type());
    CPPUNIT_ASSERT_EQUAL(lNodeAAA, op->correspondingNode());
}

void TestConflictResolverWorker::testOmitEditDelete() {
    // Simulate edit of file A/AA/AAA on local replica
    const auto lNodeAAA = _testSituationGenerator.getNode(ReplicaSide::Local, "aaa");
    lNodeAAA->setCreatedAt(testhelpers::defaultTime + 1); // Editing only the creation time is considered an omit edit.
    lNodeAAA->setChangeEvents(OperationType::Edit);

    // and delete of file A/AA/AAA on remote replica
    const auto rNodeAAA = _testSituationGenerator.getNode(ReplicaSide::Remote, "aaa");
    rNodeAAA->setChangeEvents(OperationType::Delete);

    const Conflict conflict(lNodeAAA, rNodeAAA, ConflictType::EditDelete);
    _syncPal->_conflictQueue->push(conflict);
    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), _syncPal->_syncOps->size());
    const auto opId = _syncPal->_syncOps->opSortedList().front();
    const auto op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Remote, op->targetSide());
    CPPUNIT_ASSERT_EQUAL(OperationType::Edit, op->type());
    CPPUNIT_ASSERT(op->omit());
    CPPUNIT_ASSERT_EQUAL(lNodeAAA, op->affectedNode());
    CPPUNIT_ASSERT_EQUAL(rNodeAAA, op->correspondingNode());
}


void TestConflictResolverWorker::testEditDelete1() {
    std::function<SyncOpPtr(ReplicaSide editSide)> generateAndResolveConflict = [&](ReplicaSide editSide) {
        _syncPal->_syncOps->clear();

        // Simulate edit of file A/AA/AAA on local replica
        const auto editNodeAAA = _testSituationGenerator.editNode(editSide, "aaa");
        editNodeAAA->setChangeEvents(OperationType::Edit);

        // and delete of file A/AA/AAA on remote replica
        const auto deleteNodeAAA = _testSituationGenerator.getNode(otherSide(editSide), "aaa");
        deleteNodeAAA->setChangeEvents(OperationType::Delete);

        const Conflict conflict(editNodeAAA, deleteNodeAAA, ConflictType::EditDelete);
        _syncPal->_conflictQueue->push(conflict);
        _syncPal->_conflictResolverWorker->execute();
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), _syncPal->_syncOps->size());
        const auto opId = _syncPal->_syncOps->opSortedList().front();
        return _syncPal->_syncOps->getOp(opId);
    };

    for (const auto side: {ReplicaSide::Local, ReplicaSide::Remote}) {
        auto resolveOperation = generateAndResolveConflict(side);
        CPPUNIT_ASSERT(resolveOperation);
        CPPUNIT_ASSERT(resolveOperation->newName().empty());
        CPPUNIT_ASSERT(resolveOperation->omit());
        CPPUNIT_ASSERT_EQUAL(OperationType::Delete, resolveOperation->type());
    }
}

void TestConflictResolverWorker::testEditDelete2() {
    std::function<SyncOpPtr(ReplicaSide editSide)> generateAndResolveConflict = [&](ReplicaSide editSide) {
        _syncPal->_syncOps->clear();

        // Simulate edit of file A/AA/AAA on editSide replica
        const auto editNodeAAA = _testSituationGenerator.editNode(editSide, "aaa");
        editNodeAAA->setChangeEvents(OperationType::Edit);

        // and delete of dir A/AA (and all children) on otherSide(editSide) replica
        const auto deleteNodeAA = _testSituationGenerator.deleteNode(otherSide(editSide), "aa");
        const auto deleteNodeAAA = _testSituationGenerator.getNode(otherSide(editSide), "aaa");

        const Conflict conflict(deleteNodeAAA, editNodeAAA, ConflictType::EditDelete);
        _syncPal->_conflictQueue->push(conflict);
        _syncPal->_conflictResolverWorker->execute();
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), _syncPal->_syncOps->size());
        const auto opId = _syncPal->_syncOps->opSortedList().front();
        return _syncPal->_syncOps->getOp(opId);
    };

    const auto resolveOperationLocal = generateAndResolveConflict(ReplicaSide::Local);
    CPPUNIT_ASSERT(!resolveOperationLocal->omit());
    CPPUNIT_ASSERT_EQUAL(OperationType::Move, resolveOperationLocal->type());
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, resolveOperationLocal->targetSide());
    CPPUNIT_ASSERT_EQUAL(true, resolveOperationLocal->isRescueOperation());

    const auto resolveOperationRemote = generateAndResolveConflict(ReplicaSide::Remote);
    CPPUNIT_ASSERT(!resolveOperationLocal->omit());
    CPPUNIT_ASSERT_EQUAL(OperationType::Delete, resolveOperationRemote->type());
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Remote, resolveOperationRemote->targetSide());
    CPPUNIT_ASSERT_EQUAL(true, resolveOperationLocal->isRescueOperation());
}

void TestConflictResolverWorker::testMoveDelete1() {
    // Simulate rename of node A to B on local replica
    const auto lNodeA = _testSituationGenerator.renameNode(ReplicaSide::Local, "a", Str("B"));

    // Simulate a delete of node A/AB on local replica
    const auto lNodeAB = _testSituationGenerator.deleteNode(ReplicaSide::Local, "ab");

    // Simulate a delete of node A on remote replica
    const auto rNodeA = _testSituationGenerator.deleteNode(ReplicaSide::Remote, "a");

    const Conflict conflict(lNodeA, rNodeA, ConflictType::MoveDelete);
    _syncPal->_conflictQueue->push(conflict);
    _syncPal->_conflictResolverWorker->execute();

    // Delete operation wins
    const auto opId = _syncPal->_syncOps->opSortedList().front();
    const auto op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), _syncPal->_syncOps->size());
    CPPUNIT_ASSERT_EQUAL(false, op->omit());
    CPPUNIT_ASSERT_EQUAL(OperationType::Delete, op->type());
    CPPUNIT_ASSERT_EQUAL(lNodeA, op->correspondingNode());
}

void TestConflictResolverWorker::testMoveDelete2() {
    // Simulate rename of node A to B on local replica
    const auto lNodeA = _testSituationGenerator.renameNode(ReplicaSide::Local, "a", Str("B"));

    // Simulate edit of node A/AA/AAA on local replica
    const auto lNodeAAA = _testSituationGenerator.editNode(ReplicaSide::Local, "aaa");

    // Simulate create of node A/AA/ABA on local replica
    const auto lNodeABA = _testSituationGenerator.createNode(ReplicaSide::Local, NodeType::File, "aba", "ab");

    // Simulate a delete of node A on remote replica
    const auto rNodeA = _testSituationGenerator.deleteNode(ReplicaSide::Remote, "a");

    const Conflict conflict(lNodeA, rNodeA, ConflictType::MoveDelete);
    _syncPal->_conflictQueue->push(conflict);
    _syncPal->_conflictResolverWorker->execute();

    // Delete operation wins but edited and created files should be rescued
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(3), _syncPal->_syncOps->size());

    std::array<SyncOpPtr, 3> ops;
    for (size_t i = 0; const auto opId: _syncPal->_syncOps->opSortedList()) {
        ops[i++] = _syncPal->_syncOps->getOp(opId);
    }

    const auto op1 = ops[0];
    CPPUNIT_ASSERT_EQUAL(false, op1->omit());
    CPPUNIT_ASSERT_EQUAL(OperationType::Move, op1->type());
    CPPUNIT_ASSERT_EQUAL(true, op1->isRescueOperation());

    const auto op2 = ops[1];
    CPPUNIT_ASSERT_EQUAL(false, op2->omit());
    CPPUNIT_ASSERT_EQUAL(OperationType::Move, op2->type());
    CPPUNIT_ASSERT_EQUAL(true, op2->isRescueOperation());

    const auto op3 = ops[2];
    CPPUNIT_ASSERT_EQUAL(false, op3->omit());
    CPPUNIT_ASSERT_EQUAL(OperationType::Delete, op3->type());
    CPPUNIT_ASSERT_EQUAL(false, op3->isRescueOperation());
    CPPUNIT_ASSERT_EQUAL(lNodeA, op3->correspondingNode());
}

void TestConflictResolverWorker::testMoveDelete3() {
    // Simulate rename of node A to B on local replica
    const auto lNodeA = _testSituationGenerator.renameNode(ReplicaSide::Local, "a", Str("B"));

    // Simulate move of node A/AB to AB on local replica
    const auto lNodeAB = _testSituationGenerator.moveNode(ReplicaSide::Local, "ab", "");

    // Simulate a delete of node A on remote replica
    const auto rNodeA = _testSituationGenerator.deleteNode(ReplicaSide::Remote, "a");

    const Conflict conflict(lNodeA, rNodeA, ConflictType::MoveDelete);
    _syncPal->_conflictQueue->push(conflict);
    _syncPal->_conflictResolverWorker->execute();

    // Delete operation wins
    const auto op = _syncPal->_syncOps->getOp(_syncPal->_syncOps->opSortedList().front());
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), _syncPal->_syncOps->size());
    CPPUNIT_ASSERT_EQUAL(false, op->omit());
    CPPUNIT_ASSERT_EQUAL(OperationType::Delete, op->type());
    CPPUNIT_ASSERT_EQUAL(lNodeA, op->correspondingNode());
}

void TestConflictResolverWorker::testMoveDelete4() {
    // Simulate rename of node A to B on local replica
    const auto lNodeA = _testSituationGenerator.renameNode(ReplicaSide::Local, "a", Str("B"));

    // Simulate move of node A/AB to AB on remote replica
    const auto rNodeAB = _testSituationGenerator.moveNode(ReplicaSide::Remote, "ab", "");

    // Simulate a delete of node A on remote replica
    const auto rNodeA = _testSituationGenerator.deleteNode(ReplicaSide::Remote, "a");

    const Conflict conflict(lNodeA, rNodeA, ConflictType::MoveDelete);
    _syncPal->_conflictQueue->push(conflict);
    _syncPal->_conflictResolverWorker->execute();

    // Delete operation wins
    const auto op = _syncPal->_syncOps->getOp(_syncPal->_syncOps->opSortedList().front());
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), _syncPal->_syncOps->size());
    CPPUNIT_ASSERT_EQUAL(false, op->omit());
    CPPUNIT_ASSERT_EQUAL(OperationType::Delete, op->type());
    CPPUNIT_ASSERT_EQUAL(lNodeA, op->correspondingNode());
}

void TestConflictResolverWorker::testMoveDelete5() {
    // Simulate rename of node AA to AA* on local replica
    const auto lNodeAA = _testSituationGenerator.renameNode(ReplicaSide::Local, "aa", Str("AA*"));

    // Simulate a delete of node A on remote replica
    const auto rNodeA = _testSituationGenerator.deleteNode(ReplicaSide::Remote, "a");
    const auto rNodeAA = _testSituationGenerator.getNode(ReplicaSide::Remote, "a");

    const Conflict conflict1(lNodeAA, rNodeAA, ConflictType::MoveDelete);
    _syncPal->_conflictQueue->push(conflict1);
    const Conflict conflict2(lNodeAA, rNodeA, ConflictType::MoveParentDelete);
    _syncPal->_conflictQueue->push(conflict2);

    // This should be treated as a Move-ParentDelete conflict, the Move-Delete conflict must be ignored.
    // For this test, we only make sure that the Move-Delete conflict is ignored and a Move-ParentDelete conflict resolution
    // operation is generated.
    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), _syncPal->_syncOps->opSortedList().size());
    const auto syncOp = _syncPal->_syncOps->getOp(_syncPal->_syncOps->opSortedList().front());
    CPPUNIT_ASSERT_EQUAL(ConflictType::MoveParentDelete, syncOp->conflict().type());
}

void TestConflictResolverWorker::testMoveDeletePlaceholder() {
    // Simulate move of A/AA/AAA to A on local replica
    const auto lNodeAAA = _testSituationGenerator.moveNode(ReplicaSide::Local, "aaa", "");

    // Simulate a delete of node AAA on remote replica
    const auto rNodeAAA = _testSituationGenerator.deleteNode(ReplicaSide::Remote, "aaa");

    const Conflict conflict(lNodeAAA, rNodeAAA, ConflictType::MoveDelete);

    // Since the methods needed for tests are mocked, we can put any VirtualFileMode type. It just needs to be different from
    // VirtualFileMode::off
    _syncPal->setVfsMode(VirtualFileMode::Mac);

    {
        _syncPal->_conflictQueue->push(conflict);

        // Simulate a local dehydrate placeholder
        auto mockStatus = []([[maybe_unused]] const SyncPath &absolutePath, VfsStatus &vfsStatus) -> ExitInfo {
            vfsStatus.isPlaceholder = true;
            vfsStatus.isHydrated = false;
            vfsStatus.isSyncing = false;
            vfsStatus.progress = 0;
            return ExitCode::Ok;
        };
        _mockVfs->setMockStatus(mockStatus);

        _syncPal->_conflictResolverWorker->execute();

        const auto op = _syncPal->_syncOps->getOp(_syncPal->_syncOps->opSortedList().front());
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), _syncPal->_syncOps->size());
        CPPUNIT_ASSERT_EQUAL(false, op->omit());
        CPPUNIT_ASSERT_EQUAL(OperationType::Delete, op->type());
        CPPUNIT_ASSERT_EQUAL(true, op->isDehydratedPlaceholder());
    }

    _syncPal->_syncOps->clear();
    {
        _syncPal->_conflictQueue->push(conflict);

        // Simulate a local hydrated placeholder
        auto mockStatus = [&]([[maybe_unused]] const SyncPath &absolutePath, VfsStatus &vfsStatus) -> ExitInfo {
            vfsStatus.isPlaceholder = true;
            vfsStatus.isHydrated = true;
            vfsStatus.isSyncing = false;
            vfsStatus.progress = 100;
            return ExitCode::Ok;
        };
        _mockVfs->setMockStatus(mockStatus);

        _syncPal->_conflictResolverWorker->execute();

        const auto op = _syncPal->_syncOps->getOp(_syncPal->_syncOps->opSortedList().front());
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), _syncPal->_syncOps->size());
        CPPUNIT_ASSERT_EQUAL(false, op->omit());
        CPPUNIT_ASSERT_EQUAL(OperationType::Delete, op->type());
        CPPUNIT_ASSERT_EQUAL(false, op->isDehydratedPlaceholder());
    }
}

void TestConflictResolverWorker::testMoveParentDelete() {
    // Simulate a move of node A/AA/AAA to A/AB/AAA on local replica
    const auto lNodeAAA = _testSituationGenerator.moveNode(ReplicaSide::Local, "aaa", "ab");

    // Simulate a delete of node A on remote replica
    const auto rNodeA = _testSituationGenerator.deleteNode(ReplicaSide::Remote, "a");

    const Conflict conflict(lNodeAAA, rNodeA, ConflictType::MoveParentDelete);
    _syncPal->_conflictQueue->push(conflict);
    _syncPal->_conflictResolverWorker->execute();

    // Delete operation wins
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), _syncPal->_syncOps->size());
    const auto op = _syncPal->_syncOps->getOp(_syncPal->_syncOps->opSortedList().front());
    CPPUNIT_ASSERT_EQUAL(false, op->omit());
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, op->targetSide());
    CPPUNIT_ASSERT_EQUAL(OperationType::Delete, op->type());
    CPPUNIT_ASSERT_EQUAL(_testSituationGenerator.getNode(ReplicaSide::Local, "a"), op->correspondingNode());
}

void TestConflictResolverWorker::testMoveParentDelete2() {
    // Simulate a move of node A/AA to A/AB/AA on local replica
    const auto lNodeAA = _testSituationGenerator.moveNode(ReplicaSide::Local, "aa", "ab");

    // Simulate a rename (move) of node A to A2 on remote replica
    (void) _testSituationGenerator.renameNode(ReplicaSide::Remote, "a", Str("A2"));

    // Simulate a delete of node A/AB on remote replica
    const auto rNodeAB = _testSituationGenerator.deleteNode(ReplicaSide::Remote, "ab");

    const Conflict conflict(lNodeAA, rNodeAB, ConflictType::MoveParentDelete);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), ExitInfo(_syncPal->_conflictResolverWorker->exitCode()));
    // Delete operation wins
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), _syncPal->_syncOps->size());
    const UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    const SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(!op->omit());
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, op->targetSide());
    CPPUNIT_ASSERT_EQUAL(OperationType::Delete, op->type());
}

void TestConflictResolverWorker::testMoveParentDelete3() {
    // Set up a more complex tree
    // .
    // └── A
    //     ├── AA
    //     │   └── AAA
    //     └── AB
    //         ├── ABA
    //         └── ABB
    const auto lNodeABA = _testSituationGenerator.createNode(ReplicaSide::Local, NodeType::Directory, "aba", "ab", false);
    const auto lNodeABB = _testSituationGenerator.createNode(ReplicaSide::Local, NodeType::File, "abb", "ab", false);
    const auto rNodeABA = _testSituationGenerator.createNode(ReplicaSide::Remote, NodeType::Directory, "aba", "ab", false);
    const auto rNodeABB = _testSituationGenerator.createNode(ReplicaSide::Remote, NodeType::File, "abb", "ab", false);

    // Simulate an edit of node A/AB/ABB on local replica
    (void) _testSituationGenerator.editNode(ReplicaSide::Local, "abb");

    // Simulate an edit of node A/AA/AAA on local replica
    const auto lNodeAAA = _testSituationGenerator.editNode(ReplicaSide::Local, "aaa");

    // Simulate a move of node A/AA/AAA to A/AB/AAA on local replica
    (void) _testSituationGenerator.moveNode(ReplicaSide::Local, "aaa", "ab");

    // Simulate a delete of node A/AB on remote replica
    const auto rNodeAB = _testSituationGenerator.deleteNode(ReplicaSide::Remote, "ab");

    const Conflict conflict1(lNodeAAA, rNodeAB, ConflictType::MoveParentDelete);
    _syncPal->_conflictQueue->push(conflict1);
    const Conflict conflict2(rNodeABB, lNodeABB, ConflictType::EditDelete);
    _syncPal->_conflictQueue->push(conflict2);
    const auto rNodeAAA = _testSituationGenerator.getNode(ReplicaSide::Remote, "aaa");
    const Conflict conflict3(rNodeAAA, lNodeAAA, ConflictType::MoveDelete);
    _syncPal->_conflictQueue->push(conflict3);
    _syncPal->_conflictResolverWorker->execute();

    // Delete operation wins but conflicts EditDelete and MoveDelete should be handled first and edited files should be rescued
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(3), _syncPal->_syncOps->size());

    std::array<SyncOpPtr, 3> ops;
    for (size_t i = 0; const auto opId: _syncPal->_syncOps->opSortedList()) {
        ops[i++] = _syncPal->_syncOps->getOp(opId);
    }

    const auto op1 = ops[0];
    CPPUNIT_ASSERT_EQUAL(false, op1->omit());
    CPPUNIT_ASSERT_EQUAL(OperationType::Move, op1->type());
    CPPUNIT_ASSERT_EQUAL(true, op1->isRescueOperation());
    CPPUNIT_ASSERT_EQUAL(lNodeABB, op1->correspondingNode());

    const auto op2 = ops[1];
    CPPUNIT_ASSERT_EQUAL(false, op2->omit());
    CPPUNIT_ASSERT_EQUAL(OperationType::Move, op2->type());
    CPPUNIT_ASSERT_EQUAL(true, op2->isRescueOperation());
    CPPUNIT_ASSERT_EQUAL(lNodeAAA, op2->correspondingNode());

    const auto op3 = ops[2];
    CPPUNIT_ASSERT_EQUAL(false, op3->omit());
    CPPUNIT_ASSERT_EQUAL(OperationType::Delete, op3->type());
    CPPUNIT_ASSERT_EQUAL(false, op3->isRescueOperation());
    const auto lNodeAB = _testSituationGenerator.getNode(ReplicaSide::Local, "ab");
    CPPUNIT_ASSERT_EQUAL(lNodeAB, op3->correspondingNode());
}

void TestConflictResolverWorker::testMoveParentDeleteDehydratedPlaceholder() {
    // Simulate move of A/AA/AAA to A/AB/AAA on local replica
    const auto lNodeAAA = _testSituationGenerator.moveNode(ReplicaSide::Local, "aaa", "ab");

    // Simulate delete of AA on remote replica
    const auto rNodeAA = _testSituationGenerator.deleteNode(ReplicaSide::Remote, "aa");

    // Since the methods needed for tests are mocked, we can put any VirtualFileMode type. It just needs to be different from
    // VirtualFileMode::off
    _syncPal->setVfsMode(VirtualFileMode::Mac);
    // Simulate a local dehydrate placeholder
    auto mockStatus = []([[maybe_unused]] const SyncPath &absolutePath, VfsStatus &vfsStatus) -> ExitInfo {
        vfsStatus.isPlaceholder = true;
        vfsStatus.isHydrated = false;
        vfsStatus.isSyncing = false;
        vfsStatus.progress = 0;
        return ExitCode::Ok;
    };
    _mockVfs->setMockStatus(mockStatus);

    const Conflict conflict(lNodeAAA, rNodeAA, ConflictType::MoveParentDelete);
    _syncPal->_conflictQueue->push(conflict);
    _syncPal->_conflictResolverWorker->execute();

    const auto op = _syncPal->_syncOps->getOp(_syncPal->_syncOps->opSortedList().front());
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), _syncPal->_syncOps->size());
    CPPUNIT_ASSERT_EQUAL(false, op->omit());
    CPPUNIT_ASSERT_EQUAL(OperationType::Delete, op->type());
    CPPUNIT_ASSERT_EQUAL(lNodeAAA, op->correspondingNode());
    CPPUNIT_ASSERT_EQUAL(true, op->isDehydratedPlaceholder());
}

void TestConflictResolverWorker::testCreateParentDelete() {
    // Simulate file creation A/AA/AAB on local replica
    const auto lNodeAAB = _testSituationGenerator.createNode(ReplicaSide::Local, NodeType::File, "aab", "aa");

    // Simulate a delete of node AA on remote replica
    const auto rNodeAA = _testSituationGenerator.deleteNode(ReplicaSide::Remote, "aa");

    const Conflict conflict(lNodeAAB, rNodeAA, ConflictType::CreateParentDelete);
    _syncPal->_conflictQueue->push(conflict);
    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), _syncPal->_syncOps->size());

    const auto op1 = _syncPal->_syncOps->getOp(_syncPal->_syncOps->opSortedList().front());
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, op1->targetSide());
    CPPUNIT_ASSERT_EQUAL(OperationType::Move, op1->type());
    CPPUNIT_ASSERT_EQUAL(true, op1->isRescueOperation());
    CPPUNIT_ASSERT_EQUAL(lNodeAAB, op1->correspondingNode());

    const auto op2 = _syncPal->_syncOps->getOp(_syncPal->_syncOps->opSortedList().back());
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, op2->targetSide());
    CPPUNIT_ASSERT_EQUAL(OperationType::Delete, op2->type());
    const auto lNodeAA = _testSituationGenerator.getNode(ReplicaSide::Local, "aa");
    CPPUNIT_ASSERT_EQUAL(lNodeAA, op2->correspondingNode());
}

void TestConflictResolverWorker::testMoveMoveSource() {
    // Simulate move of node A/AA/AAA to A/AB/AAA on local replica
    const auto lNodeAAA = _testSituationGenerator.moveNode(ReplicaSide::Local, "aaa", "ab");

    // Simulate move of node A/AA/AAA to A/AAA on remote replica
    const auto rNodeAAA = _testSituationGenerator.moveNode(ReplicaSide::Remote, "aaa", "a");

    const Conflict conflict(lNodeAAA, rNodeAAA, ConflictType::MoveMoveSource);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), _syncPal->_syncOps->size());
    const auto opId = _syncPal->_syncOps->opSortedList().front();
    const auto op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(!op->newName().empty());
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, op->targetSide());
    CPPUNIT_ASSERT_EQUAL(OperationType::Move, op->type());
    CPPUNIT_ASSERT(op->affectedNode()->hasChangeEvent(OperationType::Move));
    CPPUNIT_ASSERT(op->affectedNode()->moveOriginInfos().parentNodeId() != defaultInvalidNodeId);
    CPPUNIT_ASSERT(op->affectedNode()->moveOriginInfos().path() != defaultInvalidPath);
}

void TestConflictResolverWorker::testMoveMoveSourceDehydratedPlaceholder() {
    // Simulate move of A/AA/AAA to A/AB/AAA on local replica
    const auto lNodeAAA = _testSituationGenerator.moveNode(ReplicaSide::Local, "aaa", "ab");

    // Simulate move of A/AA/AAA to A/AAA on remote replica
    const auto rNodeAAA = _testSituationGenerator.moveNode(ReplicaSide::Remote, "aaa", "a");

    // Since the methods needed for tests are mocked, we can put any VirtualFileMode type. It just needs to be different from
    // VirtualFileMode::off
    _syncPal->setVfsMode(VirtualFileMode::Mac);
    // Simulate a local dehydrate placeholder
    auto mockStatus = []([[maybe_unused]] const SyncPath &absolutePath, VfsStatus &vfsStatus) -> ExitInfo {
        vfsStatus.isPlaceholder = true;
        vfsStatus.isHydrated = false;
        vfsStatus.isSyncing = false;
        vfsStatus.progress = 0;
        return ExitCode::Ok;
    };
    _mockVfs->setMockStatus(mockStatus);

    const Conflict conflict(lNodeAAA, rNodeAAA, ConflictType::MoveMoveSource);
    _syncPal->_conflictQueue->push(conflict);
    _syncPal->_conflictResolverWorker->execute();

    const auto opId = _syncPal->_syncOps->opSortedList().front();
    const auto op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), _syncPal->_syncOps->size());
    CPPUNIT_ASSERT_EQUAL(false, op->omit());
    CPPUNIT_ASSERT_EQUAL(OperationType::Delete, op->type());
    CPPUNIT_ASSERT_EQUAL(lNodeAAA, op->correspondingNode());
    CPPUNIT_ASSERT_EQUAL(true, op->isDehydratedPlaceholder());
}

void TestConflictResolverWorker::testMoveMoveDest() {
    // Simulate move of A/AA/AAA to A/AB/AAA on local replica
    const auto lNodeAAA = _testSituationGenerator.moveNode(ReplicaSide::Local, "aaa", "ab");

    // Simulate move of A/AA to A/AB/AAA on remote replica
    const auto rNodeAA = _testSituationGenerator.moveNode(ReplicaSide::Remote, "aa", "ab");
    (void) _testSituationGenerator.renameNode(ReplicaSide::Remote, "aa", Str("AAA"));

    const Conflict conflict(lNodeAAA, rNodeAA, ConflictType::MoveMoveDest);
    _syncPal->_conflictQueue->push(conflict);
    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), _syncPal->_syncOps->size());
    const auto opId = _syncPal->_syncOps->opSortedList().front();
    const auto op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(!op->newName().empty());
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, op->targetSide());
    CPPUNIT_ASSERT_EQUAL(OperationType::Move, op->type());
    CPPUNIT_ASSERT(op->affectedNode()->hasChangeEvent(OperationType::Move));
    CPPUNIT_ASSERT(op->affectedNode()->moveOriginInfos().parentNodeId() != defaultInvalidNodeId);
    CPPUNIT_ASSERT(op->affectedNode()->moveOriginInfos().path() != defaultInvalidPath);
    const auto lNodeAA = _testSituationGenerator.getNode(ReplicaSide::Local, "aa");
    CPPUNIT_ASSERT_EQUAL(lNodeAA, op->newParentNode());
}

void TestConflictResolverWorker::testMoveMoveDestDehydratedPlaceholder() {
    // Simulate move of A/AA/AAA to A/AB/AAA on local replica
    const auto lNodeAAA = _testSituationGenerator.moveNode(ReplicaSide::Local, "aaa", "ab");

    // Simulate move of A/AA to A/AB/AAA on remote replica
    const auto rNodeAA = _testSituationGenerator.moveNode(ReplicaSide::Remote, "aa", "ab");
    (void) _testSituationGenerator.renameNode(ReplicaSide::Local, "aa", Str("AAA"));

    // Since the methods needed for tests are mocked, we can put any VirtualFileMode type. It just needs to be different from
    // VirtualFileMode::off
    _syncPal->setVfsMode(VirtualFileMode::Mac);
    // Simulate a local dehydrate placeholder
    auto mockStatus = []([[maybe_unused]] const SyncPath &absolutePath, VfsStatus &vfsStatus) -> ExitInfo {
        vfsStatus.isPlaceholder = true;
        vfsStatus.isHydrated = false;
        vfsStatus.isSyncing = false;
        vfsStatus.progress = 0;
        return ExitCode::Ok;
    };
    _mockVfs->setMockStatus(mockStatus);

    const Conflict conflict(lNodeAAA, rNodeAA, ConflictType::MoveMoveDest);
    _syncPal->_conflictQueue->push(conflict);
    _syncPal->_conflictResolverWorker->execute();

    const auto opId = _syncPal->_syncOps->opSortedList().front();
    const auto op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), _syncPal->_syncOps->size());
    CPPUNIT_ASSERT_EQUAL(false, op->omit());
    CPPUNIT_ASSERT_EQUAL(OperationType::Delete, op->type());
    CPPUNIT_ASSERT_EQUAL(lNodeAAA, op->correspondingNode());
    CPPUNIT_ASSERT_EQUAL(true, op->isDehydratedPlaceholder());
}

void TestConflictResolverWorker::testMoveMoveCycle() {
    // Simulate move of node A/AA to A/AB/AA on local replica
    const auto lNodeAA = _testSituationGenerator.moveNode(ReplicaSide::Local, "aa", "ab");

    // Simulate move of node A/AB to A/AA/AB, on remote replica
    const auto rNodeAB = _testSituationGenerator.moveNode(ReplicaSide::Remote, "ab", "aa");

    const auto rNodeAA = _testSituationGenerator.getNode(ReplicaSide::Remote, "aa");
    const Conflict conflict(lNodeAA, rNodeAA, ConflictType::MoveMoveCycle);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), _syncPal->_syncOps->size());
    const auto opId = _syncPal->_syncOps->opSortedList().front();
    const auto op = _syncPal->_syncOps->getOp(opId);
    const auto lNodeA = _testSituationGenerator.getNode(ReplicaSide::Local, "a");
    CPPUNIT_ASSERT_EQUAL(lNodeA, op->newParentNode());
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, op->targetSide());
    CPPUNIT_ASSERT_EQUAL(OperationType::Move, op->type());
    CPPUNIT_ASSERT(op->affectedNode()->hasChangeEvent(OperationType::Move));
    CPPUNIT_ASSERT(op->affectedNode()->moveOriginInfos().parentNodeId() != defaultInvalidNodeId);
    CPPUNIT_ASSERT(op->affectedNode()->moveOriginInfos().path() != defaultInvalidPath);
}

void TestConflictResolverWorker::testMoveMoveCycle2() {
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

    // Simulate move of node A/AA/AAA to A/AB/AAA on local replica
    const auto lNodeAAA = _testSituationGenerator.moveNode(ReplicaSide::Local, "aaa", "ab");

    // Simulate move of node A/AB to A/AAA/AB, on remote replica
    const auto rNodeAB = _testSituationGenerator.moveNode(ReplicaSide::Remote, "ab", "aaa");

    const auto rNodeAAA = _testSituationGenerator.getNode(ReplicaSide::Remote, "aaa");
    const Conflict conflict(lNodeAAA, rNodeAAA, ConflictType::MoveMoveCycle);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), _syncPal->_syncOps->size());
    const auto opId = _syncPal->_syncOps->opSortedList().front();
    const auto op = _syncPal->_syncOps->getOp(opId);
    const auto lNodeAA = _testSituationGenerator.getNode(ReplicaSide::Local, "aa");
    CPPUNIT_ASSERT_EQUAL(lNodeAA, op->newParentNode());
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, op->targetSide());
    CPPUNIT_ASSERT_EQUAL(OperationType::Move, op->type());
    CPPUNIT_ASSERT(op->affectedNode()->hasChangeEvent(OperationType::Move));
    CPPUNIT_ASSERT(op->affectedNode()->moveOriginInfos().parentNodeId() != defaultInvalidNodeId);
    CPPUNIT_ASSERT(op->affectedNode()->moveOriginInfos().path() != defaultInvalidPath);
}

} // namespace KDC
