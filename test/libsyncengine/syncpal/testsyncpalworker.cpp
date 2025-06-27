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

#include "testsyncpalworker.h"
#include "libcommon/keychainmanager/keychainmanager.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/network/proxy.h"
#include "libsyncengine/jobs/network/API_v2/movejob.h"
#include "mocks/libcommonserver/db/mockdb.h"
#include "jobs/jobmanager.h"

#include "test_utility/testhelpers.h"
#include "test_utility/timeouthelper.h"

#include <cstdlib>
#include <atomic>


using namespace CppUnit;

namespace KDC {

void TestSyncPalWorker::setUp() {
    const testhelpers::TestVariables testVariables;

    const std::string localPathStr = _localTempDir.path().string();
    // Insert api token into keystore
    ApiToken apiToken;
    apiToken.setAccessToken(testVariables.apiToken);

    std::string keychainKey("123");
    (void) KeyChainManager::instance(true);
    (void) KeyChainManager::instance()->writeToken(keychainKey, apiToken.reconstructJsonString());

    // Create parmsDb
    bool alreadyExists = false;
    std::filesystem::path parmsDbPath = MockDb::makeDbName(alreadyExists);
    ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);

    // Insert user, account, drive & sync
    int userId = atoi(testVariables.userId.c_str());
    User user(1, userId, keychainKey);
    (void) ParmsDb::instance()->insertUser(user);

    int accountId(atoi(testVariables.accountId.c_str()));
    Account account(1, accountId, user.dbId());
    (void) ParmsDb::instance()->insertAccount(account);

    _driveDbId = 1;
    int driveId = atoi(testVariables.driveId.c_str());
    Drive drive(_driveDbId, driveId, account.dbId(), std::string(), 0, std::string());
    (void) ParmsDb::instance()->insertDrive(drive);

    _localPath = localPathStr;
    _remotePath = testVariables.remotePath;
    _sync = Sync(1, drive.dbId(), _localPath, "", _remotePath);
    (void) ParmsDb::instance()->insertSync(_sync);

    // Setup proxy
    Parameters parameters;
    bool found = false;
    if (ParmsDb::instance()->selectParameters(parameters, found) && found) {
        Proxy::instance(parameters.proxyConfig());
    }
    _testEnded = false;
}

void TestSyncPalWorker::tearDown() {
    _testEnded = true;
    // Stop SyncPal and delete sync DB
    if (_syncPal) {
        _syncPal->stop(false, true, true);
    }
    ParmsDb::instance()->close();
    ParmsDb::reset();

    for (auto &thread: _runningThreads) {
        if (thread->joinable()) {
            thread->join();
        }
    }
    JobManager::instance()->stop();
    JobManager::instance()->clear();
}

void TestSyncPalWorker::setUpTestInternalPause(const std::chrono::steady_clock::duration &longPollDuration) {
    // Setup SyncPal
    _syncPal = std::make_shared<MockSyncPal>(std::make_shared<VfsOff>(VfsSetupParams(Log::instance()->getLogger())), _sync.dbId(),
                                             KDRIVE_VERSION_STRING);
    _syncPal->start(std::chrono::seconds(0));

    auto mockSyncPal = std::dynamic_pointer_cast<MockSyncPal>(_syncPal);
    CPPUNIT_ASSERT(mockSyncPal);

    // Simulate appServer restarter
    auto restarter = [this](std::shared_ptr<SyncPal> syncPtr) {
        CPPUNIT_ASSERT(syncPtr);
        while (!_testEnded) {
            if (syncPtr->isPaused() || syncPtr->pauseAsked()) {
                if (syncPtr->pauseTime() + std::chrono::seconds(2) < std::chrono::steady_clock::now()) {
                    syncPtr->unpause();
                }
            }
            Utility::msleep(100);
        }
    };
    _runningThreads.emplace_back(std::make_shared<std::thread>(restarter, _syncPal));

    // Retrieve SyncPal components
    auto mockLfso = mockSyncPal->getMockLFSOWorker();
    auto mockRfso = mockSyncPal->getMockRFSOWorker();

    mockRfso->setLongPollDuration(longPollDuration);
    mockRfso->stop(); // Will be restarted by SyncPal

    // Let the first sync finish
    CPPUNIT_ASSERT(TimeoutHelper::waitFor([this]() { return _syncPal->step() == SyncStep::Propagation2; },
                                          std::chrono::seconds(60), std::chrono::milliseconds(5)));

    CPPUNIT_ASSERT(TimeoutHelper::waitFor([this]() { return _syncPal->step() == SyncStep::Idle; }, std::chrono::seconds(20),
                                          std::chrono::milliseconds(5)));
}

void TestSyncPalWorker::testInternalPause1() {
    setUpTestInternalPause(std::chrono::seconds(1));

    // Constants
    constexpr auto testTimeout = std::chrono::seconds(10);
    constexpr auto loopWait = std::chrono::milliseconds(5);
    const auto mockSyncPal = std::dynamic_pointer_cast<MockSyncPal>(_syncPal);
    const auto mockLfso = mockSyncPal->getMockLFSOWorker();
    const auto mockRfso = mockSyncPal->getMockRFSOWorker();
    const auto syncpalWorker = mockSyncPal->getSyncPalWorker();

    // Simulate a network error in RFSO
    CPPUNIT_ASSERT_EQUAL(SyncStep::Idle, syncpalWorker->step());
    mockRfso->setNetworkAvailability(false);

    // Ensure SyncPal pauses immediately
    CPPUNIT_ASSERT(TimeoutHelper::waitFor([&syncpalWorker]() { return syncpalWorker->isPaused(); }, testTimeout, loopWait));
    CPPUNIT_ASSERT_EQUAL(SyncStatus::Paused, _syncPal->status());

    // Ensure automatic restart & re-pausing without starting a new sync cycle while network is down even if an event is pending
    CPPUNIT_ASSERT_EQUAL(SyncStep::Idle, syncpalWorker->step());
    mockLfso->simulateFSEvent();
    CPPUNIT_ASSERT(mockLfso->liveSnapshot().updated()); // Ensure the event is pending

    CPPUNIT_ASSERT(TimeoutHelper::waitFor( // wait for automatic restart
            [&syncpalWorker]() { return (syncpalWorker->unpauseAsked() || !syncpalWorker->isPaused()); },
            [&syncpalWorker]() { CPPUNIT_ASSERT_EQUAL(SyncStep::Idle, syncpalWorker->step()); }, testTimeout, loopWait));

    CPPUNIT_ASSERT(TimeoutHelper::waitFor( // Wait for the automatic re-pause
            [&syncpalWorker]() { return syncpalWorker->pauseAsked() || syncpalWorker->isPaused(); },
            [&syncpalWorker]() { CPPUNIT_ASSERT_EQUAL(SyncStep::Idle, syncpalWorker->step()); }, testTimeout, loopWait));

    CPPUNIT_ASSERT(mockLfso->liveSnapshot().updated()); // Ensure the event is still pending

    // Restore network and check that SyncPal resumes and propagates the event
    mockRfso->setNetworkAvailability(true);

    CPPUNIT_ASSERT(TimeoutHelper::waitFor(
            [&syncpalWorker]() { return syncpalWorker->unpauseAsked() || !syncpalWorker->isPaused(); }, testTimeout, loopWait));
    // Wait for a new sync to start
    CPPUNIT_ASSERT(TimeoutHelper::waitFor([this]() { return _syncPal->step() != SyncStep::Idle; }, testTimeout, loopWait));
    // Wait for the sync to finish
    CPPUNIT_ASSERT(TimeoutHelper::waitFor([this]() { return _syncPal->step() == SyncStep::Idle; }, testTimeout, loopWait));

    // Ensure the event was propagated
    CPPUNIT_ASSERT(!mockLfso->liveSnapshot().updated());
}

void TestSyncPalWorker::testInternalPause2() {
    setUpTestInternalPause(std::chrono::seconds(1));

    // Constants
    constexpr auto testTimeout = std::chrono::seconds(20);
    constexpr auto loopWait = std::chrono::milliseconds(5);
    const auto mockSyncPal = std::dynamic_pointer_cast<MockSyncPal>(_syncPal);
    const auto mockLfso = mockSyncPal->getMockLFSOWorker();
    const auto mockRfso = mockSyncPal->getMockRFSOWorker();
    const auto syncpalWorker = mockSyncPal->getSyncPalWorker();

    // Setup custom worker
    // It will be used to simulate a long operation
    const auto mockPlatformInconsistencyChecker = mockSyncPal->getMockPlatformInconsistencyCheckerWorker();
    std::atomic_bool mockPlatformInconsistencyCheckerWaiting = false;
    mockPlatformInconsistencyChecker->setMockExecuteCallback(
            [loopWait, &mockPlatformInconsistencyCheckerWaiting, this]() -> ExitInfo {
                if (_testEnded) {
                    return ExitCode::Ok;
                }
                mockPlatformInconsistencyCheckerWaiting.store(true);
                while (!_testEnded && mockPlatformInconsistencyCheckerWaiting.load()) {
                    std::this_thread::sleep_for(loopWait);
                }

                return ExitCode::Ok;
            });

    // Simulate a FileSysem event to start a sync cycle
    mockLfso->simulateFSEvent();
    CPPUNIT_ASSERT(mockLfso->liveSnapshot().updated());

    // Wait for the sync to reach PlatformInconsistencyCheckerWorker
    CPPUNIT_ASSERT(TimeoutHelper::waitFor(
            [&mockPlatformInconsistencyCheckerWaiting]() { return mockPlatformInconsistencyCheckerWaiting.load(); }, testTimeout,
            loopWait));

    // Simulate a network error in RFSO
    mockRfso->setNetworkAvailability(false);

    // Ensure SyncPal asks for a pause
    CPPUNIT_ASSERT(TimeoutHelper::waitFor([&syncpalWorker]() { return syncpalWorker->pauseAsked(); }, testTimeout, loopWait));

    // Ensure automatic restart & re-(ask)pausing while the current worker is still running
    CPPUNIT_ASSERT(TimeoutHelper::waitFor( // Wait for the automatic restart
            [&syncpalWorker]() { return syncpalWorker->unpauseAsked() || !syncpalWorker->isPaused(); },
            [&syncpalWorker, this]() {
                CPPUNIT_ASSERT_EQUAL(SyncStep::Reconciliation1, syncpalWorker->step());
                CPPUNIT_ASSERT_EQUAL(SyncStatus::Running, _syncPal->status());
                CPPUNIT_ASSERT(!syncpalWorker->isPaused());
            },
            testTimeout, loopWait));


    CPPUNIT_ASSERT(TimeoutHelper::waitFor( // Wait for the re-pause because the network is still down
            [&syncpalWorker]() { return syncpalWorker->pauseAsked() || syncpalWorker->isPaused(); },
            [&syncpalWorker, this]() {
                CPPUNIT_ASSERT_EQUAL(SyncStep::Reconciliation1, syncpalWorker->step());
                CPPUNIT_ASSERT_EQUAL(SyncStatus::Running, _syncPal->status());
                CPPUNIT_ASSERT(!syncpalWorker->isPaused());
            },
            testTimeout, loopWait));

    // Unlocked the PlatformInconsistencyCheckerWorker
    mockPlatformInconsistencyCheckerWaiting.store(false);

    // Wait for the sync to reach the propagation2 step where it can pause.
    CPPUNIT_ASSERT(TimeoutHelper::waitFor( // Wait for the sync to finish
            [&syncpalWorker]() { return syncpalWorker->step() == SyncStep::Propagation2; },
            [this]() { CPPUNIT_ASSERT_EQUAL(SyncStatus::Running, _syncPal->status()); }, testTimeout, loopWait));

    // Ensure the sync is paused when the propagation2 step is reached
    CPPUNIT_ASSERT(TimeoutHelper::waitFor( // Wait for the sync to finish
            [&syncpalWorker]() { return syncpalWorker->pauseAsked() || syncpalWorker->isPaused(); },
            [&syncpalWorker]() { CPPUNIT_ASSERT_EQUAL(SyncStep::Propagation2, syncpalWorker->step()); }, testTimeout, loopWait));

    mockRfso->setNetworkAvailability(true);

    // Wait for the sync to finish.
    CPPUNIT_ASSERT(TimeoutHelper::waitFor( // Wait for the sync to finish
            [&syncpalWorker]() { return syncpalWorker->step() == SyncStep::Idle; }, testTimeout, loopWait));
}

void TestSyncPalWorker::testInternalPause3() {
    setUpTestInternalPause(std::chrono::seconds(1));

    // Constants
    constexpr auto testTimeout = std::chrono::seconds(60);
    constexpr auto loopWait = std::chrono::milliseconds(5);
    const auto mockSyncPal = std::dynamic_pointer_cast<MockSyncPal>(_syncPal);
    const auto mockLfso = mockSyncPal->getMockLFSOWorker();
    const auto mockRfso = mockSyncPal->getMockRFSOWorker();
    const auto syncpalWorker = mockSyncPal->getSyncPalWorker();
    const auto mockExecutorWorker = mockSyncPal->getMockExecutorWorker();

    // Setup custom worker
    std::atomic_bool mockExecutorWorkerWaiting = false;
    ExitInfo mockExecutorWorkerExitInfo;
    mockExecutorWorker->setMockExecuteCallback(
            [&mockExecutorWorkerWaiting, &mockExecutorWorkerExitInfo, this, loopWait]() -> ExitInfo {
                if (_testEnded) {
                    return ExitInfo(ExitCode::Ok, ExitCause::Unknown);
                }
                mockExecutorWorkerWaiting = true;
                while (!_testEnded && mockExecutorWorkerWaiting.load()) {
                    std::this_thread::sleep_for(loopWait);
                }
                return mockExecutorWorkerExitInfo;
            });

    // Simulate a FileSysem event to start a sync cycle
    mockLfso->simulateFSEvent();
    CPPUNIT_ASSERT(mockLfso->liveSnapshot().updated());

    // Check that if a worker fails due to a network error, the synchronization is paused and then resumed at the same step.
    // Wait for the sync to reach ExecutorWorker
    CPPUNIT_ASSERT(TimeoutHelper::waitFor([&mockExecutorWorkerWaiting]() { return mockExecutorWorkerWaiting.load(); },
                                          testTimeout, loopWait));

    CPPUNIT_ASSERT(!mockLfso->liveSnapshot().updated());
    mockLfso->simulateFSEvent(); // Simulate a new event to check that a new sync cycle will be started when this one
                                 // completes
    CPPUNIT_ASSERT(mockLfso->liveSnapshot().updated());

    mockExecutorWorkerExitInfo =
            ExitInfo(ExitCode::NetworkError, ExitCause::Unknown); // Simulate a network error in the executor worker
    mockExecutorWorkerWaiting = false; // Unlock the executor worker

    CPPUNIT_ASSERT(TimeoutHelper::waitFor(
            [&syncpalWorker]() { return syncpalWorker->pauseAsked() || syncpalWorker->isPaused(); },
            [&syncpalWorker]() { CPPUNIT_ASSERT_EQUAL(SyncStep::Propagation2, syncpalWorker->step()); }, testTimeout, loopWait));

    CPPUNIT_ASSERT_EQUAL(SyncStatus::Paused, _syncPal->status());

    // Wait for the sync to finish
    mockExecutorWorkerExitInfo = ExitCode::Ok;
    CPPUNIT_ASSERT(TimeoutHelper::waitFor([&syncpalWorker]() { return syncpalWorker->step() == SyncStep::Idle; },
                                          [&mockExecutorWorkerWaiting]() { mockExecutorWorkerWaiting = false; }, testTimeout,
                                          loopWait));
    CPPUNIT_ASSERT(mockLfso->liveSnapshot().updated());

    // Wait for a new sync to start
    CPPUNIT_ASSERT(TimeoutHelper::waitFor([&syncpalWorker]() { return syncpalWorker->step() != SyncStep::Idle; }, testTimeout,
                                          loopWait));
    // Wait for the sync to finish
    CPPUNIT_ASSERT(TimeoutHelper::waitFor([&syncpalWorker]() { return syncpalWorker->step() == SyncStep::Idle; },
                                          [&mockExecutorWorkerWaiting]() { mockExecutorWorkerWaiting = false; }, testTimeout,
                                          loopWait));
    CPPUNIT_ASSERT(!mockLfso->liveSnapshot().updated());
}


// Mocks
std::shared_ptr<TestSyncPalWorker::MockLFSO> TestSyncPalWorker::MockSyncPal::getMockLFSOWorker() {
    return std::dynamic_pointer_cast<MockLFSO>(_localFSObserverWorker);
}

std::shared_ptr<TestSyncPalWorker::MockRemoteFileSystemObserverWorker> TestSyncPalWorker::MockSyncPal::getMockRFSOWorker() {
    return std::dynamic_pointer_cast<TestSyncPalWorker::MockRemoteFileSystemObserverWorker>(_remoteFSObserverWorker);
}

std::shared_ptr<TestSyncPalWorker::MockPlatformInconsistencyCheckerWorker>
TestSyncPalWorker::MockSyncPal::getMockPlatformInconsistencyCheckerWorker() {
    return std::dynamic_pointer_cast<MockPlatformInconsistencyCheckerWorker>(_platformInconsistencyCheckerWorker);
}

std::shared_ptr<TestSyncPalWorker::MockExecutorWorker> TestSyncPalWorker::MockSyncPal::getMockExecutorWorker() {
    return std::static_pointer_cast<MockExecutorWorker>(_executorWorker);
}

void TestSyncPalWorker::MockSyncPal::createWorkers(const std::chrono::seconds &startDelay) {
    _localFSObserverWorker = std::make_shared<MockLFSO>(shared_from_this(), "Mock Local File System Observer", "M_LFSO");
    _remoteFSObserverWorker = std::make_shared<MockRemoteFileSystemObserverWorker>(shared_from_this(),
                                                                                   "Mock Remote File System Observer", "M_RFSO");
    _computeFSOperationsWorker = std::make_shared<MockComputeFSOperationWorkerTestSyncPalWorker>(
            shared_from_this(), "Mock Compute FS Operations", "M_COOP");
    _localUpdateTreeWorker =
            std::make_shared<MockUpdateTreeWorker>(shared_from_this(), "Mock Local Tree Updater", "M_LTRU", ReplicaSide::Local);
    _remoteUpdateTreeWorker =
            std::make_shared<MockUpdateTreeWorker>(shared_from_this(), "Mock Remote Tree Updater", "M_RTRU", ReplicaSide::Remote);
    _platformInconsistencyCheckerWorker = std::make_shared<MockPlatformInconsistencyCheckerWorker>(
            shared_from_this(), "Mock Platform Inconsistency Checker", "M_PICH");
    _conflictFinderWorker = std::make_shared<MockConflictFinderWorker>(shared_from_this(), "Mock Conflict Finder", "M_COFD");
    _conflictResolverWorker =
            std::make_shared<MockConflictResolverWorker>(shared_from_this(), "Mock Conflict Resolver", "M_CORE");
    _operationsGeneratorWorker =
            std::make_shared<MockOperationGeneratorWorker>(shared_from_this(), "Mock Operation Generator", "M_OPGE");
    _operationsSorterWorker = std::make_shared<MockOperationSorterWorker>(shared_from_this(), "Mock Operation Sorter", "M_OPSO");
    _executorWorker = std::make_shared<MockExecutorWorker>(shared_from_this(), "Mock Executor", "M_EXEC");
    _syncPalWorker = std::make_shared<SyncPalWorker>(shared_from_this(), "Mock Main", "M_MAIN", startDelay);

    _tmpBlacklistManager = std::make_shared<TmpBlacklistManager>(shared_from_this());
}

ExitInfo TestSyncPalWorker::MockRemoteFileSystemObserverWorker::sendLongPoll(bool &changes) {
    using namespace std::chrono;
    changes = false;
    if (!_networkAvailable) {
        return ExitCode::NetworkError;
    }

    const auto start = steady_clock::now();
    while (_networkAvailable && !stopAsked() && start + _longPollDuration < steady_clock::now()) {
        Utility::msleep(100);
    }
    if (!_networkAvailable) {
        return ExitCode::NetworkError;
    }
    return ExitCode::Ok;
}

ExitInfo TestSyncPalWorker::MockRemoteFileSystemObserverWorker::generateInitialSnapshot() {
    if (_networkAvailable) {
        return RemoteFileSystemObserverWorker::generateInitialSnapshot();
    } else {
        _liveSnapshot.init();
        invalidateSnapshot();
        _updating = false;
        return ExitCode::NetworkError;
    }
}
} // namespace KDC
