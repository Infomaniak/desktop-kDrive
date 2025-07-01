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
#include "io/filestat.h"

#include <config.h>

#include <chrono>

namespace KDC::testhelpers {
std::string loadEnvVariable(const std::string &key, bool mandatory);

inline const SyncPath localTestDirPath() {
    static SyncPath localTestDirPath;
    if (!localTestDirPath.empty()) return localTestDirPath;
    localTestDirPath = Utility::s2ws(loadEnvVariable("KDRIVE_TEST_CI_LOCAL_PATH", true));
    LOGW_INFO(Log::instance()->getLogger(), L"test_ci dir is: " << Utility::formatSyncPath(localTestDirPath));
    return localTestDirPath;
}

const SyncTime defaultTime = std::time(nullptr);
constexpr int64_t defaultFileSize = 123;
constexpr int64_t defaultDirSize = 0;

SyncName makeNfdSyncName();
SyncName makeNfcSyncName();

inline bool isRunningOnCI(bool print = true) {
    static const bool isRunningOnCI = !loadEnvVariable("KDRIVE_TEST_CI_RUNNING_ON_CI", false).empty();
    if (print && !isRunningOnCI) {
        std::cout << " (Skipped, CI only test)"; // This will show up in the test output -> KDC::TestXXX::testxxx (Skipped, CI
                                                 // only test) :  OK
    }
    return isRunningOnCI;
}

inline bool isExtendedTest(bool print = true) {
    static const bool isExtended = !loadEnvVariable("KDRIVE_TEST_CI_EXTENDED_TEST", false).empty();
    if (print && !isExtended) {
        std::cout << " (Skipped, extended test)"; // This will show up in the test output -> KDC::TestXXX::testxxx (Skipped,
                                                  // extended test) :  OK
    }
    return isExtended;
}

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
            local8MoPartitionPath = loadEnvVariable("KDRIVE_TEST_CI_8MO_PARTITION_PATH", isExtendedTest(false));
        }
};

void generateOrEditTestFile(const SyncPath &path);
/**
 * @brief Generate test files.
 * @param dirPath Directory in which the files will be created.
 * @param size The size of each file in MB.
 * @param count The number of file to generate.
 */
void generateBigFiles(const SyncPath &dirPath, uint16_t size, uint16_t count);
SyncPath generateBigFile(const SyncPath &dirPath, uint16_t size);

void setModificationDate(const SyncPath &path, const std::chrono::time_point<std::chrono::system_clock> &timePoint);

} // namespace KDC::testhelpers
