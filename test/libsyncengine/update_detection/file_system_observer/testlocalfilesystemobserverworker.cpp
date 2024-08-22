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

const uint64_t nbFileInTestDir = 5;  // Test directory contains 5 files

void TestLocalFileSystemObserverWorker::setUp() {
    _logger = Log::instance()->getLogger();

    LOGW_DEBUG(_logger, L"$$$$$ Set Up $$$$$");

    // Generate test files
    _tempDir = LocalTemporaryDirectory("TestLocalFileSystemObserverWorker");
    _rootFolderPath = _tempDir.path() / "sync_folder";
    _subDirPath = _rootFolderPath / "sub_dir";
    Poco::File(Path2Str(_subDirPath)).createDirectories();
    for (int i = 0; i < nbFileInTestDir; i++) {
        std::string filename = "test" + std::to_string(i) + ".txt";
        SyncPath filepath = _subDirPath / filename;
        KDC::testhelpers::generateTestFile(filepath);

        if (i == 0) {
            FileStat fileStat;
            bool exists = false;
            IoHelper::getFileStat(filepath, &fileStat, exists);
            _testFileId = std::to_string(fileStat.inode);
        }
    }

    // Create parmsDb
    bool alreadyExists = false;
    SyncPath parmsDbPath = Db::makeDbName(alreadyExists, true);

    ParmsDb::instance(parmsDbPath, "3.4.0", true, true);
    ParametersCache::instance()->parameters().setExtendedLog(true);
    ParametersCache::instance()->parameters().setSyncHiddenFiles(true);

    bool constraintError = false;
    ParmsDb::instance()->insertExclusionTemplate(
        ExclusionTemplate(".DS_Store", true),
        constraintError);  // TODO : to be removed once we have a default list of file excluded implemented
    const SyncPath syncDbPath = Db::makeDbName(1, 1, 1, 1, alreadyExists, true);

    // Create SyncPal
    _syncPal = std::make_shared<SyncPalTest>(syncDbPath, "3.4.0", true);
    _syncPal->syncDb()->setAutoDelete(true);
    _syncPal->setLocalPath(_rootFolderPath);

#if defined(_WIN32)
    _syncPal->_localFSObserverWorker = std::shared_ptr<FileSystemObserverWorker>(
        new LocalFileSystemObserverWorker_win(_syncPal, "Local File System Observer", "LFSO"));
#else
    _syncPal->_localFSObserverWorker = std::shared_ptr<FileSystemObserverWorker>(
        new LocalFileSystemObserverWorker_unix(_syncPal, "Local File System Observer", "LFSO"));
#endif

    _syncPal->_localFSObserverWorker->start();

    Utility::msleep(1000);  // Wait 1sec
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

void TestLocalFileSystemObserverWorker::testFolderWatcherWithInitialSnapshot() {
    std::unordered_set<NodeId> ids;
    _syncPal->snapshot(ReplicaSide::Local)->ids(ids);

    uint64_t fileCounter = 0;
    for (const auto &id : ids) {
        const auto name = _syncPal->snapshot(ReplicaSide::Local)->name(id);
        if (name == Str(".DS_Store") || name == Str(".ds_store")) {
            continue;  // Ignore ".DS_Store"
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

void TestLocalFileSystemObserverWorker::testFolderWatcherWithFiles() {
    NodeId itemId;
    const SyncName filename = Str("test_file.txt");
    SyncPath testAbsolutePath = _rootFolderPath / filename;
    {
        /// Create file
        LOGW_DEBUG(_logger, L"***** test create file *****");
        const std::string testCallStr = R"(echo "This is a create test" >> )" + testAbsolutePath.make_preferred().string();
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

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
        const std::string testCallStr = R"(echo "This is an edit test" >> )" + testAbsolutePath.make_preferred().string();
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSide::Local)->lastModified(itemId) > prevModTime);
    }

    {
        /// Move file
        LOGW_DEBUG(_logger, L"***** test move file *****");
        SyncPath source = testAbsolutePath;
        SyncPath target = _subDirPath / filename;
#ifdef _WIN32
        const std::string testCallStr =
            "move " + source.make_preferred().string() + " " + target.make_preferred().string() + " >nil";
#else
        const std::string testCallStr = "mv " + source.make_preferred().string() + " " + target.make_preferred().string();
#endif
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        const NodeId parentId = _syncPal->snapshot(ReplicaSide::Local)->parentId(itemId);
        CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSide::Local)->name(parentId) == _subDirPath.filename());
        testAbsolutePath = target;
    }

    {
        /// Rename file
        LOGW_DEBUG(_logger, L"***** test rename file *****");
        SyncPath source = testAbsolutePath;
        SyncPath target = _subDirPath / Str("test_file_renamed.txt");
#ifdef _WIN32
        const std::string testCallStr = "ren " + source.make_preferred().string() + " " + target.filename().string();
#else
        const std::string testCallStr =
            "mv " + source.make_preferred().string() + " " + target.make_preferred().string() + " >nil";
#endif
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSide::Local)->name(itemId) == Str("test_file_renamed.txt"));
        testAbsolutePath = target;
    }

    {
        /// Delete file
        LOGW_DEBUG(_logger, L"***** test delete file *****");
#ifdef _WIN32
        const std::string testCallStr = "del " + testAbsolutePath.make_preferred().string();
#else
        const std::string testCallStr = "rm -r " + testAbsolutePath.make_preferred().string();
#endif
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        CPPUNIT_ASSERT(!_syncPal->snapshot(ReplicaSide::Local)->exists(itemId));
    }

    LOGW_DEBUG(_logger, L"Tests for files successful!");
}

void TestLocalFileSystemObserverWorker::testFolderWatcherWithDirs() {
    NodeId itemId;
    SyncName dirname = Str("test_dir");
    SyncPath testAbsolutePath = _rootFolderPath / dirname;
    {
        /// Create dir
        LOGW_DEBUG(_logger, L"***** test create dir *****");
        const std::string testCallStr = "mkdir " + testAbsolutePath.make_preferred().string();
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

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
        SyncPath source = testAbsolutePath;
        SyncPath target = _subDirPath / dirname;
#ifdef _WIN32
        const std::string testCallStr =
            "move " + source.make_preferred().string() + " " + target.make_preferred().string() + " >nil";
#else
        const std::string testCallStr = "mv " + source.make_preferred().string() + " " + target.make_preferred().string();
#endif
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        SyncPath path;
        _syncPal->snapshot(ReplicaSide::Local)->path(itemId, path);
        CPPUNIT_ASSERT(path == CommonUtility::relativePath(_rootFolderPath, target));
        testAbsolutePath = target;
    }

    {
        /// Rename dir
        LOGW_DEBUG(_logger, L"***** test rename dir *****");
        SyncPath source = testAbsolutePath;
        SyncPath target = _subDirPath / Str("A_renamed");
#ifdef _WIN32
        const std::string testCallStr = "ren " + source.make_preferred().string() + " " + target.filename().string() + " >nil";
#else
        const std::string testCallStr = "mv " + source.make_preferred().string() + " " + target.make_preferred().string();
#endif
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSide::Local)->name(itemId) == target.filename());
        testAbsolutePath = target;
    }

    {
        // Generate test item outside sync folder
        SyncName dirName = Str("dir_copy");
        SyncPath sourcePath = _tempDir.path() / dirName;
        std::filesystem::copy(_subDirPath, sourcePath);

        /// Move dir from outside sync dir
        LOGW_DEBUG(_logger, L"***** test move dir from outside sync dir *****");

        SyncPath destinationPath = _rootFolderPath / dirName;
#ifdef _WIN32
        const std::string testCallStr =
            "move " + sourcePath.make_preferred().string() + " " + destinationPath.make_preferred().string() + " >nil";
#else
        const std::string testCallStr =
            "mv " + sourcePath.make_preferred().string() + " " + destinationPath.make_preferred().string();
#endif
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

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


void TestLocalFileSystemObserverWorker::testFolderWatcherDeleteDir() {
    NodeId itemId;
    {
        /// Delete dir and all its content
        LOGW_DEBUG(_logger, L"***** test delete dir *****");
#ifdef _WIN32
        const std::string testCallStr = "rmdir /s /q " + _subDirPath.make_preferred().string();
#else
        const std::string testCallStr = "rm -r " + _subDirPath.make_preferred().string();
#endif
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        CPPUNIT_ASSERT(!_syncPal->snapshot(ReplicaSide::Local)->exists(itemId));
        CPPUNIT_ASSERT(!_syncPal->snapshot(ReplicaSide::Local)->exists(_testFileId));
    }

    LOGW_DEBUG(_logger, L"***** Tests for copy and deletion of directories succesfully finished! *****");
}

void TestLocalFileSystemObserverWorker::testFolderWatcherWithSpecialCases1() {
    // Test 4.3.3.2, p.62 - a) delete(“x”) + create(“x”) + edit(“x”,<newcontent>) + move(“x”,“y”)
    LOGW_DEBUG(_logger, L"***** delete(x) + create(x) + edit(x,<newcontent>) + move(x,y) *****");

    const SyncName testFilename = Str("test0.txt");
    SyncPath sourcePath = _subDirPath / testFilename;
    //// delete
    FileStat fileStat;
    bool exists = false;
    IoHelper::getFileStat(sourcePath, &fileStat, exists);
    NodeId initItemId = std::to_string(fileStat.inode);

#ifdef _WIN32
    std::string testCallStr = "del " + sourcePath.make_preferred().string();
#else
    std::string testCallStr = "rm -r " + sourcePath.make_preferred().string();
#endif
    std::system(testCallStr.c_str());
    //// create
    KDC::testhelpers::generateTestFile(sourcePath);
    IoHelper::getFileStat(sourcePath, &fileStat, exists);
    NodeId newItemId = std::to_string(fileStat.inode);
    //// edit
    testCallStr = R"(echo "This is an edit test" >>  )" + sourcePath.make_preferred().string();
    std::system(testCallStr.c_str());
    //// move
    SyncPath destinationPath = _rootFolderPath / testFilename;
#ifdef _WIN32
    testCallStr = "move " + sourcePath.make_preferred().string() + " " + destinationPath.make_preferred().string() + " >nil";
#else
    testCallStr = "mv " + sourcePath.make_preferred().string() + " " + destinationPath.make_preferred().string();
#endif
    std::system(testCallStr.c_str());

    Utility::msleep(1000);  // Wait 1sec

    CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSide::Local)->exists(newItemId));
    CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSide::Local)->name(newItemId) == testFilename);
}

void TestLocalFileSystemObserverWorker::testFolderWatcherWithSpecialCases2() {
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
#ifdef _WIN32
    std::string testCallStr =
        "move " + sourcePath.make_preferred().string() + " " + destinationPath.make_preferred().string() + " >nil";
#else
    std::string testCallStr = "mv " + sourcePath.make_preferred().string() + " " + destinationPath.make_preferred().string();
#endif
    std::system(testCallStr.c_str());
    //// create
    KDC::testhelpers::generateTestFile(sourcePath);
    IoHelper::getFileStat(sourcePath, &fileStat, exists);
    NodeId newItemId = std::to_string(fileStat.inode);
    //// edit
    testCallStr = R"(echo "This is an edit test" >>  )" + sourcePath.make_preferred().string();
    std::system(testCallStr.c_str());
    //// delete
#ifdef _WIN32
    testCallStr = "del " + sourcePath.make_preferred().string();
#else
    testCallStr = "rm -r " + sourcePath.make_preferred().native();
#endif
    std::system(testCallStr.c_str());

    Utility::msleep(1000);  // Wait 1sec

    CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSide::Local)->exists(initItemId));
    CPPUNIT_ASSERT(!_syncPal->snapshot(ReplicaSide::Local)->exists(newItemId));
    CPPUNIT_ASSERT(_syncPal->snapshot(ReplicaSide::Local)->name(initItemId) == testFilename);
}

}  // namespace KDC
