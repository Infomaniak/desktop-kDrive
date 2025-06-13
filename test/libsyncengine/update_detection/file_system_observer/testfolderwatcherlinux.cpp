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
            std::int64_t inotifyAddWatch(const SyncPath &) override { return -1; }
    };

    FolderWatcherLinuxMock testObj(nullptr, "");
    const auto tempDir = LocalTemporaryDirectory("testInotifyRegisterPath");

    const auto expectedExitInfo = ExitInfo{ExitCode::SystemError, ExitCause::NotEnoughINotifyWatches};
    CPPUNIT_ASSERT_EQUAL(expectedExitInfo, testObj.inotifyRegisterPath(tempDir.path()));
}

} // namespace KDC
