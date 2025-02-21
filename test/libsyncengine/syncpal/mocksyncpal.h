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
#include "utility/types.h"
#include "syncpal/syncpal.h"
#include "syncpal/tmpblacklistmanager.h"

#include "update_detection/file_system_observer/filesystemobserverworker.h"
#ifdef _WIN32
#include "libsyncengine/syncpal/mocklfsoworker_win.h"
#else
#include "libsyncengine/syncpal/mocklfsoworker_unix.h"
#endif
#include "libsyncengine/syncpal/mockrfsoworker.h"
#include "libsyncengine/syncpal/mockcomputefsoperationworker.h"
#include "libsyncengine/syncpal/mockupdatetreeworker.h"
#include "libsyncengine/syncpal/mockplatforminconsistencycheckerworker.h"
#include "libsyncengine/syncpal/mockconflictfinderworker.h"
#include "libsyncengine/syncpal/mockconflictresolverworker.h"
#include "libsyncengine/syncpal/mockoperationgeneratorworker.h"
#include "libsyncengine/syncpal/mockoperationsorterworker.h"
#include "libsyncengine/syncpal/mockexecutorworker.h"
#include "syncpal/syncpalworker.h"


namespace KDC {

class MockSyncPal : public SyncPal {
    public:
        using SyncPal::SyncPal;
#ifdef _WIN32
        std::shared_ptr<MockLocalFileSystemObserverWorker_win> getMockLFSOWorker() {
            return std::static_pointer_cast<MockLocalFileSystemObserverWorker_win>(_localFSObserverWorker);
        }
#else
        std::shared_ptr<MockLocalFileSystemObserverWorker_unix> getMockLFSOWorker() {
            return std::static_pointer_cast<MockLocalFileSystemObserverWorker_unix>(_localFSObserverWorker);
        }
#endif

        std::shared_ptr<MockRemoteFileSystemObserverWorker> getMockRFSOWorker() {
            return std::static_pointer_cast<MockRemoteFileSystemObserverWorker>(_remoteFSObserverWorker);
        }
        std::shared_ptr<MockComputeFSOperationWorkerTestSyncPalWorker> getMockComputeFSOperationWorker() {
            return std::static_pointer_cast<MockComputeFSOperationWorkerTestSyncPalWorker>(_computeFSOperationsWorker);
        }
        std::shared_ptr<MockUpdateTreeWorker> getMockLocalUpdateTreeWorker() {
            return std::static_pointer_cast<MockUpdateTreeWorker>(_localUpdateTreeWorker);
        }
        std::shared_ptr<MockUpdateTreeWorker> getMockRemoteUpdateTreeWorker() {
            return std::static_pointer_cast<MockUpdateTreeWorker>(_remoteUpdateTreeWorker);
        }
        std::shared_ptr<MockPlatformInconsistencyCheckerWorker> getMockPlatformInconsistencyCheckerWorker() {
            return std::static_pointer_cast<MockPlatformInconsistencyCheckerWorker>(_platformInconsistencyCheckerWorker);
        }
        std::shared_ptr<MockConflictFinderWorker> getMockConflictFinderWorker() {
            return std::static_pointer_cast<MockConflictFinderWorker>(_conflictFinderWorker);
        }
        std::shared_ptr<MockConflictResolverWorker> getMockConflictResolverWorker() {
            return std::static_pointer_cast<MockConflictResolverWorker>(_conflictResolverWorker);
        }
        std::shared_ptr<MockOperationGeneratorWorker> getMockOperationGeneratorWorker() {
            return std::static_pointer_cast<MockOperationGeneratorWorker>(_operationsGeneratorWorker);
        }
        std::shared_ptr<MockOperationSorterWorker> getMockOperationSorterWorker() {
            return std::static_pointer_cast<MockOperationSorterWorker>(_operationsSorterWorker);
        }
        std::shared_ptr<MockExecutorWorker> getMockExecutorWorker() {
            return std::static_pointer_cast<MockExecutorWorker>(_executorWorker);
        }
        std::shared_ptr<SyncPalWorker> getSyncPalWorker() { return _syncPalWorker; }

    private:
        void createWorkers(const std::chrono::seconds &startDelay = std::chrono::seconds(0)) override {
            LOG_SYNCPAL_DEBUG(_logger, "Create workers");
#if defined(_WIN32)
            _localFSObserverWorker = std::make_shared<MockLocalFileSystemObserverWorker_win>(
                    shared_from_this(), "Mock Local File System Observer", "M_LFSO");
#else
            _localFSObserverWorker = std::make_shared<MockLocalFileSystemObserverWorker_unix>(
                    shared_from_this(), "Mock Local File System Observer", "M_LFSO");
#endif
            _remoteFSObserverWorker = std::make_shared<MockRemoteFileSystemObserverWorker>(
                    shared_from_this(), "Mock Remote File System Observer", "M_RFSO");
            _computeFSOperationsWorker = std::make_shared<MockComputeFSOperationWorkerTestSyncPalWorker>(
                    shared_from_this(), "Mock Compute FS Operations", "M_COOP");
            _localUpdateTreeWorker = std::make_shared<MockUpdateTreeWorker>(shared_from_this(), "Mock Local Tree Updater",
                                                                            "M_LTRU", ReplicaSide::Local);
            _remoteUpdateTreeWorker = std::make_shared<MockUpdateTreeWorker>(shared_from_this(), "Mock Remote Tree Updater",
                                                                             "M_RTRU", ReplicaSide::Remote);
            _platformInconsistencyCheckerWorker = std::make_shared<MockPlatformInconsistencyCheckerWorker>(
                    shared_from_this(), "Mock Platform Inconsistency Checker", "M_PICH");
            _conflictFinderWorker =
                    std::make_shared<MockConflictFinderWorker>(shared_from_this(), "Mock Conflict Finder", "M_COFD");
            _conflictResolverWorker =
                    std::make_shared<MockConflictResolverWorker>(shared_from_this(), "Mock Conflict Resolver", "M_CORE");
            _operationsGeneratorWorker =
                    std::make_shared<MockOperationGeneratorWorker>(shared_from_this(), "Mock Operation Generator", "M_OPGE");
            _operationsSorterWorker =
                    std::make_shared<MockOperationSorterWorker>(shared_from_this(), "Mock Operation Sorter", "M_OPSO");
            _executorWorker = std::make_shared<MockExecutorWorker>(shared_from_this(), "Mock Executor", "M_EXEC");
            _syncPalWorker = std::make_shared<SyncPalWorker>(shared_from_this(), "Mock Main", "M_MAIN", startDelay);

            _tmpBlacklistManager = std::make_shared<TmpBlacklistManager>(shared_from_this());
        }
};
} // namespace KDC
