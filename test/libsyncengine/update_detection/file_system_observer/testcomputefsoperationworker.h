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

#pragma once

#include "update_detection/file_system_observer/computefsoperationworker.h"
#include "testincludes.h"
#include "test_utility/localtemporarydirectory.h"
#include "syncpal/syncpal.h"
#include "test_classes/testsituationgenerator.h"

namespace KDC {

class MockComputeFSOperationWorker : public ComputeFSOperationWorker {
    public:
        MockComputeFSOperationWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName);

    private:
        ExitInfo checkIfOkToDelete(ReplicaSide, const SyncPath &, const NodeId &, bool &);
};

class TestComputeFSOperationWorker : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestComputeFSOperationWorker);
        CPPUNIT_TEST(testNoOps);
        CPPUNIT_TEST(testMultipleOps);
        CPPUNIT_TEST(testLnkFileAlreadySynchronized);
        CPPUNIT_TEST(testDifferentEncoding_NFC_NFD);
        CPPUNIT_TEST(testDifferentEncoding_NFD_NFC);
        CPPUNIT_TEST(testDifferentEncoding_NFD_NFD);
        CPPUNIT_TEST(testDifferentEncoding_NFC_NFC);
        CPPUNIT_TEST(testCreateDuplicateNamesWithDistinctEncodings);
        CPPUNIT_TEST(testDeletionOfNestedFolders);
        CPPUNIT_TEST(testAccessDenied);
        CPPUNIT_TEST(testExclusion);
        CPPUNIT_TEST(testIsInUnsyncedList);
        CPPUNIT_TEST(testHasChangedSinceLastSeen);
        CPPUNIT_TEST(testUpdateSyncNode);
#if defined(KD_LINUX)
        CPPUNIT_TEST(testPostponeCreateOperationsOnReusedIds);
#endif
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    protected:
        /**
         * No operation should be generated in this test.
         */
        void testNoOps();
        /**
         * Multiple operations should be generated in this test: the types Create, Delete, Move and Edit are all expected.
         */
        void testMultipleOps();
        /**
         * Specific test for the issue https://infomaniak.atlassian.net/browse/KDESKTOP-893.
         * Sync is looping because a `.lnk` file was already synchronized with an earlier app's version.
         * A Delete FS operation was generated on remote replica but could not be propagated since the file still exists on this
         * replica.
         * No FS operation should be generated on an excluded file.
         */
        void testLnkFileAlreadySynchronized();
        // NFC in DB, NFD on FS
        void testDifferentEncoding_NFC_NFD();
        // NFD in DB, NFC on FS
        void testDifferentEncoding_NFD_NFC();
        // NFD in DB, NFD on FS
        void testDifferentEncoding_NFD_NFD();
        // NFC in DB, NFC on FS
        void testDifferentEncoding_NFC_NFC();
        void testCreateDuplicateNamesWithDistinctEncodings();

        /**
         * The deletion of a local folders and their common parent should generate a Delete operation for each item.
         * Deletion of a blacklisted subfolder should not generate any operation.
         */
        void testDeletionOfNestedFolders();
        // Test Access Denied error in ComputeFSOperationWorker::inferChangeFromDbNode
        void testAccessDenied();

        // Test exclusions
        void testExclusion();
        void testIsInUnsyncedList();

        // Test updates of SyncDb's 'sync_node' table
        void testUpdateSyncNode();

        void testHasChangedSinceLastSeen();

#if defined(KD_LINUX)
        // Create operations on local items that reused the identifiers deleted local items are removed from the computed
        // operation list. This also holds for every descendants of such items.
        void testPostponeCreateOperationsOnReusedIds();
#endif

    private:
        void testIsInUnsyncedList(bool expectedResult, const NodeId &nodeId, ReplicaSide side) const;

        std::shared_ptr<SyncPal> _syncPal;
        LocalTemporaryDirectory _localTempDir{"TestSyncPal"};
        TestSituationGenerator _situationGenerator;
};

} // namespace KDC
