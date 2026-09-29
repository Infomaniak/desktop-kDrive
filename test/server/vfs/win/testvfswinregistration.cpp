/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "testvfswinregistration.h"

#include "comm/pipecommserver.h"
#include "jobs/syncjobmanager.h"
#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/keychainmanager/apitoken.h"
#include "libcommonserver/keychainmanager/keychainmanager.h"
#include "libparms/db/parmsdb.h"
#include "mocks/libcommonserver/db/mockdb.h"
#include "mocks/mockkeychainstorage.h"
#include "syncpal_test_helper/syncpaltesthelper.h"
#include "test_utility/testhelpers.h"
#include "test_utility/timeouthelper.h"

#include <combaseapi.h>

namespace KDC {

void TestVfsWinRegistration::setUp() {
    TestBase::start();
    if (!testhelpers::isExtendedTest(false)) return;

    _logger = Log::instance()->getLogger();

    const testhelpers::TestVariables testVariables;

    // Insert api token into keystore
    ApiToken apiToken;
    apiToken.setAccessToken(testVariables.apiToken);
    const std::string keychainKey("123");
    (void) KeyChainManager::instance(std::make_shared<MockKeyChainStorage>());
    (void) KeyChainManager::instance()->writeData(keychainKey, apiToken.reconstructJsonString());

    // Create parmsDb
    (void) ParmsDb::instance(_localParmsDbTempDir.path() / MockDb::makeDbMockFileName(), KDRIVE_VERSION_STRING, true, true);

    // Insert user, account, drive & sync.
    // Use a user ID different from the other VFS tests so that the sync root ID doesn't collide with theirs.
    _userId = 45654;
    const User user(1, _userId, keychainKey);
    (void) ParmsDb::instance()->insertUser(user);

    const Account account(1, std::stoi(testVariables.accountId), user.dbId(), "account1");
    (void) ParmsDb::instance()->insertAccount(account);

    _driveId = std::stoll(testVariables.driveId);
    const Drive drive(_driveDbId, _driveId, account.dbId(), std::string(), 0, std::string());
    (void) ParmsDb::instance()->insertDrive(drive);

    // Synchronize only a remote temporary subdirectory
    _remoteSyncDir.createDirectory(_driveDbId, testVariables.remoteDirId);
    _targetPath = SyncPath(testVariables.remotePath) / _remoteSyncDir.name();

    FileStat fileStat;
    IoError ioError = IoError::Unknown;
    (void) IoHelper::getFileStat(_localSyncDir.path(), &fileStat, ioError, IoHelper::PathCheckOption::Insensitive);

    Sync sync(_syncDbId, drive.dbId(), _localSyncDir.path(), std::to_string(fileStat.inode), _targetPath, _remoteSyncDir.id());
    sync.setVirtualFileMode(VirtualFileMode::Win);
    sync.setDbPath(MockDb::makeDbName(user.userId(), account.accountId(), drive.driveId(), sync.dbId()));
    (void) ParmsDb::instance()->insertSync(sync);

    // Initializes the COM library
    (void) CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // Initialize server pipe for VFS communication (no need to listen, just create the named pipe is enough for the vfs to start)
    const SyncPath pipePath = PipeCommServer::pipePath();
    _pipeHandle = CreateNamedPipe(pipePath.native().c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                                  PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 10, BUFSIZE * sizeof(TCHAR),
                                  BUFSIZE * sizeof(TCHAR), 5000, nullptr);
}

void TestVfsWinRegistration::tearDown() {
    if (!testhelpers::isExtendedTest(false)) {
        _remoteSyncDir.setDeleted();
        TestBase::stop();
        return;
    }

    if (_syncPal) _syncPal->stop(SyncPal::PauseCaller::Sync, SyncPal::DbBehaviorAfterStop::Remove);
    _syncPal = nullptr;

    if (_vfs) {
        _vfs->stop(true); // Unregister the sync root
        _vfs = nullptr;
    }

    _remoteSyncDir.deleteDirectory();

    ParmsDb::instance()->close();
    ParmsDb::reset();
    SyncJobManagerSingleton::instance()->stop();
    SyncJobManagerSingleton::clear();

    if (_pipeHandle != INVALID_HANDLE_VALUE) {
        (void) CloseHandle(_pipeHandle);
        _pipeHandle = INVALID_HANDLE_VALUE;
    }
    CoUninitialize();

    TestBase::stop();
}

std::shared_ptr<VfsWin> TestVfsWinRegistration::startNewVfs() const {
    VfsSetupParams vfsSetupParams;
    vfsSetupParams.syncDbId = _syncDbId;
    vfsSetupParams.driveId = _driveId;
    vfsSetupParams.userId = _userId;
    vfsSetupParams.localPath = _localSyncDir.path();
    vfsSetupParams.targetPath = _targetPath;
    vfsSetupParams.logger = _logger;
    vfsSetupParams.sentryHandler = sentry::Handler::instance();
    vfsSetupParams.executeCommand = [](const CommString &, const bool) {
        // No communication with the GUI/extension needed in this test
    };

    auto vfs = std::make_shared<VfsWin>(vfsSetupParams);
    bool installationDone = true;
    bool activationDone = true;
    bool connectionDone = true;
    if (const ExitInfo exitInfo = vfs->start(installationDone, activationDone, connectionDone); !exitInfo) {
        LOG_WARN(_logger, "Error in Vfs::start");
        return nullptr;
    }

    return vfs;
}

SyncTime TestVfsWinRegistration::storedVfsRegisteredAt() const {
    Sync sync;
    bool found = false;
    CPPUNIT_ASSERT(ParmsDb::instance()->selectSync(_syncDbId, sync, found) && found);
    return sync.vfsRegisteredAt();
}

void TestVfsWinRegistration::testLocalDeletesRevertedAfterReRegistration() {
    if (!testhelpers::isExtendedTest()) return;

    // First registration of the sync root
    _vfs = startNewVfs();
    CPPUNIT_ASSERT(_vfs);
    const SyncTime firstRegisteredAt = _vfs->registeredAt();
    CPPUNIT_ASSERT(firstRegisteredAt > 0);

    _syncPal = std::make_shared<MockSyncPal>(_vfs, _syncDbId, KDRIVE_VERSION_STRING);
    _syncPal->createSharedObjects();

    SyncpalTestHelper testHelper(_syncPal); // Starts the SyncPal and waits for the end of the 1st sync

    // No reference registration time for a new sync: the current one is stored as is.
    CPPUNIT_ASSERT_EQUAL(firstRegisteredAt, storedVfsRegisteredAt());

    // Create a remote file, synchronized locally as a dehydrated placeholder
    const Operations remoteOperations{Str2SyncName(R"({
        "operations": [
            { "type": "Create", "itemType": "File", "name": "A", "size": 1234 }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.execute(ReplicaSide::Remote, remoteOperations));
    CPPUNIT_ASSERT(testHelper.executeSyncUntilEnd());

    const Situation situation{Str2SyncName(R"({
        "content": [ {"type": "File", "name": "A", "size": 1234} ]
    })")};
    CPPUNIT_ASSERT(testHelper.matchesCurrentSituation(situation, situation));

    const SyncPath placeholderPath = _syncPal->localPath() / "A";
    VfsStatus vfsStatus;
    CPPUNIT_ASSERT(_vfs->status(placeholderPath, vfsStatus));
    CPPUNIT_ASSERT(vfsStatus.isPlaceholder);
    CPPUNIT_ASSERT(!vfsStatus.isHydrated);

    // Simulate an uninstallation of the extension: stop the sync and unregister the sync root.
    CPPUNIT_ASSERT(testHelper.stopSync());
    _vfs->stop(true);

    // The dehydrated placeholder is deleted by the OS
    CPPUNIT_ASSERT(TimeoutHelper::waitFor([&placeholderPath]() { return !std::filesystem::exists(placeholderPath); },
                                          std::chrono::seconds(10), std::chrono::milliseconds(100)));

    // Simulate an app restart: register the sync root again with a new VFS instance, then restart the sync.
    _vfs = startNewVfs();
    CPPUNIT_ASSERT(_vfs);
    const SyncTime secondRegisteredAt = _vfs->registeredAt();
    CPPUNIT_ASSERT_GREATER(firstRegisteredAt, secondRegisteredAt);

    CPPUNIT_ASSERT(testHelper.setVfs(_vfs));
    CPPUNIT_ASSERT(testHelper.executeSyncUntilEnd()); // Restarts the SyncPal, which reverts the local deletes

    // The new registration time is stored once the local deletes have been reverted
    CPPUNIT_ASSERT_EQUAL(secondRegisteredAt, storedVfsRegisteredAt());

    // The placeholder is restored locally and the file has not been deleted on the remote replica
    CPPUNIT_ASSERT(testHelper.matchesCurrentSituation(situation, situation));
    CPPUNIT_ASSERT(_vfs->status(placeholderPath, vfsStatus));
    CPPUNIT_ASSERT(vfsStatus.isPlaceholder);
}

void TestVfsWinRegistration::testSyncStoppedWhenUnregisteredWhileRunning() {
    if (!testhelpers::isExtendedTest()) return;

    _vfs = startNewVfs();
    CPPUNIT_ASSERT(_vfs);
    CPPUNIT_ASSERT(_vfs->isRegistered());

    _syncPal = std::make_shared<MockSyncPal>(_vfs, _syncDbId, KDRIVE_VERSION_STRING);
    _syncPal->createSharedObjects();

    SyncpalTestHelper testHelper(_syncPal); // Starts the SyncPal and waits for the end of the 1st sync

    // Create a remote file, synchronized locally as a dehydrated placeholder
    const Operations remoteOperations{Str2SyncName(R"({
        "operations": [
            { "type": "Create", "itemType": "File", "name": "A", "size": 1234 }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.execute(ReplicaSide::Remote, remoteOperations));
    CPPUNIT_ASSERT(testHelper.executeSyncUntilEnd());

    const Situation situation{Str2SyncName(R"({
        "content": [ {"type": "File", "name": "A", "size": 1234} ]
    })")};
    CPPUNIT_ASSERT(testHelper.matchesCurrentSituation(situation, situation));
    const SyncPath placeholderPath = _syncPal->localPath() / "A";
    CPPUNIT_ASSERT(std::filesystem::exists(placeholderPath));

    // Simulate an uninstallation of the extension while the sync is running: unregister the sync root.
    CPPUNIT_ASSERT(_syncPal->isRunning());
    _vfs->stop(true);
    CPPUNIT_ASSERT(!_vfs->isRegistered());

    // The dehydrated placeholder is deleted by the OS
    CPPUNIT_ASSERT(TimeoutHelper::waitFor([&placeholderPath]() { return !std::filesystem::exists(placeholderPath); },
                                          std::chrono::seconds(10), std::chrono::milliseconds(100)));

    // The SyncPal detects the local deletion and stops in error before propagating it
    CPPUNIT_ASSERT(TimeoutHelper::waitFor([this]() { return !_syncPal->isRunning(); }, std::chrono::seconds(60),
                                          std::chrono::milliseconds(100)));
    CPPUNIT_ASSERT(_syncPal->status() == SyncStatus::Error);

    // The file has not been deleted on the remote replica
    const Situation emptySituation{Str2SyncName(R"({ "content": [] })")};
    CPPUNIT_ASSERT(testHelper.matchesCurrentSituation(emptySituation, situation));
}

} // namespace KDC
