/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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
#include "utility/types.h"
#include "utility/utility.h"

#include <config.h>

namespace KDC::testhelpers {

const SyncPath localTestDirPath(KDC::Utility::s2ws(TEST_DIR) + L"/test_ci");
const SyncTime defaultTime = std::time(nullptr);
constexpr int64_t defaultFileSize = 1654788079;

SyncName makeNfdSyncName();
SyncName makeNfcSyncName();

std::string loadEnvVariable(const std::string &key);
struct TestVariables {
        std::string userId;
        std::string accountId;
        std::string driveId;
        std::string remoteDirId;
        std::string remotePath;
        std::string apiToken;

        TestVariables() {
            userId = loadEnvVariable("KDRIVE_TEST_CI_USER_ID");
            accountId = loadEnvVariable("KDRIVE_TEST_CI_ACCOUNT_ID");
            driveId = loadEnvVariable("KDRIVE_TEST_CI_DRIVE_ID");
            remoteDirId = loadEnvVariable("KDRIVE_TEST_CI_REMOTE_DIR_ID");
            remotePath = loadEnvVariable("KDRIVE_TEST_CI_REMOTE_PATH");
            apiToken = loadEnvVariable("KDRIVE_TEST_CI_API_TOKEN");
        }
};

void generateOrEditTestFile(const SyncPath &path);

} // namespace KDC::testhelpers
