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
    // An empty sync root disables the check.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const auto targetDirPath = temporaryDirectory.path() / "target_dir";
        std::error_code ec;
        CPPUNIT_ASSERT(std::filesystem::create_directories(targetDirPath, ec) && ec.value() == 0);

        const auto symlinkPath = temporaryDirectory.path() / "dir_link";
        auto ioError = IoError::Unknown;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::createSymlink(targetDirPath, symlinkPath, true, ioError));

        auto traversesLink = true;
        auto linkPath = SyncPath("dummy");
        CPPUNIT_ASSERT_EQUAL(IoError::Success,
                             IoHelper::checkIfPathTraversesLink(symlinkPath / "item.txt", SyncPath(), traversesLink, linkPath));
        CPPUNIT_ASSERT(!traversesLink);
        CPPUNIT_ASSERT(linkPath.empty());
    }

    // The path is not located under the sync root.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const LocalTemporaryDirectory otherTemporaryDirectory;
        auto traversesLink = true;
        auto linkPath = SyncPath("dummy");
        CPPUNIT_ASSERT_EQUAL(IoError::InvalidArgument,
                             IoHelper::checkIfPathTraversesLink(otherTemporaryDirectory.path() / "item.txt",
                                                                temporaryDirectory.path(), traversesLink, linkPath));
        CPPUNIT_ASSERT(!traversesLink);
        CPPUNIT_ASSERT(linkPath.empty());
    }

    // A parent reference below the sync root is rejected, even though the path is lexically located under the sync root.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const auto syncRootPath = temporaryDirectory.path() / "sync_root";
        std::error_code ec;
        CPPUNIT_ASSERT(std::filesystem::create_directories(syncRootPath / "dir", ec) && ec.value() == 0);
        CPPUNIT_ASSERT(std::filesystem::create_directories(temporaryDirectory.path() / "outside", ec) && ec.value() == 0);

        auto traversesLink = true;
        auto linkPath = SyncPath("dummy");
        for (const auto &path: {syncRootPath / ".." / "outside" / "item.txt", syncRootPath / "dir" / ".." / "item.txt"}) {
            CPPUNIT_ASSERT_EQUAL(IoError::InvalidArgument,
                                 IoHelper::checkIfPathTraversesLink(path, syncRootPath, traversesLink, linkPath));
            CPPUNIT_ASSERT(!traversesLink);
            CPPUNIT_ASSERT(linkPath.empty());
        }
    }

    // Links located at or above the sync root are ignored (e.g. "/var" -> "private/var" on macOS).
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const auto realParentDirPath = temporaryDirectory.path() / "real_parent_dir";
        std::error_code ec;
        CPPUNIT_ASSERT(std::filesystem::create_directories(realParentDirPath / "sync_root" / "dir", ec) && ec.value() == 0);

        // A link above the sync root.
        const auto parentLinkPath = temporaryDirectory.path() / "parent_link";
        auto ioError = IoError::Unknown;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::createSymlink(realParentDirPath, parentLinkPath, true, ioError));

        auto traversesLink = true;
        auto linkPath = SyncPath("dummy");
        const auto syncRootPath = parentLinkPath / "sync_root";
        CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::checkIfPathTraversesLink(syncRootPath / "dir" / "item.txt", syncRootPath,
                                                                                  traversesLink, linkPath));
        CPPUNIT_ASSERT(!traversesLink);
        CPPUNIT_ASSERT(linkPath.empty());

        // The sync root itself is a link. A trailing separator in the sync root path is ignored.
        const auto rootLinkPath = temporaryDirectory.path() / "root_link";
        CPPUNIT_ASSERT_MESSAGE(toString(ioError),
                               IoHelper::createSymlink(realParentDirPath / "sync_root", rootLinkPath, true, ioError));
        for (const auto &rootPath: {rootLinkPath, rootLinkPath / ""}) {
            CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::checkIfPathTraversesLink(rootLinkPath / "dir" / "item.txt", rootPath,
                                                                                      traversesLink, linkPath));
            CPPUNIT_ASSERT(!traversesLink);
        }

        // A link below the sync root is still detected when the sync root is reached through a link.
        const auto innerLinkPath = realParentDirPath / "sync_root" / "inner_link";
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::createSymlink(realParentDirPath, innerLinkPath, true, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::PathContainsSymlink,
                             IoHelper::checkIfPathTraversesLink(syncRootPath / "inner_link" / "item.txt", syncRootPath,
                                                                traversesLink, linkPath));
        CPPUNIT_ASSERT(traversesLink);
        CPPUNIT_ASSERT_EQUAL(syncRootPath / "inner_link", linkPath);
    }


    // No ancestor of the path is a followed link.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const auto dirPath = temporaryDirectory.path() / "dir";
        std::error_code ec;
        CPPUNIT_ASSERT(std::filesystem::create_directories(dirPath, ec) && ec.value() == 0);

        auto traversesLink = true;
        auto linkPath = SyncPath("dummy");
        CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::checkIfPathTraversesLink(dirPath / "item.txt", temporaryDirectory.path(),
                                                                                  traversesLink, linkPath));
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
                             IoHelper::checkIfPathTraversesLink(symlinkPath / "item.txt", temporaryDirectory.path(),
                                                                traversesLink, traversedLinkPath));
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
                             IoHelper::checkIfPathTraversesLink(symlinkPath / "item.txt", temporaryDirectory.path(),
                                                                traversesLink, traversedLinkPath));
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
        CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::checkIfPathTraversesLink(symlinkPath, temporaryDirectory.path(),
                                                                                  traversesLink, traversedLinkPath));
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
                             IoHelper::checkIfPathTraversesLink(aliasPath / "item.txt", temporaryDirectory.path(), traversesLink,
                                                                traversedLinkPath));
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
                                                                temporaryDirectory.path(), traversesLink, traversedLinkPath));
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
                             IoHelper::checkIfPathTraversesLink(junctionPath / "item.txt", temporaryDirectory.path(),
                                                                traversesLink, traversedLinkPath));
        CPPUNIT_ASSERT(traversesLink);
        CPPUNIT_ASSERT_EQUAL(junctionPath, traversedLinkPath);
    }
#endif
}

// Every IoHelper method altering the filesystem must refuse to operate on a path that traverses a followed link, so that the
// synchronization can never alter an item located outside the sync folder through a link located inside it.
void TestIo::testFilesystemAlterationsThroughLink() {
    const LocalTemporaryDirectory temporaryDirectory;
    const auto realParentDirPath = temporaryDirectory.path() / "real_parent_dir";
    const auto outsideDirPath = temporaryDirectory.path() / "outside_sync_dir";
    std::error_code ec;
    CPPUNIT_ASSERT(std::filesystem::create_directories(realParentDirPath / "sync_dir", ec) && ec.value() == 0);
    CPPUNIT_ASSERT(std::filesystem::create_directories(outsideDirPath / "sub_dir", ec) && ec.value() == 0);

    // The sync folder is reached through a link located above it, like "/var" -> "private/var" on macOS: such a link is
    // chosen by the user or the system and must not prevent the synchronization from altering the sync folder.
    const auto parentLinkPath = temporaryDirectory.path() / "parent_link";
    std::filesystem::create_directory_symlink(realParentDirPath, parentLinkPath, ec);
    CPPUNIT_ASSERT_EQUAL(0, ec.value());
    const auto syncDirPath = parentLinkPath / "sync_dir";

    const auto sourceFilePath = syncDirPath / "file.txt";
    testhelpers::generateTestFile(sourceFilePath, 10);
    const auto sourceDirPath = syncDirPath / "dir";
    CPPUNIT_ASSERT(std::filesystem::create_directories(sourceDirPath, ec) && ec.value() == 0);

    // `linkPath` is located inside the sync folder and points outside of it.
    const auto linkPath = syncDirPath / "link";
    auto ioError = IoError::Unknown;
    CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::createSymlink(outsideDirPath, linkPath, true, ioError, syncDirPath));

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
            CPPUNIT_ASSERT(!IoHelper::createDirectory(parentPath / "new_dir", recursive, ioError, syncDirPath));
            CPPUNIT_ASSERT_EQUAL(IoError::PathContainsSymlink, ioError);
            expectOutsideDirUnchanged();
        }

        // createDirectory, recursive, with missing intermediate directories below the link
        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(
                !IoHelper::createDirectory(parentPath / "missing_1" / "missing_2" / "new_dir", true, ioError, syncDirPath));
        CPPUNIT_ASSERT_EQUAL(IoError::PathContainsSymlink, ioError);
        expectOutsideDirUnchanged();

        // copyFileOrDirectory
        for (const auto &sourcePath: {sourceFilePath, sourceDirPath}) {
            ioError = IoError::Unknown;
            CPPUNIT_ASSERT(!IoHelper::copyFileOrDirectory(sourcePath, parentPath / "copy", ioError, syncDirPath));
            CPPUNIT_ASSERT_EQUAL(IoError::PathContainsSymlink, ioError);
            expectOutsideDirUnchanged();
        }

        // createSymlink
        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(!IoHelper::createSymlink(sourceFilePath, parentPath / "new_link", false, ioError, syncDirPath));
        CPPUNIT_ASSERT_EQUAL(IoError::PathContainsSymlink, ioError);
        expectOutsideDirUnchanged();

        // renameItem / moveItem
        CPPUNIT_ASSERT_EQUAL(IoError::PathContainsSymlink,
                             IoHelper::renameItem(sourceFilePath, parentPath / "moved.txt", syncDirPath));
        CPPUNIT_ASSERT_EQUAL(IoError::PathContainsSymlink,
                             IoHelper::moveItem(sourceFilePath, parentPath / "moved.txt", syncDirPath));
        CPPUNIT_ASSERT(std::filesystem::exists(sourceFilePath, ec) && ec.value() == 0);
        expectOutsideDirUnchanged();

#if defined(KD_MACOS)
        // createAliasFromPath / createAlias
        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(!IoHelper::createAliasFromPath(sourceFilePath, parentPath / "new_alias", ioError, syncDirPath));
        CPPUNIT_ASSERT_EQUAL(IoError::PathContainsSymlink, ioError);
        expectOutsideDirUnchanged();

        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(!IoHelper::createAlias("dummy alias data", parentPath / "new_alias", ioError, syncDirPath));
        CPPUNIT_ASSERT_EQUAL(IoError::PathContainsSymlink, ioError);
        expectOutsideDirUnchanged();
#elif defined(KD_WINDOWS)
        // createJunctionFromPath / createJunction
        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(!IoHelper::createJunctionFromPath(sourceDirPath, parentPath / "new_junction", ioError, syncDirPath));
        CPPUNIT_ASSERT_EQUAL(IoError::PathContainsSymlink, ioError);
        expectOutsideDirUnchanged();

        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(!IoHelper::createJunction("dummy junction data", parentPath / "new_junction", ioError, syncDirPath));
        CPPUNIT_ASSERT_EQUAL(IoError::PathContainsSymlink, ioError);
        expectOutsideDirUnchanged();
#endif
    }

    // deleteItem: `linkPath / "sub_dir"` resolves to an existing directory outside the sync folder.
    ioError = IoError::Unknown;
    CPPUNIT_ASSERT(!IoHelper::deleteItem(linkPath / "sub_dir", ioError, syncDirPath));
    CPPUNIT_ASSERT_EQUAL(IoError::PathContainsSymlink, ioError);
    expectOutsideDirUnchanged();

    // moveItemToTrash: same target as above.
    CPPUNIT_ASSERT(!IoHelper::moveItemToTrash(linkPath / "sub_dir", syncDirPath));
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
            CPPUNIT_ASSERT(!IoHelper::createDirectory(danglingLinkPath / "missing" / "new_dir", recursive, ioError, syncDirPath));
            CPPUNIT_ASSERT_EQUAL(IoError::PathContainsSymlink, ioError);
            CPPUNIT_ASSERT(!std::filesystem::exists(missingTargetPath, ec));
            expectOutsideDirUnchanged();
        }
    }

    // Sanity check: the same operations succeed when the path traverses no link below the sync folder.
    ioError = IoError::Unknown;
    CPPUNIT_ASSERT(IoHelper::createDirectory(syncDirPath / "new_dir", false, ioError, syncDirPath));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(IoHelper::createDirectory(syncDirPath / "missing" / "new_dir", true, ioError, syncDirPath));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    // A trailing separator in the sync root path is ignored.
    CPPUNIT_ASSERT(IoHelper::createDirectory(syncDirPath / "missing_2" / "new_dir", true, ioError, syncDirPath / ""));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(IoHelper::copyFileOrDirectory(sourceFilePath, syncDirPath / "copy.txt", ioError, syncDirPath));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(IoHelper::createSymlink(sourceFilePath, syncDirPath / "new_link", false, ioError, syncDirPath));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::renameItem(sourceFilePath, syncDirPath / "moved.txt", syncDirPath));
    CPPUNIT_ASSERT(IoHelper::deleteItem(syncDirPath / "copy.txt", ioError, syncDirPath));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    // Without a sync root, no link is checked (e.g. log or database files located outside any sync folder).
    CPPUNIT_ASSERT(IoHelper::createDirectory(linkPath / "unchecked_dir", false, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(IoHelper::deleteItem(linkPath / "unchecked_dir", ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    expectOutsideDirUnchanged();

    // An item located outside the sync folder is rejected.
    CPPUNIT_ASSERT(!IoHelper::deleteItem(outsideDirPath / "sub_dir", ioError, syncDirPath));
    CPPUNIT_ASSERT_EQUAL(IoError::InvalidArgument, ioError);
    expectOutsideDirUnchanged();

    // Deleting the link itself removes the link, not its target.
    CPPUNIT_ASSERT(IoHelper::deleteItem(linkPath, ioError, syncDirPath));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(!std::filesystem::exists(std::filesystem::symlink_status(linkPath, ec)));
    expectOutsideDirUnchanged();
}

} // namespace KDC
