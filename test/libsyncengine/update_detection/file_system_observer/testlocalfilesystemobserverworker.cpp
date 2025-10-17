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

#include "testlocalfilesystemobserverworker.h"
#include "test_classes/syncpaltest.h"

#include "config.h"
#if defined(KD_WINDOWS)
#include "update_detection/file_system_observer/localfilesystemobserverworker_win.h"
#else
#include "update_detection/file_system_observer/localfilesystemobserverworker_unix.h"
#endif
#include "syncpal/tmpblacklistmanager.h"
#include "requests/parameterscache.h"
#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/log/log.h"
#include "mocks/libcommonserver/db/mockdb.h"

#include "test_utility/testhelpers.h"
#include "test_utility/timeouthelper.h"

#include <log4cplus/loggingmacros.h>

#include <Poco/Path.h>
#include <Poco/File.h>

using namespace CppUnit;

namespace KDC {

constexpr uint64_t nbFileInTestDir = 5; // Test directory contains 5 files

void TestLocalFileSystemObserverWorker::setUp() {
    TestBase::start();
    _logger = Log::instance()->getLogger();

    LOGW_DEBUG(_logger, L"$$$$$ Set Up $$$$$");

    // Generate test files
    _tempDir = LocalTemporaryDirectory("TestLocalFileSystemObserverWorker");
    _rootFolderPath = _tempDir.path() / "sync_folder";
    _subDirPath = _rootFolderPath / "sub_dir";
    Poco::File(Path2Str(_subDirPath)).createDirectories();
    for (uint64_t i = 0; i < nbFileInTestDir; i++) {
        std::string filename = "test" + std::to_string(i) + ".txt";
        SyncPath filepath = _subDirPath / filename;
        testhelpers::generateOrEditTestFile(filepath);
        FileStat fileStat;
        bool exists = false;
        IoHelper::getFileStat(filepath, &fileStat, exists);
        _testFiles.emplace_back(std::to_string(fileStat.inode), filepath);
    }

    // Create parmsDb
    const testhelpers::TestVariables testVariables;
    (void) ParmsDb::instance(_localTempDir.path() / MockDb::makeDbMockFileName(), KDRIVE_VERSION_STRING, true, true);
    ParametersCache::instance()->parameters().setExtendedLog(true);

    // Insert user, account, drive & sync
    const int userId(atoi(testVariables.userId.c_str()));
    const User user(1, userId, "123");
    (void) ParmsDb::instance()->insertUser(user);

    const int accountId(atoi(testVariables.accountId.c_str()));
    const Account account(1, accountId, user.dbId());
    (void) ParmsDb::instance()->insertAccount(account);

    const int driveId = atoi(testVariables.driveId.c_str());
    const Drive drive(1, driveId, account.dbId(), std::string(), 0, std::string());
    (void) ParmsDb::instance()->insertDrive(drive);

    Sync sync(1, drive.dbId(), _rootFolderPath, "", testVariables.remotePath);
    (void) ParmsDb::instance()->insertSync(sync);

    // Create SyncPal
    _syncPal = std::make_shared<SyncPalTest>(1, KDRIVE_VERSION_STRING);
    _syncPal->setSyncHasFullyCompleted(true);
    _syncPal->syncDb()->setAutoDelete(true);
    _syncPal->createSharedObjects();
    _syncPal->createWorkers();
    _syncPal->_tmpBlacklistManager = std::make_shared<TmpBlacklistManager>(_syncPal);

#if defined(KD_WINDOWS)
    _syncPal->_localFSObserverWorker = std::shared_ptr<FileSystemObserverWorker>(
            new LocalFileSystemObserverWorker_win(_syncPal, "Local File System Observer", "LFSO"));
#else
    _syncPal->_localFSObserverWorker =
            std::make_shared<LocalFileSystemObserverWorker_unix>(_syncPal, "Local File System Observer", "LFSO");
#endif

    _syncPal->_localFSObserverWorker->start();

    Utility::msleep(1000); // Wait 1sec
}

void TestLocalFileSystemObserverWorker::tearDown() {
    LOGW_DEBUG(_logger, L"$$$$$ Tear Down $$$$$");

    if (_syncPal && _syncPal->_localFSObserverWorker) {
        _syncPal->_localFSObserverWorker->stop();
        _syncPal->_localFSObserverWorker->waitForExit();
        _syncPal->_localFSObserverWorker.reset();
    }

    ParmsDb::instance()->close();
    ParmsDb::reset();
    ParametersCache::reset();
    if (_syncPal && _syncPal->syncDb()) {
        _syncPal->syncDb()->close();
    }
    TestBase::stop();
}
void TestLocalFileSystemObserverWorker::testSyncDirChange() {
    _syncPal->_localFSObserverWorker->stop();
    _syncPal->_localFSObserverWorker->waitForExit();

    IoError ioError = IoError::Unknown;
    CPPUNIT_ASSERT(IoHelper::deleteItem(_rootFolderPath, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(IoHelper::createDirectory(_rootFolderPath, false, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    _syncPal->_localFSObserverWorker->start();
    CPPUNIT_ASSERT(TimeoutHelper::waitFor([this]() { return !_syncPal->_localFSObserverWorker->isRunning(); },
                                          std::chrono::seconds(20), std::chrono::milliseconds(5)));
    ExitInfo exitInfo = {_syncPal->_localFSObserverWorker->exitCode(), _syncPal->_localFSObserverWorker->exitCause()};
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::SystemError, ExitCause::SyncDirChanged), exitInfo);
}


void TestLocalFileSystemObserverWorker::testLFSOWithInitialSnapshot() {
    NodeSet ids;
    _syncPal->copySnapshots();
    _syncPal->liveSnapshot(ReplicaSide::Local).ids(ids);

    uint64_t fileCounter = 0;
    for (const auto &id: ids) {
        if (const auto name = _syncPal->liveSnapshot(ReplicaSide::Local).name(id);
            name == Str(".DS_Store") || name == Str(".ds_store")) {
            continue; // Ignore ".DS_Store"
        }

        const NodeId parentId = _syncPal->liveSnapshot(ReplicaSide::Local).parentId(id);
        SyncPath parentPath;
        if (bool ignore = false; !parentId.empty() &&
                                 _syncPal->liveSnapshot(ReplicaSide::Local).path(parentId, parentPath, ignore) &&
                                 parentPath.filename() == _subDirPath.filename()) {
            fileCounter++;
        }
    }
    CPPUNIT_ASSERT_EQUAL(nbFileInTestDir, fileCounter);
}

void TestLocalFileSystemObserverWorker::testLFSOWithFiles() {
    NodeId itemId;
    const SyncName filename = Str("test_file.txt");
    SyncPath testAbsolutePath = _rootFolderPath / filename;
    {
        /// Create file
        LOGW_DEBUG(_logger, L"***** test create file *****");
        testhelpers::generateOrEditTestFile(testAbsolutePath);

        Utility::msleep(1000); // Wait 1sec

        FileStat fileStat;
        bool exists = false;
        IoHelper::getFileStat(testAbsolutePath, &fileStat, exists);
        itemId = std::to_string(fileStat.inode);
        _syncPal->copySnapshots();
        CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Local).exists(itemId));
        SyncPath testSyncPath;
        bool ignore = false;
        CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Local).path(itemId, testSyncPath, ignore) && testSyncPath == filename);
    }

    {
        /// Edit file
        LOGW_DEBUG(_logger, L"***** test edit file *****");
        const SyncTime prevModTime = _syncPal->liveSnapshot(ReplicaSide::Local).lastModified(itemId);
        testhelpers::generateOrEditTestFile(testAbsolutePath);

        Utility::msleep(1000); // Wait 1sec
        _syncPal->copySnapshots();
        CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Local).lastModified(itemId) > prevModTime);
    }

    {
        /// Move file
        LOGW_DEBUG(_logger, L"***** test move file *****");
        SyncPath sourcePath = testAbsolutePath;
        SyncPath destinationPath = _subDirPath / filename;

        auto ioError = IoError::Unknown;
        IoHelper::moveItem(sourcePath, destinationPath, ioError);

        Utility::msleep(1000); // Wait 1sec
        _syncPal->copySnapshots();
        const NodeId parentId = _syncPal->liveSnapshot(ReplicaSide::Local).parentId(itemId);
        CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Local).name(parentId) == _subDirPath.filename());
        testAbsolutePath = destinationPath;
    }

    {
        /// Rename file
        LOGW_DEBUG(_logger, L"***** test rename file *****");
        SyncPath source = testAbsolutePath;
        SyncPath destinationPath = _subDirPath / Str("test_file_renamed.txt");

        auto ioError = IoError::Unknown;
        IoHelper::renameItem(source, destinationPath, ioError);

        Utility::msleep(1000); // Wait 1sec
        _syncPal->copySnapshots();
        CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Local).name(itemId) == Str("test_file_renamed.txt"));
        testAbsolutePath = destinationPath;
    }

    {
        /// Delete file
        LOGW_DEBUG(_logger, L"***** test delete file *****");
        auto ioError = IoError::Unknown;
        IoHelper::deleteItem(testAbsolutePath, ioError);

        Utility::msleep(1000); // Wait 1sec
        _syncPal->copySnapshots();
        CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Local).exists(itemId));
    }

    LOGW_DEBUG(_logger, L"Tests for files successful!");
}

void TestLocalFileSystemObserverWorker::testLFSOWithDuplicateFileNames() {
    // Create two files with the same name, up to encoding (NFC vs NFD).
    // On Windows and Linux systems, we expect to find two distinct items. But we will only consider one in the local snapshot and
    // we do not guarantee that it will always be the same one. However, during a synchronisation, we should always synchronize
    // the item for wich we detected a change last time. On MacOSX, a single item is expected as the system creates a single file
    // (overwrite).
#ifndef KD_MACOS // Duplicate file names are not allowed.
    using namespace testhelpers;
    _syncPal->_localFSObserverWorker->stop();
    _syncPal->_localFSObserverWorker->waitForExit();
    _syncPal->_localFSObserverWorker.reset();

    // Create a slow observer
    auto slowObserver = std::make_shared<MockLocalFileSystemObserverWorker>(_syncPal, "Local File System Observer", "LFSO");
    _syncPal->_localFSObserverWorker = slowObserver;
    _syncPal->_localFSObserverWorker->start();

    auto localFSO = std::dynamic_pointer_cast<LocalFileSystemObserverWorker>(_syncPal->_localFSObserverWorker);
    CPPUNIT_ASSERT(localFSO);

    int count = 0;
    while (!_syncPal->liveSnapshot(ReplicaSide::Local).isValid() ||
           !localFSO->_folderWatcher->isReady()) { // Wait for the snapshot generation
        Utility::msleep(100);
        CPPUNIT_ASSERT(count++ < 20); // Do not wait more than 2s
    }

    LOGW_DEBUG(_logger, L"***** test create file with NFC-encoded name *****");
    SnapshotRevision previousRevision = _syncPal->liveSnapshot(ReplicaSide::Local).revision();
    generateOrEditTestFile(_rootFolderPath / makeNfcSyncName());
    slowObserver->waitForUpdate(previousRevision);

    FileStat fileStat;
    bool exists = false;

    IoHelper::getFileStat(_rootFolderPath / makeNfcSyncName(), &fileStat, exists);
    const NodeId nfcNamedItemId = std::to_string(fileStat.inode);
    CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Local).exists(nfcNamedItemId));

    LOGW_DEBUG(_logger, L"***** test create file with NFD-encoded name *****");
    previousRevision = _syncPal->liveSnapshot(ReplicaSide::Local).revision();
    generateOrEditTestFile(_rootFolderPath / makeNfdSyncName()); // Should replace the NFC item in the snapshot.
    slowObserver->waitForUpdate(previousRevision);

    IoHelper::getFileStat(_rootFolderPath / makeNfdSyncName(), &fileStat, exists);
    const NodeId nfdNamedItemId = std::to_string(fileStat.inode);

    // Check that only the last modified item is in the snapshot.
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Local).exists(nfcNamedItemId));
    CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Local).exists(nfdNamedItemId));

    SyncPath testSyncPath;
    bool ignore = false;

    CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Local).path(nfdNamedItemId, testSyncPath, ignore) &&
                   testSyncPath == makeNfdSyncName());
#endif
}

void TestLocalFileSystemObserverWorker::testLFSOWithDirs() {
    NodeId itemId;
    SyncName dirname = Str("test_dir");
    SyncPath testAbsolutePath = _rootFolderPath / dirname;
    {
        /// Create dir
        LOGW_DEBUG(_logger, L"***** test create dir *****");
        testhelpers::generateOrEditTestFile(testAbsolutePath);

        Utility::msleep(1000); // Wait 1sec

        FileStat fileStat;
        bool exists = false;
        IoHelper::getFileStat(testAbsolutePath, &fileStat, exists);
        itemId = std::to_string(fileStat.inode);
        CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Local).exists(itemId));
        SyncPath path;
        bool ignore = false;
        _syncPal->liveSnapshot(ReplicaSide::Local).path(itemId, path, ignore);
        CPPUNIT_ASSERT(path == CommonUtility::relativePath(_rootFolderPath, testAbsolutePath));
    }

    {
        /// Move dir
        LOGW_DEBUG(_logger, L"***** test move dir *****");
        SyncPath sourcePath = testAbsolutePath;
        SyncPath destinationPath = _subDirPath / dirname;
        auto ioError = IoError::Unknown;
        IoHelper::moveItem(sourcePath, destinationPath, ioError);

        Utility::msleep(1000); // Wait 1sec

        SyncPath path;
        bool ignore = false;
        _syncPal->liveSnapshot(ReplicaSide::Local).path(itemId, path, ignore);
        CPPUNIT_ASSERT(path == CommonUtility::relativePath(_rootFolderPath, destinationPath));
        testAbsolutePath = destinationPath;
    }

    {
        /// Rename dir
        LOGW_DEBUG(_logger, L"***** test rename dir *****");
        SyncPath sourcePath = testAbsolutePath;
        SyncPath destinationPath = _subDirPath / (dirname + Str("_renamed"));
        auto ioError = IoError::Unknown;
        IoHelper::renameItem(sourcePath, destinationPath, ioError);

        Utility::msleep(1000); // Wait 1sec

        CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Local).name(itemId) == destinationPath.filename());
        testAbsolutePath = destinationPath;
    }

    {
        // Generate test item outside sync folder
        SyncName dirName = Str("dir_copy");
        SyncPath sourcePath = _tempDir.path() / dirName;
        auto ioError = IoError::Unknown;
        IoHelper::copyFileOrDirectory(_subDirPath, sourcePath, ioError);

        /// Move dir from outside sync dir
        LOGW_DEBUG(_logger, L"***** test move dir from outside sync dir *****");
        SyncPath destinationPath = _rootFolderPath / dirName;
        IoHelper::moveItem(sourcePath, destinationPath, ioError);

        Utility::msleep(1000); // Wait 1sec

        FileStat fileStat;
        bool exists = false;
        IoHelper::getFileStat(destinationPath, &fileStat, exists);
        itemId = std::to_string(fileStat.inode);
        CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Local).exists(itemId));

        testAbsolutePath = destinationPath / Str("test0.txt");
        IoHelper::getFileStat(testAbsolutePath, &fileStat, exists);
        itemId = std::to_string(fileStat.inode);
        CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Local).exists(itemId));
    }
    LOGW_DEBUG(_logger, L"Tests for directories successful!");
}


void TestLocalFileSystemObserverWorker::testLFSODeleteDir() {
    /// Delete dir and all its content
    LOGW_DEBUG(_logger, L"***** test delete dir *****");
    auto ioError = IoError::Unknown;
    IoHelper::deleteItem(_subDirPath, ioError);

    Utility::msleep(1000); // Wait 1sec

    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Local).exists(_testFiles[0].first));

    LOGW_DEBUG(_logger, L"***** Tests for copy and deletion of directories succesfully finished! *****");
}

void TestLocalFileSystemObserverWorker::testLFSOWithSpecialCases1() {
    // Test 4.3.3.2, p.62 - a) delete(“x”) + create(“x”) + edit(“x”,<newcontent>) + move(“x”,“y”)
    LOGW_DEBUG(_logger, L"***** delete(x) + create(x) + edit(x,<newcontent>) + move(x,y) *****");

    const SyncName testFilename = Str("test0.txt");
    SyncPath sourcePath = _subDirPath / testFilename;
    //// delete
    FileStat fileStat;
    bool exists = false;
    IoHelper::getFileStat(sourcePath, &fileStat, exists);
    NodeId initItemId = std::to_string(fileStat.inode);

    auto ioError = IoError::Unknown;
    IoHelper::deleteItem(sourcePath, ioError);
    //// create
    KDC::testhelpers::generateOrEditTestFile(sourcePath);
    IoHelper::getFileStat(sourcePath, &fileStat, exists);
    NodeId newItemId = std::to_string(fileStat.inode);
    //// edit
    KDC::testhelpers::generateOrEditTestFile(sourcePath);
    //// move
    SyncPath destinationPath = _rootFolderPath / testFilename;
    IoHelper::moveItem(sourcePath, destinationPath, ioError);

    Utility::msleep(1000); // Wait 1sec

    CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Local).exists(newItemId));
    CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Local).name(newItemId) == testFilename);
}

void TestLocalFileSystemObserverWorker::testLFSOWithSpecialCases2() {
    // Test 4.3.3.2, p.62 - b) move(“x”,“y”) + create(“x”) + edit(“x”,<newcontent>) + delete(“x”)
    LOGW_DEBUG(_logger, L"***** move(x,y) + create(x) + edit(x,<newcontent>) + delete(x) *****");

    const SyncName testFilename = Str("test0.txt");
    SyncPath sourcePath = _subDirPath / testFilename;
    SyncPath destinationPath = _rootFolderPath / testFilename;
    //// move
    FileStat fileStat;
    bool exists = false;
    IoHelper::getFileStat(sourcePath, &fileStat, exists);
    NodeId initItemId = std::to_string(fileStat.inode);

    auto ioError = IoError::Unknown;
    IoHelper::moveItem(sourcePath, destinationPath, ioError);
    //// create
    KDC::testhelpers::generateOrEditTestFile(sourcePath);
    IoHelper::getFileStat(sourcePath, &fileStat, exists);
    NodeId newItemId = std::to_string(fileStat.inode);
    //// edit
    KDC::testhelpers::generateOrEditTestFile(sourcePath);
    //// delete
    IoHelper::deleteItem(sourcePath, ioError);

    Utility::msleep(1000); // Wait 1sec

    CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Local).exists(initItemId));
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Local).exists(newItemId));
    CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Local).name(initItemId) == testFilename);
}

void TestLocalFileSystemObserverWorker::testLFSOFastMoveDeleteMove() { // MS Office test
    LOGW_DEBUG(_logger, L"***** Test fast move/delete *****")
    _syncPal->_localFSObserverWorker->stop();
    _syncPal->_localFSObserverWorker->waitForExit();
    _syncPal->_localFSObserverWorker.reset();

    // Create a slow observer
    auto slowObserver = std::make_shared<MockLocalFileSystemObserverWorker>(_syncPal, "Local File System Observer", "LFSO");
    _syncPal->_localFSObserverWorker = slowObserver;
    _syncPal->_localFSObserverWorker->start();

    auto localFSO = std::dynamic_pointer_cast<LocalFileSystemObserverWorker>(_syncPal->_localFSObserverWorker);
    CPPUNIT_ASSERT(localFSO);

    int count = 0;
    while (!_syncPal->liveSnapshot(ReplicaSide::Local).isValid() ||
           !localFSO->_folderWatcher->isReady()) { // Wait for the snapshot generation and folder watcher start
        Utility::msleep(100);
        CPPUNIT_ASSERT(count++ < 20); // Do not wait more than 2s
    }
    CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Local).exists(_testFiles[0].first));

    SnapshotRevision previousRevision = _syncPal->liveSnapshot(ReplicaSide::Local).revision();
    auto ioError = IoError::Unknown;
    const SyncPath destinationPath = _testFiles[0].second.parent_path() / (_testFiles[0].second.filename().string() + "2");
    CPPUNIT_ASSERT_MESSAGE(toString(ioError),
                           IoHelper::renameItem(_testFiles[0].second, destinationPath, ioError)); // test0.txt -> test0.txt2
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT_MESSAGE(
            toString(ioError),
            IoHelper::deleteItem(destinationPath, ioError)); // Delete test0.txt2 (before the previous rename is processed)
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT_MESSAGE(
            toString(ioError),
            IoHelper::renameItem(_testFiles[1].second, _testFiles[0].second,
                                 ioError)); // test1.txt -> test0.txt (before the previous rename and delete is processed)
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    LOG_DEBUG(_logger, "Operations finished")

    slowObserver->waitForUpdate(previousRevision);

    FileStat fileStat;
    CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::getFileStat(_testFiles[0].second, &fileStat, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Local).exists(_testFiles[0].first));
    CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Local).exists(std::to_string(fileStat.inode)));
}

void TestLocalFileSystemObserverWorker::testLFSOFastMoveDeleteMoveWithEncodingChange() {
    using namespace testhelpers;

    LOGW_DEBUG(_logger, L"***** Test fast move/delete with enconding change *****"); // Behaviour of MS office apps on macOS
    _syncPal->_localFSObserverWorker->stop();
    _syncPal->_localFSObserverWorker->waitForExit();
    _syncPal->_localFSObserverWorker.reset();

    // Create a slow observer
    auto slowObserver = std::make_shared<MockLocalFileSystemObserverWorker>(_syncPal, "Local File System Observer", "LFSO");
    _syncPal->_localFSObserverWorker = slowObserver;
    _syncPal->_localFSObserverWorker->start();

    int count = 0;

    FileStat fileStat;
    bool exists = false;

    auto localFSO = std::dynamic_pointer_cast<LocalFileSystemObserverWorker>(_syncPal->_localFSObserverWorker);
    CPPUNIT_ASSERT(localFSO);

    while (!_syncPal->liveSnapshot(ReplicaSide::Local).isValid() ||
           !localFSO->_folderWatcher->isReady()) { // Wait for the snapshot generation
        Utility::msleep(100);
        CPPUNIT_ASSERT(count++ < 20); // Do not wait more than 2s
    }

    // Create an NFC encoded file.
    SyncPath tmpDirPath = _testFiles[0].second.parent_path();
    SyncPath nfcFilePath = tmpDirPath / makeNfcSyncName();

    SnapshotRevision previousRevision = _syncPal->liveSnapshot(ReplicaSide::Local).revision();
    generateOrEditTestFile(nfcFilePath);
    IoHelper::getFileStat(nfcFilePath, &fileStat, exists);
    NodeId nfcFileId = std::to_string(fileStat.inode);

    // Prepare the path of the NFD encoded file.
    SyncPath nfdFilePath = tmpDirPath / makeNfdSyncName();

    slowObserver->waitForUpdate(previousRevision);

    CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Local).exists(nfcFileId));

    previousRevision = _syncPal->liveSnapshot(ReplicaSide::Local).revision();

    auto ioError = IoError::Unknown;
    SyncPath destinationPath = tmpDirPath / (nfcFilePath.filename().string() + "2");
    CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::renameItem(nfcFilePath, destinationPath, ioError)); // nfcFile -> nfcFile2
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT_MESSAGE(
            toString(ioError),
            IoHelper::deleteItem(destinationPath, ioError)); // Delete nfcFile2 (before the previous rename is processed)
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT_MESSAGE(
            toString(ioError),
            IoHelper::renameItem(_testFiles[1].second, nfdFilePath,
                                 ioError)); // test1.txt -> nfdFile (before the previous rename and delete is processed)
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    slowObserver->waitForUpdate(previousRevision);

    CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::getFileStat(nfdFilePath, &fileStat, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    NodeId nfdFileId = std::to_string(fileStat.inode);

    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Local).exists(nfcFileId));
    CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Local).exists(nfdFileId));
}

void TestLocalFileSystemObserverWorker::testInvalidateSnapshot() {
    CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Local).isValid());
    _syncPal->_localFSObserverWorker->invalidateSnapshot();
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Local).isValid());
}

void TestLocalFileSystemObserverWorker::testInvalidateCounter() {
    _syncPal->_localFSObserverWorker->tryToInvalidateSnapshot();
    CPPUNIT_ASSERT_EQUAL(true, _syncPal->liveSnapshot(ReplicaSide::Local).isValid()); // Snapshot is not invalidated yet.
    _syncPal->_localFSObserverWorker->tryToInvalidateSnapshot();
    CPPUNIT_ASSERT_EQUAL(true, _syncPal->liveSnapshot(ReplicaSide::Local).isValid()); // Snapshot is not invalidated yet.
    _syncPal->_localFSObserverWorker->tryToInvalidateSnapshot();
    CPPUNIT_ASSERT_EQUAL(false, _syncPal->liveSnapshot(ReplicaSide::Local).isValid()); // Snapshot has been invalidated.

    Utility::msleep(1000); // Wait for the snapshot to be rebuilt

    CPPUNIT_ASSERT_EQUAL(true, _syncPal->liveSnapshot(ReplicaSide::Local).isValid()); // Snapshot is now valid again.
    _syncPal->_localFSObserverWorker->tryToInvalidateSnapshot();
    CPPUNIT_ASSERT_EQUAL(true, _syncPal->liveSnapshot(ReplicaSide::Local).isValid()); // Snapshot is not invalidated yet.
    _syncPal->_localFSObserverWorker->tryToInvalidateSnapshot();
    CPPUNIT_ASSERT_EQUAL(true, _syncPal->liveSnapshot(ReplicaSide::Local).isValid()); // Snapshot is not invalidated yet.
    _syncPal->_localFSObserverWorker->tryToInvalidateSnapshot();
    CPPUNIT_ASSERT_EQUAL(false, _syncPal->liveSnapshot(ReplicaSide::Local).isValid()); // Snapshot has been invalidated.
}

void MockLocalFileSystemObserverWorker::waitForUpdate(SnapshotRevision previousRevision,
                                                      const std::chrono::milliseconds timeoutMs) const {
    using namespace std::chrono;
    const auto start = system_clock::now();
    while (previousRevision == liveSnapshot().revision() &&
           duration_cast<milliseconds>(system_clock::now() - start) < timeoutMs) {
        Utility::msleep(10);
    }
    CPPUNIT_ASSERT_LESS(timeoutMs.count(), duration_cast<milliseconds>(system_clock::now() - start).count());
    while (_updating && duration_cast<milliseconds>(system_clock::now() - start) < timeoutMs) {
        Utility::msleep(10);
    }
    CPPUNIT_ASSERT_LESS(timeoutMs.count(), duration_cast<milliseconds>(system_clock::now() - start).count());
}

} // namespace KDC
