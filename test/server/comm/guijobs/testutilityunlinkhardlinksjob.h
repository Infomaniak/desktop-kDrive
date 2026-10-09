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

// The behavior tests rely on IoHelper::getHardlinkPaths to locate the links of an item, which is only available on Windows.
#if defined(KD_WINDOWS)

#include "testincludes.h"
#include "test_utility/localtemporarydirectory.h"

#include <log4cplus/logger.h>

#include <optional>

namespace KDC {

class Error;
class SyncPal;

/// Tests the behavior of UtilityUnlinkHardlinksJob::unlinkHardlinks: validation of the reported error, database cleanup,
/// hardlink removal under the sync root and rescue copy of items that are not in sync with the database.
class TestUtilityUnlinkHardlinksJob : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestUtilityUnlinkHardlinksJob);
        CPPUNIT_TEST(testInSyncFile);
        CPPUNIT_TEST(testModifiedFile);
        CPPUNIT_TEST(testMissingFile);
        CPPUNIT_TEST(testRemovedReportedLink);
        CPPUNIT_TEST(testMovedFile);
        CPPUNIT_TEST(testLinkSearchFailure);
        CPPUNIT_TEST(testLinkOutsideSyncRoot);
        CPPUNIT_TEST(testModifiedThroughOtherLink);
        CPPUNIT_TEST(testRescueFilenameCollision);
        CPPUNIT_TEST(testUnknownNode);
        CPPUNIT_TEST(testInvalidPath);
        CPPUNIT_TEST(testNodeIdMismatch);
        CPPUNIT_TEST(testSeedSelection);
        CPPUNIT_TEST(testErrorRemoval);
        CPPUNIT_TEST(testErrorMismatch);
        CPPUNIT_TEST(testTmpBlacklistRemoval);
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

        /// The file has no link left under the sync root: only the node is deleted from the database.
        void testMissingFile();

        /// The reported link has been removed but the file still has links under the sync root: they are found by node id and
        /// removed, after saving a single copy of the file into the rescue folder, as they are not located at the path stored in
        /// the database. A hardlink located outside of the sync root is kept.
        void testRemovedReportedLink();

        /// The file has been moved after the error was reported: it is found by node id at its new location and removed, after
        /// saving a copy of the file into the rescue folder.
        void testMovedFile();

        /// The reported path does not exist anymore and the links of the file cannot be searched, e.g. because the sync root is
        /// not accessible: the job must fail without deleting the node and the error, as links of the file may remain.
        void testLinkSearchFailure();

        /// A hardlink located outside of the sync root is kept, the links under the sync root are removed.
        void testLinkOutsideSyncRoot();

        /// A file in sync with the database, then modified through a hardlink located outside of the sync root: a copy of the
        /// modified file is saved into the rescue folder before removing the link under the sync root, even if the metadata
        /// cached in the directory entry of this link are outdated (Windows).
        void testModifiedThroughOtherLink();

        /// A file whose rescue copy name is already used in the rescue folder: the existing rescue copy is preserved and
        /// the file is saved with a suffixed name.
        void testRescueFilenameCollision();

        /// A node id that is not present in the sync database, e.g. after a resolution whose final cleanup step failed: the
        /// job only completes the cleanup by removing the reported error.
        void testUnknownNode();

        /// An absolute path or a path escaping the sync root: the job must reject the request without touching the file
        /// system.
        void testInvalidPath();

        /// The item located at the reported path refers to another node than the reported node id: the job must reject the
        /// request without touching the file system.
        void testNodeIdMismatch();

        /// The item located at the seed path has been replaced by another file: another link of the reported node is selected
        /// as the seed path, or nothing is removed when no link refers to the reported node anymore.
        void testSeedSelection();

        /// The reported error is removed from the parameters database only when the job succeeds, and the other errors are
        /// kept.
        void testErrorRemoval();

        /// The reported error does not exist, or is not the hardlink error of the reported sync, node and path: the job must
        /// reject the request without touching the file system and the databases.
        void testErrorMismatch();

        /// Items blacklisted by the sync engine are removed from the temporary blacklist only once the node has been removed
        /// from the sync database, so that the deletion of the links cannot propagate to the remote replica.
        void testTmpBlacklistRemoval();

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

        LocalTemporaryDirectory _localTempDir{"testUtilityUnlinkHardlinksJob"};
        LocalTemporaryDirectory _localOtherDir{"testUtilityUnlinkHardlinksJobOther"};
        LocalTemporaryDirectory _localParmsDbTempDir{"testUtilityUnlinkHardlinksJobParmsDb"};

        /// Creates a file in the sync root, inserts the corresponding node into the sync database and, if linkName is not empty,
        /// creates a hardlink of the file with the given name. Returns the local node id (i.e. the file inode) of the created
        /// node.
        NodeId createFileAndDbNode(const SyncName &name, const std::string &content, const SyncName &linkName = {},
                                   int64_t dbNodeSizeOffset = 0);

        /// Inserts a node with the given name, local node id and type under the root node of the sync database.
        void insertDbNode(const SyncName &name, const NodeId &nodeId, NodeType type);

        /// Returns a hardlink error reported for the given node of the current sync.
        [[nodiscard]] Error makeHardlinkError(const NodeId &nodeId, const SyncPath &relativePath) const;

        /// Inserts the given error into the parameters database and returns its database id.
        static ErrorDbId insertParmsDbError(Error &error);

        /// Inserts a hardlink error reported for the given node into the parameters database and returns its database id.
        ErrorDbId insertHardlinkError(const NodeId &nodeId, const SyncPath &relativePath);

        /// Runs UtilityUnlinkHardlinksJob::unlinkHardlinks on the current syncPal and returns its exit info. If errorDbId is not
        /// set, a hardlink error matching the request is inserted into the parameters database and reported.
        ExitInfo runUnlinkExpect(const NodeId &nodeId, const SyncPath &relativePath,
                                 const std::optional<ErrorDbId> &errorDbId = std::nullopt);

        /// Runs UtilityUnlinkHardlinksJob::unlinkHardlinks on the current syncPal and asserts that it succeeds. If errorDbId is
        /// not set, a hardlink error matching the request is inserted into the parameters database and reported.
        void runUnlink(const NodeId &nodeId, const SyncPath &relativePath,
                       const std::optional<ErrorDbId> &errorDbId = std::nullopt);

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

#endif
