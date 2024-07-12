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

#include <log4cplus/loggingmacros.h>

#include <Poco/Path.h>
#include <Poco/File.h>

using namespace CppUnit;

namespace KDC {

const SyncPath TestLocalFileSystemObserverWorker::_testFolderPath = SyncPath(TEST_DIR) / "test_ci" / "test_local_FSO";
const SyncPath TestLocalFileSystemObserverWorker::_testPicturesFolderName = SyncPath("test_pictures");
const uint64_t TestLocalFileSystemObserverWorker::_nbFileInTestDir = 5;  // Test directory contains 5 files

void TestLocalFileSystemObserverWorker::setUp() {
    _logger = Log::instance()->getLogger();

    LOGW_DEBUG(_logger, L"$$$$$ Set Up $$$$$");

    _testRootFolderPath = _tempDir.path / "sync_folder";

    Poco::File((_testRootFolderPath / "A" / "AA").string()).createDirectories();
    Poco::File((_testRootFolderPath / "A" / "AB").string()).createDirectories();
    Poco::File((_testRootFolderPath / "B" / "BA").string()).createDirectories();
    Poco::File((_testRootFolderPath / "B" / "BB").string()).createDirectories();

    Poco::File((_testFolderPath / "test_dir").string()).copyTo((_testRootFolderPath / _testPicturesFolderName).string());
    Poco::File((_testFolderPath / "test_a").string()).copyTo((_testRootFolderPath / "test_a").string());
    Poco::File((_testFolderPath / "test_b").string()).copyTo((_testRootFolderPath / "test_b").string());

    // Create parmsDb
    bool alreadyExists = false;
    SyncPath parmsDbPath = Db::makeDbName(alreadyExists);
    std::filesystem::remove(parmsDbPath);
    ParmsDb::instance(parmsDbPath, "3.4.0", true, true);
    ParmsDb::instance()->setAutoDelete(true);
    ParametersCache::instance()->parameters().setExtendedLog(true);
    ParametersCache::instance()->parameters().setSyncHiddenFiles(true);

    bool constraintError = false;
    ParmsDb::instance()->insertExclusionTemplate(
        ExclusionTemplate(".DS_Store", true),
        constraintError);  // TODO : to be removed once we have a default list of file excluded implemented
    const SyncPath syncDbPath = Db::makeDbName(1, 1, 1, 1, alreadyExists);
    std::filesystem::remove(syncDbPath);

    // Create SyncPal
    _syncPal = std::shared_ptr<SyncPal>(new SyncPalTest(syncDbPath, "3.4.0", true));
    _syncPal->getSyncDb()->setAutoDelete(true);
    _syncPal->setLocalPath(_testRootFolderPath);

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

    _syncPal->_localFSObserverWorker->stop();

    ParmsDb::instance()->close();

    if (_syncPal && _syncPal->getSyncDb()) {
        _syncPal->getSyncDb()->close();
    }
}

void TestLocalFileSystemObserverWorker::testFolderWatcherWithInitialSnapshot() {
    // Test initial snapshot
    {
        std::unordered_set<NodeId> ids;
        _syncPal->getSnapshot(ReplicaSideLocal)->ids(ids);

        uint64_t fileCounter = 0;
        for (const auto &id : ids) {
            const auto name = _syncPal->getSnapshot(ReplicaSideLocal)->name(id);
            if (name == Str(".DS_Store") || name == Str(".ds_store")) {
                continue;  // Ignore ".DS_Store"
            }

            const NodeId parentId = _syncPal->getSnapshot(ReplicaSideLocal)->parentId(id);
            SyncPath parentPath;
            if (!parentId.empty() && _syncPal->getSnapshot(ReplicaSideLocal)->path(parentId, parentPath) &&
                parentPath.filename() == _testPicturesFolderName) {
                fileCounter++;
            }
        }
        CPPUNIT_ASSERT(fileCounter == _nbFileInTestDir);
    }
}

void TestLocalFileSystemObserverWorker::testFolderWatcherWithFiles() {
    NodeId itemId;
    {
        /// Create file
        LOGW_DEBUG(_logger, L"***** test create file *****");
        const SyncPath testFileRelativePath = SyncPath("A") / "test_file.txt";
        SyncPath testAbsolutePath = _testRootFolderPath / testFileRelativePath;
        const std::string testCallStr = R"(echo "This is a create test" >> )" + testAbsolutePath.make_preferred().string();
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        FileStat fileStat;
        bool exists = false;
        IoHelper::getFileStat(testAbsolutePath, &fileStat, exists);
        itemId = std::to_string(fileStat.inode);

        CPPUNIT_ASSERT(_syncPal->getSnapshot(ReplicaSideLocal)->exists(itemId));
        SyncPath testSyncPath;
        CPPUNIT_ASSERT(_syncPal->getSnapshot(ReplicaSideLocal)->path(itemId, testSyncPath) &&
                       testSyncPath == testFileRelativePath);
    }

    {
        /// Edit file
        LOGW_DEBUG(_logger, L"***** test edit file *****");
        const SyncTime prevModTime = _syncPal->getSnapshot(ReplicaSideLocal)->lastModified(itemId);
        const SyncPath testFileRelativePath = SyncPath("A") / "test_file.txt";
        SyncPath testAbsolutePath = _testRootFolderPath / testFileRelativePath;
        const std::string testCallStr = R"(echo "This is an edit test" >> )" + testAbsolutePath.make_preferred().string();
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        CPPUNIT_ASSERT(_syncPal->getSnapshot(ReplicaSideLocal)->lastModified(itemId) > prevModTime);
    }

    {
        /// Move file
        LOGW_DEBUG(_logger, L"***** test move file *****");
        SyncPath source = _testRootFolderPath / SyncPath("A") / "test_file.txt";
        SyncPath target = _testRootFolderPath / SyncPath("B") / "test_file.txt";
#ifdef _WIN32
        const std::string testCallStr = "move " + source.make_preferred().string() + " " + target.make_preferred().string();
#else
        const std::string testCallStr = "mv " + source.make_preferred().string() + " " + target.make_preferred().string();
#endif
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        const NodeId parentId = _syncPal->getSnapshot(ReplicaSideLocal)->parentId(itemId);
        CPPUNIT_ASSERT(_syncPal->getSnapshot(ReplicaSideLocal)->name(parentId) == SyncPath("B"));
    }

    {
        /// Rename file
        LOGW_DEBUG(_logger, L"***** test rename file *****");
        SyncPath source = _testRootFolderPath / "B" / "test_file.txt";
        SyncPath target = _testRootFolderPath / "B" / "test_file_renamed.txt";
#ifdef _WIN32
        const std::string testCallStr = "ren " + source.make_preferred().string() + " " + target.filename().string();
#else
        const std::string testCallStr = "mv " + source.make_preferred().string() + " " + target.make_preferred().string();
#endif
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        CPPUNIT_ASSERT(_syncPal->getSnapshot(ReplicaSideLocal)->name(itemId) == Str("test_file_renamed.txt"));
    }

    {
        /// Delete file
        LOGW_DEBUG(_logger, L"***** test delete file *****");
        SyncPath testAbsolutePath = _testRootFolderPath / "B" / "test_file_renamed.txt";
#ifdef _WIN32
        const std::string testCallStr = "del " + testAbsolutePath.make_preferred().string();
#else
        const std::string testCallStr = "rm -r " + testAbsolutePath.make_preferred().string();
#endif
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        CPPUNIT_ASSERT(!_syncPal->getSnapshot(ReplicaSideLocal)->exists(itemId));
    }
}

void TestLocalFileSystemObserverWorker::testFolderWatcherWithDirs() {
    NodeId itemId;
    {
        /// Create dir
        LOGW_DEBUG(_logger, L"***** test create dir *****");
        SyncPath testRelativePath = SyncPath("A") / SyncPath("AC");
        SyncPath testAbsolutePath = _testRootFolderPath / testRelativePath;
        const std::string testCallStr = "mkdir " + testAbsolutePath.make_preferred().string();
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        FileStat fileStat;
        bool exists = false;
        IoHelper::getFileStat(testAbsolutePath.make_preferred().native().c_str(), &fileStat, exists);
        itemId = std::to_string(fileStat.inode);
        CPPUNIT_ASSERT(_syncPal->getSnapshot(ReplicaSideLocal)->exists(itemId));
        SyncPath testSyncPath;
        _syncPal->getSnapshot(ReplicaSideLocal)->path(itemId, testSyncPath);
        CPPUNIT_ASSERT(testSyncPath.string() == testRelativePath);
    }

    {
        /// Move dir
        LOGW_DEBUG(_logger, L"***** test move dir *****");
        SyncPath source = _testRootFolderPath / SyncPath("A") / SyncPath("AC");
        SyncPath testRelativePath = SyncPath("B") / SyncPath("AC");
        SyncPath target = _testRootFolderPath / testRelativePath;
#ifdef _WIN32
        const std::string testCallStr = "move " + source.make_preferred().string() + " " + target.make_preferred().string();
#else
        const std::string testCallStr = "mv " + source.make_preferred().string() + " " + target.make_preferred().string();
#endif
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        SyncPath testSyncPath;
        _syncPal->getSnapshot(ReplicaSideLocal)->path(itemId, testSyncPath);
        CPPUNIT_ASSERT(testSyncPath == testRelativePath);
    }

    {
        /// Rename dir
        LOGW_DEBUG(_logger, L"***** test rename dir *****");
        SyncPath source = _testRootFolderPath / SyncPath("B") / SyncPath("AC");
        SyncPath testRelativePath = SyncPath("B") / SyncPath("ACc");
        SyncPath target = _testRootFolderPath / testRelativePath;
#ifdef _WIN32
        const std::string testCallStr = "ren " + source.make_preferred().string() + " " + target.filename().string();
#else
        const std::string testCallStr = "mv " + source.make_preferred().string() + " " + target.make_preferred().string();
#endif
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        CPPUNIT_ASSERT(_syncPal->getSnapshot(ReplicaSideLocal)->name(itemId) == Str("ACc"));
    }

    NodeId pictureItemId;

    {
        /// Copy dir from outside sync dir
        LOGW_DEBUG(_logger, L"***** test copy dir from outside sync dir *****");
        SyncPath source = _testFolderPath / "test_dir";

#ifdef _WIN32
        Poco::File(source.make_preferred().string()).copyTo(_testRootFolderPath.make_preferred().string());
#else
        const std::string testCallStr =
            "cp -R " + source.make_preferred().string() + " " + _testRootFolderPath.make_preferred().string();
#endif
        std::system(testCallStr.c_str());
        Utility::msleep(1000);  // Wait 1sec

        FileStat fileStat;
        bool exists = false;
        SyncPath testAbsolutePath = _testRootFolderPath / "test_dir";
        IoHelper::getFileStat(testAbsolutePath.make_preferred().native().c_str(), &fileStat, exists);
        itemId = std::to_string(fileStat.inode);
        CPPUNIT_ASSERT(_syncPal->getSnapshot(ReplicaSideLocal)->exists(itemId));

        SyncPath testPicturePath = testAbsolutePath / Str("picture-1.jpg");
        IoHelper::getFileStat(testPicturePath.make_preferred().native().c_str(), &fileStat, exists);
        pictureItemId = std::to_string(fileStat.inode);
        CPPUNIT_ASSERT(_syncPal->getSnapshot(ReplicaSideLocal)->exists(pictureItemId));
    }

    {
        /// Delete dir and all its content
        LOGW_DEBUG(_logger, L"***** test delete dir *****");
        SyncPath testAbsolutePath = _testRootFolderPath / "test_dir";
#ifdef _WIN32
        const std::string testCallStr = "rmdir /s /q " + testAbsolutePath.make_preferred().string();
#else
        const std::string testCallStr = "rm -r " + testAbsolutePath.make_preferred().string();
#endif
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        CPPUNIT_ASSERT(!_syncPal->getSnapshot(ReplicaSideLocal)->exists(itemId));
        CPPUNIT_ASSERT(!_syncPal->getSnapshot(ReplicaSideLocal)->exists(pictureItemId));
    }
    {
        /// Move dir from outside sync dir
        LOGW_DEBUG(_logger, L"***** test move dir from outside sync dir *****");
        std::filesystem::copy((_testFolderPath / Str("test_dir")).make_preferred().native(),
                              (_testFolderPath / Str("test_dir_copy")).make_preferred().native());

        SyncPath source = _testFolderPath / "test_dir_copy";
#ifdef _WIN32
        const std::string testCallStr =
            "move " + source.make_preferred().string() + " " + _testRootFolderPath.make_preferred().string();
#else
        const std::string testCallStr =
            "mv " + source.make_preferred().string() + " " + _testRootFolderPath.make_preferred().string();
#endif
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        SyncPath testAbsolutePath = _testRootFolderPath / "test_dir_copy";
        FileStat fileStat;
        bool exists = false;
        IoHelper::getFileStat(testAbsolutePath.make_preferred().native().c_str(), &fileStat, exists);
        itemId = std::to_string(fileStat.inode);
        CPPUNIT_ASSERT(_syncPal->getSnapshot(ReplicaSideLocal)->exists(itemId));

        testAbsolutePath /= Str("picture-1.jpg");
        IoHelper::getFileStat(testAbsolutePath.make_preferred().native().c_str(), &fileStat, exists);
        itemId = std::to_string(fileStat.inode);
        CPPUNIT_ASSERT(_syncPal->getSnapshot(ReplicaSideLocal)->exists(itemId));
    }
}

void TestLocalFileSystemObserverWorker::testFolderWatcherWithSpecialCases() {
    // Test 4.3.3.2, p.62 - a) delete(“x”) + create(“x”) + edit(“x”,<newcontent>) + move(“x”,“y”)
    {
        LOGW_DEBUG(_logger, L"***** delete(x) + create(x) + edit(x,<newcontent>) + move(x,y) *****");
        //// delete
        SyncPath testAbsolutePath = _testRootFolderPath / "test_a" / "a.jpg";
        FileStat fileStat;
        bool exists = false;
        IoHelper::getFileStat(testAbsolutePath.make_preferred().native().c_str(), &fileStat, exists);
        NodeId initItemId = std::to_string(fileStat.inode);

#ifdef _WIN32
        std::string testCallStr = "del " + testAbsolutePath.make_preferred().string();
#else
        std::string testCallStr = "rm -r " + testAbsolutePath.make_preferred().string();
#endif
        std::system(testCallStr.c_str());
        //// create
        SyncPath source = _testFolderPath / "test_a" / "a.jpg";
#ifdef _WIN32
        Poco::File(source.make_preferred().string().c_str()).copyTo(testAbsolutePath.make_preferred().string());
#else
        testCallStr = "cp -R " + source.make_preferred().string() + " " + testAbsolutePath.make_preferred().string();
        std::system(testCallStr.c_str());
#endif
        IoHelper::getFileStat(testAbsolutePath.make_preferred().native().c_str(), &fileStat, exists);
        NodeId newItemId = std::to_string(fileStat.inode);
        //// edit
        testCallStr = R"(echo "This is an edit test" >>  )" + testAbsolutePath.make_preferred().string();
        std::system(testCallStr.c_str());
        //// move
#ifdef _WIN32
        testCallStr = "move " + testAbsolutePath.make_preferred().string() + " " + _testRootFolderPath.make_preferred().string() +
                      "\\aa.jpg";
#else
        testCallStr =
            "mv " + testAbsolutePath.make_preferred().string() + " " + _testRootFolderPath.make_preferred().string() + "/aa.jpg";
#endif
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        CPPUNIT_ASSERT(_syncPal->getSnapshot(ReplicaSideLocal)->exists(newItemId));
        CPPUNIT_ASSERT(_syncPal->getSnapshot(ReplicaSideLocal)->name(newItemId) == Str("aa.jpg"));
    }

    // Test 4.3.3.2, p.62 - b) move(“x”,“y”) + create(“x”) + edit(“x”,<newcontent>) + delete(“x”)
    {
        LOGW_DEBUG(_logger, L"***** move(x,y) + create(x) + edit(x,<newcontent>) + delete(x) *****");
        //// move
        SyncPath testAbsolutePath = _testRootFolderPath / "test_b" / "b.jpg";
        FileStat fileStat;
        bool exists = false;
        IoHelper::getFileStat(testAbsolutePath.c_str(), &fileStat, exists);
        NodeId initItemId = std::to_string(fileStat.inode);
#ifdef _WIN32
        std::string testCallStr = "move " + _testRootFolderPath.make_preferred().string() + "\\test_b\\b.jpg" + " " +
                                  _testRootFolderPath.make_preferred().string() + "\\bb.jpg";
#else
        std::string testCallStr = "mv " + _testRootFolderPath.make_preferred().string() + "/test_b/b.jpg" + " " +
                                  _testRootFolderPath.make_preferred().string() + "/bb.jpg";
#endif
        std::system(testCallStr.c_str());
        //// create
        SyncPath source = _testFolderPath / "test_b" / "b.jpg";
#ifdef _WIN32
        Poco::File(source.make_preferred().string()).copyTo(testAbsolutePath.make_preferred().string());
#else
        testCallStr = Str("cp -R ") + source.make_preferred().native() + Str(" ") + testAbsolutePath.make_preferred().native();
        std::system(testCallStr.c_str());
#endif
        IoHelper::getFileStat(testAbsolutePath.c_str(), &fileStat, exists);
        NodeId newItemId = std::to_string(fileStat.inode);
        //// edit
        testCallStr = R"(echo "This is an edit test" >>  )" + testAbsolutePath.make_preferred().string();
        std::system(testCallStr.c_str());
        //// delete
#ifdef _WIN32
        testCallStr = "del " + testAbsolutePath.make_preferred().string();
#else
        testCallStr = "rm -r " + testAbsolutePath.make_preferred().native();
#endif
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        CPPUNIT_ASSERT(_syncPal->getSnapshot(ReplicaSideLocal)->exists(initItemId));
        CPPUNIT_ASSERT(!_syncPal->getSnapshot(ReplicaSideLocal)->exists(newItemId));
        CPPUNIT_ASSERT(_syncPal->getSnapshot(ReplicaSideLocal)->name(initItemId) == Str("bb.jpg"));
    }

    LOGW_DEBUG(_logger, L"***** Tests succesfully finished! *****");
}

}  // namespace KDC
