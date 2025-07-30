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

#include "testfolderwatcherlinux.h"

#include "test_utility/localtemporarydirectory.h"
#include "test_utility/testhelpers.h"
#include "update_detection/file_system_observer/folderwatcher_linux.h"

#include <Poco/File.h>
#include <sys/inotify.h>

namespace KDC {

void TestFolderWatcherLinux::testMakeSyncPath() {
    CPPUNIT_ASSERT(!FolderWatcher_linux::makeSyncPath("/A/B", "file.txt").filename().empty());
    CPPUNIT_ASSERT(!FolderWatcher_linux::makeSyncPath("/A/B", "").filename().empty());
}

void TestFolderWatcherLinux::testAddFolderRecursive() {
    // Generate test files
    const auto tempDir = LocalTemporaryDirectory("testAddFolderRecursive");
    const auto pathAA = tempDir.path() / "A/AA";
    Poco::File(Path2Str(pathAA)).createDirectories();
    for (uint64_t i = 0; i < 5; i++) {
        const auto filename = "test" + std::to_string(i) + ".txt";
        const auto filepath = pathAA / filename;
        testhelpers::generateOrEditTestFile(filepath);
    }

    FolderWatcher_linux testObj(nullptr, "");
    CPPUNIT_ASSERT_EQUAL(ExitInfo{ExitCode::Ok}, testObj.addFolderRecursive(tempDir.path()));
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(7), testObj._pathToWatch.size());
}

void TestFolderWatcherLinux::testRemoveFoldersBelow() {
    FolderWatcher_linux testObj(nullptr, "");

    const auto tempDir = LocalTemporaryDirectory("testRemoveFoldersBelow");

    testObj._fileDescriptor = inotify_init();
    for (const auto &path: {SyncPath("A"), SyncPath("A/AA"), SyncPath("A/AA/AAA")}) {
        const auto absolutePath = tempDir.path() / path;
        std::filesystem::create_directory(absolutePath);
        CPPUNIT_ASSERT_EQUAL(ExitInfo{ExitCode::Ok}, testObj.inotifyRegisterPath(absolutePath));
    }

    testObj.removeFoldersBelow(tempDir.path() / "B");
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(3), testObj._pathToWatch.size());
    testObj.removeFoldersBelow(tempDir.path() / "A/AA");
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), testObj._pathToWatch.size());
}

void TestFolderWatcherLinux::testInotifyRegisterPath() {
    class FolderWatcherLinuxMock : public FolderWatcher_linux {
        public:
            FolderWatcherLinuxMock(LocalFileSystemObserverWorker *parent, const SyncPath &path) :
                FolderWatcher_linux(parent, path) {}

        private:
            FolderWatcher_linux::AddWatchOutcome inotifyAddWatch(const SyncPath &) override {
                switch (_branchCounter++) {
                    case 0:
                        return {-1, ENOSPC};
                    case 1:
                        return {-1, ENOMEM};
                    default:
                        return {-1, EINVAL};
                }
            }
            std::int32_t _branchCounter = 0;
    };

    FolderWatcherLinuxMock testObj(nullptr, "");
    const auto tempDir = LocalTemporaryDirectory("testInotifyRegisterPath");

    // ENOSPC
    auto expectedExitInfo = ExitInfo{ExitCode::SystemError, ExitCause::NotEnoughINotifyWatches};
    CPPUNIT_ASSERT_EQUAL(expectedExitInfo, testObj.inotifyRegisterPath(tempDir.path()));

    // ENOMEM
    expectedExitInfo = ExitInfo{ExitCode::SystemError, ExitCause::NotEnoughMemory};
    CPPUNIT_ASSERT_EQUAL(expectedExitInfo, testObj.inotifyRegisterPath(tempDir.path()));

    // EINVAL
    expectedExitInfo = ExitCode::Ok; // Errors different form ENOSPC and ENOMEM are ignored for now.
    CPPUNIT_ASSERT_EQUAL(expectedExitInfo, testObj.inotifyRegisterPath(tempDir.path()));

    // Deleted folders are harmless, hence ignored.
    expectedExitInfo = ExitCode::Ok;
    CPPUNIT_ASSERT_EQUAL(expectedExitInfo, testObj.inotifyRegisterPath(SyncPath{"this-path-does-not-exist"}));
}

void TestFolderWatcherLinux::testFindSubFolders() {
    // Directory
    const LocalTemporaryDirectory temporaryDirectory;
    const SyncPath dir1Path = temporaryDirectory.path() / "dir1";
    CPPUNIT_ASSERT(std::filesystem::create_directories(dir1Path));

    // Symlink to directory
    const SyncPath dir1SymlinkPath = temporaryDirectory.path() / "dir1Symlink";
    std::error_code ec;
    std::filesystem::create_directory_symlink(dir1Path, dir1SymlinkPath, ec);
    CPPUNIT_ASSERT(!ec);

    // File
    const SyncPath file1Path = dir1Path / "file1.txt";
    { std::ofstream file1(file1Path); }

    // Symlink to file
    const SyncPath file1SymlinkPath = temporaryDirectory.path() / "file1Symlink";
    std::filesystem::create_directory_symlink(file1Path, file1SymlinkPath, ec);
    CPPUNIT_ASSERT(!ec);

    FolderWatcher_linux testObj(nullptr, "");
    std::list<SyncPath> fullList;
    CPPUNIT_ASSERT(testObj.findSubFolders(temporaryDirectory.path(), fullList));
    CPPUNIT_ASSERT(fullList.size() == 1);
    CPPUNIT_ASSERT(fullList.back() == dir1Path);
}

} // namespace KDC
