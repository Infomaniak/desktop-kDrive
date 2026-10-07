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

    private:
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

        /// Runs ErrorQuickResolveHardlinkJob::quickResolve on the current syncPal and asserts that it succeeds.
        void runQuickResolve(const NodeId &nodeId, const SyncPath &relativePath);

        /// Returns true if the node with the given local node id exists in the sync database.
        [[nodiscard]] bool nodeExistsInDb(const NodeId &nodeId) const;

        /// Returns true if the given path exists in the file system.
        [[nodiscard]] static bool pathExists(const SyncPath &path);
};

} // namespace KDC
