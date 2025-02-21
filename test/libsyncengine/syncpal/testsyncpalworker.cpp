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
#include "libsyncengine/jobs/network/API_v2/movejob.h"
#include "libcommon/keychainmanager/keychainmanager.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/network/proxy.h"
#include "test_utility/testhelpers.h"
#include "libsyncengine/syncpal/mocksyncpal.h"
#include <cstdlib>

using namespace CppUnit;

namespace KDC {

void TestSyncPalWorker::setUp() {
    const testhelpers::TestVariables testVariables;

    const std::string localPathStr = _localTempDir.path().string();
    // Insert api token into keystore
    ApiToken apiToken;
    apiToken.setAccessToken(testVariables.apiToken);

    std::string keychainKey("123");
    KeyChainManager::instance(true);
    KeyChainManager::instance()->writeToken(keychainKey, apiToken.reconstructJsonString());

    // Create parmsDb
    bool alreadyExists = false;
    std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExists, true);
    ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);

    // Insert user, account, drive & sync
    int userId = atoi(testVariables.userId.c_str());
    User user(1, userId, keychainKey);
    ParmsDb::instance()->insertUser(user);

    int accountId(atoi(testVariables.accountId.c_str()));
    Account account(1, accountId, user.dbId());
    ParmsDb::instance()->insertAccount(account);

    _driveDbId = 1;
    int driveId = atoi(testVariables.driveId.c_str());
    Drive drive(_driveDbId, driveId, account.dbId(), std::string(), 0, std::string());
    ParmsDb::instance()->insertDrive(drive);

    _localPath = localPathStr;
    _remotePath = testVariables.remotePath;
    _sync = Sync(1, drive.dbId(), _localPath, _remotePath);
    ParmsDb::instance()->insertSync(_sync);

    // Setup proxy
    Parameters parameters;
    bool found;
    if (ParmsDb::instance()->selectParameters(parameters, found) && found) {
        Proxy::instance(parameters.proxyConfig());
    }
    _testEnded = false;
}

void TestSyncPalWorker::tearDown() {
    // Stop SyncPal and delete sync DB
    ParmsDb::instance()->close();
    if (_syncPal) {
        _syncPal->stop(false, true, true);
    }
    ParmsDb::reset();
    _testEnded = true;

    for (auto& thread: _runningThreads) {
        if (thread->joinable()) {
            thread->join();
        }
    }
}

void TestSyncPalWorker::setUpTestInternalPause(const std::chrono::steady_clock::duration& longPollDuration) {
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

    // Wait for SyncPal to start
    TimeoutHelper timeoutHelper(std::chrono::seconds(4), std::chrono::milliseconds(5));
    while (_syncPal->status() != SyncStatus::Idle || !mockLfso->isRunning() || !mockRfso->isRunning()) {
        CPPUNIT_ASSERT(!timeoutHelper.timedOut());
    }
}

void TestSyncPalWorker::testInternalPause1() {
    setUpTestInternalPause(std::chrono::seconds(1));

    // Constants
    constexpr auto longPollDuration = std::chrono::seconds(1);
    constexpr auto testTimeout = std::chrono::seconds(10);
    constexpr auto loopWait = std::chrono::milliseconds(5);
    const auto mockSyncPal = std::dynamic_pointer_cast<MockSyncPal>(_syncPal);
    const auto mockLfso = mockSyncPal->getMockLFSOWorker();
    const auto mockRfso = mockSyncPal->getMockRFSOWorker();
    const auto syncpalWorker = mockSyncPal->getSyncPalWorker();

    // Simulate a network error in RFSO
    mockRfso->setNetworkAvailability(false);

    // Ensure SyncPal pauses immediately
    TimeoutHelper timeoutHelper(testTimeout, loopWait);
    while (!syncpalWorker->_pauseAsked) {
        CPPUNIT_ASSERT(!timeoutHelper.timedOut());
    }
    while (!syncpalWorker->_isPaused) {
        CPPUNIT_ASSERT(!timeoutHelper.timedOut());
    }
    CPPUNIT_ASSERT_EQUAL(SyncStatus::Paused, _syncPal->status());

    // Ensure automatic restart & re-pausing without starting a new sync cycle while network is down even if an event is pending
    mockLfso->simulateFSEvent();
    CPPUNIT_ASSERT(mockLfso->snapshot()->updated()); // Ensure the event is pending

    timeoutHelper.restart();
    while (!syncpalWorker->_unpauseAsked) { // Wait for the automatic restart
        CPPUNIT_ASSERT_EQUAL(SyncStep::Idle, syncpalWorker->step());
        CPPUNIT_ASSERT(!timeoutHelper.timedOut());
    }
    while (!syncpalWorker->_isPaused) { // Wait for the automatic re-pause
        CPPUNIT_ASSERT_EQUAL(SyncStep::Idle, syncpalWorker->step());
        CPPUNIT_ASSERT(!timeoutHelper.timedOut());
    }
    CPPUNIT_ASSERT(mockLfso->snapshot()->updated()); // Ensure the event is still pending

    // Restore network and check that SyncPal resumes and propagates the event
    mockRfso->setNetworkAvailability(true);

    timeoutHelper.restart();
    while (syncpalWorker->_isPaused) {
        CPPUNIT_ASSERT(!timeoutHelper.timedOut());
    }
    while (syncpalWorker->step() == SyncStep::Idle) { // Wait for a new sync to start
        CPPUNIT_ASSERT(!timeoutHelper.timedOut());
    }
    while (syncpalWorker->step() != SyncStep::Idle) { // Wait for the sync to finish
        CPPUNIT_ASSERT(!timeoutHelper.timedOut());
    }

    CPPUNIT_ASSERT(!mockLfso->snapshot()->updated()); // Ensure the event was propagated
}

void TestSyncPalWorker::testInternalPause3() {
    setUpTestInternalPause(std::chrono::seconds(3));

    // Constants
    constexpr auto longPollDuration = std::chrono::seconds(1);
    constexpr auto testTimeout = std::chrono::seconds(20);
    constexpr auto loopWait = std::chrono::milliseconds(5);
    const auto mockSyncPal = std::dynamic_pointer_cast<MockSyncPal>(_syncPal);
    const auto mockLfso = mockSyncPal->getMockLFSOWorker();
    const auto mockRfso = mockSyncPal->getMockRFSOWorker();
    const auto syncpalWorker = mockSyncPal->getSyncPalWorker();

    // Setup custom worker
    const auto mockExecutorWorker = mockSyncPal->getMockExecutorWorker();
    std::atomic_bool mockExecutorWorkerWaiting = false;
    std::atomic<ExitInfo> mockExecutorWorkerExitInfo;
    mockExecutorWorker->setMockExecuteCallback([&mockExecutorWorkerWaiting, &mockExecutorWorkerExitInfo, this]() -> ExitInfo {
        if (_testEnded) {
            return ExitInfo(ExitCode::Ok, ExitCause::Unknown);
        }
        mockExecutorWorkerWaiting = true;
        while (mockExecutorWorkerExitInfo.load() == ExitInfo(ExitCode::Unknown, ExitCause::Unknown)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        auto returnValue = mockExecutorWorkerExitInfo.load();
        mockExecutorWorkerExitInfo = ExitInfo(ExitCode::Unknown, ExitCause::Unknown);
        mockExecutorWorkerWaiting = false;
        return returnValue;
    });

    // Simulate a FileSysem event to start a sync cycle
    mockLfso->simulateFSEvent();
    CPPUNIT_ASSERT(mockLfso->snapshot()->updated()); // Ensure the event was reaed from the snapshot

    // Check that if a worker fails due to a network error, the synchronization is paused and then resumed at the same step.
    TimeoutHelper timeoutHelper(testTimeout, loopWait);
    while (!mockExecutorWorkerWaiting) {
        CPPUNIT_ASSERT(!timeoutHelper.timedOut());
    }

    CPPUNIT_ASSERT(!mockLfso->snapshot()->updated()); // Ensure the event was reaed from the snapshot
    mockLfso->simulateFSEvent(); // Simulate a new event to ensure a new sync cycle will be started when this one completes

    mockExecutorWorkerExitInfo =
            ExitInfo(ExitCode::NetworkError, ExitCause::Unknown); // Simulate a network error in the executor worker
    while (!syncpalWorker->_pauseAsked && !syncpalWorker->_isPaused) {
        CPPUNIT_ASSERT_EQUAL(SyncStep::Propagation2, syncpalWorker->step());
        CPPUNIT_ASSERT(!timeoutHelper.timedOut());
    }
    while (!syncpalWorker->_isPaused) {
        CPPUNIT_ASSERT_EQUAL(SyncStep::Propagation2, syncpalWorker->step());
        CPPUNIT_ASSERT(!timeoutHelper.timedOut());
    }
    CPPUNIT_ASSERT_EQUAL(SyncStatus::Paused, _syncPal->status());
    mockExecutorWorkerExitInfo = ExitCode::Ok;

    while (!syncpalWorker->_unpauseAsked) { // Wait for the automatic restart
        CPPUNIT_ASSERT_EQUAL(SyncStep::Propagation2, syncpalWorker->step());
        CPPUNIT_ASSERT(!timeoutHelper.timedOut());
    }

    while (syncpalWorker->step() != SyncStep::Idle) { // Wait for the sync to finish
        CPPUNIT_ASSERT(!timeoutHelper.timedOut());
    }
    while (syncpalWorker->step() == SyncStep::Idle) { // Wait for a new sync to start
        CPPUNIT_ASSERT(!timeoutHelper.timedOut());
    }
    while (syncpalWorker->step() != SyncStep::Idle) { // Wait for the sync to finish
        CPPUNIT_ASSERT(!timeoutHelper.timedOut());
    }


    /*
    // Setup a custom PlatformInconsistencyCheckerWorker to stall the synchronization until the test is ready
    const auto mockPlatformInconsistencyCheckerWorker = mockSyncPal->getMockPlatformInconsistencyCheckerWorker();
    CPPUNIT_ASSERT(mockPlatformInconsistencyCheckerWorker);



    // Simulate a local change
    mockSyncPal->getMockLFSOWorker()->simulateFSEvent();

    TimeoutHelper timeOutHelper(std::chrono::seconds(5));
    while (!wait) { // Wait for the synchronization to start and reach the PlatformInconsistencyCheckerWorker
        CPPUNIT_ASSERT_MESSAGE(
                "SyncPalWorker did not reach the PlatformInconsistencyCheckerWorker after " + timeOutHelper.elapsedStr(),
                !timeOutHelper.timedOut());
    }

    // Simulate a network error in rfso (should be detected when the long poll fails / 2s in
    // MockRemoteFileSystemObserverWorker)
    mockRfso->setNetworkAvailability(false);

    timeOutHelper.restart(std::chrono::seconds(2));
    while (!syncpalWorker->_pauseAsked) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        CPPUNIT_ASSERT_MESSAGE("SyncPalWorker did not ask pause after " + timeOutHelper.elapsedStr(), !timeOutHelper.timedOut());
    }
    CPPUNIT_ASSERT_EQUAL(SyncStatus::Running, _syncPal->status());

    mockRfso->setNetworkAvailability(true);

    timeOutHelper.restart(std::chrono::seconds(3));
    while (!syncpalWorker->_unpauseAsked) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        CPPUNIT_ASSERT_MESSAGE("SyncPalWorker should not pause until it is idle or just before the executorWorker.",
                               !syncpalWorker->_isPaused);
        CPPUNIT_ASSERT_MESSAGE("SyncStatus should be Running when pause is asked but not yet done",
                               _syncPal->status() == SyncStatus::Running);
        CPPUNIT_ASSERT_MESSAGE("SyncPalWorker did not ask unpause after " + timeOutHelper.elapsedStr(),
                               !timeOutHelper.timedOut());
    }

    wait = false; // Let the PlatformInconsistencyCheckerWorker and the synchronization finish
    timeOutHelper.restart(std::chrono::seconds(3));
    while (_syncPal->status() != SyncStatus::Idle) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        CPPUNIT_ASSERT_MESSAGE("SyncPalWorker did not reach the Idle state after " + timeOutHelper.elapsedStr(),
                               !timeOutHelper.timedOut());
    }*/
}


} // namespace KDC
