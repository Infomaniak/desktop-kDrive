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

#include "testsnapshot.h"
#include "test_utility/testhelpers.h"
#include "db/syncdb.h"
#include "requests/parameterscache.h"

using namespace CppUnit;

namespace KDC {


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

void TestSnapshot::setUp() {
    TestBase::start();
    ParametersCache::instance(true);

    _rootNodeId = SyncDb::driveRootNode().nodeIdLocal().value();
    _dummyRootNode = DbNode(0, std::nullopt, SyncName(), SyncName(), "1", "1", std::nullopt, std::nullopt, std::nullopt,
                            NodeType::Directory, 0, std::nullopt);
    _snapshot = std::make_unique<Snapshot>(ReplicaSide::Local, _dummyRootNode);

    // Insert node A
    const SnapshotItem itemA("a", _rootNodeId, Str("A"), testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory,
                             testhelpers::defaultFileSize, false, true, true);
    _snapshot->updateItem(itemA);

    // Insert node B
    const SnapshotItem itemB("b", _rootNodeId, Str("B"), testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory,
                             testhelpers::defaultFileSize, false, true, true);
    _snapshot->updateItem(itemB);

    // Insert child nodes
    const SnapshotItem itemAA("aa", "a", Str("AA"), testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory,
                              testhelpers::defaultFileSize, false, true, true);
    _snapshot->updateItem(itemAA);

    const SnapshotItem itemAAA("aaa", "aa", Str("AAA"), testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File,
                               testhelpers::defaultFileSize, false, true, true);
    _snapshot->updateItem(itemAAA);
}

void TestSnapshot::tearDown() {
    ParametersCache::reset();
    TestBase::stop();
}

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
    Utility::normalizedSyncName(Str("abà"), nfdNormalized, UnicodeNormalization::NFD);

    snapshot.updateItem(SnapshotItem("6", "4", nfcNormalized, 1640995202, 1640995202, NodeType::Directory, 0, false, true, true));
    CPPUNIT_ASSERT_EQUAL(NodeId("7"), snapshot.itemId(SyncPath("a/ab") / nfcNormalized / Str("abaa")));
    CPPUNIT_ASSERT_EQUAL(NodeId(""), snapshot.itemId(SyncPath("a/ab") / nfdNormalized / Str("abaa")));

    snapshot.updateItem(SnapshotItem("6", "4", nfdNormalized, 1640995202, 1640995202, NodeType::Directory, 0, false, true, true));
    CPPUNIT_ASSERT_EQUAL(NodeId("7"), snapshot.itemId(SyncPath("a/ab") / nfdNormalized / Str("abaa")));
    CPPUNIT_ASSERT_EQUAL(NodeId(""), snapshot.itemId(SyncPath("a/ab") / nfcNormalized / Str("abaa")));
}

void TestSnapshot::testSnapshot() {
    CPPUNIT_ASSERT(_snapshot->exists("a"));
    CPPUNIT_ASSERT_EQUAL(std::string("A"), SyncName2Str(_snapshot->name("a")));
    CPPUNIT_ASSERT_EQUAL(NodeType::Directory, _snapshot->type("a"));
    NodeSet childrenIds;
    _snapshot->getChildrenIds(_rootNodeId, childrenIds);
    CPPUNIT_ASSERT(childrenIds.contains("a"));

    CPPUNIT_ASSERT(_snapshot->exists("b"));
    _snapshot->getChildrenIds(_rootNodeId, childrenIds);
    CPPUNIT_ASSERT(childrenIds.contains("b"));

    CPPUNIT_ASSERT(_snapshot->exists("aa"));
    _snapshot->getChildrenIds("a", childrenIds);
    CPPUNIT_ASSERT(childrenIds.contains("aa"));

    CPPUNIT_ASSERT(_snapshot->exists("aaa"));
    _snapshot->getChildrenIds("aa", childrenIds);
    CPPUNIT_ASSERT(childrenIds.contains("aaa"));

    // Update node A
    _snapshot->updateItem(SnapshotItem("a", _rootNodeId, Str("A*"), testhelpers::defaultTime, testhelpers::defaultTime + 1,
                                       NodeType::Directory, testhelpers::defaultFileSize, false, true, true));
    CPPUNIT_ASSERT_EQUAL(std::string("A*"), SyncName2Str(_snapshot->name("a")));
    SyncPath path;
    bool ignore = false;
    _snapshot->path("aaa", path, ignore);
    CPPUNIT_ASSERT_EQUAL(SyncPath("A*/AA/AAA"), path);
    CPPUNIT_ASSERT_EQUAL(std::string("AAA"), SyncName2Str(_snapshot->name("aaa")));
    CPPUNIT_ASSERT_EQUAL(static_cast<SyncTime>(testhelpers::defaultTime), _snapshot->lastModified("aaa"));
    CPPUNIT_ASSERT_EQUAL(NodeType::File, _snapshot->type("aaa"));
    CPPUNIT_ASSERT(_snapshot->contentChecksum("aaa").empty()); // Checksum never computed for now
    CPPUNIT_ASSERT_EQUAL(NodeId("aaa"), _snapshot->itemId(std::filesystem::path("A*/AA/AAA")));

    // Move node AA under B
    _snapshot->updateItem(SnapshotItem("aa", "b", Str("AA"), testhelpers::defaultTime, testhelpers::defaultTime,
                                       NodeType::Directory, testhelpers::defaultFileSize, false, true, true));
    CPPUNIT_ASSERT(_snapshot->parentId("aa") == "b");
    _snapshot->getChildrenIds("b", childrenIds);
    CPPUNIT_ASSERT(childrenIds.contains("aa"));
    _snapshot->getChildrenIds("a", childrenIds);
    CPPUNIT_ASSERT(childrenIds.empty());

    // Remove node B
    _snapshot->removeItem("b");
    _snapshot->getChildrenIds(_rootNodeId, childrenIds);
    CPPUNIT_ASSERT(!_snapshot->exists("aaa"));
    CPPUNIT_ASSERT(!_snapshot->exists("aa"));
    CPPUNIT_ASSERT(!_snapshot->exists("b"));
    CPPUNIT_ASSERT(!childrenIds.contains("b"));

    // Reset snapshot
    _snapshot->init();
    CPPUNIT_ASSERT_EQUAL(static_cast<uint64_t>(1), _snapshot->nbItems());
}

void TestSnapshot::testSize() {
    const NodeId rootNodeId = *SyncDb::driveRootNode().nodeIdLocal();

    const DbNode dummyRootNode(0, std::nullopt, Str("Local Drive"), SyncName(), "1", "1", std::nullopt, std::nullopt,
                               std::nullopt, NodeType::Directory, 0, std::nullopt);
    Snapshot snapshot(ReplicaSide::Local, dummyRootNode);

    const int64_t file11Size = 10;
    const int64_t file12Size = 20;
    const int64_t file111Size = 30;
    const int64_t file112Size = 40;
    const int64_t expectedDir11Size = file111Size + file112Size;
    const int64_t expectedDir1Size = expectedDir11Size + file11Size + file12Size;
    const int64_t expectedRootDirSize = expectedDir1Size;

    const SnapshotItem dir1("dir1", rootNodeId, Str("dir1"), testhelpers::defaultTime, testhelpers::defaultTime,
                            NodeType::Directory, testhelpers::defaultDirSize, false, true, true);
    const SnapshotItem dir11("dir1.1", "dir1", Str("dir1.1"), testhelpers::defaultTime, testhelpers::defaultTime,
                             NodeType::Directory, testhelpers::defaultDirSize, false, true, true);
    const SnapshotItem file11("file1.1", "dir1", Str("file1.1"), testhelpers::defaultTime, testhelpers::defaultTime,
                              NodeType::File, file11Size, false, true, true);
    const SnapshotItem file12("file1.2", "dir1", Str("file1.2"), testhelpers::defaultTime, testhelpers::defaultTime,
                              NodeType::File, file12Size, false, true, true);
    const SnapshotItem file111("file1.1.1", "dir1.1", Str("file1.1.1"), testhelpers::defaultTime, testhelpers::defaultTime,
                               NodeType::File, file111Size, false, true, true);
    const SnapshotItem file112("file1.1.2", "dir1.1", Str("file1.1.2"), testhelpers::defaultTime, testhelpers::defaultTime,
                               NodeType::File, file112Size, false, true, true);
    snapshot.updateItem(dir1);
    snapshot.updateItem(dir11);
    snapshot.updateItem(file11);
    snapshot.updateItem(file12);
    snapshot.updateItem(file111);
    snapshot.updateItem(file112);
    snapshot.updateItem(file112);
    snapshot.updateItem(file112);

    CPPUNIT_ASSERT_EQUAL(file11Size, snapshot.size("file1.1"));
    CPPUNIT_ASSERT_EQUAL(file12Size, snapshot.size("file1.2"));
    CPPUNIT_ASSERT_EQUAL(file111Size, snapshot.size("file1.1.1"));
    CPPUNIT_ASSERT_EQUAL(file112Size, snapshot.size("file1.1.2"));
    CPPUNIT_ASSERT_EQUAL(expectedDir1Size, snapshot.size("dir1"));
    CPPUNIT_ASSERT_EQUAL(expectedDir11Size, snapshot.size("dir1.1"));
    CPPUNIT_ASSERT_EQUAL(expectedRootDirSize, snapshot.size("1" /*root node id*/));
}

void TestSnapshot::testDuplicatedItem() {
    const NodeId rootNodeId = *SyncDb::driveRootNode().nodeIdLocal();

    const DbNode dummyRootNode(0, std::nullopt, Str("Local Drive"), SyncName(), "1", "1", std::nullopt, std::nullopt,
                               std::nullopt, NodeType::Directory, 0, std::nullopt);
    Snapshot snapshot(ReplicaSide::Local, dummyRootNode);

    const SnapshotItem file1("A", rootNodeId, Str("file1"), 1640995201, -1640995201, NodeType::File, 123, false, true, true);
    const SnapshotItem file2("B", rootNodeId, Str("file1"), 1640995201, -1640995201, NodeType::File, 123, false, true, true);

    snapshot.updateItem(file1);
    snapshot.updateItem(file2);

    CPPUNIT_ASSERT(!snapshot.exists("A"));
    CPPUNIT_ASSERT(snapshot.exists("B"));
}

void TestSnapshot::testSnapshotInsertionWithDifferentEncodings() {
    const SnapshotItem nfcItem("A", _rootNodeId, testhelpers::makeNfcSyncName(), testhelpers::defaultTime,
                               -testhelpers::defaultTime, NodeType::Directory, testhelpers::defaultFileSize, false, true, true);
    const SnapshotItem nfdItem("B", _rootNodeId, testhelpers::makeNfdSyncName(), testhelpers::defaultTime,
                               -testhelpers::defaultTime, NodeType::Directory, testhelpers::defaultFileSize, false, true, true);
    {
        _snapshot->updateItem(nfcItem);
        SyncPath syncPath;
        bool ignore = false;
        _snapshot->path("A", syncPath, ignore);
        CPPUNIT_ASSERT_EQUAL(SyncPath(testhelpers::makeNfcSyncName()), syncPath);
    }
    {
        _snapshot->updateItem(nfdItem);
        SyncPath syncPath;
        bool ignore = false;
        _snapshot->path("B", syncPath, ignore);
        CPPUNIT_ASSERT_EQUAL(SyncPath(testhelpers::makeNfdSyncName()), syncPath);
    }
}

void TestSnapshot::testPath() {
    // Normal case
    {
        const auto id = CommonUtility::generateRandomStringAlphaNum();
        const auto name = Str("test");
        const SnapshotItem item(id, "a", name, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory,
                                testhelpers::defaultFileSize, false, true, true);
        _snapshot->updateItem(item);
        SyncPath path;
        bool ignore = false;
        CPPUNIT_ASSERT(_snapshot->path(id, path, ignore));
        CPPUNIT_ASSERT_EQUAL(SyncPath("A") / name, path);
        CPPUNIT_ASSERT(!ignore);
    }
    // Item name starting by pattern "X:", should be ignored (as well as its descendants) on Windows only
    {
        const auto id = CommonUtility::generateRandomStringAlphaNum();
        const auto name = Str("E:S");
        const SnapshotItem item(id, "a", name, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory,
                                testhelpers::defaultFileSize, false, true, true);
        _snapshot->updateItem(item);
        SyncPath path;
        bool ignore = false;
        // On Windows, if the file name starts with "X:" pattern, the previous element of the path are overrode
        // (https://en.cppreference.com/w/cpp/filesystem/path/append)
#ifdef _WIN32
        CPPUNIT_ASSERT(!_snapshot->path(id, path, ignore));
        CPPUNIT_ASSERT(ignore);
#else
        CPPUNIT_ASSERT(_snapshot->path(id, path, ignore));
        CPPUNIT_ASSERT_EQUAL(SyncPath("A") / name, path);
        CPPUNIT_ASSERT(!ignore);
#endif

        const auto childId = CommonUtility::generateRandomStringAlphaNum();
        const auto childName = Str("test");
        const SnapshotItem childItem(childId, id, childName, testhelpers::defaultTime, testhelpers::defaultTime,
                                     NodeType::Directory, testhelpers::defaultFileSize, false, true, true);
        _snapshot->updateItem(childItem);
#ifdef _WIN32
        CPPUNIT_ASSERT(!_snapshot->path(childId, path, ignore));
        CPPUNIT_ASSERT(ignore);
#else
        CPPUNIT_ASSERT(_snapshot->path(childId, path, ignore));
        CPPUNIT_ASSERT_EQUAL(SyncPath("A") / name / childName, path);
        CPPUNIT_ASSERT(!ignore);
#endif
    }
    // Same test but on the snapshot copy (meaning using paths stored in cache)
    std::vector<SyncName> names = {Str("E:S"), Str("E:/S")};
    for (const auto &name: names) {
        Snapshot snapshotCopy(ReplicaSide::Local, _dummyRootNode);
        snapshotCopy = *_snapshot;
        const auto id = CommonUtility::generateRandomStringAlphaNum();
        const SnapshotItem item(id, "a", name, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory,
                                testhelpers::defaultFileSize, false, true, true);
        snapshotCopy.updateItem(item);
        SyncPath path;
        bool ignore = false;
        // On Windows, if the file name starts with the "X:" pattern, the previous elements of the path are overrode
        // (https://en.cppreference.com/w/cpp/filesystem/path/append)
#ifdef _WIN32
        CPPUNIT_ASSERT(!snapshotCopy.path(id, path, ignore));
        CPPUNIT_ASSERT(ignore);
#else
        CPPUNIT_ASSERT(snapshotCopy.path(id, path, ignore));
        CPPUNIT_ASSERT_EQUAL(SyncPath("A") / name, path);
        CPPUNIT_ASSERT(!ignore);
#endif

        const auto childId = CommonUtility::generateRandomStringAlphaNum();
        const auto childName = Str("test");
        const SnapshotItem childItem(childId, id, childName, testhelpers::defaultTime, testhelpers::defaultTime,
                                     NodeType::Directory, testhelpers::defaultFileSize, false, true, true);
        snapshotCopy.updateItem(childItem);
#ifdef _WIN32
        CPPUNIT_ASSERT(!snapshotCopy.path(childId, path, ignore));
        CPPUNIT_ASSERT(ignore);
#else
        CPPUNIT_ASSERT(snapshotCopy.path(childId, path, ignore));
        CPPUNIT_ASSERT_EQUAL(SyncPath("A") / name / childName, path);
        CPPUNIT_ASSERT(!ignore);
#endif
    }
    // Item name starting by pattern "X:", should be ignored on Windows only
    {
        const auto id = CommonUtility::generateRandomStringAlphaNum();
        const auto name = Str("a:b");
        const SnapshotItem item(id, "a", name, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory,
                                testhelpers::defaultFileSize, false, true, true);
        _snapshot->updateItem(item);
        SyncPath path;
        bool ignore = false;
        // On Windows, if the file name starts with "X:" pattern, the previous element of the path are overrode
        // (https://en.cppreference.com/w/cpp/filesystem/path/append)
#ifdef _WIN32
        CPPUNIT_ASSERT(!_snapshot->path(id, path, ignore));
        CPPUNIT_ASSERT(ignore);
#else
        CPPUNIT_ASSERT(_snapshot->path(id, path, ignore));
        CPPUNIT_ASSERT_EQUAL(SyncPath("A") / name, path);
        CPPUNIT_ASSERT(!ignore);
#endif
    }
    // Item name starting by pattern "X:", should be ignored on Windows only
    {
        const auto id = CommonUtility::generateRandomStringAlphaNum();
        const auto name = Str("C:");
        const SnapshotItem item(id, "a", name, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory,
                                testhelpers::defaultFileSize, false, true, true);
        _snapshot->updateItem(item);

        SyncPath path;
        bool ignore = false;
        // On Windows, if the file name starts with "X:" pattern, the previous element of the path are overrode
        // (https://en.cppreference.com/w/cpp/filesystem/path/append)
#ifdef _WIN32
        CPPUNIT_ASSERT(!_snapshot->path(id, path, ignore));
        CPPUNIT_ASSERT(ignore);
#else
        CPPUNIT_ASSERT(_snapshot->path(id, path, ignore));
        CPPUNIT_ASSERT_EQUAL(SyncPath("A") / name, path);
        CPPUNIT_ASSERT(!ignore);
#endif
    }
    // Item name starting with more than 1 character before `:`, should be accepted
    {
        const auto id = CommonUtility::generateRandomStringAlphaNum();
        const auto name = Str("aa:b");
        const SnapshotItem item(id, "a", name, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory,
                                testhelpers::defaultFileSize, false, true, true);
        _snapshot->updateItem(item);
        SyncPath path;
        bool ignore = false;
        CPPUNIT_ASSERT(_snapshot->path(id, path, ignore));
        CPPUNIT_ASSERT_EQUAL(SyncPath("A") / name, path);
        CPPUNIT_ASSERT(!ignore);
    }
}

void TestSnapshot::testCopySnapshot() {
    const NodeId rootNodeId = *SyncDb::driveRootNode().nodeIdLocal();

    const DbNode dummyRootNode(0, std::nullopt, Str("Local Drive"), SyncName(), "1", "1", std::nullopt, std::nullopt,
                               std::nullopt, NodeType::Directory, 0, std::nullopt);
    Snapshot snapshot(ReplicaSide::Local, dummyRootNode);

    const SnapshotItem dir1("dir1", rootNodeId, Str("dir1"), testhelpers::defaultTime, testhelpers::defaultTime,
                            NodeType::Directory, testhelpers::defaultDirSize, false, true, true);
    const SnapshotItem file11("file1.1", "dir1", Str("file1.1"), testhelpers::defaultTime, testhelpers::defaultTime,
                              NodeType::File, testhelpers::defaultFileSize, false, true, true);

    snapshot.updateItem(dir1);
    snapshot.updateItem(file11);

    Snapshot snapShotCopy(ReplicaSide::Local, dummyRootNode);
    snapShotCopy = snapshot;

    CPPUNIT_ASSERT_EQUAL(snapshot.nbItems(), snapShotCopy.nbItems());

    NodeSet snapshotIds;
    NodeSet snapshotCopyIds;
    snapshot.ids(snapshotIds);
    snapShotCopy.ids(snapshotCopyIds);
    CPPUNIT_ASSERT(snapshotIds == snapshotCopyIds);

    // Ensure _items elements are copied, not just the pointer
    snapshot.setName("file1.1", Str("newFile1.1"));
    CPPUNIT_ASSERT(Str2SyncName("newFile1.1") == snapshot.name("file1.1"));
    CPPUNIT_ASSERT(Str2SyncName("file1.1") == snapShotCopy.name("file1.1"));

    // Ensure the snapshot revision is correctly handled
    snapshot.setName("file1.1", Str("File1.1")); // Ensure the live snapshot revision increase
    CPPUNIT_ASSERT_GREATER(snapShotCopy.revision(), snapshot.revision());
    CPPUNIT_ASSERT_GREATER(snapShotCopy.lastChangeRevision("file1.1"), snapshot.lastChangeRevision("file1.1"));

    // Ensure SnapshotItems::_children elements are copied, not just the pointer
    const auto snapshotDir1 = snapshot.findItem("dir1");
    const auto snapshotCopyDir1 = snapShotCopy.findItem("dir1");
    CPPUNIT_ASSERT(snapshotDir1);
    CPPUNIT_ASSERT(snapshotCopyDir1);

    const auto snapshotDir1Children = snapshotDir1->children();
    const auto snapshotCopyDir1Children = snapshotCopyDir1->children();
    CPPUNIT_ASSERT_EQUAL(size_t(1), snapshotDir1Children.size());
    CPPUNIT_ASSERT_EQUAL(size_t(1), snapshotCopyDir1Children.size());

    const auto snapshotDir1ChildPtr = snapshotDir1Children.begin()->get();
    const auto snapshotCopyDir1ChildPtr = snapshotCopyDir1Children.begin()->get();
    CPPUNIT_ASSERT_EQUAL(NodeId("file1.1"), snapshotDir1ChildPtr->id());
    CPPUNIT_ASSERT_EQUAL(NodeId("file1.1"), snapshotCopyDir1ChildPtr->id());

    CPPUNIT_ASSERT(snapshotDir1ChildPtr != snapshotCopyDir1ChildPtr);
    CPPUNIT_ASSERT_EQUAL(snapshot.findItem("file1.1").get(), snapshotDir1ChildPtr);
    CPPUNIT_ASSERT_EQUAL(snapShotCopy.findItem("file1.1").get(), snapshotCopyDir1ChildPtr);
}

void TestSnapshot::testSnapshotRevision() {
    const NodeId rootNodeId = *SyncDb::driveRootNode().nodeIdLocal();
    const DbNode dummyRootNode(0, std::nullopt, Str("Local Drive"), SyncName(), "1", "1", std::nullopt, std::nullopt,
                               std::nullopt, NodeType::Directory, 0, std::nullopt);

    Snapshot snapshot(ReplicaSide::Local, dummyRootNode);
    SnapshotRevision lastRevision = snapshot.revision();
    const SnapshotItem dir1("dir1", rootNodeId, Str("dir1"), testhelpers::defaultTime, testhelpers::defaultTime,
                            NodeType::Directory, testhelpers::defaultDirSize, false, true, true);
    const SnapshotItem file11("file1.1", "dir1", Str("file1.1"), testhelpers::defaultTime, testhelpers::defaultTime,
                              NodeType::File, testhelpers::defaultFileSize, false, true, true);

    snapshot.updateItem(dir1);
    CPPUNIT_ASSERT_GREATER(lastRevision, snapshot.revision());
    lastRevision = snapshot.revision();

    snapshot.updateItem(file11);
    CPPUNIT_ASSERT_GREATER(lastRevision, snapshot.revision());
    lastRevision = snapshot.revision();

    snapshot.setCreatedAt("dir1", 123);
    CPPUNIT_ASSERT_GREATER(lastRevision, snapshot.revision());
    lastRevision = snapshot.revision();

    snapshot.setLastModified("dir1", 123);
    CPPUNIT_ASSERT_GREATER(lastRevision, snapshot.revision());
    lastRevision = snapshot.revision();

    snapshot.setName("dir1", Str2SyncName("dir1b"));
    CPPUNIT_ASSERT_GREATER(lastRevision, snapshot.revision());
    lastRevision = snapshot.revision();

    snapshot.setContentChecksum("file1.1", "abcd");
    CPPUNIT_ASSERT_GREATER(lastRevision, snapshot.revision());
    lastRevision = snapshot.revision();

    snapshot.updateItem(dir1);
    CPPUNIT_ASSERT_GREATER(lastRevision, snapshot.revision());
    lastRevision = snapshot.revision();

    snapshot.updateItem(dir1);
    CPPUNIT_ASSERT_GREATER(lastRevision, snapshot.revision());
    lastRevision = snapshot.revision();
}

} // namespace KDC
