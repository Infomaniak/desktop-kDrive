/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#pragma once

#include "testincludes.h"
#include "test_utility/localtemporarydirectory.h"

#include <log4cplus/logger.h>

namespace KDC {

class SyncPal;

/// Tests the behavior of ErrorQuickResolveHardlinkJob::quickResolve: database cleanup, hardlink removal under the sync root
/// and rescue copy of items that are not in sync with the database.
class TestErrorQuickResolveHardlinkJob : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestErrorQuickResolveHardlinkJob);
        CPPUNIT_TEST(testInSyncFile);
        CPPUNIT_TEST(testModifiedFile);
        CPPUNIT_TEST(testMissingFile);
        CPPUNIT_TEST(testLinkOutsideSyncRoot);
        CPPUNIT_TEST(testRescueFilenameCollision);
        CPPUNIT_TEST(testUnknownNode);
        CPPUNIT_TEST(testInvalidPath);
        CPPUNIT_TEST(testNodeIdMismatch);
        CPPUNIT_TEST(testErrorRemoval);
        CPPUNIT_TEST(testDirectoryNode);
        CPPUNIT_TEST(testSymlinkKept);
        CPPUNIT_TEST(testSymlinkSeed);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    protected:
        /// A file in sync with the database, hardlinked once under the sync root: all the links are removed, no rescue copy is
        /// made and the node is deleted from the database.
        void testInSyncFile();

        /// A file whose content does not match the database anymore: a copy of the file is saved into the rescue folder before
        /// removing all the links and deleting the node from the database.
        void testModifiedFile();

        /// The file does not exist anymore: only the node is deleted from the database.
        void testMissingFile();

        /// A hardlink located outside of the sync root is kept, the links under the sync root are removed.
        void testLinkOutsideSyncRoot();

        /// A file whose rescue copy name is already used in the rescue folder: the existing rescue copy is preserved and
        /// the file is saved with a suffixed name.
        void testRescueFilenameCollision();

        /// A node id that is not present in the sync database: the job must reject the request without touching the file
        /// system.
        void testUnknownNode();

        /// An absolute path or a path escaping the sync root: the job must reject the request without touching the file
        /// system.
        void testInvalidPath();

        /// The item located at the reported path refers to another node than the reported node id: the job must reject the
        /// request without touching the file system.
        void testNodeIdMismatch();

        /// The reported error is removed from the parameters database only when the job succeeds, and the other errors are
        /// kept.
        void testErrorRemoval();

        /// A directory node, or a file node whose reported item is a directory: the job must reject the request without
        /// touching the file system, as the whole content of the directory would be removed.
        void testDirectoryNode();

        /// A symbolic link targeting a hardlinked file is not a link of the file: it is kept while the hardlinks are removed.
        void testSymlinkKept();

        /// The reported item is a symbolic link: the job must reject the request without touching the file system, as symbolic
        /// links are never followed.
        void testSymlinkSeed();

    private:
        /// An error database id that is not present in the parameters database.
        static constexpr ErrorDbId unknownErrorDbId = 42;

        log4cplus::Logger _logger;
        std::shared_ptr<SyncPal> _syncPal = nullptr;

        LocalTemporaryDirectory _localTempDir{"testErrorQuickResolveHardlinkJob"};
        LocalTemporaryDirectory _localOtherDir{"testErrorQuickResolveHardlinkJobOther"};
        LocalTemporaryDirectory _localParmsDbTempDir{"testErrorQuickResolveHardlinkJobParmsDb"};

        /// Creates a file in the sync root, inserts the corresponding node into the sync database and, if linkName is not empty,
        /// creates a hardlink of the file with the given name. Returns the local node id (i.e. the file inode) of the created
        /// node.
        NodeId createFileAndDbNode(const SyncName &name, const std::string &content, const SyncName &linkName = {},
                                   int64_t dbNodeSizeOffset = 0);

        /// Inserts a node with the given name, local node id and type under the root node of the sync database.
        void insertDbNode(const SyncName &name, const NodeId &nodeId, NodeType type);

        /// Inserts a hardlink error reported for the given node into the parameters database and returns its database id.
        ErrorDbId insertHardlinkError(const NodeId &nodeId, const SyncPath &relativePath);

        /// Runs ErrorQuickResolveHardlinkJob::quickResolve on the current syncPal and returns its exit info.
        ExitInfo runQuickResolveExpect(const NodeId &nodeId, const SyncPath &relativePath,
                                       ErrorDbId errorDbId = unknownErrorDbId);

        /// Runs ErrorQuickResolveHardlinkJob::quickResolve on the current syncPal and asserts that it succeeds.
        void runQuickResolve(const NodeId &nodeId, const SyncPath &relativePath, ErrorDbId errorDbId = unknownErrorDbId);

        /// Returns true if the node with the given local node id exists in the sync database.
        [[nodiscard]] bool nodeExistsInDb(const NodeId &nodeId) const;

        /// Returns true if the error with the given database id exists in the parameters database.
        [[nodiscard]] static bool errorExistsInDb(ErrorDbId errorDbId);

        /// Returns the local node id of the item located at the given path. Symbolic links are not followed.
        [[nodiscard]] static NodeId localNodeId(const SyncPath &path);

        /// Returns true if the given path exists in the file system.
        [[nodiscard]] static bool pathExists(const SyncPath &path);

        /// Returns true if the item located at the given path is a symbolic link, whether its target exists or not.
        [[nodiscard]] static bool isSymlink(const SyncPath &path);
};

} // namespace KDC
