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

#include "testworkers.h"

#include "propagation/executor/executorworker.h"
#include "keychainmanager/keychainmanager.h"
#include "network/proxy.h"
#include "io/iohelper.h"
#include "test_utility/testhelpers.h"

namespace KDC {

#if defined(__APPLE__)
std::unique_ptr<VfsMac> TestWorkers::_vfsPtr = nullptr;
#elif defined(_WIN32)
std::unique_ptr<VfsWin> TestWorkers::_vfsPtr = nullptr;
#else
std::unique_ptr<VfsOff> TestWorkers::_vfsPtr = nullptr;
#endif

bool TestWorkers::createPlaceholder(int syncDbId, const SyncPath &relativeLocalPath, const SyncFileItem &item) {
    (void) syncDbId;

    if (_vfsPtr && !_vfsPtr->createPlaceholder(relativeLocalPath, item)) {
        return false;
    }
    return true;
}

bool TestWorkers::convertToPlaceholder(int syncDbId, const SyncPath &relativeLocalPath, const SyncFileItem &item) {
    (void) syncDbId;

    if (_vfsPtr && !_vfsPtr->convertToPlaceholder(Path2QStr(relativeLocalPath), item)) {
        return false;
    }
    return true;
}

bool TestWorkers::setPinState(int syncDbId, const SyncPath &relativeLocalPath, PinState pinState) {
    (void) syncDbId;

    if (_vfsPtr && !_vfsPtr->setPinState(Path2QStr(relativeLocalPath), pinState)) {
        return false;
    }
    return true;
}

void TestWorkers::setUp() {
    _logger = Log::instance()->getLogger();

    const testhelpers::TestVariables testVariables;

    const std::string localPathStr = _localTempDir.path().string();

    // Insert api token into keystore
    std::string keychainKey("123");
    KeyChainManager::instance(true);
    KeyChainManager::instance()->writeToken(keychainKey, testVariables.apiToken);

    // Create parmsDb
    bool alreadyExists = false;
    std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExists, true);
    std::filesystem::remove(parmsDbPath);
    ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);

    // Insert user, account, drive & sync
    int userId(12321);
    User user(1, userId, keychainKey);
    ParmsDb::instance()->insertUser(user);

    int accountId(atoi(testVariables.accountId.c_str()));
    Account account(1, accountId, user.dbId());
    ParmsDb::instance()->insertAccount(account);

    int driveDbId = 1;
    int driveId = atoi(testVariables.driveId.c_str());
    Drive drive(driveDbId, driveId, account.dbId(), std::string(), 0, std::string());
    ParmsDb::instance()->insertDrive(drive);

    _sync = Sync(1, drive.dbId(), localPathStr, testVariables.remotePath);
#if defined(__APPLE__)
    _sync.setVirtualFileMode(VirtualFileMode::Mac);
#elif defined(_WIN32)
    _sync.setVirtualFileMode(VirtualFileMode::Win);
#else
    _sync.setVirtualFileMode(VirtualFileMode::Off);
#endif

    ParmsDb::instance()->insertSync(_sync);

    // Setup proxy
    Parameters parameters;
    if (bool found = false; ParmsDb::instance()->selectParameters(parameters, found) && found) {
        Proxy::instance(parameters.proxyConfig());
    }

    // Create VFS instance
    VfsSetupParams vfsSetupParams;
    vfsSetupParams._syncDbId = _sync.dbId();
#ifdef _WIN32
    vfsSetupParams._driveId = drive.driveId();
    vfsSetupParams._userId = user.userId();
#endif
    vfsSetupParams._localPath = _sync.localPath();
    vfsSetupParams._targetPath = _sync.targetPath();
    vfsSetupParams._logger = _logger;

#if defined(__APPLE__)
    _vfsPtr = std::unique_ptr<VfsMac>(new VfsMac(vfsSetupParams));
#elif defined(_WIN32)
    //_vfsPtr = std::unique_ptr<VfsWin>(new VfsWin(vfsSetupParams, nullptr));
#else
    _vfsPtr = std::unique_ptr<VfsOff>(new VfsOff(vfsSetupParams));
#endif

    // Setup SyncPal
    _syncPal = std::make_shared<SyncPal>(_sync.dbId(), KDRIVE_VERSION_STRING);
    _syncPal->createWorkers();
    _syncPal->syncDb()->setAutoDelete(true);
    _syncPal->createProgressInfo();
    _syncPal->setVfsCreatePlaceholderCallback(createPlaceholder);
    _syncPal->setVfsConvertToPlaceholderCallback(convertToPlaceholder);
    _syncPal->setVfsSetPinStateCallback(setPinState);
}

void TestWorkers::tearDown() {
    ParmsDb::instance()->close();
    ParmsDb::reset();
    if (_syncPal && _syncPal->syncDb()) {
        _syncPal->syncDb()->close();
    }
    if (_vfsPtr) {
        _vfsPtr = nullptr;
    }
}

void TestWorkers::testCreatePlaceholder() {
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
        _syncPal->initProgress(syncItem);

        // Folder doesn't exist (normal case)
        exitInfo = _syncPal->_executorWorker->createPlaceholder(relativeFolderPath);
        CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitInfo.code());
        CPPUNIT_ASSERT_EQUAL(ExitCause::Unknown, exitInfo.cause());

#if defined(__APPLE__) || defined(_WIN32)
        // Folder already exists
        exitInfo = _syncPal->_executorWorker->createPlaceholder(relativeFolderPath);
        CPPUNIT_ASSERT_EQUAL(ExitCode::DataError, exitInfo.code());
        CPPUNIT_ASSERT_EQUAL(ExitCause::InvalidSnapshot, exitInfo.cause());
#endif
    }

    // Create file operation
    SyncPath relativeFilePath{relativeFolderPath / "placeholder_file"};
    {
        SyncFileItem syncItem;
        syncItem.setPath(relativeFilePath);
        syncItem.setType(NodeType::File);
        syncItem.setDirection(SyncDirection::Down);
        _syncPal->initProgress(syncItem);

#if defined(__APPLE__) || defined(_WIN32)
        // Folder access denied
        IoError ioError{IoError::Unknown};
        CPPUNIT_ASSERT(IoHelper::setRights(_syncPal->localPath() / relativeFolderPath, false, false, false, ioError) &&
                       ioError == IoError::Success);

        exitInfo = _syncPal->_executorWorker->createPlaceholder(relativeFilePath);
        CPPUNIT_ASSERT_EQUAL(ExitCode::SystemError, exitInfo.code());
        CPPUNIT_ASSERT_EQUAL(ExitCause::FileAccessError, exitInfo.cause());

        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::setRights(_syncPal->localPath() / relativeFolderPath, true, true, true, ioError) &&
                       ioError == IoError::Success);
#endif

        // File doesn't exist (normal case)
        exitInfo = _syncPal->_executorWorker->createPlaceholder(relativeFilePath);
        CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitInfo.code());
        CPPUNIT_ASSERT_EQUAL(ExitCause::Unknown, exitInfo.cause());

#if defined(__APPLE__) || defined(_WIN32)
        // File already exists
        exitInfo = _syncPal->_executorWorker->createPlaceholder(relativeFilePath);
        CPPUNIT_ASSERT_EQUAL(ExitCode::DataError, exitInfo.code());
        CPPUNIT_ASSERT_EQUAL(ExitCause::InvalidSnapshot, exitInfo.cause());
#endif
    }
}

void TestWorkers::testConvertToPlaceholder() {
    _syncPal->resetEstimateUpdates();
    ExitInfo exitInfo;
    // Progress not intialized
    {
        exitInfo = _syncPal->_executorWorker->convertToPlaceholder(SyncPath("dummy"), true);
        CPPUNIT_ASSERT_EQUAL(ExitCode::DataError, exitInfo.code());
        CPPUNIT_ASSERT_EQUAL(ExitCause::InvalidSnapshot, exitInfo.cause());
    }

    // Convert folder operation
    SyncPath relativeFolderPath{"placeholders_folder"};
    {
        SyncFileItem syncItem;
        syncItem.setPath(relativeFolderPath);
        syncItem.setType(NodeType::Directory);
        syncItem.setDirection(SyncDirection::Down);
        _syncPal->initProgress(syncItem);

#if defined(__APPLE__) || defined(_WIN32)
        // Folder doesn't exist
        exitInfo = _syncPal->_executorWorker->convertToPlaceholder(relativeFolderPath, true);
        CPPUNIT_ASSERT_EQUAL(ExitCode::DataError, exitInfo.code());
        CPPUNIT_ASSERT_EQUAL(ExitCause::InvalidSnapshot, exitInfo.cause());
#endif

        // Folder already exists (normal case)
        std::error_code ec;
        CPPUNIT_ASSERT(std::filesystem::create_directory(_syncPal->localPath() / relativeFolderPath, ec) && ec.value() == 0);

        exitInfo = _syncPal->_executorWorker->convertToPlaceholder(relativeFolderPath, true);
        CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitInfo.code());
        CPPUNIT_ASSERT_EQUAL(ExitCause::Unknown, exitInfo.cause());
    }

    // Convert file operation
    SyncPath relativeFilePath{relativeFolderPath / "placeholder_file"};
    {
        SyncFileItem syncItem;
        syncItem.setPath(relativeFilePath);
        syncItem.setType(NodeType::File);
        syncItem.setDirection(SyncDirection::Down);
        _syncPal->initProgress(syncItem);

#if defined(__APPLE__) || defined(_WIN32)
        // Folder access denied
        IoError ioError{IoError::Unknown};
        CPPUNIT_ASSERT(IoHelper::setRights(_syncPal->localPath() / relativeFolderPath, false, false, false, ioError) &&
                       ioError == IoError::Success);

        exitInfo = _syncPal->_executorWorker->createPlaceholder(relativeFilePath);
        CPPUNIT_ASSERT_EQUAL(ExitCode::SystemError, exitInfo.code());
        CPPUNIT_ASSERT_EQUAL(ExitCause::FileAccessError, exitInfo.cause());

        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::setRights(_syncPal->localPath() / relativeFolderPath, true, true, true, ioError) &&
                       ioError == IoError::Success);

        // File doesn't exist
        exitInfo = _syncPal->_executorWorker->convertToPlaceholder(relativeFilePath, true);
        CPPUNIT_ASSERT_EQUAL(ExitCode::DataError, exitInfo.code());
        CPPUNIT_ASSERT_EQUAL(ExitCause::InvalidSnapshot, exitInfo.cause());
#endif

        // File already exists (normal case)
        {
            std::ofstream ofs(_syncPal->localPath() / relativeFilePath);
            ofs << "Some content.\n";
        }

        exitInfo = _syncPal->_executorWorker->convertToPlaceholder(relativeFilePath, true);
        CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitInfo.code());
        CPPUNIT_ASSERT_EQUAL(ExitCause::Unknown, exitInfo.cause());
    }
}
} // namespace KDC
