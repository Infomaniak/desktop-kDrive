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

#include "testworkers.h"

#include "propagation/executor/executorworker.h"
#include "libcommon/keychainmanager/keychainmanager.h"
#include "libcommonserver/network/proxy.h"
#include "libcommonserver/io/iohelper.h"
#include "mocks/libcommonserver/db/mockdb.h"

#include "test_utility/testhelpers.h"

#ifdef _WIN32
#include <combaseapi.h>
#endif

namespace KDC {

#if defined(__APPLE__)
std::shared_ptr<VfsMac> TestWorkers::_vfsPtr = nullptr;
#elif defined(_WIN32)
std::shared_ptr<VfsWin> TestWorkers::_vfsPtr = nullptr;
#else
std::shared_ptr<VfsOff> TestWorkers::_vfsPtr = nullptr;
#endif

bool TestWorkers::_vfsInstallationDone = false;
bool TestWorkers::_vfsActivationDone = false;
bool TestWorkers::_vfsConnectionDone = false;

#ifdef __APPLE__
// TODO: On macOS, SIP should be deactivated and LiteSync extension signed to be able to install the Lite Sync extension.
// Set to true if the Login Item Agent and the Lite Sync extensions are already installed on the test machine.
constexpr bool connectorsAreAlreadyInstalled = false;
#endif

void TestWorkers::setUp() {
    TestBase::start();
#ifdef _WIN32
    if (!testhelpers::isExtendedTest(false)) return;
#endif

    _logger = Log::instance()->getLogger();

    const testhelpers::TestVariables testVariables;

    const std::string localPathStr = _localTempDir.path().string();

    // Insert api token into keystore
    std::string keychainKey("123");
    (void) KeyChainManager::instance(true);
    KeyChainManager::instance()->writeToken(keychainKey, testVariables.apiToken);

    // Create parmsDb
    bool alreadyExists = false;
    std::filesystem::path parmsDbPath = MockDb::makeDbName(alreadyExists);
    std::filesystem::remove(parmsDbPath);
    ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);

    // Insert user, account, drive & sync
    int userId(12321);
    User user(1, userId, keychainKey);
    (void) ParmsDb::instance()->insertUser(user);

    int accountId(atoi(testVariables.accountId.c_str()));
    Account account(1, accountId, user.dbId());
    (void) ParmsDb::instance()->insertAccount(account);

    int driveDbId = 1;
    int driveId = atoi(testVariables.driveId.c_str());
    Drive drive(driveDbId, driveId, account.dbId(), std::string(), 0, std::string());
    (void) ParmsDb::instance()->insertDrive(drive);

    _sync = Sync(1, drive.dbId(), localPathStr, testVariables.remotePath);
#if defined(__APPLE__)
    _sync.setVirtualFileMode(VirtualFileMode::Mac);
#elif defined(_WIN32)
    _sync.setVirtualFileMode(VirtualFileMode::Win);
#else
    _sync.setVirtualFileMode(VirtualFileMode::Off);
#endif

    (void) ParmsDb::instance()->insertSync(_sync);

    // Setup proxy
    Parameters parameters;
    if (bool found = false; ParmsDb::instance()->selectParameters(parameters, found) && found) {
        Proxy::instance(parameters.proxyConfig());
    }

    // Create VFS instance
    VfsSetupParams vfsSetupParams;
    vfsSetupParams.syncDbId = _sync.dbId();
#ifdef _WIN32
    vfsSetupParams.driveId = drive.driveId();
    vfsSetupParams.userId = user.userId();
#endif
    vfsSetupParams.localPath = _sync.localPath();
    vfsSetupParams.targetPath = _sync.targetPath();
    vfsSetupParams.logger = _logger;
    vfsSetupParams.sentryHandler = sentry::Handler::instance();
    vfsSetupParams.executeCommand = [](const char *) {};

#if defined(__APPLE__)
    _vfsPtr = std::shared_ptr<VfsMac>(new VfsMac(vfsSetupParams));
#elif defined(_WIN32)
    _vfsPtr = std::shared_ptr<VfsWin>(new VfsWin(vfsSetupParams));
#else
    _vfsPtr = std::shared_ptr<VfsOff>(new VfsOff(vfsSetupParams));
#endif

#if defined(__APPLE__)
    _vfsPtr->setExclusionAppListCallback([](QString &) {});
#endif

    // Setup SyncPal
    _syncPal = std::make_shared<SyncPal>(_vfsPtr, _sync.dbId(), KDRIVE_VERSION_STRING);
    _syncPal->createSharedObjects();
    _syncPal->createWorkers(std::chrono::seconds(0));
    _syncPal->syncDb()->setAutoDelete(true);
    _syncPal->createProgressInfo();

    // Setup SocketApi
    std::unordered_map<int, std::shared_ptr<KDC::SyncPal>> syncPalMap;
    syncPalMap[_sync.dbId()] = _syncPal;
    std::unordered_map<int, std::shared_ptr<KDC::Vfs>> vfsMap;
    vfsMap[_sync.dbId()] = _vfsPtr;
    _socketApi = std::make_unique<SocketApi>(syncPalMap, vfsMap);

#ifdef _WIN32
    // Initializes the COM library
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
#endif

    // Start Vfs
#ifdef __APPLE__
    if (connectorsAreAlreadyInstalled) {
        _vfsInstallationDone = true;
        _vfsActivationDone = true;
        startVfs();
    }
#else
    startVfs();
#endif
}

void TestWorkers::tearDown() {
#ifdef _WIN32
    if (!testhelpers::isExtendedTest(false)) return;
#endif
    ParmsDb::instance()->close();
    ParmsDb::reset();
    if (_syncPal && _syncPal->syncDb()) {
        _syncPal->syncDb()->close();
    }
    if (_vfsPtr) {
        // Stop Vfs
        _vfsPtr->stopImpl(true);
        _vfsPtr = nullptr;
    }
    TestBase::stop();
}

void TestWorkers::testStartVfs() {
#ifdef _WIN32
    if (!testhelpers::isExtendedTest()) return;
#endif
#if defined(__APPLE__)
    if (connectorsAreAlreadyInstalled) {
        // Make sure that Vfs is installed/activated/connected
        CPPUNIT_ASSERT(_vfsInstallationDone);
        CPPUNIT_ASSERT(_vfsActivationDone);
        CPPUNIT_ASSERT(_vfsConnectionDone);

        // Try to start Vfs another time
        CPPUNIT_ASSERT(startVfs());
        CPPUNIT_ASSERT(_vfsInstallationDone);
        CPPUNIT_ASSERT(_vfsActivationDone);
        CPPUNIT_ASSERT(_vfsConnectionDone);
    }
#elif defined(_WIN32)
    // Try to start Vfs another time
    // => WinRT error caught : hr 8007017a - The cloud sync root is already connected with another cloud sync provider.
    CPPUNIT_ASSERT(!startVfs());
#endif
}

void TestWorkers::testCreatePlaceholder() {
#ifdef _WIN32
    if (!testhelpers::isExtendedTest()) return;
#endif
    _syncPal->resetEstimateUpdates();
    ExitInfo exitInfo;
    // Progress not intialized
    {
        exitInfo = _syncPal->_executorWorker->createPlaceholder(SyncPath("dummy"));
        CPPUNIT_ASSERT_EQUAL(ExitCode::DataError, exitInfo.code());
        CPPUNIT_ASSERT_EQUAL(ExitCause::InvalidSnapshot, exitInfo.cause());
    }

    // Create folder operation
    SyncPath relativeFolderPath{"placeholders_folder"};
    {
        SyncFileItem syncItem;
        syncItem.setPath(relativeFolderPath);
        syncItem.setType(NodeType::Directory);
        syncItem.setDirection(SyncDirection::Down);
#ifdef _WIN32
        syncItem.setRemoteNodeId("1");
#endif

        CPPUNIT_ASSERT(_syncPal->initProgress(syncItem));

        // Folder doesn't exist (normal case)
        exitInfo = _syncPal->_executorWorker->createPlaceholder(relativeFolderPath);
        CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), exitInfo);

#if defined(__APPLE__) || defined(_WIN32)
        // Folder already exists
        exitInfo = _syncPal->_executorWorker->createPlaceholder(relativeFolderPath);
        CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::SystemError, ExitCause::FileExists), exitInfo);
#endif
    }

    // Create file operation
    SyncPath relativeFilePath{relativeFolderPath / "placeholder_file"};
    {
        SyncFileItem syncItem;
        syncItem.setPath(relativeFilePath);
        syncItem.setType(NodeType::File);
        syncItem.setDirection(SyncDirection::Down);
#ifdef _WIN32
        syncItem.setRemoteNodeId("2");
#endif

        CPPUNIT_ASSERT(_syncPal->initProgress(syncItem));

#if defined(__APPLE__) || defined(_WIN32)
        // Folder access denied
        IoError ioError{IoError::Unknown};
        CPPUNIT_ASSERT_MESSAGE(toString(ioError),
                               IoHelper::setRights(_syncPal->localPath() / relativeFolderPath, false, false, false, ioError) &&
                                       ioError == IoError::Success);

        exitInfo = _syncPal->_executorWorker->createPlaceholder(relativeFilePath);
#ifdef __APPLE__
        CPPUNIT_ASSERT_EQUAL(ExitCode::SystemError, exitInfo.code());
        CPPUNIT_ASSERT_EQUAL(ExitCause::FileAccessError, exitInfo.cause());
#else
        // Strangely (bug?), the Windows api is able to create a placeholder in a folder for which the user does not have
        // rights
        CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitInfo.code());
        CPPUNIT_ASSERT_EQUAL(ExitCause::Unknown, exitInfo.cause());

        // Remove placeholder
        std::error_code ec;
        std::filesystem::remove(_syncPal->localPath() / relativeFilePath);
        if (ec) {
            // Cannot remove file
            CPPUNIT_ASSERT(false);
        }
#endif

        ioError = IoError::Unknown;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError),
                               IoHelper::setRights(_syncPal->localPath() / relativeFolderPath, true, true, true, ioError) &&
                                       ioError == IoError::Success);
#endif

        // File doesn't exist (normal case)
        exitInfo = _syncPal->_executorWorker->createPlaceholder(relativeFilePath);
        CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitInfo.code());
        CPPUNIT_ASSERT_EQUAL(ExitCause::Unknown, exitInfo.cause());

#if defined(__APPLE__) || defined(_WIN32)
        // File already exists
        exitInfo = _syncPal->_executorWorker->createPlaceholder(relativeFilePath);
        CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::SystemError, ExitCause::FileExists), exitInfo);
#endif
    }
}

void TestWorkers::testConvertToPlaceholder() {
#ifdef _WIN32
    if (!testhelpers::isExtendedTest()) return;
#endif
    _syncPal->resetEstimateUpdates();
    ExitInfo exitInfo;
    // Progress not intialized
    {
        exitInfo = _syncPal->_executorWorker->convertToPlaceholder(SyncPath("dummy"), true);
        CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::DataError, ExitCause::InvalidSnapshot), exitInfo);
    }

    // Convert folder operation
    SyncPath relativeFolderPath{"placeholders_folder"};
    {
        SyncFileItem syncItem;
        syncItem.setPath(relativeFolderPath);
        syncItem.setType(NodeType::Directory);
        syncItem.setDirection(SyncDirection::Down);
        CPPUNIT_ASSERT(_syncPal->initProgress(syncItem));

#if defined(__APPLE__) || defined(_WIN32)
        // Folder doesn't exist
        exitInfo = _syncPal->_executorWorker->convertToPlaceholder(relativeFolderPath, true);
        CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::SystemError, ExitCause::NotFound), exitInfo);
#endif

        // Folder already exists (normal case)
        std::error_code ec;
        CPPUNIT_ASSERT(std::filesystem::create_directory(_syncPal->localPath() / relativeFolderPath, ec) && ec.value() == 0);

        exitInfo = _syncPal->_executorWorker->convertToPlaceholder(relativeFolderPath, true);
        CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), exitInfo);
    }

    // Convert file operation
    SyncPath relativeFilePath{relativeFolderPath / "placeholder_file"};
    {
        SyncFileItem syncItem;
        syncItem.setPath(relativeFilePath);
        syncItem.setType(NodeType::File);
        syncItem.setDirection(SyncDirection::Down);
        syncItem.setRemoteNodeId("1");
        CPPUNIT_ASSERT(_syncPal->initProgress(syncItem));

#if defined(__APPLE__) || defined(_WIN32)
        // Folder access denied
        IoError ioError{IoError::Unknown};
        CPPUNIT_ASSERT_MESSAGE(toString(ioError),
                               IoHelper::setRights(_syncPal->localPath() / relativeFolderPath, false, false, false, ioError) &&
                                       ioError == IoError::Success);

        exitInfo = _syncPal->_executorWorker->createPlaceholder(relativeFilePath);
#if defined(__APPLE__)
        CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::SystemError, ExitCause::FileAccessError), exitInfo);
#else
        // Strangely (bug?), the Windows api is able to create a placeholder in a folder for which the user does not have
        // rights
        CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), exitInfo);
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::deleteItem(_syncPal->localPath() / relativeFilePath, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
#endif

        ioError = IoError::Unknown;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError),
                               IoHelper::setRights(_syncPal->localPath() / relativeFolderPath, true, true, true, ioError) &&
                                       ioError == IoError::Success);

        // File doesn't exist
        exitInfo = _syncPal->_executorWorker->convertToPlaceholder(relativeFilePath, true);
        CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::SystemError, ExitCause::NotFound), exitInfo);
#endif

        // File already exists (normal case)
        {
            std::ofstream ofs(_syncPal->localPath() / relativeFilePath);
            ofs << "Some content.\n";
        }

        exitInfo = _syncPal->_executorWorker->convertToPlaceholder(relativeFilePath, true);
        (CppUnit::assertEquals((ExitInfo(ExitCode::Ok)), (exitInfo),
                               CppUnit::SourceLine("C:\\Projects\\desktop-kDrive\\test\\server\\workers\\testworkers.cpp", 374),
                               ""));
    }
}

bool TestWorkers::startVfs() {
    return _vfsPtr->startImpl(_vfsInstallationDone, _vfsActivationDone, _vfsConnectionDone);
}

} // namespace KDC
