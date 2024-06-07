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

const std::string testFolderPath(TEST_DIR "/test_ci/test_local_FSO");
const std::string testRootFolderPath(TEST_DIR "/test_ci/test_local_FSO/sync_folder");
const std::string testPicturesFolderPath("test_pictures");
const uint64_t nbFileInTestDir = 5;  // Test directory contains 5 files

void TestLocalFileSystemObserverWorker::setUp() {
    _logger = Log::instance()->getLogger();

    LOGW_DEBUG(_logger, L"$$$$$ Set Up $$$$$");

    // Reset test directory
    if (std::filesystem::exists(testRootFolderPath)) {
        Poco::File(testRootFolderPath.c_str()).remove(true);  // Make sure the test directory has been deleted
    }

    Poco::File((testRootFolderPath + "/A/AA").c_str()).createDirectories();
    Poco::File((testRootFolderPath + "/A/AB").c_str()).createDirectories();
    Poco::File((testRootFolderPath + "/B/BA").c_str()).createDirectories();
    Poco::File((testRootFolderPath + "/B/BB").c_str()).createDirectories();

    Poco::File((testFolderPath + "/test_dir").c_str()).copyTo(testRootFolderPath + "/" + testPicturesFolderPath);
    Poco::File((testFolderPath + "/test_a").c_str()).copyTo(testRootFolderPath + "/test_a");
    Poco::File((testFolderPath + "/test_b").c_str()).copyTo(testRootFolderPath + "/test_b");

    // Create parmsDb
    bool alreadyExists;
    std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExists);
    std::filesystem::remove(parmsDbPath);
    ParmsDb::instance(parmsDbPath, "3.4.0", true, true);
    ParmsDb::instance()->setAutoDelete(true);
    ParametersCache::instance()->parameters().setExtendedLog(true);

    bool constraintError;
    ParmsDb::instance()->insertExclusionTemplate(
        ExclusionTemplate(".DS_Store", true),
        constraintError);  // TODO : to be removed once we have a default list of file excluded implemented
    // Create SyncPal
    SyncPath syncDbPath = Db::makeDbName(1, 1, 1, 1, alreadyExists);
    std::filesystem::remove(syncDbPath);
    _syncPal = std::shared_ptr<SyncPal>(new SyncPal(syncDbPath, "3.4.0", true));
    _syncPal->_syncDb->setAutoDelete(true);

    _syncPal->_localPath = testRootFolderPath;

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

    Poco::File(testRootFolderPath.c_str()).remove(true);

    ParmsDb::instance()->close();

    if (_syncPal && _syncPal->_syncDb) {
        _syncPal->_syncDb->close();
    }
}

void TestLocalFileSystemObserverWorker::testFolderWatcher() {
    // Test initial snapshot
    {
        std::unordered_set<NodeId> ids;
        _syncPal->_localSnapshot->ids(ids);

        uint64_t fileCounter = 0;
        for (const auto &id : ids) {
            if (_syncPal->_localSnapshot->name(id) == Str(".DS_Store") ||
                _syncPal->_localSnapshot->name(id) == Str(".ds_store")) {
                continue;  // Ignore ".DS_Store"
            }

            NodeId parentId = _syncPal->_localSnapshot->parentId(id);
            SyncPath parentPath;
            if (_syncPal->_localSnapshot->path(parentId, parentPath) && parentPath.filename() == testPicturesFolderPath) {
                fileCounter++;
            }
        }
        CPPUNIT_ASSERT(fileCounter == nbFileInTestDir);
    }

    // Test files
    {
        /// Create file
        LOGW_DEBUG(_logger, L"***** test create file *****");
        std::filesystem::path testFileRelativePath = "A/test_file.txt";
        std::filesystem::path testAbsolutePath = testRootFolderPath / testFileRelativePath;
        std::string testCallStr = R"(echo "This is a create test" >> )" + testAbsolutePath.make_preferred().string();
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        FileStat fileStat;
        bool exists = false;
        IoHelper::getFileStat(testAbsolutePath, &fileStat, exists);
        NodeId itemId = std::to_string(fileStat.inode);
        CPPUNIT_ASSERT(_syncPal->_localSnapshot->exists(itemId));
        SyncPath testSyncPath;
        CPPUNIT_ASSERT(_syncPal->_localSnapshot->path(itemId, testSyncPath) && testSyncPath == testFileRelativePath);

        /// Edit file
        LOGW_DEBUG(_logger, L"***** test edit file *****");
        SyncTime prevModTime = _syncPal->_localSnapshot->lastModified(itemId);
        testCallStr = R"(echo "This is an edit test" >> )" + testAbsolutePath.make_preferred().string();
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        CPPUNIT_ASSERT(_syncPal->_localSnapshot->lastModified(itemId) > prevModTime);

        /// Move file
        LOGW_DEBUG(_logger, L"***** test move file *****");
        std::filesystem::path source = testAbsolutePath;
        testFileRelativePath = "B";
        testAbsolutePath = testRootFolderPath / testFileRelativePath;
#ifdef _WIN32
        testCallStr = "move " + source.make_preferred().string() + " " + testAbsolutePath.make_preferred().string();
#else
        testCallStr = "mv " + source.make_preferred().string() + " " + testAbsolutePath.make_preferred().string();
#endif
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        NodeId parentId = _syncPal->_localSnapshot->parentId(itemId);
        CPPUNIT_ASSERT(_syncPal->_localSnapshot->name(parentId) == testFileRelativePath);

        /// Rename file
        LOGW_DEBUG(_logger, L"***** test rename file *****");
        source = testRootFolderPath + "/B/test_file.txt";
        testFileRelativePath = Str("B/test_file_renamed.txt");
        testAbsolutePath = testRootFolderPath / testFileRelativePath;
#ifdef _WIN32
        testCallStr = "ren " + source.make_preferred().string() + " " + testAbsolutePath.filename().string();
#else
        testCallStr = "mv " + source.make_preferred().string() + " " + testAbsolutePath.make_preferred().string();
#endif
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        SyncName tmp = _syncPal->_localSnapshot->name(itemId);
        CPPUNIT_ASSERT(_syncPal->_localSnapshot->name(itemId) == Str("test_file_renamed.txt"));

        /// Delete file
        LOGW_DEBUG(_logger, L"***** test delete file *****");
#ifdef _WIN32
        testCallStr = "del " + testAbsolutePath.make_preferred().string();
#else
        testCallStr = "rm -r " + testAbsolutePath.make_preferred().string();
#endif
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        CPPUNIT_ASSERT(!_syncPal->_localSnapshot->exists(itemId));
    }

    // Test directories
    {
        /// Create dir
        LOGW_DEBUG(_logger, L"***** test create dir *****");
        std::filesystem::path testRelativePath = Str("A/AC");
        std::filesystem::path testAbsolutePath = testRootFolderPath / testRelativePath;
        std::string testCallStr = "mkdir " + testAbsolutePath.make_preferred().string();
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        FileStat fileStat;
        bool exists = false;
        IoHelper::getFileStat(testAbsolutePath.make_preferred().native().c_str(), &fileStat, exists);
        NodeId itemId = std::to_string(fileStat.inode);
        CPPUNIT_ASSERT(_syncPal->_localSnapshot->exists(itemId));
        SyncPath testSyncPath;
        _syncPal->_localSnapshot->path(itemId, testSyncPath);
        CPPUNIT_ASSERT(testSyncPath.string() == testRelativePath);

        /// Move dir
        LOGW_DEBUG(_logger, L"***** test move dir *****");
        std::filesystem::path source = testAbsolutePath;
        testRelativePath = "B/AC";
        testAbsolutePath = testRootFolderPath / testRelativePath;
#ifdef _WIN32
        testCallStr = "move " + source.make_preferred().string() + " " + testAbsolutePath.make_preferred().string();
#else
        testCallStr = "mv " + source.make_preferred().string() + " " + testAbsolutePath.make_preferred().string();
#endif
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        _syncPal->_localSnapshot->path(itemId, testSyncPath);
        CPPUNIT_ASSERT(testSyncPath.string() == testRelativePath);

        /// Rename dir
        LOGW_DEBUG(_logger, L"***** test rename dir *****");
        source = testAbsolutePath;
        testRelativePath = "B/ACc";
        testAbsolutePath = testRootFolderPath / testRelativePath;
#ifdef _WIN32
        testCallStr = "ren " + source.make_preferred().string() + " " + testAbsolutePath.filename().string();
#else
        testCallStr = "mv " + source.make_preferred().string() + " " + testAbsolutePath.make_preferred().string();
#endif
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        CPPUNIT_ASSERT(_syncPal->_localSnapshot->name(itemId) == Str("ACc"));

        /// Copy dir from outside sync dir
        LOGW_DEBUG(_logger, L"***** test copy dir from outside sync dir *****");
        source = testFolderPath + "/test_dir";
        testAbsolutePath = testRootFolderPath;

#ifdef _WIN32
        Poco::File(source.make_preferred().string()).copyTo(testAbsolutePath.make_preferred().string());
#else
        testCallStr = "cp -R " + source.make_preferred().string() + " " + testRootFolderPath;
        std::system(testCallStr.c_str());
#endif

        Utility::msleep(1000);  // Wait 1sec

        testAbsolutePath = testRootFolderPath + "/test_dir";
        IoHelper::getFileStat(testAbsolutePath.make_preferred().native().c_str(), &fileStat, exists);
        itemId = std::to_string(fileStat.inode);
        CPPUNIT_ASSERT(_syncPal->_localSnapshot->exists(itemId));

        SyncPath testPicturePath = testAbsolutePath / Str("picture-1.jpg");
        IoHelper::getFileStat(testPicturePath.make_preferred().native().c_str(), &fileStat, exists);
        NodeId pictureItemId = std::to_string(fileStat.inode);
        CPPUNIT_ASSERT(_syncPal->_localSnapshot->exists(pictureItemId));

        /// Delete dir and all its content
        LOGW_DEBUG(_logger, L"***** test delete dir *****");
#ifdef _WIN32
        testCallStr = "rmdir /s /q " + testAbsolutePath.make_preferred().string();
#else
        testCallStr = "rm -r " + testAbsolutePath.make_preferred().string();
#endif
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        CPPUNIT_ASSERT(!_syncPal->_localSnapshot->exists(itemId));
        CPPUNIT_ASSERT(!_syncPal->_localSnapshot->exists(pictureItemId));

        /// Move dir from outside sync dir
        LOGW_DEBUG(_logger, L"***** test move dir from outside sync dir *****");
        std::filesystem::copy((std::filesystem::path(testFolderPath) / Str("test_dir")).make_preferred().native(),
                              (std::filesystem::path(testFolderPath) / Str("test_dir_copy")).make_preferred().native());

        source = testFolderPath + "/test_dir_copy";
#ifdef _WIN32
        testCallStr = "move " + source.make_preferred().string() + " " +
                      std::filesystem::path(testRootFolderPath).make_preferred().string();
#else
        testCallStr = "mv " + source.make_preferred().string() + " " + testRootFolderPath;
#endif
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        testAbsolutePath = testRootFolderPath + "/test_dir_copy";
        IoHelper::getFileStat(testAbsolutePath.make_preferred().native().c_str(), &fileStat, exists);
        itemId = std::to_string(fileStat.inode);
        CPPUNIT_ASSERT(_syncPal->_localSnapshot->exists(itemId));

        testAbsolutePath += Str("/picture-1.jpg");
        IoHelper::getFileStat(testAbsolutePath.make_preferred().native().c_str(), &fileStat, exists);
        itemId = std::to_string(fileStat.inode);
        CPPUNIT_ASSERT(_syncPal->_localSnapshot->exists(itemId));
    }

    // Test 4.3.3.2, p.62 - a) delete(“x”) + create(“x”) + edit(“x”,<newcontent>) + move(“x”,“y”)
    {
        LOGW_DEBUG(_logger, L"***** delete(x) + create(x) + edit(x,<newcontent>) + move(x,y) *****");
        //// delete
        SyncPath testAbsolutePath = testRootFolderPath + "/test_a/a.jpg";
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
        SyncPath source = testFolderPath + "/test_a/a.jpg";
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
        testCallStr = "move " + testAbsolutePath.make_preferred().string() + " " + testRootFolderPath + "aa.jpg";
#else
        testCallStr = "mv " + testAbsolutePath.make_preferred().string() + " " + testRootFolderPath + "/aa.jpg";
#endif
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        CPPUNIT_ASSERT(_syncPal->_localSnapshot->exists(newItemId));
        CPPUNIT_ASSERT(_syncPal->_localSnapshot->name(newItemId) == Str("aa.jpg"));
    }

    // Test 4.3.3.2, p.62 - b) move(“x”,“y”) + create(“x”) + edit(“x”,<newcontent>) + delete(“x”)
    {
        LOGW_DEBUG(_logger, L"***** move(x,y) + create(x) + edit(x,<newcontent>) + delete(x) *****");
        //// move
        std::string testAbsolutePath = testRootFolderPath + "/test_b/b.jpg";
        FileStat fileStat;
        bool exists = false;
        IoHelper::getFileStat(testAbsolutePath.c_str(), &fileStat, exists);
        NodeId initItemId = std::to_string(fileStat.inode);
#ifdef _WIN32
        std::string testCallStr = "move " + testRootFolderPath + "/test_b/b.jpg" + " " + testRootFolderPath + "/bb.jpg";
#else
        std::string testCallStr = "mv " + testRootFolderPath + "/test_b/b.jpg" + " " + testRootFolderPath + "/bb.jpg";
#endif
        std::system(testCallStr.c_str());
        //// create
        std::filesystem::path source = testFolderPath + "/test_b/b.jpg";
#ifdef _WIN32
        Poco::File(source.make_preferred().string()).copyTo(testAbsolutePath);
#else
        testCallStr = Str("cp -R ") + source.make_preferred().native() + Str(" ") + testAbsolutePath;
        std::system(testCallStr.c_str());
#endif
        IoHelper::getFileStat(testAbsolutePath.c_str(), &fileStat, exists);
        NodeId newItemId = std::to_string(fileStat.inode);
        //// edit
        testCallStr = R"(echo "This is an edit test" >>  )" + testAbsolutePath;
        std::system(testCallStr.c_str());
        //// delete
#ifdef _WIN32
        testCallStr = "del " + testAbsolutePath;
#else
        testCallStr = "rm -r " + testAbsolutePath;
#endif
        std::system(testCallStr.c_str());

        Utility::msleep(1000);  // Wait 1sec

        CPPUNIT_ASSERT(_syncPal->_localSnapshot->exists(initItemId));
        CPPUNIT_ASSERT(!_syncPal->_localSnapshot->exists(newItemId));
        CPPUNIT_ASSERT(_syncPal->_localSnapshot->name(initItemId) == Str("bb.jpg"));
    }

    LOGW_DEBUG(_logger, L"***** Tests succesfully finished! *****");
}

}  // namespace KDC
