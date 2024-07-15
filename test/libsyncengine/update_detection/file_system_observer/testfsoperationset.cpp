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

#include "testfsoperationset.h"
#include "update_detection/file_system_observer/fsoperationset.h"
#include "thread"


using namespace CppUnit;

namespace KDC {

void TestFsOperationSet::setUp() {
    _operationTypes.push_back(OperationTypeNone);
    _operationTypes.push_back(OperationTypeCreate);
    _operationTypes.push_back(OperationTypeMove);
    _operationTypes.push_back(OperationTypeEdit);
    _operationTypes.push_back(OperationTypeDelete);
    _operationTypes.push_back(OperationTypeRights);
}

void TestFsOperationSet::tearDown() {}

void TestFsOperationSet::testGetOp() {
    FSOperationSet fsOperationSet(ReplicaSideUnknown);
    FSOpPtr opPtr;

    // Test getOp with an empty set
    CPPUNIT_ASSERT(!fsOperationSet.getOp(1, opPtr));

    // Test getOp with an existing operation
    auto op = std::make_shared<FSOperation>();
    fsOperationSet.insertOp(op);

    CPPUNIT_ASSERT(fsOperationSet.getOp(op->id(), opPtr));
    CPPUNIT_ASSERT(opPtr == op);
}

void TestFsOperationSet::testGetOpsByType() {
    FSOperationSet fsOperationSet(ReplicaSideUnknown);
    NodeId nodeId = "nodeId";
    char nodeIdSuffix = '0';

    for (OperationType type : _operationTypes) {
        for (int i = 0; i < 3; i++) {
            std::unordered_set<UniqueId> ops;
            fsOperationSet.getOpsByType(type, ops);
            CPPUNIT_ASSERT_EQUAL(size_t(i), ops.size());

            auto op = std::make_shared<FSOperation>(type, nodeId + nodeIdSuffix);
            nodeIdSuffix++;
            fsOperationSet.insertOp(op);
        }
    }
}

void TestFsOperationSet::testGetOpsByNodeId() {
    // Initialize the set
    FSOperationSet fsOperationSet(ReplicaSideUnknown);
    NodeId nodeId = "nodeId";
    std::vector<UniqueId> opsIds;
    for (OperationType type : _operationTypes) {
        auto op = std::make_shared<FSOperation>(type, nodeId);
        fsOperationSet.insertOp(op);
        opsIds.push_back(op->id());
    }

    // Test with a non existing node
    std::unordered_set<UniqueId> ops;
    ops.insert(1);
    CPPUNIT_ASSERT(!fsOperationSet.getOpsByNodeId("nonExistingNodeId", ops));
    CPPUNIT_ASSERT_EQUAL(size_t(0), ops.size());

    // Test with an existing node
    CPPUNIT_ASSERT(fsOperationSet.getOpsByNodeId(nodeId, ops));
    CPPUNIT_ASSERT_EQUAL(size_t(6), ops.size());
    for (auto id : opsIds) {
        CPPUNIT_ASSERT(ops.contains(id));
    }

    // Test with an empty set
    fsOperationSet.clear();
    CPPUNIT_ASSERT(!fsOperationSet.getOpsByNodeId(nodeId, ops));
}

void TestFsOperationSet::testNbOpsByType() {
    FSOperationSet fsOperationSet(ReplicaSideUnknown);
    NodeId nodeId = "nodeId";
    char nodeIdSuffix = '0';

    for (OperationType type : _operationTypes) {
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
    FSOperationSet fsOperationSet(ReplicaSideUnknown);
    NodeId nodeId = "nodeId";
    char nodeIdSuffix = '0';

    for (OperationType type : _operationTypes) {
        for (int i = 1; i <= 2; i++) {
            auto op = std::make_shared<FSOperation>(type, nodeId + nodeIdSuffix);
            nodeIdSuffix++;
            fsOperationSet.insertOp(op);
        }
    }

    // Test clear
    fsOperationSet.clear();
    CPPUNIT_ASSERT_EQUAL(size_t(0), fsOperationSet._ops.size());
    CPPUNIT_ASSERT_EQUAL(size_t(0), fsOperationSet._opsByType.size());
    CPPUNIT_ASSERT_EQUAL(size_t(0), fsOperationSet._opsByNodeId.size());
}

void TestFsOperationSet::testInsertOp() {
    FSOperationSet fsOperationSet(ReplicaSideUnknown);
    NodeId nodeId = "nodeId";
    char nodeIdSuffix = 0;

    for (OperationType type : _operationTypes) {
        for (int i = 0; i < 3; i++) {
            auto op = std::make_shared<FSOperation>(type, nodeId + nodeIdSuffix);
            fsOperationSet.insertOp(op);
            nodeIdSuffix++;
            CPPUNIT_ASSERT(fsOperationSet._ops.contains(op->id()));
            CPPUNIT_ASSERT(fsOperationSet._opsByType[type].contains(op->id()));
            CPPUNIT_ASSERT(fsOperationSet._opsByNodeId[op->nodeId()].contains(op->id()));
        }
    }
}

void TestFsOperationSet::testRemoveOp() {
    FSOperationSet fsOperationSet(ReplicaSideUnknown);
    NodeId nodeId = "nodeId";
    auto op1 = std::make_shared<FSOperation>(OperationTypeCreate, nodeId);
    fsOperationSet.insertOp(op1);
    auto op2 = std::make_shared<FSOperation>(OperationTypeMove, nodeId);
    fsOperationSet.insertOp(op2);

    // Test removeOp with an existing operation (uniqueId)
    CPPUNIT_ASSERT(fsOperationSet.removeOp(op1->id()));
    CPPUNIT_ASSERT(!fsOperationSet._ops.contains(op1->id()));
    CPPUNIT_ASSERT(!fsOperationSet._opsByType[OperationTypeCreate].contains(op1->id()));
    CPPUNIT_ASSERT(!fsOperationSet._opsByNodeId[nodeId].contains(op1->id()));

    // Test removeOp with an existing operation (Node id + type)
    CPPUNIT_ASSERT(fsOperationSet.removeOp(nodeId, OperationTypeMove));
    CPPUNIT_ASSERT(!fsOperationSet._ops.contains(op2->id()));
    CPPUNIT_ASSERT(!fsOperationSet._opsByType[OperationTypeMove].contains(op2->id()));
    CPPUNIT_ASSERT(!fsOperationSet._opsByNodeId[nodeId].contains(op2->id()));

    // Test removeOp with a not existing operation
    CPPUNIT_ASSERT(!fsOperationSet.removeOp(1));
}

void TestFsOperationSet::testfindOp() {
    FSOperationSet fsOperationSet(ReplicaSideUnknown);
    NodeId nodeId = "nodeId";

    for (auto type : _operationTypes) {
        auto op = std::make_shared<FSOperation>(type, nodeId);
        fsOperationSet.insertOp(op);
    }

    for (auto type : _operationTypes) {
        FSOpPtr res;
        CPPUNIT_ASSERT(fsOperationSet.findOp(nodeId, type, res));
        CPPUNIT_ASSERT(res->operationType() == type);
    }

    // Test findOp with a non existing operation.
    fsOperationSet.clear();
    FSOpPtr res;
    fsOperationSet.insertOp(std::make_shared<FSOperation>(OperationTypeCreate, nodeId));
    CPPUNIT_ASSERT(!fsOperationSet.findOp("nonExistingNodeId", OperationTypeCreate, res));

    fsOperationSet.insertOp(std::make_shared<FSOperation>(OperationTypeCreate, nodeId));
    CPPUNIT_ASSERT(!fsOperationSet.findOp(nodeId, OperationTypeMove, res));
}

void TestFsOperationSet::testOperatorEqual() {
    FSOperationSet fsOperationSet1(ReplicaSideUnknown);
    FSOperationSet fsOperationSet2(ReplicaSideUnknown);

    // Test operator the assignment operator '=' with an empty set
    fsOperationSet1 = fsOperationSet2;
    CPPUNIT_ASSERT_EQUAL(size_t(0), fsOperationSet1._ops.size());
    CPPUNIT_ASSERT_EQUAL(size_t(0), fsOperationSet1._opsByType.size());
    CPPUNIT_ASSERT_EQUAL(size_t(0), fsOperationSet1._opsByNodeId.size());

    // Test operator = with a not empty set
    NodeId nodeId = "nodeId";
    char nodeIdSuffix = '0';
    for (OperationType type : _operationTypes) {
        for (int i = 0; i < 3; i++) {
            auto op = std::make_shared<FSOperation>(type, nodeId + nodeIdSuffix);
            nodeIdSuffix++;
            fsOperationSet2.insertOp(op);
        }
    }

    fsOperationSet1 = fsOperationSet2;
    CPPUNIT_ASSERT_EQUAL(fsOperationSet2._ops.size(), fsOperationSet1._ops.size());
    CPPUNIT_ASSERT_EQUAL(fsOperationSet2._opsByType.size(), fsOperationSet1._opsByType.size());
    CPPUNIT_ASSERT_EQUAL(fsOperationSet2._opsByNodeId.size(), fsOperationSet1._opsByNodeId.size());
    for (auto &op : fsOperationSet2._ops) {
        CPPUNIT_ASSERT(fsOperationSet1._ops.contains(op.first));
    }
    for (auto &op : fsOperationSet2._opsByType) {
        CPPUNIT_ASSERT(fsOperationSet1._opsByType.contains(op.first));
    }
    for (auto &op : fsOperationSet2._opsByNodeId) {
        CPPUNIT_ASSERT(fsOperationSet1._opsByNodeId.contains(op.first));
    }

    // Test that the two sets are independent
    CPPUNIT_ASSERT(&fsOperationSet1 != &fsOperationSet2);
}

void TestFsOperationSet::testMultithreadSupport() {
    // getOp
    {
        FSOperationSet fsOperationSet(ReplicaSideUnknown);
        bool functionEnded = false;
        fsOperationSet._mutex.lock();

        std::thread t1([&fsOperationSet, &functionEnded]() {
            FSOpPtr opPtr;
            functionEnded = false;
            fsOperationSet.getOp(1, opPtr);
            functionEnded = true;
        });
        Utility::msleep(50);  // Normal delay is < 1 microsecond
        bool functionEndedBeforeUnlock = functionEnded;
        fsOperationSet._mutex.unlock();
        Utility::msleep(50);
        bool functionEndedAfterUnlock = functionEnded;
        t1.join();

        CPPUNIT_ASSERT_MESSAGE("getOp did not lock the mutex", !functionEndedBeforeUnlock);
        CPPUNIT_ASSERT_MESSAGE("getOp seems to be deadlock", functionEndedAfterUnlock);
    }

    // getOpsByType
    {
        FSOperationSet fsOperationSet(ReplicaSideUnknown);
        bool functionEnded = false;
        fsOperationSet._mutex.lock();

        std::thread t1([&fsOperationSet, &functionEnded]() {
            FSOpPtr opPtr;
            functionEnded = false;
            std::unordered_set<UniqueId> ops;
            fsOperationSet.getOpsByType(OperationTypeCreate, ops);
            functionEnded = true;
        });

        Utility::msleep(50);  // Normal delay is < 1 microsecond
        bool functionEndedBeforeUnlock = functionEnded;
        fsOperationSet._mutex.unlock();
        Utility::msleep(50);
        bool functionEndedAfterUnlock = functionEnded;
        t1.join();

        CPPUNIT_ASSERT_MESSAGE("getOpsByType did not lock the mutex", !functionEndedBeforeUnlock);
        CPPUNIT_ASSERT_MESSAGE("getOpsByType seems to be deadlock", functionEndedAfterUnlock);
    }

    // getOpsByNodeId
    {
        FSOperationSet fsOperationSet(ReplicaSideUnknown);
        bool functionEnded = false;
        fsOperationSet._mutex.lock();

        std::thread t1([&fsOperationSet, &functionEnded]() {
            FSOpPtr opPtr;
            functionEnded = false;
            std::unordered_set<UniqueId> ops;
            fsOperationSet.getOpsByNodeId("nodeId", ops);
            functionEnded = true;
        });

        Utility::msleep(50);  // Normal delay is < 1 microsecond
        bool functionEndedBeforeUnlock = functionEnded;
        fsOperationSet._mutex.unlock();
        Utility::msleep(50);
        bool functionEndedAfterUnlock = functionEnded;
        t1.join();

        CPPUNIT_ASSERT_MESSAGE("getOpsByNodeId did not lock the mutex", !functionEndedBeforeUnlock);
        CPPUNIT_ASSERT_MESSAGE("getOpsByNodeId seems to be deadlock", functionEndedAfterUnlock);
    }

    // nbOpsByType
    {
        FSOperationSet fsOperationSet(ReplicaSideUnknown);
        bool functionEnded = false;
        fsOperationSet._mutex.lock();

        std::thread t1([&fsOperationSet, &functionEnded]() {
            FSOpPtr opPtr;
            functionEnded = false;
            std::unordered_set<UniqueId> ops;
            fsOperationSet.nbOpsByType(OperationTypeCreate);
            functionEnded = true;
        });

        Utility::msleep(50);  // Normal delay is < 1 microsecond
        bool functionEndedBeforeUnlock = functionEnded;
        fsOperationSet._mutex.unlock();
        Utility::msleep(50);
        bool functionEndedAfterUnlock = functionEnded;
        t1.join();

        CPPUNIT_ASSERT_MESSAGE("nbOpsByType did not lock the mutex", !functionEndedBeforeUnlock);
        CPPUNIT_ASSERT_MESSAGE("nbOpsByType seems to be deadlock", functionEndedAfterUnlock);
    }

    // clear
    {
        FSOperationSet fsOperationSet(ReplicaSideUnknown);
        bool functionEnded = false;
        fsOperationSet._mutex.lock();

        std::thread t1([&fsOperationSet, &functionEnded]() {
            FSOpPtr opPtr;
            functionEnded = false;
            fsOperationSet.clear();
            functionEnded = true;
        });

        Utility::msleep(50);  // Normal delay is < 1 microsecond
        bool functionEndedBeforeUnlock = functionEnded;
        fsOperationSet._mutex.unlock();
        Utility::msleep(50);
        bool functionEndedAfterUnlock = functionEnded;
        t1.join();

        CPPUNIT_ASSERT_MESSAGE("clear did not lock the mutex", !functionEndedBeforeUnlock);
        CPPUNIT_ASSERT_MESSAGE("clear seems to be deadlock", functionEndedAfterUnlock);
    }

    // insertOp
    {
        FSOperationSet fsOperationSet(ReplicaSideUnknown);
        bool functionEnded = false;
        fsOperationSet._mutex.lock();

        std::thread t1([&fsOperationSet, &functionEnded]() {
            FSOpPtr opPtr;
            functionEnded = false;
            fsOperationSet.insertOp(std::make_shared<FSOperation>());
            functionEnded = true;
        });

        Utility::msleep(50);  // Normal delay is < 1 microsecond
        bool functionEndedBeforeUnlock = functionEnded;
        fsOperationSet._mutex.unlock();
        Utility::msleep(50);
        bool functionEndedAfterUnlock = functionEnded;
        t1.join();

        CPPUNIT_ASSERT_MESSAGE("insertOp did not lock the mutex", !functionEndedBeforeUnlock);
        CPPUNIT_ASSERT_MESSAGE("insertOp seems to be deadlock", functionEndedAfterUnlock);
    }

    // removeOp (uniqueId)
    {
        FSOperationSet fsOperationSet(ReplicaSideUnknown);
        bool functionEnded = false;
        fsOperationSet._mutex.lock();

        std::thread t1([&fsOperationSet, &functionEnded]() {
            FSOpPtr opPtr;
            functionEnded = false;
            fsOperationSet.removeOp(1);
            functionEnded = true;
        });

        Utility::msleep(50);  // Normal delay is < 1 microsecond
        bool functionEndedBeforeUnlock = functionEnded;
        fsOperationSet._mutex.unlock();
        Utility::msleep(50);
        bool functionEndedAfterUnlock = functionEnded;
        t1.join();

        CPPUNIT_ASSERT_MESSAGE("removeOp (uniqueId) did not lock the mutex", !functionEndedBeforeUnlock);
        CPPUNIT_ASSERT_MESSAGE("removeOp (uniqueId) seems to be deadlock", functionEndedAfterUnlock);
    }

    // removeOp (nodeId + type)
    {
        FSOperationSet fsOperationSet(ReplicaSideUnknown);
        bool functionEnded = false;
        fsOperationSet._mutex.lock();

        std::thread t1([&fsOperationSet, &functionEnded]() {
            FSOpPtr opPtr;
            functionEnded = false;
            fsOperationSet.removeOp("nodeId", OperationTypeCreate);
            functionEnded = true;
        });

        Utility::msleep(50);  // Normal delay is < 1 microsecond
        bool functionEndedBeforeUnlock = functionEnded;
        fsOperationSet._mutex.unlock();
        Utility::msleep(50);
        bool functionEndedAfterUnlock = functionEnded;
        t1.join();

        CPPUNIT_ASSERT_MESSAGE("removeOp (nodeId + type) did not lock the mutex", !functionEndedBeforeUnlock);
        CPPUNIT_ASSERT_MESSAGE("removeOp (nodeId + type) seems to be deadlock", functionEndedAfterUnlock);
    }

    // findOp
    {
        FSOperationSet fsOperationSet(ReplicaSideUnknown);
        bool functionEnded = false;
        fsOperationSet._mutex.lock();

        std::thread t1([&fsOperationSet, &functionEnded]() {
            FSOpPtr opPtr;
            functionEnded = false;
            FSOpPtr res;
            fsOperationSet.findOp("nodeId", OperationTypeCreate, res);
            functionEnded = true;
        });

        Utility::msleep(50);  // Normal delay is < 1 microsecond
        bool functionEndedBeforeUnlock = functionEnded;
        fsOperationSet._mutex.unlock();
        Utility::msleep(50);
        bool functionEndedAfterUnlock = functionEnded;
        t1.join();

        CPPUNIT_ASSERT_MESSAGE("findOp did not lock the mutex", !functionEndedBeforeUnlock);
        CPPUNIT_ASSERT_MESSAGE("findOp seems to be deadlock", functionEndedAfterUnlock);
    }

    // operator =
    {
        FSOperationSet fsOperationSet1(ReplicaSideUnknown);
        FSOperationSet fsOperationSet2(ReplicaSideUnknown);
        bool functionEnded = false;
        fsOperationSet1._mutex.lock();
        fsOperationSet2._mutex.lock();

        std::thread t1([&fsOperationSet1, &fsOperationSet2, &functionEnded]() {
            functionEnded = false;
            fsOperationSet1 = fsOperationSet2;
            functionEnded = true;
        });

        Utility::msleep(50);  // Normal delay is < 1 microsecond
        bool functionEndedBeforeUnlock = functionEnded;
        fsOperationSet1._mutex.unlock();
        Utility::msleep(50);  // Normal delay is < 1 microsecond
        functionEndedBeforeUnlock = functionEndedBeforeUnlock || functionEnded;
        fsOperationSet2._mutex.unlock();
        Utility::msleep(50);
        bool functionEndedAfterUnlock = functionEnded;
        t1.join();

        CPPUNIT_ASSERT_MESSAGE("operator = did not lock the mutex", !functionEndedBeforeUnlock);
        CPPUNIT_ASSERT_MESSAGE("operator = seems to be deadlock", functionEndedAfterUnlock);
    }
}

void TestFsOperationSet::testCopyConstructor() {
    FSOperationSet fsOperationSet(ReplicaSideUnknown);
    NodeId nodeId = "nodeId";
    char nodeIdSuffix = 0;

    for (OperationType type : _operationTypes) {
        for (int i = 0; i < 3; i++) {
            auto op = std::make_shared<FSOperation>(type, nodeId + nodeIdSuffix);
            nodeIdSuffix++;
            fsOperationSet.insertOp(op);
        }
    }

    FSOperationSet fsOperationSetCopy(fsOperationSet);
    CPPUNIT_ASSERT_EQUAL(fsOperationSet._ops.size(), fsOperationSetCopy._ops.size());
    CPPUNIT_ASSERT_EQUAL(fsOperationSet._opsByType.size(), fsOperationSetCopy._opsByType.size());
    CPPUNIT_ASSERT_EQUAL(fsOperationSet._opsByNodeId.size(), fsOperationSetCopy._opsByNodeId.size());
    for (auto &op : fsOperationSet._ops) {
        CPPUNIT_ASSERT(fsOperationSetCopy._ops.contains(op.first));
    }
    for (auto &op : fsOperationSet._opsByType) {
        CPPUNIT_ASSERT(fsOperationSetCopy._opsByType.contains(op.first));
    }
    for (auto &op : fsOperationSet._opsByNodeId) {
        CPPUNIT_ASSERT(fsOperationSetCopy._opsByNodeId.contains(op.first));
    }
}


}  // namespace KDC
