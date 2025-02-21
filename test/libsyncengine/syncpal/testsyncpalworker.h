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

using namespace CppUnit;

namespace KDC {

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
                TimeoutHelper(const std::chrono::steady_clock::duration& duration,
                              const std::chrono::steady_clock::duration& loopWait = std::chrono::milliseconds(0)) :
                    _duration(duration), _loopWait(loopWait) {
                    _start = std::chrono::steady_clock::now();
                }

                void restart(const std::chrono::steady_clock::duration& duration = std::chrono::milliseconds(0)) {
                    if (duration != std::chrono::milliseconds(0)) _duration = duration;
                    _start = std::chrono::steady_clock::now();
                }

                bool timedOut() {
                    bool result = (_start + _duration) < std::chrono::steady_clock::now();
                    if (!result && _loopWait != std::chrono::milliseconds(0)) {
                        Utility::msleep(std::chrono::duration_cast<std::chrono::milliseconds>(_loopWait).count());
                    }
                    return result;
                }
                uint64_t elapsedMs() {
                    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - _start)
                            .count();
                }
                std::string elapsedStr() { return std::to_string(elapsedMs()) + "ms"; }

                uint64_t remainingMs() {
                    return std::chrono::duration_cast<std::chrono::milliseconds>(_duration -
                                                                                 (std::chrono::steady_clock::now() - _start))
                            .count();
                }

            private:
                std::chrono::steady_clock::time_point _start;
                std::chrono::steady_clock::duration _duration;
                std::chrono::steady_clock::duration _loopWait;
        };
};

} // namespace KDC
