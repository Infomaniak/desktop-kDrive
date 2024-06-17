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

namespace KDC {

void TestConflictResolverWorker::setUp() {
    // Create SyncPal
    bool alreadyExists;
    std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExists);
    std::filesystem::remove(parmsDbPath);
    ParmsDb::instance(parmsDbPath, "3.4.0", true, true);
    ParmsDb::instance()->setAutoDelete(true);

    SyncPath syncDbPath = Db::makeDbName(1, 1, 1, 1, alreadyExists);
    std::filesystem::remove(syncDbPath);
    _syncPal = std::shared_ptr<SyncPal>(new SyncPal(syncDbPath, "3.4.0", true));
    _syncPal->_syncDb->setAutoDelete(true);

    _syncPal->_conflictResolverWorker =
        std::shared_ptr<ConflictResolverWorker>(new ConflictResolverWorker(_syncPal, "Conflict Resolver", "CORE"));

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
    time_t tLoc = std::time(0);
    time_t tRemote = std::time(0);
    DbNodeId dbNodeIdA;
    DbNodeId dbNodeIdAA;
    DbNodeId dbNodeIdAB;
    DbNodeId dbNodeIdAAA;

    bool constraintError = false;
    DbNode dbNodeA(0, _syncPal->_syncDb->rootNode().nodeId(), Str("A"), Str("A"), "lA", "rA", tLoc, tLoc, tRemote,
                   NodeType::Directory, 0, std::nullopt);
    _syncPal->_syncDb->insertNode(dbNodeA, dbNodeIdA, constraintError);
    DbNode dbNodeAA(0, dbNodeIdA, Str("AA"), Str("AA"), "lAA", "rAA", tLoc, tLoc, tRemote, NodeType::Directory, 0,
                    std::nullopt);
    _syncPal->_syncDb->insertNode(dbNodeAA, dbNodeIdAA, constraintError);
    DbNode dbNodeAB(0, dbNodeIdA, Str("AB"), Str("AB"), "lAB", "rAB", tLoc, tLoc, tRemote, NodeType::Directory, 0,
                    std::nullopt);
    _syncPal->_syncDb->insertNode(dbNodeAB, dbNodeIdAB, constraintError);
    DbNode dbNodeAAA(0, dbNodeIdAA, Str("AAA"), Str("AAA"), "lAAA", "rAAA", tLoc, tLoc, tRemote, NodeType::File, 0,
                     std::nullopt);
    _syncPal->_syncDb->insertNode(dbNodeAAA, dbNodeIdAAA, constraintError);

    // Build update trees
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 1654788079;
    std::shared_ptr<Node> lNodeA = std::shared_ptr<Node>(new Node(dbNodeIdA, _syncPal->_localUpdateTree->side(), Str("A"),
                                                                  NodeType::Directory, OperationTypeNone, "lA", createdAt,
                                                                  lastmodified, size, _syncPal->_localUpdateTree->rootNode()));
    _syncPal->_localUpdateTree->rootNode()->insertChildren(lNodeA);
    _syncPal->_localUpdateTree->insertNode(lNodeA);
    std::shared_ptr<Node> lNodeAA =
        std::shared_ptr<Node>(new Node(dbNodeIdAA, _syncPal->_localUpdateTree->side(), Str("AA"), NodeType::Directory,
                                       OperationTypeNone, "lAA", createdAt, lastmodified, size, lNodeA));
    lNodeA->insertChildren(lNodeAA);
    _syncPal->_localUpdateTree->insertNode(lNodeAA);
    std::shared_ptr<Node> lNodeAB =
        std::shared_ptr<Node>(new Node(dbNodeIdAB, _syncPal->_localUpdateTree->side(), Str("AB"), NodeType::Directory,
                                       OperationTypeNone, "lAB", createdAt, lastmodified, size, lNodeA));
    lNodeA->insertChildren(lNodeAB);
    _syncPal->_localUpdateTree->insertNode(lNodeAB);
    std::shared_ptr<Node> lNodeAAA =
        std::shared_ptr<Node>(new Node(dbNodeIdAAA, _syncPal->_localUpdateTree->side(), Str("AAA"), NodeType::File,
                                       OperationTypeNone, "lAAA", createdAt, lastmodified, size, lNodeAA));
    lNodeAA->insertChildren(lNodeAAA);
    _syncPal->_localUpdateTree->insertNode(lNodeAAA);

    std::shared_ptr<Node> rNodeA = std::shared_ptr<Node>(new Node(dbNodeIdA, _syncPal->_remoteUpdateTree->side(), Str("A"),
                                                                  NodeType::Directory, OperationTypeNone, "rA", createdAt,
                                                                  lastmodified, size, _syncPal->_remoteUpdateTree->rootNode()));
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(rNodeA);
    _syncPal->_remoteUpdateTree->insertNode(rNodeA);
    std::shared_ptr<Node> rNodeAA =
        std::shared_ptr<Node>(new Node(dbNodeIdAA, _syncPal->_remoteUpdateTree->side(), Str("AA"), NodeType::Directory,
                                       OperationTypeNone, "rAA", createdAt, lastmodified, size, rNodeA));
    rNodeA->insertChildren(rNodeAA);
    _syncPal->_remoteUpdateTree->insertNode(rNodeAA);
    std::shared_ptr<Node> rNodeAB =
        std::shared_ptr<Node>(new Node(dbNodeIdAB, _syncPal->_remoteUpdateTree->side(), Str("AB"), NodeType::Directory,
                                       OperationTypeNone, "rAB", createdAt, lastmodified, size, rNodeA));
    rNodeA->insertChildren(rNodeAB);
    _syncPal->_remoteUpdateTree->insertNode(rNodeAB);
    std::shared_ptr<Node> rNodeAAA =
        std::shared_ptr<Node>(new Node(dbNodeIdAAA, _syncPal->_remoteUpdateTree->side(), Str("AAA"), NodeType::File,
                                       OperationTypeNone, "rAAA", createdAt, lastmodified, size, rNodeAA));
    rNodeAA->insertChildren(rNodeAAA);
    _syncPal->_remoteUpdateTree->insertNode(rNodeAAA);
}

void TestConflictResolverWorker::tearDown() {
    ParmsDb::instance()->close();
    _syncPal->_syncDb->close();
}

void TestConflictResolverWorker::testCreateCreate() {
    // Simulate file creation on both replica
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 1654788079;

    std::shared_ptr<Node> lNodeAA = _syncPal->_localUpdateTree->getNodeById("lAA");
    std::shared_ptr<Node> lNodeAAB =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_localUpdateTree->side(), Str("AAB.txt"), NodeType::File,
                                       OperationTypeCreate, "lAAB", createdAt, lastmodified, size, lNodeAA));
    lNodeAA->insertChildren(lNodeAAB);
    _syncPal->_localUpdateTree->insertNode(lNodeAAB);
    _syncPal->_localSnapshot->updateItem(
        SnapshotItem("lAAB", "lAA", Str("AAB.txt"), 1654788079, 1654788079, NodeType::File, 123, "lcs1"));

    std::shared_ptr<Node> rNodeAA = _syncPal->_remoteUpdateTree->getNodeById("rAA");
    std::shared_ptr<Node> rNodeAAB =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_remoteUpdateTree->side(), Str("AAB.txt"), NodeType::File,
                                       OperationTypeCreate, "rAAB", createdAt, lastmodified, size, rNodeAA));
    rNodeAA->insertChildren(rNodeAAB);
    _syncPal->_remoteUpdateTree->insertNode(rNodeAAB);
    _syncPal->_remoteSnapshot->updateItem(
        SnapshotItem("rAAB", "rAA", Str("AAB.txt"), 1654788079, 1654788079, NodeType::File, 123, "rcs1"));

    Conflict conflict(lNodeAAB, rNodeAA, ConflictType::CreateCreate);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() == 1);
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(!op->newName().empty());
    CPPUNIT_ASSERT(op->targetSide() == ReplicaSide::Local);
    CPPUNIT_ASSERT(op->type() == OperationTypeMove);
}

void TestConflictResolverWorker::testEditEdit() {
    // Simulate edit conflict of file AAA on both replica
    std::shared_ptr<Node> lNodeAAA = _syncPal->_localUpdateTree->getNodeById("lAAA");
    lNodeAAA->setChangeEvents(OperationTypeEdit);
    std::shared_ptr<Node> rNodeAAA = _syncPal->_remoteUpdateTree->getNodeById("rAAA");
    rNodeAAA->setChangeEvents(OperationTypeEdit);
    Conflict conflict(lNodeAAA, rNodeAAA, ConflictType::EditEdit);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() == 1);
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(!op->newName().empty());
    CPPUNIT_ASSERT(op->targetSide() == ReplicaSide::Local);
    CPPUNIT_ASSERT(op->type() == OperationTypeMove);
}

void TestConflictResolverWorker::testMoveCreate() {
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 1654788079;

    // Simulate create file ABA in AB on local replica
    std::shared_ptr<Node> lNodeAB = _syncPal->_localUpdateTree->getNodeById("lAB");
    std::shared_ptr<Node> lNodeABA =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_localUpdateTree->side(), Str("ABA"), NodeType::File,
                                       OperationTypeCreate, "lABA", createdAt, lastmodified, size, lNodeAB));
    lNodeAB->insertChildren(lNodeABA);
    _syncPal->_localUpdateTree->insertNode(lNodeABA);

    // Simulate move of file AAA in AA to ABA in AB on remote replica
    std::shared_ptr<Node> rNodeAA = _syncPal->_remoteUpdateTree->getNodeById("rAA");
    std::shared_ptr<Node> rNodeAAA = _syncPal->_remoteUpdateTree->getNodeById("rAAA");
    std::shared_ptr<Node> rNodeAB = _syncPal->_remoteUpdateTree->getNodeById("rAB");
    std::shared_ptr<Node> rNodeABA = rNodeAAA;
    rNodeABA->setName(Str("ABA"));
    rNodeABA->setParentNode(rNodeAB);
    rNodeABA->setChangeEvents(OperationTypeMove);
    rNodeABA->setMoveOriginParentDbId(rNodeAA->idb());

    Conflict conflict(lNodeABA, rNodeABA, ConflictType::MoveCreate);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() == 1);
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(!op->newName().empty());
    CPPUNIT_ASSERT(op->targetSide() == ReplicaSide::Local);
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

    Conflict conflict(lNodeAAA, rNodeAAA, ConflictType::EditDelete);
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

    Conflict conflict(lNodeAAA, rNodeAAA, ConflictType::EditDelete);
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
    CPPUNIT_ASSERT(_syncPal->_syncOps->size() == 1);
    CPPUNIT_ASSERT(op->omit() == true);
    CPPUNIT_ASSERT(op->type() == OperationTypeDelete);
}

void TestConflictResolverWorker::testMoveDelete2() {
    // Simulate rename of node A to B on local replica
    std::shared_ptr<Node> lNodeA = _syncPal->_localUpdateTree->getNodeById("lA");
    lNodeA->setName(Str("B"));
    lNodeA->setChangeEvents(OperationTypeMove);

    // Simulate edit of node AAA on local replica
    std::shared_ptr<Node> lNodeAAA = _syncPal->_localUpdateTree->getNodeById("lAAA");
    lNodeAAA->setChangeEvents(OperationTypeEdit);

    // Simulate create of node ABA on local replica
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 1654788079;
    std::shared_ptr<Node> lNodeAB = _syncPal->_localUpdateTree->getNodeById("lAB");
    std::shared_ptr<Node> lNodeABA =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_localUpdateTree->side(), Str("ABA"), NodeType::File,
                                       OperationTypeCreate, "lABA", createdAt, lastmodified, size, lNodeAB));
    lNodeAB->insertChildren(lNodeABA);
    _syncPal->_localUpdateTree->insertNode(lNodeABA);

    // Simulate a delete of node A on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->_remoteUpdateTree->getNodeById("rA");
    rNodeA->setChangeEvents(OperationTypeDelete);
    _syncPal->_remoteUpdateTree->rootNode()->deleteChildren(rNodeA);

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
    CPPUNIT_ASSERT(_syncPal->_syncOps->size() == 1);
    CPPUNIT_ASSERT(op->omit() == true);
    CPPUNIT_ASSERT(op->type() == OperationTypeDelete);
}

void TestConflictResolverWorker::testMoveDelete3() {
    // Simulate rename of node A to B on local replica
    std::shared_ptr<Node> lNodeA = _syncPal->_localUpdateTree->getNodeById("lA");
    lNodeA->setName(Str("B"));
    lNodeA->setChangeEvents(OperationTypeMove);

    // Simulate move of node AB under root on local replica
    std::shared_ptr<Node> lNodeAB = _syncPal->_localUpdateTree->getNodeById("lAB");
    lNodeAB->setChangeEvents(OperationTypeMove);
    lNodeAB->setParentNode(_syncPal->_localUpdateTree->rootNode());
    lNodeA->deleteChildren(lNodeAB);
    _syncPal->_localUpdateTree->rootNode()->insertChildren(lNodeAB);

    // Simulate a delete of node A on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->_remoteUpdateTree->getNodeById("rA");
    rNodeA->setChangeEvents(OperationTypeDelete);
    _syncPal->_remoteUpdateTree->rootNode()->deleteChildren(rNodeA);

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
    CPPUNIT_ASSERT(_syncPal->_syncOps->size() == 1);
    CPPUNIT_ASSERT(op->omit() == true);
    CPPUNIT_ASSERT(op->type() == OperationTypeDelete);
}

void TestConflictResolverWorker::testMoveDelete4() {
    // Simulate rename of node A to B on local replica
    std::shared_ptr<Node> lNodeA = _syncPal->_localUpdateTree->getNodeById("lA");
    lNodeA->setName(Str("B"));
    lNodeA->setChangeEvents(OperationTypeMove);

    // Simulate move of node AB under root on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->_remoteUpdateTree->getNodeById("rA");
    std::shared_ptr<Node> rNodeAB = _syncPal->_remoteUpdateTree->getNodeById("rAB");
    rNodeAB->setChangeEvents(OperationTypeMove);
    rNodeAB->setParentNode(_syncPal->_remoteUpdateTree->rootNode());
    rNodeA->deleteChildren(rNodeAB);
    _syncPal->_remoteUpdateTree->rootNode()->insertChildren(rNodeAB);

    // Simulate a delete of node A on remote replica
    rNodeA->setChangeEvents(OperationTypeDelete);
    _syncPal->_remoteUpdateTree->rootNode()->deleteChildren(rNodeA);

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
    CPPUNIT_ASSERT(_syncPal->_syncOps->size() == 2);
    CPPUNIT_ASSERT(_syncPal->_conflictResolverWorker->registeredOrphans().size() > 0);
    for (const auto &opId : _syncPal->_syncOps->opSortedList()) {
        SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
        CPPUNIT_ASSERT(op->omit() == true);

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
    // Simulate move of node AA to AA' on local replica
    std::shared_ptr<Node> lNodeAA = _syncPal->_localUpdateTree->getNodeById("lAA");
    lNodeAA->setName(Str("A"));
    lNodeAA->setChangeEvents(OperationTypeMove);

    // Simulate a delete of node A on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->_remoteUpdateTree->getNodeById("rA");
    rNodeA->setChangeEvents(OperationTypeDelete);
    _syncPal->_remoteUpdateTree->rootNode()->deleteChildren(rNodeA);

    Conflict conflict1(lNodeAA, rNodeA, ConflictType::MoveDelete);
    _syncPal->_conflictQueue->push(conflict1);

    // This should be treated as a Move-ParentDelete conflict, the Move-Delete conflict must be ignored
    // For this test, we only make sure that the Move-Delete conflict is ignored
    // In real situation, a Move-ParentDelete should have been genereated as well
    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() == 0);
}

void TestConflictResolverWorker::testMoveParentDelete() {
    // Simulate rename of node AAA to AB on local replica
    std::shared_ptr<Node> lNodeAA = _syncPal->_localUpdateTree->getNodeById("lAA");
    std::shared_ptr<Node> lNodeAB = _syncPal->_localUpdateTree->getNodeById("lAB");
    std::shared_ptr<Node> lNodeAAA = _syncPal->_localUpdateTree->getNodeById("lAAA");
    lNodeAAA->setChangeEvents(OperationTypeMove);
    lNodeAAA->setMoveOriginParentDbId(lNodeAA->idb());
    lNodeAA->deleteChildren(lNodeAAA);
    lNodeAB->insertChildren(lNodeAAA);

    // Simulate a delete of node A on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->_remoteUpdateTree->getNodeById("rA");
    rNodeA->setChangeEvents(OperationTypeDelete);
    _syncPal->_remoteUpdateTree->rootNode()->deleteChildren(rNodeA);

    Conflict conflict(lNodeAAA, rNodeA, ConflictType::MoveParentDelete);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    // We should only undo the move operation on the move replica
    CPPUNIT_ASSERT(_syncPal->_syncOps->size() == 1);
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(op->omit() == false);
    CPPUNIT_ASSERT(op->targetSide() == ReplicaSide::Local);
    CPPUNIT_ASSERT(op->type() == OperationTypeMove);
}

void TestConflictResolverWorker::testCreateParentDelete() {
    // Simulate file creation on local replica
    SyncTime createdAt = 1654788079;
    SyncTime lastmodified = 1654788079;
    int64_t size = 1654788079;

    std::shared_ptr<Node> lNodeAA = _syncPal->_localUpdateTree->getNodeById("lAA");
    std::shared_ptr<Node> lNodeAAB =
        std::shared_ptr<Node>(new Node(std::nullopt, _syncPal->_localUpdateTree->side(), Str("AAB.txt"), NodeType::File,
                                       OperationTypeCreate, "lAAB", createdAt, lastmodified, size, lNodeAA));
    lNodeAA->insertChildren(lNodeAAB);
    _syncPal->_localUpdateTree->insertNode(lNodeAAB);
    _syncPal->_localSnapshot->updateItem(
        SnapshotItem("lAAB", "lAA", Str("AAB.txt"), 1654788079, 1654788079, NodeType::File, 123, "lcs1"));

    // Simulate a delete of node AA on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->_remoteUpdateTree->getNodeById("rA");
    std::shared_ptr<Node> rNodeAA = _syncPal->_remoteUpdateTree->getNodeById("rAA");
    rNodeAA->setChangeEvents(OperationTypeDelete);
    rNodeA->deleteChildren(rNodeAA);

    Conflict conflict(lNodeAAB, rNodeAA, ConflictType::CreateParentDelete);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() == 1);
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(!op->newName().empty());
    CPPUNIT_ASSERT(op->targetSide() == ReplicaSide::Local);
    CPPUNIT_ASSERT(op->type() == OperationTypeMove);
}

void TestConflictResolverWorker::testMoveMoveSource1() {
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

    Conflict conflict(lNodeAAA, rNodeAAA, ConflictType::MoveMoveSource);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() == 1);
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(!op->newName().empty());
    CPPUNIT_ASSERT(op->targetSide() == ReplicaSide::Local);
    CPPUNIT_ASSERT(op->type() == OperationTypeMove);
}

void TestConflictResolverWorker::testMoveMoveSource2() {
    // Simulate move of node AAA to AB on local replica
    std::shared_ptr<Node> lNodeAA = _syncPal->_localUpdateTree->getNodeById("lAA");
    std::shared_ptr<Node> lNodeAB = _syncPal->_localUpdateTree->getNodeById("lAB");
    std::shared_ptr<Node> lNodeAAA = _syncPal->_localUpdateTree->getNodeById("lAAA");
    lNodeAAA->setChangeEvents(OperationTypeMove);
    lNodeAAA->setParentNode(lNodeAB);
    lNodeAAA->setMoveOriginParentDbId(lNodeAA->idb());
    lNodeAAA->setMoveOrigin("A/AA/AAA");
    lNodeAA->deleteChildren(lNodeAAA);
    lNodeAB->insertChildren(lNodeAAA);

    // Simulate move of node AAA to A on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->_remoteUpdateTree->getNodeById("rA");
    std::shared_ptr<Node> rNodeAA = _syncPal->_remoteUpdateTree->getNodeById("rAA");
    std::shared_ptr<Node> rNodeAAA = _syncPal->_remoteUpdateTree->getNodeById("rAAA");
    rNodeAAA->setChangeEvents(OperationTypeMove);
    lNodeAAA->setParentNode(rNodeA);
    rNodeAAA->setMoveOriginParentDbId(rNodeAA->idb());
    rNodeAAA->setMoveOrigin("A/AA/AAA");
    rNodeAA->deleteChildren(lNodeAAA);
    rNodeA->insertChildren(lNodeAAA);

    Conflict conflict(lNodeAAA, rNodeAAA, ConflictType::MoveMoveSource);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->_registeredOrphans.insert({*rNodeAAA->idb(), ReplicaSide::Remote});

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() == 1);
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(op->newName() == Str("AAA"));
    CPPUNIT_ASSERT(op->targetSide() == ReplicaSide::Local);
    CPPUNIT_ASSERT(op->type() == OperationTypeMove);
}

void TestConflictResolverWorker::testMoveMoveDest() {
    // Simulate move of node AAA to AB on local replica
    std::shared_ptr<Node> lNodeAA = _syncPal->_localUpdateTree->getNodeById("lAA");
    std::shared_ptr<Node> lNodeAB = _syncPal->_localUpdateTree->getNodeById("lAB");
    std::shared_ptr<Node> lNodeAAA = _syncPal->_localUpdateTree->getNodeById("lAAA");
    lNodeAAA->setChangeEvents(OperationTypeMove);
    lNodeAAA->setParentNode(lNodeAB);
    lNodeAAA->setMoveOriginParentDbId(lNodeAA->idb());
    lNodeAAA->setMoveOrigin("A/AA/AAA");
    lNodeAA->deleteChildren(lNodeAAA);
    lNodeAB->insertChildren(lNodeAAA);

    // Simulate move of node AA to AB, and rename AA to AAA, on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->_remoteUpdateTree->getNodeById("rA");
    std::shared_ptr<Node> rNodeAA = _syncPal->_remoteUpdateTree->getNodeById("rAA");
    std::shared_ptr<Node> rNodeAB = _syncPal->_remoteUpdateTree->getNodeById("rAB");
    rNodeAA->setChangeEvents(OperationTypeMove);
    rNodeAA->setParentNode(rNodeAB);
    rNodeAA->setName(Str("AAA"));
    rNodeAA->setMoveOriginParentDbId(rNodeA->idb());
    rNodeAA->setMoveOrigin("A/AA");
    rNodeA->deleteChildren(rNodeAA);
    rNodeAB->insertChildren(rNodeAA);

    Conflict conflict(lNodeAAA, rNodeAA, ConflictType::MoveMoveDest);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() == 1);
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(!op->newName().empty());
    CPPUNIT_ASSERT(op->targetSide() == ReplicaSide::Local);
    CPPUNIT_ASSERT(op->type() == OperationTypeMove);
}

void TestConflictResolverWorker::testMoveMoveCycle() {
    // Simulate move of node AA to AB on local replica
    std::shared_ptr<Node> lNodeA = _syncPal->_localUpdateTree->getNodeById("lA");
    std::shared_ptr<Node> lNodeAA = _syncPal->_localUpdateTree->getNodeById("lAA");
    std::shared_ptr<Node> lNodeAB = _syncPal->_localUpdateTree->getNodeById("lAB");
    lNodeAA->setChangeEvents(OperationTypeMove);
    lNodeAA->setParentNode(lNodeAB);
    lNodeAA->setMoveOriginParentDbId(lNodeA->idb());
    lNodeAA->setMoveOrigin("A/AA");
    lNodeA->deleteChildren(lNodeAA);
    lNodeAB->insertChildren(lNodeAA);

    // Simulate move of node AB to AA, on remote replica
    std::shared_ptr<Node> rNodeA = _syncPal->_remoteUpdateTree->getNodeById("rA");
    std::shared_ptr<Node> rNodeAA = _syncPal->_remoteUpdateTree->getNodeById("rAA");
    std::shared_ptr<Node> rNodeAB = _syncPal->_remoteUpdateTree->getNodeById("rAB");
    rNodeAB->setChangeEvents(OperationTypeMove);
    rNodeAB->setParentNode(rNodeAA);
    rNodeAB->setMoveOriginParentDbId(rNodeA->idb());
    rNodeAB->setMoveOrigin("A/AB");
    rNodeA->deleteChildren(rNodeAB);
    rNodeAA->insertChildren(rNodeAB);

    Conflict conflict(lNodeAA, rNodeAA, ConflictType::MoveMoveCycle);
    _syncPal->_conflictQueue->push(conflict);

    _syncPal->_conflictResolverWorker->execute();

    CPPUNIT_ASSERT(_syncPal->_syncOps->size() == 1);
    UniqueId opId = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
    CPPUNIT_ASSERT(op->newParentNode() == lNodeA);
    CPPUNIT_ASSERT(op->targetSide() == ReplicaSide::Local);
    CPPUNIT_ASSERT(op->type() == OperationTypeMove);
}

}  // namespace KDC
