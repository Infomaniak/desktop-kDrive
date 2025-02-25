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
#include "test_utility/localtemporarydirectory.h"
#include "syncpal/syncpal.h"
#include "syncpal/tmpblacklistmanager.h"
#ifdef _WIN32
#include "update_detection/file_system_observer/localfilesystemobserverworker_win.h"
#else
#include "update_detection/file_system_observer/localfilesystemobserverworker_unix.h"
#endif
#include "update_detection/file_system_observer/remotefilesystemobserverworker.h"
#include "update_detection/update_detector/updatetreeworker.h"
#include "update_detection/file_system_observer/computefsoperationworker.h"
#include "reconciliation/conflict_finder/conflictfinderworker.h"
#include "reconciliation/conflict_resolver/conflictresolverworker.h"
#include "propagation/executor/executorworker.h"
#include "reconciliation/operation_generator/operationgeneratorworker.h"
#include "propagation/operation_sorter/operationsorterworker.h"
#include "reconciliation/platform_inconsistency_checker/platforminconsistencycheckerworker.h"
#include "syncpal/syncpalworker.h"

using namespace CppUnit;

namespace KDC {

template<class LFSOW>
concept LFSOWorker = std::is_base_of_v<LocalFileSystemObserverWorker, LFSOW>;

class TestSyncPalWorker : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(TestSyncPalWorker);
        CPPUNIT_TEST(testInternalPause1);
        CPPUNIT_TEST(testInternalPause2);
        CPPUNIT_TEST(testInternalPause3);
        CPPUNIT_TEST_SUITE_END();

    public:
        TestSyncPalWorker() {}
        void setUp() override;
        void tearDown() override;

    private:
        Sync _sync;
        std::shared_ptr<SyncPal> _syncPal;
        std::shared_ptr<ParmsDb> _parmsDb;
        LocalTemporaryDirectory _localTempDir = LocalTemporaryDirectory("TestSyncpal");
        int _driveDbId;
        SyncPath _localPath;
        SyncPath _remotePath;

        void setUpTestInternalPause(const std::chrono::steady_clock::duration& longPollDuration);

        /* This test ensure that a RFSO network error while the synchronization is idle lead to a pause state immediately and
         * that the synchronization is automatically restarted when the network is back.
         */
        void testInternalPause1();

        /* This test ensure that a RFSO network error while the synchronization is running lead to a pause state when the
         * synchronization reach the Idle state and that the synchronization is automatically restarted when the network is back.
         */
        void testInternalPause2();

        /* This test ensure that a network error in the executor worker while lead to a pause
         * state.
         */
        void testInternalPause3();

        void testStopDuringInternalPause();
        void testDestroyDuringInternalPause();

        bool _testEnded = false;
        std::vector<std::shared_ptr<std::thread>> _runningThreads;

        class TimeoutHelper {
            public:
                /* Wait for condition() to return true,
                 * Return false if timed out
                 */
                static bool waitFor(std::function<bool()> condition, const std::chrono::steady_clock::duration& duration,
                                    const std::chrono::steady_clock::duration& loopWait) {
                    TimeoutHelper timeout(duration, loopWait);
                    while (!condition()) {
                        if (timeout.timedOut()) return false;
                    }
                    return true;
                }

                /* Wait for condition() to return true, and run stateCheck() after each loop,
                 * Return false if timed out
                 */
                static bool waitFor(std::function<bool()> condition, std::function<void()> stateCheck,
                                    const std::chrono::steady_clock::duration& duration,
                                    const std::chrono::steady_clock::duration& loopWait) {
                    TimeoutHelper timeout(duration, loopWait);
                    while (!condition()) {
                        if (timeout.timedOut()) return false;
                        stateCheck();
                    }
                    return true;
                }

            private:
                TimeoutHelper(const std::chrono::steady_clock::duration& duration,
                              const std::chrono::steady_clock::duration& loopWait = std::chrono::milliseconds(0)) :
                    _duration(duration), _loopWait(loopWait) {
                    _start = std::chrono::steady_clock::now();
                }

                bool timedOut() {
                    bool result = (_start + _duration) < std::chrono::steady_clock::now();
                    if (!result && _loopWait != std::chrono::milliseconds(0)) {
                        Utility::msleep(std::chrono::duration_cast<std::chrono::milliseconds>(_loopWait).count());
                    }
                    return result;
                }
                std::chrono::steady_clock::time_point _start;
                std::chrono::steady_clock::duration _duration;
                std::chrono::steady_clock::duration _loopWait;
        };

        // Mocks
        class MockUpdateTreeWorker : public UpdateTreeWorker {
            public:
                using UpdateTreeWorker::UpdateTreeWorker;

            private:
                void execute() override {
                    std::dynamic_pointer_cast<TestSyncPalWorker::MockSyncPal>(_syncPal)
                            ->updateTree(ReplicaSide::Local)
                            ->startUpdate();
                    setDone(ExitCode::Ok);
                }
        };

        class MockRemoteFileSystemObserverWorker : public RemoteFileSystemObserverWorker {
            public:
                using RemoteFileSystemObserverWorker::RemoteFileSystemObserverWorker;
                void setNetworkAvailability(bool networkAvailable) { _networkAvailable = networkAvailable; }
                void simulateFSEvent() {
                    std::dynamic_pointer_cast<TestSyncPalWorker::MockSyncPal>(_syncPal)
                            ->snapshot(ReplicaSide::Local)
                            ->startUpdate();
                }
                void setLongPollDuration(std::chrono::steady_clock::duration duration) { _longPollDuration = duration; }

            private:
                bool _networkAvailable{true};
                std::chrono::steady_clock::duration _longPollDuration = std::chrono::seconds(50);
                ExitCode sendLongPoll(bool& changes) override;
                ExitCode generateInitialSnapshot() override;
        };

        class MockComputeFSOperationWorkerTestSyncPalWorker : public ComputeFSOperationWorker {
            public:
                using ComputeFSOperationWorker::ComputeFSOperationWorker;

            private:
                void execute() override {
                    std::dynamic_pointer_cast<TestSyncPalWorker::MockSyncPal>(_syncPal)
                            ->operationSet(ReplicaSide::Local)
                            ->startUpdate();
                    setDone(ExitCode::Ok);
                }
        };

        class MockConflictFinderWorker : public ConflictFinderWorker {
            public:
                using ConflictFinderWorker::ConflictFinderWorker;

            private:
                void execute() override { setDone(ExitCode::Ok); }
        };

        class MockConflictResolverWorker : public ConflictResolverWorker {
            public:
                using ConflictResolverWorker::ConflictResolverWorker;

            private:
                void execute() override { setDone(ExitCode::Ok); }
        };

        class MockExecutorWorker : public ExecutorWorker {
            public:
                using ExecutorWorker::ExecutorWorker;
                void setMockExecuteCallback(std::function<ExitInfo()> callback) { _mockExecuteCallback = callback; }

            private:
                std::function<ExitInfo()> _mockExecuteCallback;
                void execute() override {
                    if (!_mockExecuteCallback) return setDone(ExitCode::Ok);
                    ExitInfo exitInfo = _mockExecuteCallback();
                    setExitCause(exitInfo.cause());
                    setDone(exitInfo.code());
                }
        };


#ifdef _WIN32
        class MockLFSO : public LocalFileSystemObserverWorker_win {
            public:
                using LocalFileSystemObserverWorker_win::LocalFileSystemObserverWorker_win;
#else
        class MockLFSO : public LocalFileSystemObserverWorker_unix {
            public:
                using LocalFileSystemObserverWorker_unix::LocalFileSystemObserverWorker_unix;
#endif
                void simulateFSEvent() { _snapshot->startUpdate(); }
        };

        class MockOperationGeneratorWorker : public OperationGeneratorWorker {
            public:
                using OperationGeneratorWorker::OperationGeneratorWorker;

            private:
                void execute() override { setDone(ExitCode::Ok); }
        };

        class MockOperationSorterWorker : public OperationSorterWorker {
            public:
                using OperationSorterWorker::OperationSorterWorker;

            private:
                void execute() override { setDone(ExitCode::Ok); }
        };

        class MockPlatformInconsistencyCheckerWorker : public PlatformInconsistencyCheckerWorker {
            public:
                using PlatformInconsistencyCheckerWorker::PlatformInconsistencyCheckerWorker;
                void setMockExecuteCallback(std::function<ExitInfo()> callback) { _mockExecuteCallback = callback; }

            private:
                std::function<ExitInfo()> _mockExecuteCallback;
                void execute() override {
                    if (!_mockExecuteCallback) return setDone(ExitCode::Ok);
                    ExitInfo exitInfo = _mockExecuteCallback();
                    setExitCause(exitInfo.cause());
                    setDone(exitInfo.code());
                }
        };

        class MockSyncPal : public SyncPal {
            public:
                using SyncPal::SyncPal;
                std::shared_ptr<MockLFSO> getMockLFSOWorker();
                std::shared_ptr<TestSyncPalWorker::MockRemoteFileSystemObserverWorker> getMockRFSOWorker();
                std::shared_ptr<MockPlatformInconsistencyCheckerWorker> getMockPlatformInconsistencyCheckerWorker();
                std::shared_ptr<MockExecutorWorker> getMockExecutorWorker();
                std::shared_ptr<SyncPalWorker> getSyncPalWorker() { return _syncPalWorker; }
                std::shared_ptr<UpdateTree> updateTree(ReplicaSide side) const { return SyncPal::updateTree(side); }
                std::shared_ptr<Snapshot> snapshot(ReplicaSide side, bool copy = false) const {
                    return SyncPal::snapshot(side, copy);
                }
                std::shared_ptr<FSOperationSet> operationSet(ReplicaSide side) const { return SyncPal::operationSet(side); }


            private:
                void createWorkers(const std::chrono::seconds& startDelay = std::chrono::seconds(0)) override;
        };
};

} // namespace KDC
