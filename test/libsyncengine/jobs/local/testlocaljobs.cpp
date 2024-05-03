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

#include "testlocaljobs.h"

#include <memory>

#include "config.h"
#include "jobs/local/localcreatedirjob.h"
#include "jobs/local/localdeletejob.h"
#include "jobs/local/localmovejob.h"
#include "db/db.h"
#include "db/parmsdb.h"
#include "keychainmanager/keychainmanager.h"
#include "libcommonserver/network/proxy.h"
#include "libcommon/utility/utility.h"

using namespace CppUnit;

namespace KDC {

static const SyncPath localTestDirPath(std::wstring(L"" TEST_DIR) + L"/test_ci");

void KDC::TestLocalJobs::setUp() {
    const std::string userIdStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_USER_ID");
    const std::string accountIdStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_ACCOUNT_ID");
    const std::string driveIdStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_DRIVE_ID");
    const std::string localPathStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_LOCAL_PATH");
    const std::string remotePathStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_REMOTE_PATH");
    const std::string apiTokenStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_API_TOKEN");
    const std::string remoteDirIdStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_REMOTE_DIR_ID");

    if (userIdStr.empty() || accountIdStr.empty() || driveIdStr.empty() || localPathStr.empty() || remotePathStr.empty() ||
        apiTokenStr.empty() || remoteDirIdStr.empty()) {
        throw std::runtime_error("Some environment variables are missing!");
    }

    // Insert api token into keystore
    std::string keychainKey("123");
    KeyChainManager::instance(true);
    KeyChainManager::instance()->writeToken(keychainKey, apiTokenStr.c_str());

    // Create parmsDb
    bool alreadyExists;
    std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExists);
    std::filesystem::remove(parmsDbPath);
    ParmsDb::instance(parmsDbPath, "3.4.0", true, true);
    ParmsDb::instance()->setAutoDelete(true);

    // Insert user, account, drive & sync
    int userId(atoi(userIdStr.c_str()));
    User user(1, userId, keychainKey);
    ParmsDb::instance()->insertUser(user);

    int accountId(atoi(accountIdStr.c_str()));
    Account account(1, accountId, user.dbId());
    ParmsDb::instance()->insertAccount(account);

    _driveDbId = 1;
    int driveId = atoi(driveIdStr.c_str());
    Drive drive(_driveDbId, driveId, account.dbId(), std::string(), 0, std::string());
    ParmsDb::instance()->insertDrive(drive);

    Sync sync(1, drive.dbId(), localPathStr, remotePathStr);
    ParmsDb::instance()->insertSync(sync);

    // Setup proxy
    Parameters parameters;
    bool found;
    if (ParmsDb::instance()->selectParameters(parameters, found) && found) {
        Proxy::instance(parameters.proxyConfig());
    }

    _syncPal = std::make_shared<SyncPal>(sync.dbId(), "3.6.0");
    _syncPal->_syncDb->setAutoDelete(true);
}

void KDC::TestLocalJobs::tearDown() {
    ParmsDb::instance()->close();
    if (_syncPal && _syncPal->_syncDb) {
        _syncPal->_syncDb->close();
    }
}

void KDC::TestLocalJobs::testCreateMoveDeleteDir() {
    // Create
    SyncPath localDirPath = localTestDirPath / "tmp_dir";
    LocalCreateDirJob createJob(localDirPath);
    createJob.runSynchronously();

    CPPUNIT_ASSERT(std::filesystem::exists(localDirPath));

    SyncPath picturesPath = localTestDirPath / "test_pictures";
    std::filesystem::copy(picturesPath / "picture-1.jpg", picturesPath / "tmp_picture.jpg");

    // Move
    SyncPath finalPath(localDirPath / "tmp_picture.jpg");
    LocalMoveJob moveJob(picturesPath / "tmp_picture.jpg", finalPath);
    moveJob.runSynchronously();

    CPPUNIT_ASSERT(std::filesystem::exists(finalPath));

    // Delete
    LocalDeleteJob deletejob(_driveDbId, localTestDirPath, "tmp_dir", false, "-1");
    deletejob.runSynchronously();

    CPPUNIT_ASSERT(!std::filesystem::exists(localDirPath));
}

}  // namespace KDC
