/*
 *
 *  * Infomaniak kDrive - Desktop
 *  * Copyright (C) 2023-2024 Infomaniak Network SA
 *  *
 *  * This program is free software: you can redistribute it and/or modify
 *  * it under the terms of the GNU General Public License as published by
 *  * the Free Software Foundation, either version 3 of the License, or
 *  * (at your option) any later version.
 *  *
 *  * This program is distributed in the hope that it will be useful,
 *  * but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  * GNU General Public License for more details.
 *  *
 *  * You should have received a copy of the GNU General Public License
 *  * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
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

#ifdef __APPLE__
std::unique_ptr<Vfs> TestExecutor::_vfs;
bool TestExecutor::vfsStatus(int syncDbId, const SyncPath &itemPath, bool &isPlaceholder, bool &isHydrated, bool &isSyncing,
                             int &progress) {
    if (!_vfs) {
        return false;
    }

    return _vfs->status(SyncName2QStr(itemPath.native()), isPlaceholder, isHydrated, isSyncing, progress);
}
#endif

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

void TestExecutor::tearDown() {}

void TestExecutor::testCheckLiteSyncInfoForCreate() {
#ifdef __APPLE__
    VfsSetupParams vfsSetupParams;
    vfsSetupParams._syncDbId = _sync.dbId();
    vfsSetupParams._localPath = _sync.localPath();
    vfsSetupParams._targetPath = _sync.targetPath();
    vfsSetupParams._logger = Log::instance()->getLogger();
    _vfs = std::make_unique<VfsMac>(vfsSetupParams);

    _syncPal->setVfsStatusCallback(&TestExecutor::vfsStatus);

    const SyncName filename = "test_file.txt";

    // A hydrated placeholder
    {
        // Create temp directory and file.
        const TmpTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / filename;
        {
            std::ofstream ofs(path);
            ofs << "abc";
            ofs.close();
        }
        // Convert file to placeholder
        const auto extConnector = LiteSyncExtConnector::instance(Log::instance()->getLogger(), ExecuteCommand());
        extConnector->vfsConvertToPlaceHolder(Path2QStr(path), true);

        auto opPtr = std::make_shared<SyncOperation>();
        opPtr->setTargetSide(ReplicaSideRemote);
        const auto node =
            std::make_shared<Node>(1, ReplicaSideLocal, filename, NodeTypeFile, "1234", defaultTime, defaultTime, 123);
        opPtr->setAffectedNode(node);
        bool isDehydratedPlaceholder = false;
        _syncPal->_executorWorker->checkLiteSyncInfoForCreate(opPtr, path, isDehydratedPlaceholder);

        CPPUNIT_ASSERT(!isDehydratedPlaceholder);
    }

    // A dehydrated placeholder
    {
        // Create temp directory
        const TmpTemporaryDirectory temporaryDirectory;
        // Create placeholder
        const auto extConnector = LiteSyncExtConnector::instance(Log::instance()->getLogger(), ExecuteCommand());
        struct stat fileInfo;
        fileInfo.st_size = 10;
        fileInfo.st_mtimespec = {defaultTime, 0};
        fileInfo.st_atimespec = {defaultTime, 0};
        fileInfo.st_birthtimespec = {defaultTime, 0};
        fileInfo.st_mode = S_IFREG;
        extConnector->vfsCreatePlaceHolder(SyncName2QStr(filename), Path2QStr(temporaryDirectory.path), &fileInfo);

        auto opPtr = std::make_shared<SyncOperation>();
        opPtr->setTargetSide(ReplicaSideRemote);
        const auto node =
            std::make_shared<Node>(1, ReplicaSideLocal, filename, NodeTypeFile, "1234", defaultTime, defaultTime, 123);
        opPtr->setAffectedNode(node);
        bool isDehydratedPlaceholder = false;
        _syncPal->_executorWorker->checkLiteSyncInfoForCreate(opPtr, temporaryDirectory.path / filename, isDehydratedPlaceholder);

        CPPUNIT_ASSERT(isDehydratedPlaceholder);
    }

    // A partially hydrated placeholder (syncing item)
    {
        // Create temp directory
        const TmpTemporaryDirectory temporaryDirectory;
        // Create placeholder
        const auto extConnector = LiteSyncExtConnector::instance(Log::instance()->getLogger(), ExecuteCommand());
        struct stat fileInfo;
        fileInfo.st_size = 10;
        fileInfo.st_mtimespec = {defaultTime, 0};
        fileInfo.st_atimespec = {defaultTime, 0};
        fileInfo.st_birthtimespec = {defaultTime, 0};
        fileInfo.st_mode = S_IFREG;
        extConnector->vfsCreatePlaceHolder(SyncName2QStr(filename), Path2QStr(temporaryDirectory.path), &fileInfo);
        const SyncPath filePAth = temporaryDirectory.path / filename;
        {
            std::ofstream ofs(filePAth);
            ofs << "abc";
            ofs.close();
        }
        IoError ioError = IoErrorUnknown;
        IoHelper::setXAttrValue(filePAth, "com.infomaniak.drive.desktopclient.litesync.status", "H30", ioError);

        auto opPtr = std::make_shared<SyncOperation>();
        opPtr->setTargetSide(ReplicaSideRemote);
        const auto node =
            std::make_shared<Node>(1, ReplicaSideLocal, filename, NodeTypeFile, "1234", defaultTime, defaultTime, 123);
        opPtr->setAffectedNode(node);
        bool isDehydratedPlaceholder = false;
        _syncPal->_executorWorker->checkLiteSyncInfoForCreate(opPtr, temporaryDirectory.path / filename, isDehydratedPlaceholder);

        CPPUNIT_ASSERT(!isDehydratedPlaceholder);
    }
#endif
}

}  // namespace KDC