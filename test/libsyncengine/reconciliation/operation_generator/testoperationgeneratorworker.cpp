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

#include "testoperationgeneratorworker.h"

#include "mocks/libcommonserver/db/mockdb.h"
#include "reconciliation/operation_generator/operationgeneratorworker.h"

#include "test_utility/testhelpers.h"

#include <memory>

namespace KDC {

void KDC::TestOperationGeneratorWorker::setUp() {
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
    _syncPal->createWorkers();
    _syncPal->_operationsGeneratorWorker = std::make_shared<OperationGeneratorWorker>(_syncPal, "Conflict Finder", "OPGE");

    // Generate initial situation
    // .
    // ├── A
    // │   └── AA
    // └── B
    _situationGenerator.setSyncpal(_syncPal);
    _situationGenerator.generateInitialSituation(R"({"a":{"aa":1},"b":{}})");
    _syncPal->copySnapshots();
}

void KDC::TestOperationGeneratorWorker::tearDown() {
    ParmsDb::instance()->close();
    ParmsDb::reset();
    if (_syncPal && _syncPal->syncDb()) {
        _syncPal->syncDb()->close();
    }
    TestBase::stop();
}

void TestOperationGeneratorWorker::testCreateOp() {
    // Simulate the creation of A/AB on local replica.
    const auto nodeAB = _situationGenerator.createNode(ReplicaSide::Local, NodeType::File, "ab", "a");

    _syncPal->_operationsGeneratorWorker->execute();

    CPPUNIT_ASSERT(!_syncPal->syncOps()->isEmpty());
    const auto op = _syncPal->syncOps()->getOp(_syncPal->syncOps()->_opSortedList.front());
    CPPUNIT_ASSERT_EQUAL(OperationType::Create, op->type());
    CPPUNIT_ASSERT_EQUAL(nodeAB, op->affectedNode());
    CPPUNIT_ASSERT(!op->omit());
}

void TestOperationGeneratorWorker::testCreateOpWithPseudoConflict() {
    // Simulate the creation of A/AB on both replicas.
    (void) _situationGenerator.createNode(ReplicaSide::Local, NodeType::File, "ab", "a");
    (void) _situationGenerator.createNode(ReplicaSide::Remote, NodeType::File, "ab", "a");

    _syncPal->_operationsGeneratorWorker->execute();

    CPPUNIT_ASSERT(!_syncPal->syncOps()->isEmpty());
    const auto op = _syncPal->syncOps()->getOp(_syncPal->syncOps()->_opSortedList.front());
    CPPUNIT_ASSERT_EQUAL(OperationType::Create, op->type());
    CPPUNIT_ASSERT(op->omit());
}

void TestOperationGeneratorWorker::testMoveOp() {
    // Simulate the move of A/AA to B/AA on remote replica.
    const auto nodeAA = _situationGenerator.moveNode(ReplicaSide::Remote, "aa", "b");

    _syncPal->_operationsGeneratorWorker->execute();

    CPPUNIT_ASSERT(!_syncPal->syncOps()->isEmpty());
    const auto op = _syncPal->syncOps()->getOp(_syncPal->syncOps()->_opSortedList.front());
    CPPUNIT_ASSERT_EQUAL(OperationType::Move, op->type());
    CPPUNIT_ASSERT_EQUAL(nodeAA, op->affectedNode());
    CPPUNIT_ASSERT(!op->omit());
}

void TestOperationGeneratorWorker::testMoveOpWithPseudoConflict() {
    // Simulate the move of A/AA to B/AA on both replicas.
    (void) _situationGenerator.moveNode(ReplicaSide::Local, "aa", "b");
    (void) _situationGenerator.moveNode(ReplicaSide::Remote, "aa", "b");

    _syncPal->_operationsGeneratorWorker->execute();

    CPPUNIT_ASSERT(!_syncPal->syncOps()->isEmpty());
    const auto op = _syncPal->syncOps()->getOp(_syncPal->syncOps()->_opSortedList.front());
    CPPUNIT_ASSERT_EQUAL(OperationType::Move, op->type());
    CPPUNIT_ASSERT(op->omit());
}

void TestOperationGeneratorWorker::testMoveOpWithPseudoConflictButDifferentEncoding() {
    // Simulate the move of A/AA to B/AA on both replicas but renamed with different encodings.
    (void) _situationGenerator.moveNode(ReplicaSide::Local, "aa", "b", testhelpers::makeNfcSyncName());
    (void) _situationGenerator.moveNode(ReplicaSide::Remote, "aa", "b", testhelpers::makeNfdSyncName());

    _syncPal->_operationsGeneratorWorker->execute();

    CPPUNIT_ASSERT(!_syncPal->syncOps()->isEmpty());
    const auto op = _syncPal->syncOps()->getOp(_syncPal->syncOps()->_opSortedList.front());
    CPPUNIT_ASSERT_EQUAL(OperationType::Move, op->type());
    CPPUNIT_ASSERT(op->omit());
}

void TestOperationGeneratorWorker::testEditOp() {
    // Simulate the edition of A/AA on local replica.
    const auto nodeAA = _situationGenerator.editNode(ReplicaSide::Local, "aa");

    _syncPal->_operationsGeneratorWorker->execute();

    CPPUNIT_ASSERT(!_syncPal->syncOps()->isEmpty());
    const auto op = _syncPal->syncOps()->getOp(_syncPal->syncOps()->_opSortedList.front());
    CPPUNIT_ASSERT_EQUAL(OperationType::Edit, op->type());
    CPPUNIT_ASSERT_EQUAL(nodeAA, op->affectedNode());
    CPPUNIT_ASSERT(!op->omit());
}

void TestOperationGeneratorWorker::testEditOpWithPseudoConflict() {
    // Simulate the edition of A/AA on both replicas.
    const auto modificationTime = _situationGenerator.getNode(ReplicaSide::Local, "aa")->modificationTime().value() + 10;
    (void) _situationGenerator.editNode(ReplicaSide::Local, "aa", modificationTime);
    (void) _situationGenerator.editNode(ReplicaSide::Remote, "aa", modificationTime);

    _syncPal->_operationsGeneratorWorker->execute();

    CPPUNIT_ASSERT(!_syncPal->syncOps()->isEmpty());
    const auto op = _syncPal->syncOps()->getOp(_syncPal->syncOps()->_opSortedList.front());
    CPPUNIT_ASSERT_EQUAL(OperationType::Edit, op->type());
    CPPUNIT_ASSERT(op->omit());
}

void TestOperationGeneratorWorker::testDeleteFileOp() {
    // Simulate the deletion of A/AA on local replica.
    const auto nodeAA = _situationGenerator.deleteNode(ReplicaSide::Local, "aa");

    _syncPal->_operationsGeneratorWorker->execute();

    CPPUNIT_ASSERT(!_syncPal->syncOps()->isEmpty());
    const auto op = _syncPal->syncOps()->getOp(_syncPal->syncOps()->_opSortedList.front());
    CPPUNIT_ASSERT_EQUAL(OperationType::Delete, op->type());
    CPPUNIT_ASSERT_EQUAL(nodeAA, op->affectedNode());
    CPPUNIT_ASSERT(!op->omit());
}

void TestOperationGeneratorWorker::testDeleteFolderOp() {
    // Simulate the deletion of A on local replica.
    const auto nodeA = _situationGenerator.deleteNode(ReplicaSide::Local, "a");

    _syncPal->_operationsGeneratorWorker->execute();

    CPPUNIT_ASSERT(!_syncPal->syncOps()->isEmpty());
    const auto op = _syncPal->syncOps()->getOp(_syncPal->syncOps()->_opSortedList.front());
    CPPUNIT_ASSERT_EQUAL(OperationType::Delete, op->type());
    CPPUNIT_ASSERT_EQUAL(nodeA, op->affectedNode());
    CPPUNIT_ASSERT(!op->omit());
}

void TestOperationGeneratorWorker::testDeleteOpWithPseudoConflict() {
    // Simulate the deletion of A/AA on both replicas.
    (void) _situationGenerator.deleteNode(ReplicaSide::Local, "a");
    (void) _situationGenerator.deleteNode(ReplicaSide::Remote, "a");

    _syncPal->_operationsGeneratorWorker->execute();

    CPPUNIT_ASSERT(!_syncPal->syncOps()->isEmpty());
    const auto op = _syncPal->syncOps()->getOp(_syncPal->syncOps()->_opSortedList.front());
    CPPUNIT_ASSERT_EQUAL(OperationType::Delete, op->type());
    CPPUNIT_ASSERT(op->omit());
}

void TestOperationGeneratorWorker::testMoveEditOps() {
    // Simulate the edition of A/AA on remote replica.
    const auto nodeA = _situationGenerator.editNode(ReplicaSide::Local, "a");
    // Simulate the deletion of A/AA to B/AA on remote replica.
    (void) _situationGenerator.moveNode(ReplicaSide::Local, "aa", "b");

    _syncPal->_operationsGeneratorWorker->execute();

    bool hasMoveOp = false;
    bool hasEditOp = false;
    for (auto &opId: _syncPal->syncOps()->opSortedList()) {
        const auto op = _syncPal->syncOps()->getOp(opId);
        if (op->type() == OperationType::Move) {
            hasMoveOp = true;
        }
        if (op->type() == OperationType::Edit) {
            hasEditOp = true;
        }
    }

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), _syncPal->syncOps()->size());
    CPPUNIT_ASSERT(hasMoveOp);
    CPPUNIT_ASSERT(hasEditOp);
    const auto opId = _syncPal->syncOps()->_opSortedList.front();
    const auto op = _syncPal->syncOps()->getOp(opId);
    CPPUNIT_ASSERT(!op->omit());
}

void TestOperationGeneratorWorker::testEditChangeShouldBePropagated() {
    const auto nodeAAl = _situationGenerator.getNode(ReplicaSide::Local, "aa");

    bool propagateEdit = true;
    CPPUNIT_ASSERT(_syncPal->_operationsGeneratorWorker->editChangeShouldBePropagated(nodeAAl, propagateEdit) && propagateEdit);

    // Edit of modification time are always propagated
    nodeAAl->setModificationTime(testhelpers::defaultTime + 1);
    CPPUNIT_ASSERT(_syncPal->_operationsGeneratorWorker->editChangeShouldBePropagated(nodeAAl, propagateEdit) && propagateEdit);
    nodeAAl->setModificationTime(testhelpers::defaultTime);

    // Edit of size are always propagated
    nodeAAl->setSize(testhelpers::defaultFileSize + 1);
    CPPUNIT_ASSERT(_syncPal->_operationsGeneratorWorker->editChangeShouldBePropagated(nodeAAl, propagateEdit) && propagateEdit);
    nodeAAl->setSize(testhelpers::defaultFileSize);

    // Local Edit of createdAt are not propagated if the other attributes are the same
    nodeAAl->setCreatedAt(testhelpers::defaultTime + 1);
    CPPUNIT_ASSERT(_syncPal->_operationsGeneratorWorker->editChangeShouldBePropagated(nodeAAl, propagateEdit) && !propagateEdit);

    nodeAAl->setSize(testhelpers::defaultFileSize + 1);
    CPPUNIT_ASSERT(_syncPal->_operationsGeneratorWorker->editChangeShouldBePropagated(nodeAAl, propagateEdit) && propagateEdit);
    nodeAAl->setModificationTime(testhelpers::defaultTime + 1);
    CPPUNIT_ASSERT(_syncPal->_operationsGeneratorWorker->editChangeShouldBePropagated(nodeAAl, propagateEdit) && propagateEdit);
    nodeAAl->setSize(testhelpers::defaultFileSize);
    CPPUNIT_ASSERT(_syncPal->_operationsGeneratorWorker->editChangeShouldBePropagated(nodeAAl, propagateEdit) && propagateEdit);
}

} // namespace KDC
