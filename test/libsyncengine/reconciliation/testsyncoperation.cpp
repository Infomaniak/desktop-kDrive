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
}

void TestSyncOperation::tearDown() {
    TestBase::stop();
}

void TestSyncOperation::testAll() {
    SyncOperationList syncOperationList;

    const auto localRootNode =
            std::make_shared<Node>(ReplicaSide::Local, Str(""), NodeType::Directory, OperationType::None, "1", 0, 0, 0, nullptr);

    const auto remoteRootNode =
            std::make_shared<Node>(ReplicaSide::Remote, Str(""), NodeType::Directory, OperationType::None, "1", 0, 0, 0, nullptr);

    const auto localNodeA = std::make_shared<Node>(ReplicaSide::Local, Str("a"), NodeType::Directory, OperationType::None, "100",
                                                   0, 0, 0, localRootNode);

    const auto remoteNodeA = std::make_shared<Node>(ReplicaSide::Remote, Str("a"), NodeType::Directory, OperationType::None,
                                                    "100000", 0, 0, 0, remoteRootNode);

    const auto localNodeAA = std::make_shared<Node>(ReplicaSide::Local, Str("aa"), NodeType::Directory, OperationType::None,
                                                    "200", 0, 0, 0, localNodeA);

    const auto remoteNodeAA = std::make_shared<Node>(ReplicaSide::Remote, Str("aa"), NodeType::Directory, OperationType::None,
                                                     "200000", 0, 0, 0, remoteNodeA);

    const auto localNodeAAA = std::make_shared<Node>(ReplicaSide::Local, Str("aaa"), NodeType::File, OperationType::None, "300",
                                                     0, 0, 0, localNodeAA);

    const auto remoteNodeAAA = std::make_shared<Node>(ReplicaSide::Remote, Str("aaa"), NodeType::File, OperationType::Edit,
                                                      "300000", 0, 12345, 999, remoteNodeAA);

    const auto localNodeAAB = std::make_shared<Node>(ReplicaSide::Local, Str("aab"), NodeType::File, OperationType::Delete, "301",
                                                     0, 0, 0, localNodeAA);

    const auto remoteNodeAAB = std::make_shared<Node>(ReplicaSide::Remote, Str("aab"), NodeType::File, OperationType::None,
                                                      "300001", 0, 0, 0, remoteNodeAA);

    const auto op1 = generateSyncOperation(OperationType::Edit, remoteNodeAAA, localNodeAAA);
    CPPUNIT_ASSERT(syncOperationList.pushOp(op1));

    const auto op2 = generateSyncOperation(OperationType::Delete, localNodeAAB, remoteNodeAAB);
    CPPUNIT_ASSERT(syncOperationList.pushOp(op2));

    // getOpFromTargetNodeId
    auto op = syncOperationList.getOpFromTargetNodeId(*localNodeAAA->id(), ReplicaSide::Local, OperationType::Edit,
                                                      SyncPath(Str("a/aa/aaa")));
    CPPUNIT_ASSERT(op == op1);

    op = syncOperationList.getOpFromTargetNodeId(*remoteNodeAAB->id(), ReplicaSide::Remote, OperationType::Delete,
                                                 SyncPath(Str("a/aa/aab")));
    CPPUNIT_ASSERT(op == op2);

    op = syncOperationList.getOpFromTargetNodeId(*localNodeAA->id(), ReplicaSide::Local, OperationType::Edit,
                                                 SyncPath(Str("a/aa")));
    CPPUNIT_ASSERT(op == nullptr);

    op = syncOperationList.getOpFromTargetNodeId(*localNodeAAA->id(), ReplicaSide::Remote, OperationType::Edit,
                                                 SyncPath(Str("a/aa/aaa")));
    CPPUNIT_ASSERT(op == nullptr);

    op = syncOperationList.getOpFromTargetNodeId(*localNodeAAA->id(), ReplicaSide::Local, OperationType::Create,
                                                 SyncPath(Str("a/aa/aaa")));
    CPPUNIT_ASSERT(op == nullptr);

    // isLocalEditCausedBySync
    auto res = syncOperationList.isLocalEditCausedBySync(*localNodeAAA->id(), SyncPath(Str("/Users/John/kDrive")),
                                                         SyncPath(Str("a/aa/aaa")), 12345, 999);
    CPPUNIT_ASSERT(res == false);

    op1->setPropagationStatus(SyncOperation::PropagationStatus::InProgress);
    res = syncOperationList.isLocalEditCausedBySync(*localNodeAAA->id(), SyncPath(Str("/Users/John/kDrive")),
                                                    SyncPath(Str("a/aa/aaa")), 12345, 999);
    CPPUNIT_ASSERT(res == true);

    op1->setPropagationStatus(SyncOperation::PropagationStatus::Propagated);
    res = syncOperationList.isLocalEditCausedBySync(*localNodeAAA->id(), SyncPath(Str("/Users/John/kDrive")),
                                                    SyncPath(Str("a/aa/aaa")), 12345, 999);
    CPPUNIT_ASSERT(res == true);

    res = syncOperationList.isLocalEditCausedBySync(*localNodeAAA->id(), SyncPath(Str("/Users/John/kDrive")),
                                                    SyncPath(Str("a/aa/aaa")), 24680, 999);
    CPPUNIT_ASSERT(res == false);

    res = syncOperationList.isLocalEditCausedBySync(*localNodeAAA->id(), SyncPath(Str("/Users/John/kDrive")),
                                                    SyncPath(Str("a/aa/aaa")), 12345, 555);
    CPPUNIT_ASSERT(res == false);
}

SyncOpPtr TestSyncOperation::generateSyncOperation(OperationType opType, const std::shared_ptr<Node> affectedNode,
                                                   const std::shared_ptr<Node> correspondingNode) const {
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
    return op;
}

} // namespace KDC
