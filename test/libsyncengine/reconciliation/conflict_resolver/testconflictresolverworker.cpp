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
#include "requests/parameterscache.h"
#include "reconciliation/platform_inconsistency_checker/platforminconsistencycheckerutility.h"
#include "test_utility/testhelpers.h"

#include <memory>

namespace KDC {

void TestConflictResolverWorker::setUp() {
    // Create SyncPal
    bool alreadyExists = false;
    std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExists, true);
    ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);

    SyncPath syncDbPath = Db::makeDbName(1, 1, 1, 1, alreadyExists);
    std::filesystem::remove(syncDbPath);
    _syncPal = std::make_shared<SyncPal>(std::make_shared<VfsOff>(), syncDbPath, KDRIVE_VERSION_STRING, true);
    _syncPal->syncDb()->setAutoDelete(true);
    _syncPal->createSharedObjects();

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
    DbNode dbNodeA(0, _syncPal->syncDb()->rootNode().nodeId(), Str("A"), Str("A"), "lA", "rA", testhelpers::defaultTime,
                   testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(dbNodeA, dbNodeIdA, constraintError);
    DbNode dbNodeAA(0, dbNodeIdA, Str("AA"), Str("AA"), "lAA", "rAA", testhelpers::defaultTime, testhelpers::defaultTime,
                    testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(dbNodeAA, dbNodeIdAA, constraintError);
    DbNode dbNodeAB(0, dbNodeIdA, Str("AB"), Str("AB"), "lAB", "rAB", testhelpers::defaultTime, testhelpers::defaultTime,
                    testhelpers::defaultTime, NodeType::Directory, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(dbNodeAB, dbNodeIdAB, constraintError);
    DbNode dbNodeAAA(0, dbNodeIdAA, Str("AAA"), Str("AAA"), "lAAA", "rAAA", testhelpers::defaultTime, testhelpers::defaultTime,
                     testhelpers::defaultTime, NodeType::File, 0, std::nullopt);
    _syncPal->syncDb()->insertNode(dbNodeAAA, dbNodeIdAAA, constraintError);

    // Build update trees
    std::shared_ptr<Node> lNodeA =
            std::make_shared<Node>(dbNodeIdA, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("A"), NodeType::Directory,
                                   OperationType::None, "lA", testhelpers::defaultTime, testhelpers::defaultTime,
                                   testhelpers::defaultFileSize, _syncPal->updateTree(ReplicaSide::Local)->rootNode());
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->insertChildren(lNodeA));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(lNodeA);
    std::shared_ptr<Node> lNodeAA = std::make_shared<Node>(
            dbNodeIdAA, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("AA"), NodeType::Directory, OperationType::None,
            "lAA", testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, lNodeA);
    CPPUNIT_ASSERT(lNodeA->insertChildren(lNodeAA));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(lNodeAA);
    std::shared_ptr<Node> lNodeAB = std::make_shared<Node>(
            dbNodeIdAB, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("AB"), NodeType::Directory, OperationType::None,
            "lAB", testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, lNodeA);
    CPPUNIT_ASSERT(lNodeA->insertChildren(lNodeAB));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(lNodeAB);
    std::shared_ptr<Node> lNodeAAA = std::make_shared<Node>(
            dbNodeIdAAA, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("AAA"), NodeType::File, OperationType::None,
            "lAAA", testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, lNodeAA);
    CPPUNIT_ASSERT(lNodeAA->insertChildren(lNodeAAA));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(lNodeAAA);

    std::shared_ptr<Node> rNodeA =
            std::make_shared<Node>(dbNodeIdA, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("A"), NodeType::Directory,
                                   OperationType::None, "rA", testhelpers::defaultTime, testhelpers::defaultTime,
                                   testhelpers::defaultFileSize, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNodeA));
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeA);
    std::shared_ptr<Node> rNodeAA = std::make_shared<Node>(
            dbNodeIdAA, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("AA"), NodeType::Directory, OperationType::None,
            "rAA", testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, rNodeA);
    CPPUNIT_ASSERT(rNodeA->insertChildren(rNodeAA));
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeAA);
    std::shared_ptr<Node> rNodeAB = std::make_shared<Node>(
            dbNodeIdAB, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("AB"), NodeType::Directory, OperationType::None,
            "rAB", testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, rNodeA);
    CPPUNIT_ASSERT(rNodeA->insertChildren(rNodeAB));
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeAB);
    std::shared_ptr<Node> rNodeAAA = std::make_shared<Node>(
            dbNodeIdAAA, _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("AAA"), NodeType::File, OperationType::None,
            "rAAA", testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, rNodeAA);
    CPPUNIT_ASSERT(rNodeAA->insertChildren(rNodeAAA));
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeAAA);
}

void TestConflictResolverWorker::tearDown() {
    ParmsDb::instance()->close();
    ParmsDb::reset();
    if (_syncPal && _syncPal->syncDb()) {
        _syncPal->syncDb()->close();
    }
}

void TestConflictResolverWorker::testCreateCreate() {
    // Simulate file creation on both replica
    std::shared_ptr<Node> lNodeAA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAA");
    std::shared_ptr<Node> lNodeAAB = std::make_shared<Node>(
            std::nullopt, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("AAB.txt"), NodeType::File, OperationType::Create,
            "lAAB", testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, lNodeAA);
    CPPUNIT_ASSERT(lNodeAA->insertChildren(lNodeAAB));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(lNodeAAB);
    _syncPal->_localSnapshot->updateItem(SnapshotItem("lAAB", "lAA", Str("AAB.txt"), testhelpers::defaultTime,
                                                      testhelpers::defaultTime, NodeType::File, 123, false, true, true));

    std::shared_ptr<Node> rNodeAA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rAA");
    std::shared_ptr<Node> rNodeAAB = std::make_shared<Node>(
            _syncPal->updateTree(ReplicaSide::Remote)->side(), Str("AAB.txt"), NodeType::File, OperationType::Create, "rAAB",
            testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, rNodeAA);
    CPPUNIT_ASSERT(rNodeAA->insertChildren(rNodeAAB));
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeAAB);
    _syncPal->_remoteSnapshot->updateItem(SnapshotItem("rAAB", "rAA", Str("AAB.txt"), testhelpers::defaultTime,
                                                       testhelpers::defaultTime, NodeType::File, 123, false, true, true));

    Conflict conflict(lNodeAAB, rNodeAA, ConflictType::CreateCreate);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT_EQUAL(size_t(1), _syncPal->_syncOps->size());
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(!op->newName().empty());
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, op->targetSide());
    CPPUNIT_ASSERT_EQUAL(OperationType::Move, op->type());
}

void TestConflictResolverWorker::testEditEdit() {
    // Simulate edit conflict of file AAA on both replica
    std::shared_ptr<Node> lNodeAAA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAAA");
    lNodeAAA->setChangeEvents(OperationType::Edit);
    std::shared_ptr<Node> rNodeAAA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rAAA");
    rNodeAAA->setChangeEvents(OperationType::Edit);
    Conflict conflict(lNodeAAA, rNodeAAA, ConflictType::EditEdit);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT_EQUAL(size_t(1), _syncPal->_syncOps->size());
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(!op->newName().empty());
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, op->targetSide());
    CPPUNIT_ASSERT_EQUAL(OperationType::Move, op->type());
}

void TestConflictResolverWorker::testMoveCreate() {
    // Simulate create file ABA in AB on local replica
    std::shared_ptr<Node> lNodeAB = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAB");
    std::shared_ptr<Node> lNodeABA = std::make_shared<Node>(
            std::nullopt, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("ABA"), NodeType::File, OperationType::Create,
            "lABA", testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, lNodeAB);
    CPPUNIT_ASSERT(lNodeAB->insertChildren(lNodeABA));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(lNodeABA);

    // Simulate move of file AAA in AA to ABA in AB on remote replica
    std::shared_ptr<Node> rNodeAA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rAA");
    std::shared_ptr<Node> rNodeAAA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rAAA");
    std::shared_ptr<Node> rNodeAB = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rAB");
    std::shared_ptr<Node> rNodeABA = rNodeAAA;

    rNodeABA->setChangeEvents(OperationType::Move);
    rNodeABA->setMoveOrigin(rNodeABA->getPath());
    rNodeABA->setMoveOriginParentDbId(rNodeAA->idb());
    CPPUNIT_ASSERT(rNodeABA->setParentNode(rNodeAB));
    rNodeABA->setName(Str("ABA"));

    Conflict conflict(lNodeABA, rNodeABA, ConflictType::MoveCreate);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT_EQUAL(size_t(1), _syncPal->_syncOps->size());
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(!op->newName().empty());
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, op->targetSide());
    CPPUNIT_ASSERT_EQUAL(OperationType::Move, op->type());
}

void TestConflictResolverWorker::testEditDelete1() {
    // Simulate edit of file AAA on local replica
    std::shared_ptr<Node> lNodeAAA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAAA");
    lNodeAAA->setChangeEvents(OperationType::Edit);

    // and delete of file AAA on remote replica
    std::shared_ptr<Node> rNodeAA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rAA");
    std::shared_ptr<Node> rNodeAAA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rAAA");
    rNodeAAA->setChangeEvents(OperationType::Delete);
    rNodeAA->deleteChildren(rNodeAAA);
    _syncPal->updateTree(ReplicaSide::Remote)->insertNode(rNodeAAA);

    Conflict conflict(lNodeAAA, rNodeAAA, ConflictType::EditDelete);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT_EQUAL(size_t(1), _syncPal->_syncOps->size());
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(op->newName().empty());
    CPPUNIT_ASSERT(op->omit());
    CPPUNIT_ASSERT_EQUAL(OperationType::Delete, op->type());
}

void TestConflictResolverWorker::testEditDelete2() {
    // Simulate edit of file AAA on local replica
    std::shared_ptr<Node> lNodeAAA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAAA");
    lNodeAAA->setChangeEvents(OperationType::Edit);

    // and delete of dir AA (and all children) on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rA");
    std::shared_ptr<Node> rNodeAA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rAA");
    std::shared_ptr<Node> rNodeAAA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rAAA");
    rNodeAA->setChangeEvents(OperationType::Delete);
    rNodeAAA->setChangeEvents(OperationType::Delete);
    rNodeA->deleteChildren(rNodeAA);
    rNodeAA->deleteChildren(rNodeAAA);

    Conflict conflict(lNodeAAA, rNodeAAA, ConflictType::EditDelete);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT_EQUAL(size_t(2), _syncPal->_syncOps->size());
    for (const auto &opId: _syncPal->_syncOps->opSortedList()) {
        SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
        if (op->type() == OperationType::Move) {
            CPPUNIT_ASSERT(!op->newName().empty());
            CPPUNIT_ASSERT_EQUAL(_syncPal->updateTree(ReplicaSide::Remote)->rootNode(), op->newParentNode());
            CPPUNIT_ASSERT(!op->omit());
            CPPUNIT_ASSERT_EQUAL(rNodeAAA, op->affectedNode());
        } else if (op->type() == OperationType::Delete) {
            CPPUNIT_ASSERT(op->omit());
            CPPUNIT_ASSERT_EQUAL(rNodeAAA, op->affectedNode());
        } else {
            CPPUNIT_ASSERT(false); // Should not happen
        }
    }
}

void TestConflictResolverWorker::testMoveDelete1() {
    // Simulate rename of node A to B on local replica
    std::shared_ptr<Node> lNodeA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lA");
    lNodeA->setMoveOrigin(lNodeA->getPath());
    lNodeA->setMoveOriginParentDbId(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->idb());
    lNodeA->setName(Str("B"));
    lNodeA->setChangeEvents(OperationType::Move);

    // Simulate a delete of node AB on local replica
    std::shared_ptr<Node> lNodeAB = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAB");
    lNodeAB->setChangeEvents(OperationType::Delete);
    lNodeA->deleteChildren(lNodeA);

    // Simulate a delete of node A on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rA");
    rNodeA->setChangeEvents(OperationType::Delete);
    _syncPal->updateTree(ReplicaSide::Remote)->rootNode()->deleteChildren(rNodeA);

    Conflict conflict(lNodeA, rNodeA, ConflictType::MoveDelete);
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
    CPPUNIT_ASSERT_EQUAL(size_t(1), _syncPal->_syncOps->size());
    CPPUNIT_ASSERT_EQUAL(true, op->omit());
    CPPUNIT_ASSERT_EQUAL(OperationType::Delete, op->type());
}

void TestConflictResolverWorker::testMoveDelete2() {
    // Simulate rename of node A to B on local replica
    std::shared_ptr<Node> lNodeA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lA");
    lNodeA->setMoveOrigin(lNodeA->getPath());
    lNodeA->setMoveOriginParentDbId(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->idb());
    lNodeA->setName(Str("B"));
    lNodeA->setChangeEvents(OperationType::Move);

    // Simulate edit of node AAA on local replica
    std::shared_ptr<Node> lNodeAAA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAAA");
    lNodeAAA->setChangeEvents(OperationType::Edit);

    // Simulate create of node ABA on local replica
    std::shared_ptr<Node> lNodeAB = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAB");
    std::shared_ptr<Node> lNodeABA = std::make_shared<Node>(
            std::nullopt, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("ABA"), NodeType::File, OperationType::Create,
            "lABA", testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, lNodeAB);
    CPPUNIT_ASSERT(lNodeAB->insertChildren(lNodeABA));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(lNodeABA);

    // Simulate a delete of node A on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rA");
    rNodeA->setChangeEvents(OperationType::Delete);
    _syncPal->updateTree(ReplicaSide::Remote)->rootNode()->deleteChildren(rNodeA);

    Conflict conflict(lNodeA, rNodeA, ConflictType::MoveDelete);
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
    CPPUNIT_ASSERT_EQUAL(size_t(1), _syncPal->_syncOps->size());
    CPPUNIT_ASSERT(op->omit());
    CPPUNIT_ASSERT_EQUAL(OperationType::Delete, op->type());
}

void TestConflictResolverWorker::testMoveDelete3() {
    // Simulate rename of node A to B on local replica
    std::shared_ptr<Node> lNodeA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lA");
    lNodeA->setMoveOrigin(lNodeA->getPath());
    lNodeA->setMoveOriginParentDbId(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->idb());
    lNodeA->setName(Str("B"));
    lNodeA->setChangeEvents(OperationType::Move);

    // Simulate move of node AB under root on local replica
    std::shared_ptr<Node> lNodeAB = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAB");
    lNodeAB->setMoveOrigin(lNodeAB->getPath());
    lNodeAB->setMoveOriginParentDbId(lNodeA->idb());
    lNodeAB->setChangeEvents(OperationType::Move);
    CPPUNIT_ASSERT(lNodeAB->setParentNode(_syncPal->updateTree(ReplicaSide::Local)->rootNode()));
    lNodeA->deleteChildren(lNodeAB);
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->insertChildren(lNodeAB));

    // Simulate a delete of node A on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rA");
    rNodeA->setChangeEvents(OperationType::Delete);
    _syncPal->updateTree(ReplicaSide::Remote)->rootNode()->deleteChildren(rNodeA);

    Conflict conflict(lNodeA, rNodeA, ConflictType::MoveDelete);
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
    CPPUNIT_ASSERT_EQUAL(size_t(1), _syncPal->_syncOps->size());
    CPPUNIT_ASSERT(op->omit());
    CPPUNIT_ASSERT_EQUAL(OperationType::Delete, op->type());
}

void TestConflictResolverWorker::testMoveDelete4() {
    // Simulate rename of node A to B on local replica
    std::shared_ptr<Node> lNodeA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lA");
    lNodeA->setMoveOrigin(lNodeA->getPath());
    lNodeA->setMoveOriginParentDbId(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->idb());
    lNodeA->setName(Str("B"));
    lNodeA->setChangeEvents(OperationType::Move);

    // Simulate move of node AB under root on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rA");
    std::shared_ptr<Node> rNodeAB = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rAB");
    rNodeAB->setChangeEvents(OperationType::Move);
    rNodeAB->setMoveOrigin(rNodeAB->getPath());
    rNodeAB->setMoveOriginParentDbId(rNodeA->idb());
    CPPUNIT_ASSERT(rNodeAB->setParentNode(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()));
    rNodeA->deleteChildren(rNodeAB);
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNodeAB));

    // Simulate a delete of node A on remote replica
    rNodeA->setChangeEvents(OperationType::Delete);
    _syncPal->updateTree(ReplicaSide::Remote)->rootNode()->deleteChildren(rNodeA);

    Conflict conflict(lNodeA, rNodeA, ConflictType::MoveDelete);
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
    CPPUNIT_ASSERT_EQUAL((size_t) 2, _syncPal->_syncOps->size());
    CPPUNIT_ASSERT(!_syncPal->_conflictResolverWorker->registeredOrphans().empty());
    for (const auto &opId: _syncPal->_syncOps->opSortedList()) {
        SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
        CPPUNIT_ASSERT(op->omit());

        if (op->type() == OperationType::Move) {
            CPPUNIT_ASSERT(!op->newName().empty());
            CPPUNIT_ASSERT_EQUAL(_syncPal->updateTree(ReplicaSide::Remote)->rootNode(), op->newParentNode());
            CPPUNIT_ASSERT_EQUAL(rNodeAB, op->affectedNode());
        } else if (op->type() == OperationType::Delete) {
            CPPUNIT_ASSERT_EQUAL(rNodeA, op->affectedNode());
        } else {
            CPPUNIT_ASSERT(false); // Should not happen
        }
    }
}

void TestConflictResolverWorker::testMoveDelete5() {
    // Simulate rename of node AA to AA' on local replica
    std::shared_ptr<Node> lNodeA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lA");
    std::shared_ptr<Node> lNodeAA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAA");
    lNodeAA->setMoveOrigin(lNodeAA->getPath());
    lNodeAA->setMoveOriginParentDbId(lNodeA->idb());
    lNodeAA->setName(Str("AA'"));
    lNodeAA->setChangeEvents(OperationType::Move);

    // Simulate a delete of node A on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rA");
    rNodeA->setChangeEvents(OperationType::Delete);
    _syncPal->updateTree(ReplicaSide::Remote)->rootNode()->deleteChildren(rNodeA);

    std::shared_ptr<Node> rNodeAA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rAA");

    Conflict conflict1(lNodeAA, rNodeAA, ConflictType::MoveDelete);
    _syncPal->_conflictQueue->push(conflict1);
    Conflict conflict2(lNodeAA, rNodeA, ConflictType::MoveParentDelete);
    _syncPal->_conflictQueue->push(conflict2);

    // This should be treated as a Move-ParentDelete conflict, the Move-Delete conflict must be ignored.
    // For this test, we only make sure that the Move-Delete conflict is ignored.
    // In real situation, a Move-ParentDelete conflict should have been detected as well.
    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT_EQUAL(size_t(1), _syncPal->_syncOps->opSortedList().size());
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    auto syncOp = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT_EQUAL(ConflictType::MoveParentDelete, syncOp->conflict().type());
}

void TestConflictResolverWorker::testMoveParentDelete() {
    // Simulate a move of node AAA from AA to AB on local replica
    std::shared_ptr<Node> lNodeAA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAA");
    std::shared_ptr<Node> lNodeAB = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAB");
    std::shared_ptr<Node> lNodeAAA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAAA");
    lNodeAAA->setChangeEvents(OperationType::Move);
    lNodeAAA->setMoveOrigin(lNodeAAA->getPath());
    lNodeAAA->setMoveOriginParentDbId(lNodeAA->idb());
    lNodeAA->deleteChildren(lNodeAAA);
    CPPUNIT_ASSERT(lNodeAB->insertChildren(lNodeAAA));

    // Simulate a delete of node A on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rA");
    rNodeA->setChangeEvents(OperationType::Delete);
    _syncPal->updateTree(ReplicaSide::Remote)->rootNode()->deleteChildren(rNodeA);

    Conflict conflict(lNodeAAA, rNodeA, ConflictType::MoveParentDelete);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    // We should only undo the move operation on the move replica
    CPPUNIT_ASSERT_EQUAL(size_t(1), _syncPal->_syncOps->size());
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(!op->omit());
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, op->targetSide());
    CPPUNIT_ASSERT_EQUAL(OperationType::Move, op->type());
}

void TestConflictResolverWorker::testCreateParentDelete() {
    // Simulate file creation on local replica
    std::shared_ptr<Node> lNodeAA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAA");
    std::shared_ptr<Node> lNodeAAB = std::make_shared<Node>(
            std::nullopt, _syncPal->updateTree(ReplicaSide::Local)->side(), Str("AAB.txt"), NodeType::File, OperationType::Create,
            "lAAB", testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, lNodeAA);
    CPPUNIT_ASSERT(lNodeAA->insertChildren(lNodeAAB));
    _syncPal->updateTree(ReplicaSide::Local)->insertNode(lNodeAAB);
    _syncPal->_localSnapshot->updateItem(SnapshotItem("lAAB", "lAA", Str("AAB.txt"), testhelpers::defaultTime,
                                                      testhelpers::defaultTime, NodeType::File, 123, false, true, true));

    // Simulate a delete of node AA on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rA");
    std::shared_ptr<Node> rNodeAA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rAA");
    rNodeAA->setChangeEvents(OperationType::Delete);
    rNodeA->deleteChildren(rNodeAA);

    Conflict conflict(lNodeAAB, rNodeAA, ConflictType::CreateParentDelete);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT_EQUAL((size_t) 1, _syncPal->_syncOps->size());
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, op->targetSide());
    CPPUNIT_ASSERT_EQUAL(OperationType::Delete, op->type());
}

void TestConflictResolverWorker::testMoveMoveSource() {
    // Simulate move of node AAA to AB on local replica
    std::shared_ptr<Node> lNodeAA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAA");
    std::shared_ptr<Node> lNodeAB = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAB");
    std::shared_ptr<Node> lNodeAAA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAAA");
    lNodeAAA->setChangeEvents(OperationType::Move);
    lNodeAAA->setMoveOriginParentDbId(lNodeAA->idb());
    lNodeAAA->setMoveOrigin("A/AA/AAA");
    lNodeAA->deleteChildren(lNodeAAA);
    CPPUNIT_ASSERT(lNodeAB->insertChildren(lNodeAAA));

    // Simulate move of node AAA to A on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rA");
    std::shared_ptr<Node> rNodeAA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rAA");
    std::shared_ptr<Node> rNodeAAA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rAAA");
    rNodeAAA->setChangeEvents(OperationType::Move);
    rNodeAAA->setMoveOriginParentDbId(lNodeAA->idb());
    rNodeAAA->setMoveOrigin("A/AA/AAA");
    rNodeAA->deleteChildren(lNodeAAA);
    CPPUNIT_ASSERT(rNodeA->insertChildren(lNodeAAA));

    Conflict conflict(lNodeAAA, rNodeAAA, ConflictType::MoveMoveSource);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT_EQUAL(size_t(1), _syncPal->_syncOps->size());
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(!op->newName().empty());
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, op->targetSide());
    CPPUNIT_ASSERT_EQUAL(OperationType::Move, op->type());
}

void TestConflictResolverWorker::testMoveMoveSourceWithOrphanNodes() {
    // Initial state : Node AAA is orphan.
    const SyncName orphanName = PlatformInconsistencyCheckerUtility::instance()->generateNewValidName(
            "AAA", PlatformInconsistencyCheckerUtility::SuffixType::Orphan);

    std::shared_ptr<Node> lNodeAAA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAAA");
    lNodeAAA->setName(orphanName);
    lNodeAAA->parentNode()->deleteChildren(lNodeAAA);
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->insertChildren(lNodeAAA));
    CPPUNIT_ASSERT(lNodeAAA->setParentNode(_syncPal->updateTree(ReplicaSide::Local)->rootNode()));

    std::shared_ptr<Node> rNodeAAA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rAAA");
    rNodeAAA->setName(orphanName);
    rNodeAAA->parentNode()->deleteChildren(rNodeAAA);
    CPPUNIT_ASSERT(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->insertChildren(rNodeAAA));
    CPPUNIT_ASSERT(rNodeAAA->setParentNode(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()));

    _syncPal->_conflictResolverWorker->_registeredOrphans.insert({*rNodeAAA->idb(), ReplicaSide::Remote});

    // Simulate move of node AAA to AB on local replica
    std::shared_ptr<Node> lNodeAB = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAB");

    lNodeAAA->setChangeEvents(OperationType::Move);
    lNodeAAA->setMoveOriginParentDbId(_syncPal->updateTree(ReplicaSide::Local)->rootNode()->idb());
    lNodeAAA->setMoveOrigin(lNodeAAA->getPath());
    CPPUNIT_ASSERT(lNodeAAA->setParentNode(lNodeAB));
    lNodeAAA->setName(Str("AAA"));
    _syncPal->updateTree(ReplicaSide::Local)->rootNode()->deleteChildren(lNodeAAA);
    CPPUNIT_ASSERT(lNodeAB->insertChildren(lNodeAAA));

    // Simulate move of node AAA to A on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rA");

    rNodeAAA->setChangeEvents(OperationType::Move);
    rNodeAAA->setMoveOriginParentDbId(_syncPal->updateTree(ReplicaSide::Remote)->rootNode()->idb());
    rNodeAAA->setMoveOrigin(rNodeAAA->getPath());
    CPPUNIT_ASSERT(rNodeAAA->setParentNode(rNodeA));
    rNodeAAA->setName(Str("AAA"));
    _syncPal->updateTree(ReplicaSide::Remote)->rootNode()->deleteChildren(rNodeAAA);
    CPPUNIT_ASSERT(rNodeA->insertChildren(rNodeAAA));

    Conflict conflict(lNodeAAA, rNodeAAA, ConflictType::MoveMoveSource);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT_EQUAL(size_t(1), _syncPal->_syncOps->size());
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(orphanName), SyncName2Str(op->newName()));
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, op->targetSide());
    CPPUNIT_ASSERT_EQUAL(_syncPal->updateTree(ReplicaSide::Local)->rootNode(), op->newParentNode());
    CPPUNIT_ASSERT_EQUAL(OperationType::Move, op->type());
}

void TestConflictResolverWorker::testMoveMoveDest() {
    // Simulate move of node AAA to AB on local replica
    std::shared_ptr<Node> lNodeAA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAA");
    std::shared_ptr<Node> lNodeAB = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAB");
    std::shared_ptr<Node> lNodeAAA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAAA");
    lNodeAAA->setChangeEvents(OperationType::Move);
    lNodeAAA->setMoveOriginParentDbId(lNodeAA->idb());
    lNodeAAA->setMoveOrigin("A/AA/AAA");
    CPPUNIT_ASSERT(lNodeAAA->setParentNode(lNodeAB));
    lNodeAA->deleteChildren(lNodeAAA);
    CPPUNIT_ASSERT(lNodeAB->insertChildren(lNodeAAA));

    // Simulate move of node AA to AB, and rename AA to AAA, on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rA");
    std::shared_ptr<Node> rNodeAA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rAA");
    std::shared_ptr<Node> rNodeAB = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rAB");
    rNodeAA->setChangeEvents(OperationType::Move);
    rNodeAA->setMoveOriginParentDbId(rNodeA->idb());
    rNodeAA->setMoveOrigin("A/AA");
    CPPUNIT_ASSERT(rNodeAA->setParentNode(rNodeAB));
    rNodeAA->setName(Str("AAA"));
    rNodeA->deleteChildren(rNodeAA);
    CPPUNIT_ASSERT(rNodeAB->insertChildren(rNodeAA));

    Conflict conflict(lNodeAAA, rNodeAA, ConflictType::MoveMoveDest);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT_EQUAL(size_t(1), _syncPal->_syncOps->size());
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(!op->newName().empty());
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, op->targetSide());
    CPPUNIT_ASSERT_EQUAL(OperationType::Move, op->type());
}

void TestConflictResolverWorker::testMoveMoveCycle() {
    // Simulate move of node AA to AB on local replica
    std::shared_ptr<Node> lNodeA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lA");
    std::shared_ptr<Node> lNodeAA = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAA");
    std::shared_ptr<Node> lNodeAB = _syncPal->updateTree(ReplicaSide::Local)->getNodeById("lAB");
    lNodeAA->setChangeEvents(OperationType::Move);
    lNodeAA->setMoveOriginParentDbId(lNodeA->idb());
    lNodeAA->setMoveOrigin("A/AA");
    CPPUNIT_ASSERT(lNodeAA->setParentNode(lNodeAB));
    lNodeA->deleteChildren(lNodeAA);
    CPPUNIT_ASSERT(lNodeAB->insertChildren(lNodeAA));

    // Simulate move of node AB to AA, on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rA");
    std::shared_ptr<Node> rNodeAA = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rAA");
    std::shared_ptr<Node> rNodeAB = _syncPal->updateTree(ReplicaSide::Remote)->getNodeById("rAB");
    rNodeAB->setChangeEvents(OperationType::Move);
    rNodeAB->setMoveOriginParentDbId(rNodeA->idb());
    rNodeAB->setMoveOrigin("A/AB");
    CPPUNIT_ASSERT(rNodeAB->setParentNode(rNodeAA));
    rNodeA->deleteChildren(rNodeAB);
    CPPUNIT_ASSERT(rNodeAA->insertChildren(rNodeAB));

    Conflict conflict(lNodeAA, rNodeAA, ConflictType::MoveMoveCycle);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT_EQUAL(size_t(1), _syncPal->_syncOps->size());
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT_EQUAL(lNodeA, op->newParentNode());
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, op->targetSide());
    CPPUNIT_ASSERT_EQUAL(OperationType::Move, op->type());
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

    // Simulate move of node AAA to AB on local replica
    std::shared_ptr<Node> lNodeA = _syncPal->_localUpdateTree->getNodeById("lA");
    std::shared_ptr<Node> lNodeAA = _syncPal->_localUpdateTree->getNodeById("lAA");
    std::shared_ptr<Node> lNodeAAA = _syncPal->_localUpdateTree->getNodeById("lAAA");
    std::shared_ptr<Node> lNodeAB = _syncPal->_localUpdateTree->getNodeById("lAB");
    lNodeAAA->setChangeEvents(OperationType::Move);
    lNodeAAA->setMoveOriginParentDbId(lNodeAA->idb());
    lNodeAAA->setMoveOrigin("A/AA/AAA");
    CPPUNIT_ASSERT(lNodeAAA->setParentNode(lNodeAB));
    lNodeAA->deleteChildren(lNodeAAA);
    CPPUNIT_ASSERT(lNodeAB->insertChildren(lNodeAAA));

    // Simulate move of node AB to AAA, on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->_remoteUpdateTree->getNodeById("rA");
    std::shared_ptr<Node> rNodeAA = _syncPal->_remoteUpdateTree->getNodeById("rAA");
    std::shared_ptr<Node> rNodeAAA = _syncPal->_remoteUpdateTree->getNodeById("rAAA");
    std::shared_ptr<Node> rNodeAB = _syncPal->_remoteUpdateTree->getNodeById("rAB");
    rNodeAB->setChangeEvents(OperationType::Move);
    rNodeAB->setMoveOriginParentDbId(rNodeA->idb());
    rNodeAB->setMoveOrigin("A/AB");
    CPPUNIT_ASSERT(rNodeAB->setParentNode(rNodeAAA));
    rNodeA->deleteChildren(rNodeAB);
    CPPUNIT_ASSERT(rNodeAAA->insertChildren(rNodeAB));

    Conflict conflict(lNodeAAA, rNodeAAA, ConflictType::MoveMoveCycle);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT_EQUAL(size_t(1), _syncPal->_syncOps->size());
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT_EQUAL(lNodeAA, op->newParentNode());
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, op->targetSide());
    CPPUNIT_ASSERT_EQUAL(OperationType::Move, op->type());
}
} // namespace KDC
