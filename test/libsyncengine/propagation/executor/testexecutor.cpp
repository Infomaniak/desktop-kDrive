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

#include "testexecutor.h"

#include "vfs.h"

#include <memory>
#include "propagation/executor/executorworker.h"
#include "libcommonserver/io/testio.h"
#include "server/vfs/mac/litesyncextconnector.h"
#include "io/filestat.h"
#include "keychainmanager/keychainmanager.h"
#include "network/proxy.h"
#include "server/vfs/mac/vfs_mac.h"
#include "utility/utility.h"

namespace KDC {

// TODO : to be removed once merged to `develop`. Use `LocalTemporaryDirectory` instead.
struct TmpTemporaryDirectory {
        SyncPath path;
        TmpTemporaryDirectory() {
            const std::time_t now = std::time(nullptr);
            const std::tm tm = *std::localtime(&now);
            std::ostringstream woss;
            woss << std::put_time(&tm, "%Y%m%d_%H%M");

            path = std::filesystem::temp_directory_path() / ("kdrive_io_unit_tests_" + woss.str());
            std::filesystem::create_directory(path);
        }

        ~TmpTemporaryDirectory() { std::filesystem::remove_all(path); }
};

static const SyncTime defaultTime = std::time(nullptr);

void TestExecutor::setUp() {
    const std::string accountIdStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_ACCOUNT_ID");
    const std::string driveIdStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_DRIVE_ID");
    const std::string localPathStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_LOCAL_PATH");
    const std::string remotePathStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_REMOTE_PATH");
    const std::string apiTokenStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_API_TOKEN");

    if (accountIdStr.empty() || driveIdStr.empty() || localPathStr.empty() || remotePathStr.empty() || apiTokenStr.empty()) {
        throw std::runtime_error("Some environment variables are missing!");
    }

    // Insert api token into keystore
    std::string keychainKey("123");
    KeyChainManager::instance(true);
    KeyChainManager::instance()->writeToken(keychainKey, apiTokenStr);

    // Create parmsDb
    bool alreadyExists = false;
    std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExists);
    std::filesystem::remove(parmsDbPath);
    ParmsDb::instance(parmsDbPath, "3.4.0", true, true);
    ParmsDb::instance()->setAutoDelete(true);

    // Insert user, account, drive & sync
    int userId(12321);
    User user(1, userId, keychainKey);
    ParmsDb::instance()->insertUser(user);

    int accountId(atoi(accountIdStr.c_str()));
    Account account(1, accountId, user.dbId());
    ParmsDb::instance()->insertAccount(account);

    int driveDbId = 1;
    int driveId = atoi(driveIdStr.c_str());
    Drive drive(driveDbId, driveId, account.dbId(), std::string(), 0, std::string());
    ParmsDb::instance()->insertDrive(drive);

    _sync = Sync(1, drive.dbId(), localPathStr, remotePathStr);
    ParmsDb::instance()->insertSync(_sync);

    // Setup proxy
    Parameters parameters;
    bool found = false;
    if (ParmsDb::instance()->selectParameters(parameters, found) && found) {
        Proxy::instance(parameters.proxyConfig());
    }

    _syncPal = std::make_shared<SyncPal>(_sync.dbId(), "3.4.0");
    _syncPal->createWorkers();
}

void TestExecutor::testCheckLiteSyncInfoForCreate() {
#ifdef __APPLE__
    // Setup dummy values. Test inputs are set in the callbacks defined below.
    const auto opPtr = std::make_shared<SyncOperation>();
    opPtr->setTargetSide(ReplicaSideRemote);
    const auto node =
        std::make_shared<Node>(1, ReplicaSideLocal, "test_file.txt", NodeTypeFile, "1234", defaultTime, defaultTime, 123);
    opPtr->setAffectedNode(node);

    // A hydrated placeholder.
    {
        _syncPal->setVfsStatusCallback([]([[maybe_unused]] int syncDbId, [[maybe_unused]] const SyncPath &itemPath,
                                          bool &isPlaceholder, bool &isHydrated, bool &isSyncing, int &progress) -> bool {
            isPlaceholder = true;
            isHydrated = true;
            isSyncing = false;
            progress = 0;
            return true;
        });

        bool isDehydratedPlaceholder = false;
        _syncPal->_executorWorker->checkLiteSyncInfoForCreate(opPtr, "/", isDehydratedPlaceholder);

        CPPUNIT_ASSERT(!isDehydratedPlaceholder);
    }

    // A dehydrated placeholder.
    {
        _syncPal->setVfsStatusCallback([]([[maybe_unused]] int syncDbId, [[maybe_unused]] const SyncPath &itemPath,
                                          bool &isPlaceholder, bool &isHydrated, bool &isSyncing, int &progress) -> bool {
            isPlaceholder = true;
            isHydrated = false;
            isSyncing = false;
            progress = 0;
            return true;
        });

        bool isDehydratedPlaceholder = false;
        _syncPal->_executorWorker->checkLiteSyncInfoForCreate(opPtr, "/", isDehydratedPlaceholder);

        CPPUNIT_ASSERT(isDehydratedPlaceholder);
    }

    // A partially hydrated placeholder (syncing item).
    {
        _syncPal->setVfsStatusCallback([]([[maybe_unused]] int syncDbId, [[maybe_unused]] const SyncPath &itemPath,
                                          bool &isPlaceholder, bool &isHydrated, bool &isSyncing, int &progress) -> bool {
            isPlaceholder = true;
            isHydrated = true;
            isSyncing = true;
            progress = 30;
            return true;
        });

        bool isDehydratedPlaceholder = false;
        _syncPal->_executorWorker->checkLiteSyncInfoForCreate(opPtr, "/", isDehydratedPlaceholder);

        CPPUNIT_ASSERT(!isDehydratedPlaceholder);
    }

    // Not a placeholder.
    {
        _syncPal->setVfsStatusCallback([]([[maybe_unused]] int syncDbId, [[maybe_unused]] const SyncPath &itemPath,
                                          bool &isPlaceholder, bool &isHydrated, bool &isSyncing, int &progress) -> bool {
            isPlaceholder = false;
            isHydrated = false;
            isSyncing = false;
            progress = 0;
            return true;
        });

        bool isDehydratedPlaceholder = false;
        _syncPal->_executorWorker->checkLiteSyncInfoForCreate(opPtr, "/", isDehydratedPlaceholder);

        CPPUNIT_ASSERT(!isDehydratedPlaceholder);
    }
#endif
}

}  // namespace KDC