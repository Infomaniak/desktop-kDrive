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

#include "testio.h"

#include <filesystem>

using namespace CppUnit;

namespace KDC {

void TestIo::testCheckIfPathTraversesLink() {
    // The path has no ancestor at all.
    {
        bool traversesLink = true;
        SyncPath linkPath = SyncPath("dummy");
        CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::checkIfPathTraversesLink(SyncPath("item.txt"), traversesLink, linkPath));
        CPPUNIT_ASSERT(!traversesLink);
        CPPUNIT_ASSERT(linkPath.empty());
    }

    // No ancestor of the path is a followed link.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath dirPath = temporaryDirectory.path() / "dir";
        std::error_code ec;
        CPPUNIT_ASSERT(std::filesystem::create_directories(dirPath, ec) && ec.value() == 0);

        bool traversesLink = true;
        SyncPath linkPath = SyncPath("dummy");
        CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::checkIfPathTraversesLink(dirPath / "item.txt", traversesLink, linkPath));
        CPPUNIT_ASSERT(!traversesLink);
        CPPUNIT_ASSERT(linkPath.empty());
    }

    // The direct parent of the path is a symbolic link.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetDirPath = temporaryDirectory.path() / "target_dir";
        std::error_code ec;
        CPPUNIT_ASSERT(std::filesystem::create_directories(targetDirPath, ec) && ec.value() == 0);

        const SyncPath symlinkPath = temporaryDirectory.path() / "dir_link";
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::createSymlink(targetDirPath, symlinkPath, true, ioError));

        bool traversesLink = false;
        SyncPath traversedLinkPath;
        CPPUNIT_ASSERT_EQUAL(IoError::Success,
                             IoHelper::checkIfPathTraversesLink(symlinkPath / "item.txt", traversesLink, traversedLinkPath));
        CPPUNIT_ASSERT(traversesLink);
        CPPUNIT_ASSERT(symlinkPath == traversedLinkPath);
    }

    // A deeper ancestor of the path is a symbolic link.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetDirPath = temporaryDirectory.path() / "target_dir";
        std::error_code ec;
        CPPUNIT_ASSERT(std::filesystem::create_directories(targetDirPath, ec) && ec.value() == 0);

        const SyncPath subDirPath = temporaryDirectory.path() / "sub_dir";
        CPPUNIT_ASSERT(std::filesystem::create_directories(subDirPath, ec) && ec.value() == 0);

        const SyncPath symlinkPath = subDirPath / "dir_link";
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::createSymlink(targetDirPath, symlinkPath, true, ioError));

        bool traversesLink = false;
        SyncPath traversedLinkPath;
        CPPUNIT_ASSERT_EQUAL(IoError::Success,
                             IoHelper::checkIfPathTraversesLink(symlinkPath / "item.txt", traversesLink, traversedLinkPath));
        CPPUNIT_ASSERT(traversesLink);
        CPPUNIT_ASSERT(symlinkPath == traversedLinkPath);
    }

    // The final component of the path is a symbolic link: only the ancestors are inspected.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures" / "picture-1.jpg";
        const SyncPath symlinkPath = temporaryDirectory.path() / "item_link";

        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::createSymlink(targetPath, symlinkPath, false, ioError));

        bool traversesLink = true;
        SyncPath traversedLinkPath = SyncPath("dummy");
        CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::checkIfPathTraversesLink(symlinkPath, traversesLink, traversedLinkPath));
        CPPUNIT_ASSERT(!traversesLink);
        CPPUNIT_ASSERT(traversedLinkPath.empty());
    }

    // One of the ancestors of the path does not exist.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        bool traversesLink = true;
        SyncPath traversedLinkPath = SyncPath("dummy");
        CPPUNIT_ASSERT_EQUAL(IoError::NoSuchFileOrDirectory,
                             IoHelper::checkIfPathTraversesLink(temporaryDirectory.path() / "non_existing_dir" / "item.txt",
                                                                traversesLink, traversedLinkPath));
        CPPUNIT_ASSERT(!traversesLink);
        CPPUNIT_ASSERT(traversedLinkPath.empty());
    }

#if defined(KD_WINDOWS)
    // A junction as direct parent of the path is detected as well.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetDirPath = temporaryDirectory.path() / "target_dir";
        std::error_code ec;
        CPPUNIT_ASSERT(std::filesystem::create_directories(targetDirPath, ec) && ec.value() == 0);

        const SyncPath junctionPath = temporaryDirectory.path() / "dir_junction";
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::createJunctionFromPath(targetDirPath, junctionPath, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        bool traversesLink = false;
        SyncPath traversedLinkPath;
        CPPUNIT_ASSERT_EQUAL(IoError::Success,
                             IoHelper::checkIfPathTraversesLink(junctionPath / "item.txt", traversesLink, traversedLinkPath));
        CPPUNIT_ASSERT(traversesLink);
        CPPUNIT_ASSERT(junctionPath == traversedLinkPath);
    }
#endif
}

} // namespace KDC
