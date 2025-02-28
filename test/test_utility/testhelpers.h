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

#include "libcommon/utility/utility.h"
#include "libcommon/utility/types.h"
#include "libcommonserver/utility/utility.h"
#include "version.h"

#include <config.h>

#include <chrono>

namespace KDC {
namespace testhelpers {

const SyncPath localTestDirPath(KDC::Utility::s2ws(TEST_DIR) + L"/test_ci");
const SyncTime defaultTime = std::time(nullptr);
constexpr int64_t defaultFileSize = 1654788079;

SyncName makeNfdSyncName();
SyncName makeNfcSyncName();

std::string loadEnvVariable(const std::string &key, bool mandatory);
struct TestVariables {
        std::string userId;
        std::string accountId;
        std::string driveId;
        std::string remoteDirId;
        std::string remotePath;
        std::string apiToken;
        SyncPath local8MoPartitionPath;

        TestVariables() {
            userId = loadEnvVariable("KDRIVE_TEST_CI_USER_ID", true);
            accountId = loadEnvVariable("KDRIVE_TEST_CI_ACCOUNT_ID", true);
            driveId = loadEnvVariable("KDRIVE_TEST_CI_DRIVE_ID", true);
            remoteDirId = loadEnvVariable("KDRIVE_TEST_CI_REMOTE_DIR_ID", true);
            remotePath = loadEnvVariable("KDRIVE_TEST_CI_REMOTE_PATH", true);
            apiToken = loadEnvVariable("KDRIVE_TEST_CI_API_TOKEN", true);
            local8MoPartitionPath = loadEnvVariable("KDRIVE_TEST_CI_8MO_PARTITION_PATH", true);
        }
};

void generateOrEditTestFile(const SyncPath& path);
void setModificationDate(const SyncPath& path, const std::chrono::time_point<std::chrono::system_clock>& timePoint);
  
inline bool isRunningOnCI(bool print = true) {
    static const bool isRunningOnCI = !loadEnvVariable("KDRIVE_TEST_CI_RUNNING_ON_CI", false).empty();
    if (print && !isRunningOnCI) {
        std::cout << " (Skipped, CI only test)"; // This will show up in the test output -> KDC::TestXXX::testxxx (Skipped, CI only
                                               // test) :  OK
    }
    return isRunningOnCI;
}

inline bool isExtendedTest(bool print = true) {
    static const bool isExtended = !loadEnvVariable("KDRIVE_TEST_CI_EXTENDED_TEST", false).empty();
    if (print && !isExtended) {
        std::cout << " (Skipped, extended test)"; // This will show up in the test output -> KDC::TestXXX::testxxx (Skipped, extended
                                               // test) :  OK
    }
    return isExtended;
}
} // namespace testhelpers

class TimeoutHelper {
    public:
        /* Wait for condition() to return true,
         * Return false if timed out */
        static bool waitFor(std::function<bool()> condition, const std::chrono::steady_clock::duration& duration,
                            const std::chrono::steady_clock::duration& loopWait);

        /* Wait for condition() to return true, and run stateCheck() after each loop,
         * Return false if timed out */
        static bool waitFor(std::function<bool()> condition, std::function<void()> stateCheck,
                            const std::chrono::steady_clock::duration& duration,
                            const std::chrono::steady_clock::duration& loopWait);

        /* Waits for function() to return. If, when function() returns, more than the specified duration has elapsed, it will
         * return false. */
        template<typename T>
        static bool checkExecutionTime(std::function<T()> function, T& result,
                                       const std::chrono::steady_clock::duration& maximumDuration) {
            TimeoutHelper timeoutHelper(maximumDuration);
            result = function();
            return !timeoutHelper.timedOut();
        }

        /* Waits for function() to return. If, when function() returns, less than minimumDuration or more than maximumDuration has
         * elapsed, it will return false. */
        template<typename T>
        static bool checkExecutionTime(std::function<T()> function, T& result,
                                       const std::chrono::steady_clock::duration& minimumDuration,
                                       const std::chrono::steady_clock::duration& maximumDuration) {
            TimeoutHelper timeoutHelper(maximumDuration);
            result = function();
            if (timeoutHelper.elapsed() < minimumDuration) {
                std::cout << std::endl
                          << "Execution (" << timeoutHelper.elapsed().count() << "ms) time is less than minimum duration ("
                          << std::chrono::duration_cast<std::chrono::milliseconds>(minimumDuration).count() << "ms)" << std::endl;
                return false;
            }
            if (timeoutHelper.elapsed() > maximumDuration) {
                std::cout << std::endl
                          << "Execution (" << timeoutHelper.elapsed().count() << "ms) time is more than maximum duration ("
                          << std::chrono::duration_cast<std::chrono::milliseconds>(maximumDuration).count() << "ms)" << std::endl;
                return false;
            }

            return true;
        }

    private:
        TimeoutHelper(const std::chrono::steady_clock::duration& duration,
                      const std::chrono::steady_clock::duration& loopWait = std::chrono::milliseconds(0)) :
            _start(std::chrono::steady_clock::now()), _duration(duration), _loopWait(loopWait) {}

        bool timedOut();
        std::chrono::milliseconds elapsed() const {
            return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - _start);
        }
        std::chrono::steady_clock::time_point _start;
        std::chrono::steady_clock::duration _duration;
        std::chrono::steady_clock::duration _loopWait;
};
} // namespace KDC
