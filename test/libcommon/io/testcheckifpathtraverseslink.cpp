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
#include "test_utility/testhelpers.h"

#include <filesystem>

using namespace CppUnit;

namespace KDC {

void TestIo::testCheckIfPathTraversesLink() {
    // The path has no ancestor at all.
    {
        auto traversesLink = true;
        auto linkPath = SyncPath("dummy");
        CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::checkIfPathTraversesLink(SyncPath("item.txt"), traversesLink, linkPath));
        CPPUNIT_ASSERT(!traversesLink);
        CPPUNIT_ASSERT(linkPath.empty());
    }

    // No ancestor of the path is a followed link.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const auto dirPath = temporaryDirectory.path() / "dir";
        std::error_code ec;
        CPPUNIT_ASSERT(std::filesystem::create_directories(dirPath, ec) && ec.value() == 0);

        auto traversesLink = true;
        auto linkPath = SyncPath("dummy");
        CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::checkIfPathTraversesLink(dirPath / "item.txt", traversesLink, linkPath));
        CPPUNIT_ASSERT(!traversesLink);
        CPPUNIT_ASSERT(linkPath.empty());
    }

    // The direct parent of the path is a symbolic link.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const auto targetDirPath = temporaryDirectory.path() / "target_dir";
        std::error_code ec;
        CPPUNIT_ASSERT(std::filesystem::create_directories(targetDirPath, ec) && ec.value() == 0);

        const auto symlinkPath = temporaryDirectory.path() / "dir_link";
        auto ioError = IoError::Unknown;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::createSymlink(targetDirPath, symlinkPath, true, ioError));

        auto traversesLink = false;
        SyncPath traversedLinkPath;
        CPPUNIT_ASSERT_EQUAL(IoError::PathContainsSymlink,
                             IoHelper::checkIfPathTraversesLink(symlinkPath / "item.txt", traversesLink, traversedLinkPath));
        CPPUNIT_ASSERT(traversesLink);
        CPPUNIT_ASSERT_EQUAL(symlinkPath, traversedLinkPath);
    }

    // A deeper ancestor of the path is a symbolic link.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const auto targetDirPath = temporaryDirectory.path() / "target_dir";
        std::error_code ec;
        CPPUNIT_ASSERT(std::filesystem::create_directories(targetDirPath, ec) && ec.value() == 0);

        const auto subDirPath = temporaryDirectory.path() / "sub_dir";
        CPPUNIT_ASSERT(std::filesystem::create_directories(subDirPath, ec) && ec.value() == 0);

        const auto symlinkPath = subDirPath / "dir_link";
        auto ioError = IoError::Unknown;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::createSymlink(targetDirPath, symlinkPath, true, ioError));

        auto traversesLink = false;
        SyncPath traversedLinkPath;
        CPPUNIT_ASSERT_EQUAL(IoError::PathContainsSymlink,
                             IoHelper::checkIfPathTraversesLink(symlinkPath / "item.txt", traversesLink, traversedLinkPath));
        CPPUNIT_ASSERT(traversesLink);
        CPPUNIT_ASSERT_EQUAL(symlinkPath, traversedLinkPath);
    }

    // The final component of the path is a symbolic link: only the ancestors are inspected.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const auto targetPath = _localTestDirPath / "test_pictures" / "picture-1.jpg";
        const auto symlinkPath = temporaryDirectory.path() / "item_link";

        auto ioError = IoError::Unknown;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::createSymlink(targetPath, symlinkPath, false, ioError));

        auto traversesLink = true;
        auto traversedLinkPath = SyncPath("dummy");
        CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::checkIfPathTraversesLink(symlinkPath, traversesLink, traversedLinkPath));
        CPPUNIT_ASSERT(!traversesLink);
        CPPUNIT_ASSERT(traversedLinkPath.empty());
    }

#if defined(KD_MACOS)
    // A Finder alias as an ancestor is not considered as traversing a link.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const auto targetDirPath = temporaryDirectory.path() / "target_dir";
        std::error_code ec;
        CPPUNIT_ASSERT(std::filesystem::create_directories(targetDirPath, ec) && ec.value() == 0);

        const auto aliasPath = temporaryDirectory.path() / "dir_alias";
        auto ioError = IoError::Unknown;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::createAliasFromPath(targetDirPath, aliasPath, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

        auto traversesLink = true;
        auto traversedLinkPath = SyncPath("dummy");
        CPPUNIT_ASSERT_EQUAL(IoError::Success,
                             IoHelper::checkIfPathTraversesLink(aliasPath / "item.txt", traversesLink, traversedLinkPath));
        CPPUNIT_ASSERT(!traversesLink);
        CPPUNIT_ASSERT(traversedLinkPath.empty());
    }
#endif

    // One of the ancestors of the path does not exist.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        auto traversesLink = true;
        auto traversedLinkPath = SyncPath("dummy");
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
        const auto targetDirPath = temporaryDirectory.path() / "target_dir";
        std::error_code ec;
        CPPUNIT_ASSERT(std::filesystem::create_directories(targetDirPath, ec) && ec.value() == 0);

        const auto junctionPath = temporaryDirectory.path() / "dir_junction";
        auto ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::createJunctionFromPath(targetDirPath, junctionPath, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        auto traversesLink = false;
        SyncPath traversedLinkPath;
        CPPUNIT_ASSERT_EQUAL(IoError::PathContainsSymlink,
                             IoHelper::checkIfPathTraversesLink(junctionPath / "item.txt", traversesLink, traversedLinkPath));
        CPPUNIT_ASSERT(traversesLink);
        CPPUNIT_ASSERT_EQUAL(junctionPath, traversedLinkPath);
    }
#endif
}

// Every IoHelper method altering the filesystem must refuse to operate on a path that traverses a followed link, so that the
// synchronization can never alter an item located outside the sync folder through a link located inside it.
void TestIo::testFilesystemAlterationsThroughLink() {
    const LocalTemporaryDirectory temporaryDirectory;
    const auto syncDirPath = temporaryDirectory.path() / "sync_dir";
    const auto outsideDirPath = temporaryDirectory.path() / "outside_sync_dir";
    std::error_code ec;
    CPPUNIT_ASSERT(std::filesystem::create_directories(syncDirPath, ec) && ec.value() == 0);
    CPPUNIT_ASSERT(std::filesystem::create_directories(outsideDirPath / "sub_dir", ec) && ec.value() == 0);

    const auto sourceFilePath = syncDirPath / "file.txt";
    testhelpers::generateTestFile(sourceFilePath, 10);
    const auto sourceDirPath = syncDirPath / "dir";
    CPPUNIT_ASSERT(std::filesystem::create_directories(sourceDirPath, ec) && ec.value() == 0);

    // `linkPath` is located inside the sync folder and points outside of it.
    const auto linkPath = syncDirPath / "link";
    auto ioError = IoError::Unknown;
    CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::createSymlink(outsideDirPath, linkPath, true, ioError));

    const auto expectOutsideDirUnchanged = [&outsideDirPath]() {
        std::error_code ec;
        auto count = 0;
        for (auto it = std::filesystem::recursive_directory_iterator(outsideDirPath, ec);
             it != std::filesystem::recursive_directory_iterator(); ++it) {
            ++count;
        }
        CPPUNIT_ASSERT_EQUAL(0, ec.value());
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Only `sub_dir` is expected in the outside directory", 1, count);
    };

    // The link is either the direct parent or a deeper ancestor of the altered path.
    for (const auto &parentPath: {linkPath, linkPath / "sub_dir"}) {
        // createDirectory
        for (const bool recursive: {false, true}) {
            ioError = IoError::Unknown;
            CPPUNIT_ASSERT(!IoHelper::createDirectory(parentPath / "new_dir", recursive, ioError));
            CPPUNIT_ASSERT_EQUAL(IoError::PathContainsSymlink, ioError);
            expectOutsideDirUnchanged();
        }

        // createDirectory, recursive, with missing intermediate directories below the link
        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(!IoHelper::createDirectory(parentPath / "missing_1" / "missing_2" / "new_dir", true, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::PathContainsSymlink, ioError);
        expectOutsideDirUnchanged();

        // copyFileOrDirectory
        for (const auto &sourcePath: {sourceFilePath, sourceDirPath}) {
            ioError = IoError::Unknown;
            CPPUNIT_ASSERT(!IoHelper::copyFileOrDirectory(sourcePath, parentPath / "copy", ioError));
            CPPUNIT_ASSERT_EQUAL(IoError::PathContainsSymlink, ioError);
            expectOutsideDirUnchanged();
        }

        // createSymlink
        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(!IoHelper::createSymlink(sourceFilePath, parentPath / "new_link", false, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::PathContainsSymlink, ioError);
        expectOutsideDirUnchanged();

        // renameItem / moveItem
        CPPUNIT_ASSERT_EQUAL(IoError::PathContainsSymlink, IoHelper::renameItem(sourceFilePath, parentPath / "moved.txt"));
        CPPUNIT_ASSERT_EQUAL(IoError::PathContainsSymlink, IoHelper::moveItem(sourceFilePath, parentPath / "moved.txt"));
        CPPUNIT_ASSERT(std::filesystem::exists(sourceFilePath, ec) && ec.value() == 0);
        expectOutsideDirUnchanged();

#if defined(KD_MACOS)
        // createAliasFromPath / createAlias
        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(!IoHelper::createAliasFromPath(sourceFilePath, parentPath / "new_alias", ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::PathContainsSymlink, ioError);
        expectOutsideDirUnchanged();

        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(!IoHelper::createAlias("dummy alias data", parentPath / "new_alias", ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::PathContainsSymlink, ioError);
        expectOutsideDirUnchanged();
#elif defined(KD_WINDOWS)
        // createJunctionFromPath / createJunction
        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(!IoHelper::createJunctionFromPath(sourceDirPath, parentPath / "new_junction", ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::PathContainsSymlink, ioError);
        expectOutsideDirUnchanged();

        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(!IoHelper::createJunction("dummy junction data", parentPath / "new_junction", ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::PathContainsSymlink, ioError);
        expectOutsideDirUnchanged();
#endif
    }

    // deleteItem: `linkPath / "sub_dir"` resolves to an existing directory outside the sync folder.
    ioError = IoError::Unknown;
    CPPUNIT_ASSERT(!IoHelper::deleteItem(linkPath / "sub_dir", ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::PathContainsSymlink, ioError);
    expectOutsideDirUnchanged();

    // moveItemToTrash: same target as above.
    CPPUNIT_ASSERT(!IoHelper::moveItemToTrash(linkPath / "sub_dir"));
    expectOutsideDirUnchanged();

    // A dangling link, whose target is a missing directory outside the sync folder, is detected as well when creating a
    // directory with missing intermediate directories below it: the dangling link itself exists, so it is checked.
    {
        const auto missingTargetPath = outsideDirPath / "missing_target";
        const auto danglingLinkPath = syncDirPath / "dangling_link";
        std::filesystem::create_directory_symlink(missingTargetPath, danglingLinkPath, ec);
        CPPUNIT_ASSERT_EQUAL(0, ec.value());

        for (const bool recursive: {false, true}) {
            ioError = IoError::Unknown;
            CPPUNIT_ASSERT(!IoHelper::createDirectory(danglingLinkPath / "missing" / "new_dir", recursive, ioError));
            CPPUNIT_ASSERT_EQUAL(IoError::PathContainsSymlink, ioError);
            CPPUNIT_ASSERT(!std::filesystem::exists(missingTargetPath, ec));
            expectOutsideDirUnchanged();
        }
    }

    // Sanity check: the same operations succeed when the path does not traverse any link.
    ioError = IoError::Unknown;
    CPPUNIT_ASSERT(IoHelper::createDirectory(syncDirPath / "new_dir", false, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(IoHelper::copyFileOrDirectory(sourceFilePath, syncDirPath / "copy.txt", ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(IoHelper::createSymlink(sourceFilePath, syncDirPath / "new_link", false, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::renameItem(sourceFilePath, syncDirPath / "moved.txt"));

    // Deleting the link itself removes the link, not its target.
    CPPUNIT_ASSERT(IoHelper::deleteItem(linkPath, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(!std::filesystem::exists(std::filesystem::symlink_status(linkPath, ec)));
    expectOutsideDirUnchanged();
}

} // namespace KDC
