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

#include "testlocalfilesystemobserverworker.h"
#include "test_classes/syncpaltest.h"

#include "config.h"
#if defined(_WIN32)
#include "update_detection/file_system_observer/localfilesystemobserverworker_win.h"
#else
#include "update_detection/file_system_observer/localfilesystemobserverworker_unix.h"
#endif
#include "syncpal/tmpblacklistmanager.h"
#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/log/log.h"
#include "requests/parameterscache.h"
#include "test_utility/testhelpers.h"

#include <log4cplus/loggingmacros.h>

#include <Poco/Path.h>
#include <Poco/File.h>

using namespace CppUnit;

namespace KDC {

const uint64_t nbFileInTestDir = 5; // Test directory contains 5 files

void TestLocalFileSystemObserverWorker::setUp() {
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
        _testFiles.emplace_back(std::make_pair(std::to_string(fileStat.inode), filepath));
    }

    // Create parmsDb
    bool alreadyExists = false;
    const SyncPath parmsDbPath = Db::makeDbName(alreadyExists, true);

    ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);
    ParametersCache::instance()->parameters().setExtendedLog(true);
    ParametersCache::instance()->parameters().setSyncHiddenFiles(true);

    bool constraintError = false;
    ParmsDb::instance()->insertExclusionTemplate(
            ExclusionTemplate(".DS_Store", true),
            constraintError); // TODO : to be removed once we have a default list of file excluded implemented
    const SyncPath syncDbPath = Db::makeDbName(1, 1, 1, 1, alreadyExists, true);

    // Create SyncPal
    _syncPal = std::make_shared<SyncPalTest>(syncDbPath, KDRIVE_VERSION_STRING, true);
    _syncPal->syncDb()->setAutoDelete(true);
    _syncPal->setLocalPath(_rootFolderPath);
    _syncPal->_tmpBlacklistManager = std::make_shared<TmpBlacklistManager>(_syncPal);
#if defined(_WIN32)
    _syncPal->_localFSObserverWorker = std::shared_ptr<FileSystemObserverWorker>(
            new LocalFileSystemObserverWorker_win(_syncPal, "Local File System Observer", "LFSO"));
#else
    _syncPal->_localFSObserverWorker = std::shared_ptr<FileSystemObserverWorker>(
            new LocalFileSystemObserverWorker_unix(_syncPal, "Local File System Observer", "LFSO"));
#endif

    _syncPal->_localFSObserverWorker->start();

    Utility::msleep(1000); // Wait 1sec
}

void TestLocalFileSystemObserverWorker::tearDown() {
    LOGW_DEBUG(_logger, L"$$$$$ Tear Down $$$$$");

    if (_syncPal && _syncPal->_localFSObserverWorker) {
        _syncPal->_localFSObserverWorker->stop();
    }

    ParmsDb::instance()->close();
    ParmsDb::reset();
    if (_syncPal && _syncPal->syncDb()) {
        _syncPal->syncDb()->close();
    }
}

void TestLocalFileSystemObserverWorker::testLFSOWithInitialSnapshot() {
    std::unordered_set<NodeId> ids;
    _syncPal->snapshot(ReplicaSide::Local)->ids(ids);

    uint64_t fileCounter = 0;
    for (const auto &id: ids) {
        const auto name = _syncPal->snapshot(ReplicaSide::Local)->name(id);
        if (name == Str(".DS_Store") || name == Str(".ds_store")) {
            continue; // Ignore ".DS_Store"
        }

        const NodeId parentId = _syncPal->snapshot(ReplicaSide::Local)->parentId(id);
        SyncPath parentPath;
        if (!parentId.empty() && _syncPal->snapshot(ReplicaSide::Local)->path(parentId, parentPath) &&
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

        CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSide::Local)->exists(itemId));
        SyncPath testSyncPath;
        CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSide::Local)->path(itemId, testSyncPath) && testSyncPath == filename);
    }

    {
        /// Edit file
        LOGW_DEBUG(_logger, L"***** test edit file *****");
        const SyncTime prevModTime = _syncPal->snapshot(ReplicaSide::Local)->lastModified(itemId);
        testhelpers::generateOrEditTestFile(testAbsolutePath);

        Utility::msleep(1000); // Wait 1sec

        CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSide::Local)->lastModified(itemId) > prevModTime);
    }

    {
        /// Move file
        LOGW_DEBUG(_logger, L"***** test move file *****");
        SyncPath sourcePath = testAbsolutePath;
        SyncPath destinationPath = _subDirPath / filename;

        IoError ioError = IoError::Unknown;
        IoHelper::moveItem(sourcePath, destinationPath, ioError);

        Utility::msleep(1000); // Wait 1sec

        const NodeId parentId = _syncPal->snapshot(ReplicaSide::Local)->parentId(itemId);
        CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSide::Local)->name(parentId) == _subDirPath.filename());
        testAbsolutePath = destinationPath;
    }

    {
        /// Rename file
        LOGW_DEBUG(_logger, L"***** test rename file *****");
        SyncPath source = testAbsolutePath;
        SyncPath destinationPath = _subDirPath / Str("test_file_renamed.txt");

        IoError ioError = IoError::Unknown;
        IoHelper::renameItem(source, destinationPath, ioError);

        Utility::msleep(1000); // Wait 1sec

        CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSide::Local)->name(itemId) == Str("test_file_renamed.txt"));
        testAbsolutePath = destinationPath;
    }

    {
        /// Delete file
        LOGW_DEBUG(_logger, L"***** test delete file *****");
        IoError ioError = IoError::Unknown;
        IoHelper::deleteItem(testAbsolutePath, ioError);

        Utility::msleep(1000); // Wait 1sec

        CPPUNIT_ASSERT(!_syncPal->snapshot(ReplicaSide::Local)->exists(itemId));
    }

    LOGW_DEBUG(_logger, L"Tests for files successful!");
}

void TestLocalFileSystemObserverWorker::testLFSOWithDuplicateFileNames() {
    // Create two files with the same name, up to encoding (NFC vs NFC).
    // On Windows and Linux systems, we expect to find two distinct items. But we will only consider one in the local snapshot and
    // we do not guarantee that it will always be the same one. However, durring a synchronisation, we should always synchorize
    // the item for wich we detected a change last time. On MacOSX, a single item is expected as the system creates a single file
    // (overwrite).
#ifndef __APPLE__ // Duplicate file names are not allowed.
    using namespace testhelpers;
    _syncPal->_localFSObserverWorker->stop();
    _syncPal->_localFSObserverWorker.reset();

    // Create a slow observer
    auto slowObserver = std::make_shared<MockLocalFileSystemObserverWorker>(_syncPal, "Local File System Observer", "LFSO");
    _syncPal->_localFSObserverWorker = slowObserver;
    _syncPal->_localFSObserverWorker->start();

     int count = 0;
    while (!_syncPal->snapshot(ReplicaSide::Local)->isValid()) { // Wait for the snapshot generation
        Utility::msleep(100);
        CPPUNIT_ASSERT(count++ < 20); // Do not wait more than 2s
    }

    LOGW_DEBUG(_logger, L"***** test create file with NFC-encoded name *****");
    generateOrEditTestFile(_rootFolderPath / makeNfcSyncName());
    CPPUNIT_ASSERT_MESSAGE("No update detected in the expected time.", slowObserver->waitForUpdate());

    FileStat fileStat;
    bool exists = false;

    IoHelper::getFileStat(_rootFolderPath / makeNfcSyncName(), &fileStat, exists);
    const NodeId nfcNamedItemId = std::to_string(fileStat.inode);
    CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSide::Local)->exists(nfcNamedItemId));

    LOGW_DEBUG(_logger, L"***** test create file with NFD-encoded name *****");
    generateOrEditTestFile(_rootFolderPath / makeNfdSyncName()); // Should replace the Nfc item in the snapshot.
    CPPUNIT_ASSERT_MESSAGE("No update detected in the expected time.", slowObserver->waitForUpdate());

    IoHelper::getFileStat(_rootFolderPath / makeNfdSyncName(), &fileStat, exists);
    const NodeId nfdNamedItemId = std::to_string(fileStat.inode);

    // Check that only the last modified item is in the snapshot.
    CPPUNIT_ASSERT(!_syncPal->snapshot(ReplicaSide::Local)->exists(nfcNamedItemId));
    CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSide::Local)->exists(nfdNamedItemId));
    SyncPath testSyncPath;

    CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSide::Local)->path(nfdNamedItemId, testSyncPath) &&
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
        CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSide::Local)->exists(itemId));
        SyncPath path;
        _syncPal->snapshot(ReplicaSide::Local)->path(itemId, path);
        CPPUNIT_ASSERT(path == CommonUtility::relativePath(_rootFolderPath, testAbsolutePath));
    }

    {
        /// Move dir
        LOGW_DEBUG(_logger, L"***** test move dir *****");
        SyncPath sourcePath = testAbsolutePath;
        SyncPath destinationPath = _subDirPath / dirname;
        IoError ioError = IoError::Unknown;
        IoHelper::moveItem(sourcePath, destinationPath, ioError);

        Utility::msleep(1000); // Wait 1sec

        SyncPath path;
        _syncPal->snapshot(ReplicaSide::Local)->path(itemId, path);
        CPPUNIT_ASSERT(path == CommonUtility::relativePath(_rootFolderPath, destinationPath));
        testAbsolutePath = destinationPath;
    }

    {
        /// Rename dir
        LOGW_DEBUG(_logger, L"***** test rename dir *****");
        SyncPath sourcePath = testAbsolutePath;
        SyncPath destinationPath = _subDirPath / (dirname + Str("_renamed"));
        IoError ioError = IoError::Unknown;
        IoHelper::renameItem(sourcePath, destinationPath, ioError);

        Utility::msleep(1000); // Wait 1sec

        CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSide::Local)->name(itemId) == destinationPath.filename());
        testAbsolutePath = destinationPath;
    }

    {
        // Generate test item outside sync folder
        SyncName dirName = Str("dir_copy");
        SyncPath sourcePath = _tempDir.path() / dirName;
        IoError ioError = IoError::Unknown;
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
        CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSide::Local)->exists(itemId));

        testAbsolutePath = destinationPath / Str("test0.txt");
        IoHelper::getFileStat(testAbsolutePath, &fileStat, exists);
        itemId = std::to_string(fileStat.inode);
        CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSide::Local)->exists(itemId));
    }
    LOGW_DEBUG(_logger, L"Tests for directories successful!");
}


void TestLocalFileSystemObserverWorker::testLFSODeleteDir() {
    NodeId itemId;
    {
        /// Delete dir and all its content
        LOGW_DEBUG(_logger, L"***** test delete dir *****");
        IoError ioError = IoError::Unknown;
        IoHelper::deleteItem(_subDirPath, ioError);

        Utility::msleep(1000); // Wait 1sec

        CPPUNIT_ASSERT(!_syncPal->snapshot(ReplicaSide::Local)->exists(itemId));
        CPPUNIT_ASSERT(!_syncPal->snapshot(ReplicaSide::Local)->exists(_testFiles[0].first));
    }

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

    IoError ioError = IoError::Unknown;
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

    CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSide::Local)->exists(newItemId));
    CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSide::Local)->name(newItemId) == testFilename);
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

    IoError ioError = IoError::Unknown;
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

    CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSide::Local)->exists(initItemId));
    CPPUNIT_ASSERT(!_syncPal->snapshot(ReplicaSide::Local)->exists(newItemId));
    CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSide::Local)->name(initItemId) == testFilename);
}

void TestLocalFileSystemObserverWorker::testLFSOFastMoveDeleteMove() { // MS Office test
    LOGW_DEBUG(_logger, L"***** Test fast move/delete *****");
    _syncPal->_localFSObserverWorker->stop();
    _syncPal->_localFSObserverWorker.reset();

    // Create a slow observer
    auto slowObserver = std::make_shared<MockLocalFileSystemObserverWorker>(_syncPal, "Local File System Observer", "LFSO");
    _syncPal->_localFSObserverWorker = slowObserver;
    _syncPal->_localFSObserverWorker->start();

    int count = 0;
    while (!_syncPal->snapshot(ReplicaSide::Local)->isValid()) { // Wait for the snapshot generation
        Utility::msleep(100);
        CPPUNIT_ASSERT(count++ < 20); // Do not wait more than 2s
    }
    CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSide::Local)->exists(_testFiles[0].first));

    IoError ioError = IoError::Unknown;
    SyncPath destinationPath = _testFiles[0].second.parent_path() / (_testFiles[0].second.filename().string() + "2");
    CPPUNIT_ASSERT(IoHelper::renameItem(_testFiles[0].second, destinationPath, ioError)); // test0.txt -> test0.txt2
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(IoHelper::deleteItem(destinationPath, ioError)); // Delete test0.txt2 (before the previous rename is processed)
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(IoHelper::renameItem(_testFiles[1].second, _testFiles[0].second,
                                        ioError)); // test1.txt -> test0.txt (before the previous rename and delete is processed)
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    CPPUNIT_ASSERT_MESSAGE("No update detected in the expected time.", slowObserver->waitForUpdate());

    FileStat fileStat;
    CPPUNIT_ASSERT(IoHelper::getFileStat(_testFiles[0].second, &fileStat, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    CPPUNIT_ASSERT(!_syncPal->snapshot(ReplicaSide::Local)->exists(_testFiles[0].first));
    CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSide::Local)->exists(std::to_string(fileStat.inode)));
}

void TestLocalFileSystemObserverWorker::testLFSOFastMoveDeleteMoveWithEncodingChange() {
    using namespace testhelpers;

    LOGW_DEBUG(_logger, L"***** Test fast move/delete with enconding change*****"); // Behaviour ou MS office apps on macOS
    _syncPal->_localFSObserverWorker->stop();
    _syncPal->_localFSObserverWorker.reset();

    // Create a slow observer
    auto slowObserver = std::make_shared<MockLocalFileSystemObserverWorker>(_syncPal, "Local File System Observer", "LFSO");
    _syncPal->_localFSObserverWorker = slowObserver;
    _syncPal->_localFSObserverWorker->start();

    int count = 0;

    FileStat fileStat;
    bool exists = false;

    // Create an NFC encoded file.
    SyncPath tmpDirPath = _testFiles[0].second.parent_path();
    SyncPath nfcFilePath = tmpDirPath / makeNfcSyncName();
    generateOrEditTestFile(nfcFilePath);
    NodeId nfcFileId;
    IoHelper::getFileStat(nfcFilePath, &fileStat, exists);
    nfcFileId = std::to_string(fileStat.inode);

    // Prepare the path of the nfd encoded file.
    SyncPath nfdFilePath = tmpDirPath / makeNfdSyncName();

    while (!_syncPal->snapshot(ReplicaSide::Local)->isValid()) { // Wait for the snapshot generation
        Utility::msleep(100);
        CPPUNIT_ASSERT(count++ < 20); // Do not wait more than 2s
    }

    CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSide::Local)->exists(nfcFileId));

    IoError ioError = IoError::Unknown;
    SyncPath destinationPath = tmpDirPath / (nfcFilePath.filename().string() + "2");
    CPPUNIT_ASSERT(IoHelper::renameItem(nfcFilePath, destinationPath, ioError)); // nfcFile -> nfcFile2
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(IoHelper::deleteItem(destinationPath, ioError)); // Delete nfcFile2 (before the previous rename is processed)
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(IoHelper::renameItem(_testFiles[1].second, nfdFilePath,
                                        ioError)); // test1.txt -> nfdFile (before the previous rename and delete is processed)
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    CPPUNIT_ASSERT_MESSAGE("No update detected in the expected time.", slowObserver->waitForUpdate());

    CPPUNIT_ASSERT(IoHelper::getFileStat(nfdFilePath, &fileStat, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    NodeId nfdFileId = std::to_string(fileStat.inode);

    CPPUNIT_ASSERT(!_syncPal->snapshot(ReplicaSide::Local)->exists(nfcFileId));
    CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSide::Local)->exists(nfdFileId));
}

bool MockLocalFileSystemObserverWorker::waitForUpdate(uint64_t timeoutMs) const {
    using namespace std::chrono;
    auto start = system_clock::now();
    while (!_updating && duration_cast<milliseconds>(system_clock::now() - start).count() < timeoutMs) {
        Utility::msleep(10);
    }
    while (_updating && duration_cast<milliseconds>(system_clock::now() - start).count() < timeoutMs) {
        Utility::msleep(10);
    }
    return duration_cast<milliseconds>(system_clock::now() - start).count() < timeoutMs;
}

} // namespace KDC
