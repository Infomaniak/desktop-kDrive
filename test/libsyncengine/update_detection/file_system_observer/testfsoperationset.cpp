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

#include "testfsoperationset.h"
#include "update_detection/file_system_observer/fsoperationset.h"

using namespace CppUnit;

namespace KDC {

void TestFsOperationSet::setUp() {
    TestBase::start();
    _operationTypes.push_back(OperationType::None);
    _operationTypes.push_back(OperationType::Create);
    _operationTypes.push_back(OperationType::Move);
    _operationTypes.push_back(OperationType::Edit);
    _operationTypes.push_back(OperationType::Delete);
    _operationTypes.push_back(OperationType::Rights);
}

void TestFsOperationSet::tearDown() {
    TestBase::stop();
}


void TestFsOperationSet::testGetOp() {
    FSOperationSet fsOperationSet(ReplicaSide::Unknown);
    FSOpPtr opPtr;

    // Test getOp with an empty set
    CPPUNIT_ASSERT(!fsOperationSet.getOp(1, opPtr));

    // Test getOp with an existing operation
    auto op = std::make_shared<FSOperation>(OperationType::Create, "node_1", NodeType::File);
    fsOperationSet.insertOp(op);

    CPPUNIT_ASSERT(fsOperationSet.getOp(op->id(), opPtr));
    CPPUNIT_ASSERT(opPtr == op);
}

void TestFsOperationSet::testGetOpsByType() {
    FSOperationSet fsOperationSet(ReplicaSide::Unknown);
    NodeId nodeId = "nodeId";
    char nodeIdSuffix = '0';

    for (OperationType type: _operationTypes) {
        for (int i = 0; i < 3; i++) {
            std::unordered_set<UniqueId> ops = fsOperationSet.getOpsByType(type);
            CPPUNIT_ASSERT_EQUAL(size_t(i), ops.size());

            auto op = std::make_shared<FSOperation>(type, nodeId + nodeIdSuffix);
            nodeIdSuffix++;
            fsOperationSet.insertOp(op);
        }
    }
}

void TestFsOperationSet::testGetOpsByNodeId() {
    // Initialize the set
    FSOperationSet fsOperationSet(ReplicaSide::Unknown);
    NodeId nodeId = "nodeId";
    std::vector<UniqueId> opsIds;
    for (OperationType type: _operationTypes) {
        auto op = std::make_shared<FSOperation>(type, nodeId);
        fsOperationSet.insertOp(op);
        opsIds.push_back(op->id());
    }

    // Test with a non existing node
    std::unordered_set<UniqueId> ops = fsOperationSet.getOpsByNodeId("nonExistingNodeId");
    CPPUNIT_ASSERT_EQUAL(size_t(0), ops.size());

    // Test with an existing node
    ops = fsOperationSet.getOpsByNodeId(nodeId);
    CPPUNIT_ASSERT_EQUAL(size_t(6), ops.size());
    for (auto id: opsIds) {
        CPPUNIT_ASSERT(ops.contains(id));
    }

    // Test with an empty set
    fsOperationSet.clear();
    ops = fsOperationSet.getOpsByNodeId(nodeId);
    CPPUNIT_ASSERT_EQUAL(size_t(0), ops.size());
}

void TestFsOperationSet::testNbOpsByType() {
    FSOperationSet fsOperationSet(ReplicaSide::Unknown);
    NodeId nodeId = "nodeId";
    char nodeIdSuffix = '0';

    for (OperationType type: _operationTypes) {
        for (int i = 0; i < 3; i++) {
            uint64_t nbOps = 0;
            nbOps = fsOperationSet.nbOpsByType(type);
            CPPUNIT_ASSERT_EQUAL(uint64_t(i), nbOps);

            auto op = std::make_shared<FSOperation>(type, nodeId + nodeIdSuffix);
            nodeIdSuffix++;
            fsOperationSet.insertOp(op);
        }
    }
}

void TestFsOperationSet::testClear() {
    // Initialize the set
    FSOperationSet fsOperationSet(ReplicaSide::Unknown);
    NodeId nodeId = "nodeId";
    char nodeIdSuffix = '0';

    for (OperationType type: _operationTypes) {
        for (int i = 1; i <= 2; i++) {
            auto op = std::make_shared<FSOperation>(type, nodeId + nodeIdSuffix);
            nodeIdSuffix++;
            fsOperationSet.insertOp(op);
        }
    }

    // Test clear
    fsOperationSet.clear();

    nodeIdSuffix = '0';
    CPPUNIT_ASSERT_EQUAL(uint64_t{0}, fsOperationSet.nbOps());
    for (OperationType type: _operationTypes) {
        CPPUNIT_ASSERT_EQUAL(uint64_t{0}, fsOperationSet.nbOpsByType(type));
        for (int i = 1; i <= 2; i++) {
            std::unordered_set<UniqueId> ops = fsOperationSet.getOpsByNodeId(nodeId + nodeIdSuffix);
            CPPUNIT_ASSERT_EQUAL(size_t(0), ops.size());
            nodeIdSuffix++;
        }
    }
}

void TestFsOperationSet::testInsertOp() {
    FSOperationSet fsOperationSet(ReplicaSide::Unknown);
    NodeId nodeId = "nodeId";
    char nodeIdSuffix = 0;

    for (OperationType type: _operationTypes) {
        for (int i = 0; i < 3; i++) {
            auto op = std::make_shared<FSOperation>(type, nodeId + nodeIdSuffix);
            fsOperationSet.insertOp(op);
            nodeIdSuffix++;
            std::unordered_map<UniqueId, FSOpPtr> opsAll = fsOperationSet.getAllOps();
            CPPUNIT_ASSERT(opsAll.contains(op->id()));
            CPPUNIT_ASSERT_EQUAL(uint64_t(i + 1), fsOperationSet.nbOpsByType(type));
            std::unordered_set<UniqueId> ops = fsOperationSet.getOpsByNodeId(op->nodeId());
            CPPUNIT_ASSERT(ops.contains(op->id()));
            ops = fsOperationSet.getOpsByType(type);
            CPPUNIT_ASSERT(ops.contains(op->id()));
        }
    }
}

void TestFsOperationSet::testRemoveOp() {
    FSOperationSet fsOperationSet(ReplicaSide::Unknown);
    NodeId nodeId = "nodeId";
    auto op1 = std::make_shared<FSOperation>(OperationType::Create, nodeId);
    fsOperationSet.insertOp(op1);
    auto op2 = std::make_shared<FSOperation>(OperationType::Move, nodeId);
    fsOperationSet.insertOp(op2);

    std::unordered_map<UniqueId, FSOpPtr> allOps;

    // Test removeOp with an existing operation (uniqueId)
    CPPUNIT_ASSERT(fsOperationSet.removeOp(op1->id()));
    allOps = fsOperationSet.getAllOps();
    CPPUNIT_ASSERT(!allOps.contains(op1->id()));
    std::unordered_set<UniqueId> ops = fsOperationSet.getOpsByType(OperationType::Create);
    CPPUNIT_ASSERT(!ops.contains(op1->id()));
    ops = fsOperationSet.getOpsByNodeId(nodeId);
    CPPUNIT_ASSERT(!ops.contains(op1->id()));

    // Test removeOp with an existing operation (Node id + type)
    CPPUNIT_ASSERT(fsOperationSet.removeOp(nodeId, OperationType::Move));
    allOps = fsOperationSet.getAllOps();
    CPPUNIT_ASSERT(!allOps.contains(op2->id()));
    ops = fsOperationSet.getOpsByType(OperationType::Move);
    CPPUNIT_ASSERT(!ops.contains(op2->id()));
    ops = fsOperationSet.getOpsByNodeId(nodeId);
    CPPUNIT_ASSERT(!ops.contains(op2->id()));

    // Test removeOp with a not existing operation
    CPPUNIT_ASSERT(!fsOperationSet.removeOp(1));
}

void TestFsOperationSet::testfindOp() {
    FSOperationSet fsOperationSet(ReplicaSide::Unknown);
    NodeId nodeId = "nodeId";

    for (auto type: _operationTypes) {
        auto op = std::make_shared<FSOperation>(type, nodeId);
        fsOperationSet.insertOp(op);
    }

    for (auto type: _operationTypes) {
        FSOpPtr res;
        CPPUNIT_ASSERT(fsOperationSet.findOp(nodeId, type, res));
        CPPUNIT_ASSERT(res->operationType() == type);
    }

    // Test findOp with a non existing operation.
    fsOperationSet.clear();
    FSOpPtr res;
    fsOperationSet.insertOp(std::make_shared<FSOperation>(OperationType::Create, nodeId));
    CPPUNIT_ASSERT(!fsOperationSet.findOp("nonExistingNodeId", OperationType::Create, res));
    CPPUNIT_ASSERT(!fsOperationSet.findOp(nodeId, OperationType::Move, res));
}

void TestFsOperationSet::testOperatorEqual() {
    FSOperationSet fsOperationSet1(ReplicaSide::Unknown);
    FSOperationSet fsOperationSet2(ReplicaSide::Unknown);

    NodeId nodeId = "nodeId";
    char nodeIdSuffix = '0';
    for (OperationType type: _operationTypes) {
        for (int i = 0; i < 3; i++) {
            auto op = std::make_shared<FSOperation>(type, nodeId + nodeIdSuffix);
            nodeIdSuffix++;
            fsOperationSet2.insertOp(op);
        }
    }

    fsOperationSet1 = fsOperationSet2;

    /// ops()
    CPPUNIT_ASSERT_EQUAL(fsOperationSet2.nbOps(), fsOperationSet1.nbOps());
    std::unordered_map<UniqueId, FSOpPtr> ops1;
    std::unordered_map<UniqueId, FSOpPtr> ops2;
    ops1 = fsOperationSet1.getAllOps();
    ops2 = fsOperationSet2.getAllOps();
    for (const auto &[key, value]: ops2) {
        CPPUNIT_ASSERT(ops2.contains(key));
    }

    /// getOpsByType() and getOpsByNodeId()
    nodeIdSuffix = '0';
    for (OperationType type: _operationTypes) {
        std::unordered_set<UniqueId> opsByType1;
        std::unordered_set<UniqueId> opsByType2;
        opsByType1 = fsOperationSet1.getOpsByType(type);
        opsByType2 = fsOperationSet2.getOpsByType(type);
        CPPUNIT_ASSERT_EQUAL(opsByType2.size(), opsByType1.size());
        for (const auto &id: opsByType2) {
            CPPUNIT_ASSERT(opsByType1.contains(id));
        }

        for (int i = 0; i < 3; i++) {
            std::unordered_set<UniqueId> opsByNodeId1;
            std::unordered_set<UniqueId> opsByNodeId2;
            opsByNodeId1 = fsOperationSet1.getOpsByNodeId(nodeId + nodeIdSuffix);
            opsByNodeId2 = fsOperationSet2.getOpsByNodeId(nodeId + nodeIdSuffix);
            CPPUNIT_ASSERT_EQUAL(opsByNodeId2.size(), opsByNodeId1.size());
            for (const auto &id: opsByNodeId2) {
                CPPUNIT_ASSERT(opsByNodeId1.contains(id));
            }
            nodeIdSuffix++;
        }
    }

    // Test that the two sets are independent
    CPPUNIT_ASSERT(&fsOperationSet1 != &fsOperationSet2);
}

void TestFsOperationSet::testCopyConstructor() {
    FSOperationSet fsOperationSet(ReplicaSide::Unknown);
    NodeId nodeId = "nodeId";
    char nodeIdSuffix = 0;

    for (OperationType type: _operationTypes) {
        for (int i = 0; i < 3; i++) {
            auto op = std::make_shared<FSOperation>(type, nodeId + nodeIdSuffix);
            nodeIdSuffix++;
            fsOperationSet.insertOp(op);
        }
    }

    FSOperationSet fsOperationSetCopy(fsOperationSet);
    /// ops()
    std::unordered_map<UniqueId, FSOpPtr> ops;
    std::unordered_map<UniqueId, FSOpPtr> opsCopy;
    ops = fsOperationSetCopy.getAllOps();
    opsCopy = fsOperationSetCopy.getAllOps();
    CPPUNIT_ASSERT_EQUAL(ops.size(), opsCopy.size());
    for (const auto &[key, value]: ops) {
        CPPUNIT_ASSERT(opsCopy.contains(key));
    }
    /// getOpsByType() and getOpsByNodeId()
    nodeIdSuffix = '0';
    for (OperationType type: _operationTypes) {
        std::unordered_set<UniqueId> opsByType1 = fsOperationSet.getOpsByType(type);
        std::unordered_set<UniqueId> opsByType2 = fsOperationSetCopy.getOpsByType(type);
        CPPUNIT_ASSERT_EQUAL(opsByType2.size(), opsByType1.size());
        for (const auto &id: opsByType2) {
            CPPUNIT_ASSERT(opsByType1.contains(id));
        }

        for (int i = 0; i < 3; i++) {
            std::unordered_set<UniqueId> opsByNodeId1 = fsOperationSet.getOpsByNodeId(nodeId + nodeIdSuffix);
            std::unordered_set<UniqueId> opsByNodeId2 = fsOperationSetCopy.getOpsByNodeId(nodeId + nodeIdSuffix);
            CPPUNIT_ASSERT_EQUAL(opsByNodeId2.size(), opsByNodeId1.size());
            for (const auto &id: opsByNodeId2) {
                CPPUNIT_ASSERT(opsByNodeId1.contains(id));
            }
            nodeIdSuffix++;
        }
    }
}


} // namespace KDC
