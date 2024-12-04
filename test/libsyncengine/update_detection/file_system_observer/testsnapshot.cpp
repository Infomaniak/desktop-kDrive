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

#include "testsnapshot.h"
#include "test_utility/testhelpers.h"

#include "db/syncdb.h"
#include "requests/parameterscache.h"

using namespace CppUnit;

namespace KDC {

void TestSnapshot::setUp() {
    ParametersCache::instance(true);
}

void TestSnapshot::tearDown() {}

void TestSnapshot::testItemId() {
    const NodeId rootNodeId = SyncDb::driveRootNode().nodeIdLocal().value();

    const DbNode dummyRootNode(0, std::nullopt, SyncName(), SyncName(), "1", "1", std::nullopt, std::nullopt, std::nullopt,
                               NodeType::Directory, 0, std::nullopt);
    Snapshot snapshot(ReplicaSide::Local, dummyRootNode);

    snapshot.updateItem(
            SnapshotItem("2", rootNodeId, Str("a"), 1640995202, 1640995202, NodeType::Directory, 0, false, true, true));
    snapshot.updateItem(SnapshotItem("3", "2", Str("aa"), 1640995202, 1640995202, NodeType::Directory, 0, false, true, true));
    snapshot.updateItem(SnapshotItem("4", "2", Str("ab"), 1640995202, 1640995202, NodeType::Directory, 0, false, true, true));
    snapshot.updateItem(SnapshotItem("5", "2", Str("ac"), 1640995202, 1640995202, NodeType::Directory, 0, false, true, true));
    snapshot.updateItem(SnapshotItem("6", "4", Str("aba"), 1640995202, 1640995202, NodeType::Directory, 0, false, true, true));
    snapshot.updateItem(SnapshotItem("7", "6", Str("abaa"), 1640995202, 1640995202, NodeType::File, 10, false, true, true));
    CPPUNIT_ASSERT_EQUAL(NodeId("7"), snapshot.itemId(SyncPath("a/ab/aba/abaa")));

    SyncName nfcNormalized;
    Utility::normalizedSyncName(Str("abà"), nfcNormalized);

    SyncName nfdNormalized;
    Utility::normalizedSyncName(Str("abà"), nfdNormalized, Utility::UnicodeNormalization::NFD);

    snapshot.updateItem(SnapshotItem("6", "4", nfcNormalized, 1640995202, 1640995202, NodeType::Directory, 0, false, true, true));
    CPPUNIT_ASSERT_EQUAL(NodeId("7"), snapshot.itemId(SyncPath("a/ab") / nfcNormalized / Str("abaa")));
    CPPUNIT_ASSERT_EQUAL(NodeId(""), snapshot.itemId(SyncPath("a/ab") / nfdNormalized / Str("abaa")));
    CPPUNIT_ASSERT_EQUAL(NodeId("7"), snapshot.itemId(SyncPath("a/ab") / nfcNormalized / Str("abaa"), true));

    snapshot.updateItem(SnapshotItem("6", "4", nfdNormalized, 1640995202, 1640995202, NodeType::Directory, 0, false, true, true));
    CPPUNIT_ASSERT_EQUAL(NodeId("7"), snapshot.itemId(SyncPath("a/ab") / nfdNormalized / Str("abaa")));
    CPPUNIT_ASSERT_EQUAL(NodeId(""), snapshot.itemId(SyncPath("a/ab") / nfcNormalized / Str("abaa")));
    CPPUNIT_ASSERT_EQUAL(NodeId("7"), snapshot.itemId(SyncPath("a/ab") / nfcNormalized / Str("abaa"), true));
}

/**
 * Tree:
 *
 *            root
 *        _____|_____
 *       |          |
 *       A          B
 *       |
 *      AA
 *       |
 *      AAA
 */

void TestSnapshot::testSnapshot() {
    const NodeId rootNodeId = SyncDb::driveRootNode().nodeIdLocal().value();

    const DbNode dummyRootNode(0, std::nullopt, SyncName(), SyncName(), "1", "1", std::nullopt, std::nullopt, std::nullopt,
                               NodeType::Directory, 0, std::nullopt);
    Snapshot snapshot(ReplicaSide::Local, dummyRootNode);

    // Insert node A
    const SnapshotItem itemA("a", rootNodeId, Str("A"), 1640995201, -1640995201, NodeType::Directory, 123, false, true, true);
    snapshot.updateItem(itemA);
    CPPUNIT_ASSERT(snapshot.exists("a"));
    CPPUNIT_ASSERT_EQUAL(std::string("A"), SyncName2Str(snapshot.name("a")));
    CPPUNIT_ASSERT_EQUAL(NodeType::Directory, snapshot.type("a"));
    std::unordered_set<NodeId> childrenIds;
    snapshot.getChildrenIds(rootNodeId, childrenIds);
    CPPUNIT_ASSERT(childrenIds.contains("a"));

    // Update node A
    snapshot.updateItem(
            SnapshotItem("a", rootNodeId, Str("A*"), 1640995202, 1640995202, NodeType::Directory, 123, false, true, true));
    CPPUNIT_ASSERT_EQUAL(std::string("A*"), SyncName2Str(snapshot.name("a")));

    // Insert node B
    const SnapshotItem itemB("b", rootNodeId, Str("B"), 1640995203, 1640995203, NodeType::Directory, 123, false, true, true);
    snapshot.updateItem(itemB);
    CPPUNIT_ASSERT(snapshot.exists("b"));
    snapshot.getChildrenIds(rootNodeId, childrenIds);
    CPPUNIT_ASSERT(childrenIds.contains("b"));

    // Insert child nodes
    const SnapshotItem itemAA("aa", "a", Str("AA"), 1640995204, 1640995204, NodeType::Directory, 123, false, true, true);
    snapshot.updateItem(itemAA);
    CPPUNIT_ASSERT(snapshot.exists("aa"));
    snapshot.getChildrenIds("a", childrenIds);
    CPPUNIT_ASSERT(childrenIds.contains("aa"));

    const SnapshotItem itemAAA("aaa", "aa", Str("AAA"), 1640995205, 1640995205, NodeType::File, 123, false, true, true);
    snapshot.updateItem(itemAAA);
    CPPUNIT_ASSERT(snapshot.exists("aaa"));
    snapshot.getChildrenIds("aa", childrenIds);
    CPPUNIT_ASSERT(childrenIds.contains("aaa"));

    SyncPath path;
    snapshot.path("aaa", path);
    CPPUNIT_ASSERT_EQUAL(SyncPath("A*/AA/AAA"), path);
    CPPUNIT_ASSERT_EQUAL(std::string("AAA"), SyncName2Str(snapshot.name("aaa")));
    CPPUNIT_ASSERT_EQUAL(static_cast<SyncTime>(1640995205), snapshot.lastModified("aaa"));
    CPPUNIT_ASSERT_EQUAL(NodeType::File, snapshot.type("aaa"));
    CPPUNIT_ASSERT(snapshot.contentChecksum("aaa").empty()); // Checksum never computed for now
    CPPUNIT_ASSERT_EQUAL(NodeId("aaa"), snapshot.itemId(SyncPath("A*/AA/AAA")));

    // Move node AA under B
    snapshot.updateItem(SnapshotItem("aa", "b", Str("AA"), 1640995204, -1640995204, NodeType::Directory, 123, false, true, true));
    CPPUNIT_ASSERT(snapshot.parentId("aa") == "b");
    snapshot.getChildrenIds("b", childrenIds);
    CPPUNIT_ASSERT(childrenIds.contains("aa"));
    snapshot.getChildrenIds("a", childrenIds);
    CPPUNIT_ASSERT(childrenIds.empty());

    // Remove node B
    snapshot.removeItem("b");
    snapshot.getChildrenIds(rootNodeId, childrenIds);
    CPPUNIT_ASSERT(!snapshot.exists("aaa"));
    CPPUNIT_ASSERT(!snapshot.exists("aa"));
    CPPUNIT_ASSERT(!snapshot.exists("b"));
    CPPUNIT_ASSERT(!childrenIds.contains("b"));

    // Reset snapshot
    snapshot.init();
    CPPUNIT_ASSERT_EQUAL(static_cast<uint64_t>(1), snapshot.nbItems());
}

void TestSnapshot::testDuplicatedItem() {
    const NodeId rootNodeId = *SyncDb::driveRootNode().nodeIdLocal();

    const DbNode dummyRootNode(0, std::nullopt, Str("Local Drive"), SyncName(), "1", "1", std::nullopt, std::nullopt,
                               std::nullopt, NodeType::Directory, 0, std::nullopt);
    Snapshot snapshot(ReplicaSide::Local, dummyRootNode);

    const SnapshotItem file1("A", rootNodeId, Str("file1"), 1640995201, -1640995201, NodeType::File, 123,
                               false, true, true);
    const SnapshotItem file2("B", rootNodeId, Str("file1"), 1640995201, -1640995201, NodeType::File, 123,
                               false, true, true);

    snapshot.updateItem(file1);
    snapshot.updateItem(file2);

    CPPUNIT_ASSERT(!snapshot.exists("A"));
    CPPUNIT_ASSERT(snapshot.exists("B"));
}

void TestSnapshot::testSnapshotInsertionWithDifferentEncodings() {
    const NodeId rootNodeId = *SyncDb::driveRootNode().nodeIdLocal();

    const DbNode dummyRootNode(0, std::nullopt, Str("Local Drive"), SyncName(), "1", "1", std::nullopt, std::nullopt,
                               std::nullopt, NodeType::Directory, 0, std::nullopt);
    Snapshot snapshot(ReplicaSide::Local, dummyRootNode);

    const SnapshotItem nfcItem("A", rootNodeId, testhelpers::makeNfcSyncName(), 1640995201, -1640995201, NodeType::Directory, 123,
                               false, true, true);
    const SnapshotItem nfdItem("B", rootNodeId, testhelpers::makeNfdSyncName(), 1640995201, -1640995201, NodeType::Directory, 123,
                               false, true, true);
    {
        snapshot.updateItem(nfcItem);
        SyncPath syncPath;
        snapshot.path("A", syncPath);
        CPPUNIT_ASSERT_EQUAL(SyncPath(testhelpers::makeNfcSyncName()), syncPath);
    }
    {
        snapshot.updateItem(nfdItem);
        SyncPath syncPath;
        snapshot.path("B", syncPath);
        CPPUNIT_ASSERT_EQUAL(SyncPath(testhelpers::makeNfdSyncName()), syncPath);
    }
}

} // namespace KDC
