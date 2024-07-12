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

#include <memory>
#include "update_detection/file_system_observer/snapshot/snapshot.h"
#include "libcommon/keychainmanager/keychainmanager.h"
#include "libcommon/utility/utility.h"
#include "db/syncdb.h"
#include "db/parmsdb.h"
#include "requests/parameterscache.h"

using namespace CppUnit;

namespace KDC {

void TestSnapshot::setUp() {
    ParametersCache::instance(true);
}

void TestSnapshot::tearDown() {}

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
    const DbNode dummyRootNode(0, std::nullopt, SyncName(), SyncName(), "1", "1", std::nullopt, std::nullopt, std::nullopt,
                               NodeTypeDirectory, 0, std::nullopt);
    Snapshot snapshot(ReplicaSideLocal, dummyRootNode);

    // Insert node A
    const SnapshotItem itemA("a", SyncDb::driveRootNode().nodeIdLocal().value(), Str("A"), 1640995201, 1640995201,
                             NodeType::NodeTypeDirectory, 123);
    snapshot.updateItem(itemA);
    CPPUNIT_ASSERT(snapshot.exists("a"));
    CPPUNIT_ASSERT_EQUAL(std::string("A"), SyncName2Str(snapshot.name("a")));
    CPPUNIT_ASSERT_EQUAL(NodeType::NodeTypeDirectory, snapshot.type("a"));
    auto itItem = snapshot._items.find(SyncDb::driveRootNode().nodeIdLocal().value());
    CPPUNIT_ASSERT(itItem->second.childrenIds().contains("a"));

    // Update node A
    snapshot.updateItem(SnapshotItem("a", SyncDb::driveRootNode().nodeIdLocal().value(), Str("A*"), 1640995202, 1640995202,
                                     NodeType::NodeTypeDirectory, 123));
    CPPUNIT_ASSERT_EQUAL(std::string("A*"), SyncName2Str(snapshot.name("a")));

    // Insert node B
    const SnapshotItem itemB("b", SyncDb::driveRootNode().nodeIdLocal().value(), Str("B"), 1640995203, 1640995203,
                             NodeType::NodeTypeDirectory, 123);
    snapshot.updateItem(itemB);
    CPPUNIT_ASSERT(snapshot.exists("b"));
    itItem = snapshot._items.find(SyncDb::driveRootNode().nodeIdLocal().value());
    CPPUNIT_ASSERT(itItem->second.childrenIds().contains("b"));

    // Insert child nodes
    const SnapshotItem itemAA("aa", "a", Str("AA"), 1640995204, 1640995204, NodeType::NodeTypeDirectory, 123);
    snapshot.updateItem(itemAA);
    itItem = snapshot._items.find("a");
    CPPUNIT_ASSERT(itItem->second.childrenIds().contains("aa"));

    const SnapshotItem itemAAA("aaa", "aa", Str("AAA"), 1640995205, 1640995205, NodeType::NodeTypeFile, 123);
    snapshot.updateItem(itemAAA);
    CPPUNIT_ASSERT(snapshot.exists("aaa"));
    itItem = snapshot._items.find("aa");
    CPPUNIT_ASSERT(itItem->second.childrenIds().contains("aaa"));

    SyncPath path;
    snapshot.path("aaa", path);
    CPPUNIT_ASSERT(path == std::filesystem::path("A*/AA/AAA"));
    CPPUNIT_ASSERT_EQUAL(std::string("AAA"), SyncName2Str(snapshot.name("aaa")));
    CPPUNIT_ASSERT_EQUAL(SyncTime(1640995205), snapshot.lastModified("aaa"));
    CPPUNIT_ASSERT_EQUAL(NodeType::NodeTypeFile, snapshot.type("aaa"));
    CPPUNIT_ASSERT(snapshot.contentChecksum("aaa").empty());  // Checksum never computed for now
    CPPUNIT_ASSERT_EQUAL(NodeId("aaa"), snapshot.itemId(std::filesystem::path("A*/AA/AAA")));

    // Move node AA under B
    snapshot.updateItem(SnapshotItem("aa", "b", Str("AA"), 1640995204, 1640995204, NodeType::NodeTypeDirectory, 123));
    CPPUNIT_ASSERT(snapshot.parentId("aa") == "b");
    itItem = snapshot._items.find("b");
    CPPUNIT_ASSERT(itItem->second.childrenIds().contains("aa"));
    itItem = snapshot._items.find("a");
    CPPUNIT_ASSERT(itItem->second.childrenIds().empty());

    // Remove node B
    snapshot.removeItem("b");
    CPPUNIT_ASSERT(!snapshot.exists("aaa"));
    CPPUNIT_ASSERT(!snapshot.exists("aa"));
    CPPUNIT_ASSERT(!snapshot.exists("b"));
    itItem = snapshot._items.find(SyncDb::driveRootNode().nodeIdLocal().value());
    CPPUNIT_ASSERT(!itItem->second.childrenIds().contains("b"));

    // Reset snapshot
    snapshot.init();
    CPPUNIT_ASSERT_EQUAL(size_t(1), snapshot._items.size());
}

}  // namespace KDC
