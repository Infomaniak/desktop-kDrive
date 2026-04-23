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

#include "testsyncoperation.h"
#include "update_detection/update_detector/node.h"

#include "test_utility/testhelpers.h"

using namespace CppUnit;

namespace KDC {

void TestSyncOperation::setUp() {
    TestBase::start();

    const auto localRootNode =
            std::make_shared<Node>(ReplicaSide::Local, Str(""), NodeType::Directory, OperationType::None, "1", 0, 0, 0, nullptr);

    const auto remoteRootNode =
            std::make_shared<Node>(ReplicaSide::Remote, Str(""), NodeType::Directory, OperationType::None, "1", 0, 0, 0, nullptr);

    const auto localNodeA = std::make_shared<Node>(ReplicaSide::Local, Str("a"), NodeType::Directory, OperationType::None, "100",
                                                   0, 0, 0, localRootNode);

    const auto remoteNodeA = std::make_shared<Node>(ReplicaSide::Remote, Str("a"), NodeType::Directory, OperationType::None,
                                                    "100000", 0, 0, 0, remoteRootNode);

    _localNodeAA = std::make_shared<Node>(ReplicaSide::Local, Str("aa"), NodeType::Directory, OperationType::None, "200", 0, 0, 0,
                                          localNodeA);

    _remoteNodeAA = std::make_shared<Node>(ReplicaSide::Remote, Str("aa"), NodeType::Directory, OperationType::None, "200000", 0,
                                           0, 0, remoteNodeA);

    _localNodeAAA = std::make_shared<Node>(ReplicaSide::Local, Str("aaa"), NodeType::File, OperationType::None, "300", 0, 0, 0,
                                           _localNodeAA);

    _remoteNodeAAA = std::make_shared<Node>(ReplicaSide::Remote, Str("aaa"), NodeType::File, OperationType::Edit, "300000", 0,
                                            12345, 999, _remoteNodeAA);

    _localNodeAAB = std::make_shared<Node>(ReplicaSide::Local, Str("aab"), NodeType::File, OperationType::Delete, "301", 0, 0, 0,
                                           _localNodeAA);

    _remoteNodeAAB = std::make_shared<Node>(ReplicaSide::Remote, Str("aab"), NodeType::File, OperationType::None, "300001", 0, 0,
                                            0, _remoteNodeAA);

    _op1 = generateSyncOperation(OperationType::Edit, _remoteNodeAAA, _localNodeAAA);
    _op2 = generateSyncOperation(OperationType::Delete, _localNodeAAB, _remoteNodeAAB);
}

void TestSyncOperation::tearDown() {
    TestBase::stop();
}

void TestSyncOperation::testGetOpIdsFromSourceNodeId() {
    auto ops = _syncOperationList.getOpIdsFromSourceNodeId(*_remoteNodeAAA->id(), ReplicaSide::Remote);
    CPPUNIT_ASSERT(ops.size() == 1);
    CPPUNIT_ASSERT(ops.front() == _op1->id());

    ops = _syncOperationList.getOpIdsFromSourceNodeId(*_localNodeAAA->id(), ReplicaSide::Local);
    CPPUNIT_ASSERT(ops.empty());

    ops = _syncOperationList.getOpIdsFromSourceNodeId(*_localNodeAAB->id(), ReplicaSide::Local);
    CPPUNIT_ASSERT(ops.size() == 1);
    CPPUNIT_ASSERT(ops.front() == _op2->id());

    ops = _syncOperationList.getOpIdsFromSourceNodeId(*_remoteNodeAAB->id(), ReplicaSide::Remote);
    CPPUNIT_ASSERT(ops.empty());
}

void TestSyncOperation::testGetOpFromTargetNodeId() {
    auto op = _syncOperationList.getOpFromTargetNodeId(*_localNodeAAA->id(), ReplicaSide::Local, OperationType::Edit,
                                                       SyncPath(Str("a/aa/aaa")));
    CPPUNIT_ASSERT(op == _op1);

    op = _syncOperationList.getOpFromTargetNodeId(*_remoteNodeAAB->id(), ReplicaSide::Remote, OperationType::Delete,
                                                  SyncPath(Str("a/aa/aab")));
    CPPUNIT_ASSERT(op == _op2);

    op = _syncOperationList.getOpFromTargetNodeId(*_localNodeAA->id(), ReplicaSide::Local, OperationType::Edit,
                                                  SyncPath(Str("a/aa")));
    CPPUNIT_ASSERT(op == nullptr);

    op = _syncOperationList.getOpFromTargetNodeId(*_localNodeAAA->id(), ReplicaSide::Remote, OperationType::Edit,
                                                  SyncPath(Str("a/aa/aaa")));
    CPPUNIT_ASSERT(op == nullptr);

    op = _syncOperationList.getOpFromTargetNodeId(*_localNodeAAA->id(), ReplicaSide::Local, OperationType::Create,
                                                  SyncPath(Str("a/aa/aaa")));
    CPPUNIT_ASSERT(op == nullptr);
}

void TestSyncOperation::testIsLocalEditCausedBySync() {
    const auto rootPath = SyncPath(Str("kDrive"));

    auto res = _syncOperationList.isLocalEditCausedBySync(*_localNodeAAA->id(), rootPath, SyncPath(Str("a/aa/aaa")), 12345, 999);
    CPPUNIT_ASSERT(res == false);

    _op1->setPropagationStatus(SyncOperation::PropagationStatus::InProgress);
    res = _syncOperationList.isLocalEditCausedBySync(*_localNodeAAA->id(), rootPath, SyncPath(Str("a/aa/aaa")), 12345, 999);
    CPPUNIT_ASSERT(res == true);

    _op1->setPropagationStatus(SyncOperation::PropagationStatus::Propagated);
    res = _syncOperationList.isLocalEditCausedBySync(*_localNodeAAA->id(), rootPath, SyncPath(Str("a/aa/aaa")), 12345, 999);
    CPPUNIT_ASSERT(res == true);

    res = _syncOperationList.isLocalEditCausedBySync(*_localNodeAAA->id(), rootPath, SyncPath(Str("a/aa/aaa")), 24680, 999);
    CPPUNIT_ASSERT(res == false);

    res = _syncOperationList.isLocalEditCausedBySync(*_localNodeAAA->id(), rootPath, SyncPath(Str("a/aa/aaa")), 12345, 555);
    CPPUNIT_ASSERT(res == false);
}

void TestSyncOperation::testCountOps() {
    CPPUNIT_ASSERT(_syncOperationList.countOps(ReplicaSide::Local, OperationType::Delete) == 1);
    CPPUNIT_ASSERT(_syncOperationList.countOps(ReplicaSide::Local, OperationType::Edit) == 0);
    CPPUNIT_ASSERT(_syncOperationList.countOps(ReplicaSide::Local, OperationType::Create) == 0);
    CPPUNIT_ASSERT(_syncOperationList.countOps(ReplicaSide::Remote, OperationType::Delete) == 0);
    CPPUNIT_ASSERT(_syncOperationList.countOps(ReplicaSide::Remote, OperationType::Edit) == 1);
}

SyncOpPtr TestSyncOperation::generateSyncOperation(OperationType opType, const std::shared_ptr<Node> affectedNode,
                                                   const std::shared_ptr<Node> correspondingNode) {
    const auto op = std::make_shared<SyncOperation>();
    op->setType(opType);
    op->setAffectedNode(affectedNode);
    const auto targetSide = otherSide(affectedNode->side());
    if (opType != OperationType::Create) {
        op->setCorrespondingNode(correspondingNode);
    }
    op->setNewName(affectedNode->name());
    op->setTargetSide(targetSide);
    op->setNewParentNode(affectedNode->parentNode());
    (void) _syncOperationList.pushOp(op);
    return op;
}

} // namespace KDC
