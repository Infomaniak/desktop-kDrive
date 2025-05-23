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

namespace KDC::testhelpers {

inline const SyncPath localTestDirPath() {
    static SyncPath testDirPath;
    if (!testDirPath.empty()) return testDirPath;
    const std::wstring targetDirName = L"test_ci";

    testDirPath = SyncPath();

    // Helper lambda to recursively search downward
    SyncPath previousPath;
    auto findDownward = [&](const std::filesystem::path& base) -> std::optional<std::filesystem::path> {
        auto it = std::filesystem::recursive_directory_iterator(base);
        for (auto const& entry: it) {
            if (entry.is_directory() && entry.path().filename() == targetDirName) {
                return entry.path();
            }
            if (entry.path() == previousPath) {
                it.disable_recursion_pending();
            }
        }
        previousPath = base;
        return std::nullopt;
    };

    // Look downward from current and then upward
    std::filesystem::path scanPath = std::filesystem::current_path();

    while (true) {
        // First check direct subdirectory
        std::filesystem::path direct = scanPath / targetDirName;
        if (std::filesystem::exists(direct) && std::filesystem::is_directory(direct)) {
            testDirPath = direct;
            LOGW_DEBUG(Log::instance()->getLogger(), L"test_ci dir found at -> " << Utility::formatSyncPath(testDirPath));
            return testDirPath;
        }

        // Then search downward from here
        auto found = findDownward(scanPath);
        if (found.has_value()) {
            testDirPath = found.value();
            break;
        }

        // Go up one level
        if (scanPath.has_parent_path()) {
            scanPath = scanPath.parent_path();
        } else {
            assert(false && "test_ci folder not found");
            LOG_FATAL(Log::instance()->getLogger(), "test_ci dir not found");
            break; // Reached the root, give up
        }
    }
    LOGW_DEBUG(Log::instance()->getLogger(), L"test_ci dir found at -> " << Utility::formatSyncPath(testDirPath));
    return testDirPath;
}

const SyncTime defaultTime = std::time(nullptr);
constexpr int64_t defaultFileSize = 123;
constexpr int64_t defaultDirSize = 0;

SyncName makeNfdSyncName();
SyncName makeNfcSyncName();

std::string loadEnvVariable(const std::string& key, bool mandatory);
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
/**
 * @brief Generate test files.
 * @param dirPath Directory in which the files will be created.
 * @param size The size of each file in MB.
 * @param count The number of file to generate.
 */
void generateBigFiles(const SyncPath& dirPath, uint16_t size, uint16_t count);

void setModificationDate(const SyncPath& path, const std::chrono::time_point<std::chrono::system_clock>& timePoint);

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
} // namespace KDC::testhelpers
