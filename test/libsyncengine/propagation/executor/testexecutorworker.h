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

#include "testincludes.h"
#include "propagation/executor/executorworker.h"
#include "test_utility/localtemporarydirectory.h"
#include "libcommonserver/vfs/vfs.h"

namespace KDC {

class MockVfs final : public VfsOff {
    public:
        explicit MockVfs() : VfsOff(vfsSetupParams) {}
        void setVfsStatusOutput(const VfsStatus &vfsStatus) {
            vfsStatusIsHydrated = vfsStatus.isHydrated;
            vfsStatusIsSyncing = vfsStatus.isSyncing;
            vfsStatusIsPlaceholder = vfsStatus.isPlaceholder;
            vfsStatusProgress = vfsStatus.progress;
        }
        ExitInfo status([[maybe_unused]] const SyncPath &filePath, VfsStatus &vfsStatus) override {
            vfsStatus.isHydrated = vfsStatusIsHydrated;
            vfsStatus.isSyncing = vfsStatusIsSyncing;
            vfsStatus.isPlaceholder = vfsStatusIsPlaceholder;
            vfsStatus.progress = vfsStatusProgress;
            return ExitCode::Ok;
        }

    private:
        bool vfsStatusIsHydrated = false;
        bool vfsStatusIsSyncing = false;
        bool vfsStatusIsPlaceholder = false;
        int64_t vfsStatusProgress = 0;
        VfsSetupParams vfsSetupParams;
};

class TestExecutorWorker : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(TestExecutorWorker);
        CPPUNIT_TEST(testCheckLiteSyncInfoForCreate);
        CPPUNIT_TEST(testFixModificationDate);
        CPPUNIT_TEST(testAffectedUpdateTree);
        CPPUNIT_TEST(testTargetUpdateTree);
        CPPUNIT_TEST(testRemoveDependentOps);
        CPPUNIT_TEST(testIsValidDestination);
        CPPUNIT_TEST(testTerminatedJobsQueue);
        CPPUNIT_TEST(testPropagateConflictToDbAndTree);
        CPPUNIT_TEST(testDeleteOpNodes);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    private:
        void testCheckLiteSyncInfoForCreate();
        void testFixModificationDate();
        void testAffectedUpdateTree();
        void testTargetUpdateTree();
        void testRemoveDependentOps();
        void testIsValidDestination();
        void testTerminatedJobsQueue();
        void testPropagateConflictToDbAndTree();
        void testDeleteOpNodes();

        bool opsExist(SyncOpPtr op);
        SyncOpPtr generateSyncOperation(const DbNodeId dbNodeId, const SyncName &filename,
                                        const OperationType opType = OperationType::None);
        SyncOpPtr generateSyncOperationWithNestedNodes(const DbNodeId dbNodeId, const SyncName &filename,
                                                       const OperationType opType, const NodeType nodeType);

        std::shared_ptr<SyncPal> _syncPal;
        Sync _sync;
        std::shared_ptr<ExecutorWorker> _executorWorker;
        LocalTemporaryDirectory _localTempDir{"TestExecutorWorker"};
};

} // namespace KDC
