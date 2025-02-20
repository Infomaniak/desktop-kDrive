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
        explicit MockVfs() : VfsOff(_vfsSetupParams) {}
        void setVfsStatusOutput(const VfsStatus &vfsStatus) {
            _vfsStatusIsHydrated = vfsStatus.isHydrated;
            _vfsStatusIsSyncing = vfsStatus.isSyncing;
            _vfsStatusIsPlaceholder = vfsStatus.isPlaceholder;
            _vfsStatusProgress = vfsStatus.progress;
        }
        ExitInfo status([[maybe_unused]] const SyncPath &filePath, VfsStatus &vfsStatus) override {
            vfsStatus.isHydrated = _vfsStatusIsHydrated;
            vfsStatus.isSyncing = _vfsStatusIsSyncing;
            vfsStatus.isPlaceholder = _vfsStatusIsPlaceholder;
            vfsStatus.progress = _vfsStatusProgress;
            return ExitCode::Ok;
        }

    private:
        bool _vfsStatusIsHydrated = false;
        bool _vfsStatusIsSyncing = false;
        bool _vfsStatusIsPlaceholder = false;
        int16_t _vfsStatusProgress = 0;
        VfsSetupParams _vfsSetupParams;
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
