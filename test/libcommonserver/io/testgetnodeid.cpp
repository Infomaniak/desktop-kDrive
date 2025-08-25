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

#include "testio.h"

#include <filesystem>

using namespace CppUnit;

namespace KDC {

void TestIo::testGetNodeId() {
    // A regular file
    {
        const SyncPath path = _localTestDirPath / "test_pictures/picture-1.jpg";
        NodeId nodeId;
        CPPUNIT_ASSERT(IoHelper::getNodeId(path, nodeId));
        CPPUNIT_ASSERT(!nodeId.empty());
    }

    // A regular directory
    {
        const SyncPath path = _localTestDirPath / "test_pictures";
        NodeId nodeId;
        CPPUNIT_ASSERT(IoHelper::getNodeId(path, nodeId));
        CPPUNIT_ASSERT(!nodeId.empty());
    }

    // A regular symbolic link on a file
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "regular_file_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        NodeId nodeId;
        CPPUNIT_ASSERT(IoHelper::getNodeId(path, nodeId));
        CPPUNIT_ASSERT(!nodeId.empty());
    }

    // A regular symbolic link on a folder
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "regular_dir_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        NodeId nodeId;
        CPPUNIT_ASSERT(IoHelper::getNodeId(path, nodeId));
        CPPUNIT_ASSERT(!nodeId.empty());
    }

    // A non-existing file
    {
        const SyncPath path = _localTestDirPath / "non-existing.jpg"; // This file does not exist.
        NodeId nodeId;
        CPPUNIT_ASSERT(!IoHelper::getNodeId(path, nodeId));
        CPPUNIT_ASSERT(nodeId.empty());
    }

    // A non-existing file with a very long name
    {
        const std::string veryLongfileName(1000,
                                           'a'); // Exceeds the max allowed name length on every file system of interest.
        const SyncPath path = _localTestDirPath / veryLongfileName; // This file doesn't exist.
        NodeId nodeId;
        CPPUNIT_ASSERT(!IoHelper::getNodeId(path, nodeId));
        CPPUNIT_ASSERT(nodeId.empty());
    }

    // A non-existing file with a very long path
    {
        const std::string pathSegment(50, 'a');
        SyncPath path = _localTestDirPath;
        for (int i = 0; i < 1000; ++i) {
            path /= pathSegment; // Eventually exceeds the max allowed path length on every file system of interest.
        }
        NodeId nodeId;
        CPPUNIT_ASSERT(!IoHelper::getNodeId(path, nodeId));
        CPPUNIT_ASSERT(nodeId.empty());
    }

#if defined(KD_MACOS)
    // An existing file with dots and colons in its name
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / ":.file.::.name.:";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
            ofs.close();
        }

        NodeId nodeId;
        CPPUNIT_ASSERT(IoHelper::getNodeId(path, nodeId));
        CPPUNIT_ASSERT(!nodeId.empty());
    }

    // An existing file with emojis in its name
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / makeFileNameWithEmojis();
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
            ofs.close();
        }

        NodeId nodeId;
        CPPUNIT_ASSERT(IoHelper::getNodeId(path, nodeId));
        CPPUNIT_ASSERT(!nodeId.empty());
    }
#endif
    // A dangling symbolic link
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path() / "non_existing_test_file.txt"; // This file does not exist.
        const SyncPath path = temporaryDirectory.path() / "dangling_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        std::error_code ec;
        const SyncPath targetPath_ = std::filesystem::read_symlink(path, ec);
        CPPUNIT_ASSERT(targetPath == targetPath_ && ec.value() == 0);

        NodeId nodeId;
        CPPUNIT_ASSERT(IoHelper::getNodeId(path, nodeId));
        CPPUNIT_ASSERT(!nodeId.empty());
    }

#if defined(KD_MACOS)
    // A MacOSX Finder alias on a regular file.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = temporaryDirectory.path() / "regular_file_alias";

        IoError aliasError;
        CPPUNIT_ASSERT_MESSAGE(toString(aliasError), IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoError::Success);
        CPPUNIT_ASSERT(std::filesystem::exists(path));

        NodeId nodeId;
        CPPUNIT_ASSERT(IoHelper::getNodeId(path, nodeId));
        CPPUNIT_ASSERT(!nodeId.empty());
    }

    // A MacOSX Finder alias on a regular directory.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const SyncPath path = temporaryDirectory.path() / "regular_dir_alias";

        IoError aliasError;
        CPPUNIT_ASSERT_MESSAGE(toString(aliasError), IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoError::Success);
        CPPUNIT_ASSERT(std::filesystem::exists(path));

        NodeId nodeId;
        CPPUNIT_ASSERT(IoHelper::getNodeId(path, nodeId));
        CPPUNIT_ASSERT(!nodeId.empty());
    }

    // A dangling MacOSX Finder alias on a non-existing file.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path() / "file_to_be_deleted.png"; // This file will be deleted.
        const SyncPath path = temporaryDirectory.path() / "dangling_file_alias";
        {
            std::ofstream ofs(targetPath);
            ofs << "Some content.\n";
            ofs.close();
        }

        IoError aliasError;
        CPPUNIT_ASSERT_MESSAGE(toString(aliasError), IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoError::Success);
        CPPUNIT_ASSERT(std::filesystem::exists(path));

        std::filesystem::remove(targetPath);
        CPPUNIT_ASSERT(!std::filesystem::exists(targetPath));

        NodeId nodeId;
        CPPUNIT_ASSERT(IoHelper::getNodeId(path, nodeId));
        CPPUNIT_ASSERT(!nodeId.empty());
    }

    // A dangling MacOSX Finder alias on a non-existing directory.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path() / "directory_to_be_deleted"; // This directory will be deleted.
        std::filesystem::create_directory(targetPath);

        const SyncPath path = temporaryDirectory.path() / "dangling_directory_alias";

        IoError aliasError;
        CPPUNIT_ASSERT_MESSAGE(toString(aliasError), IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoError::Success);
        CPPUNIT_ASSERT(std::filesystem::exists(path));

        std::filesystem::remove_all(targetPath);
        CPPUNIT_ASSERT(!std::filesystem::exists(targetPath));

        NodeId nodeId;
        CPPUNIT_ASSERT(IoHelper::getNodeId(path, nodeId));
        CPPUNIT_ASSERT(!nodeId.empty());
    }

#endif
    // A regular file missing all permissions (no error expected)
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "permission_less_file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
            ofs.close();
        }
        const auto allPermissions =
                std::filesystem::perms::owner_all | std::filesystem::perms::others_all | std::filesystem::perms::group_all;
        std::filesystem::permissions(path, allPermissions, std::filesystem::perm_options::remove);

        NodeId nodeId;
        CPPUNIT_ASSERT(IoHelper::getNodeId(path, nodeId));

        std::filesystem::permissions(path, allPermissions, std::filesystem::perm_options::add);

        CPPUNIT_ASSERT(!nodeId.empty());
    }

    // A regular directory missing all permissions (no error expected)
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "permission_less_directory";
        std::filesystem::create_directory(path);

        const auto allPermissions =
                std::filesystem::perms::owner_all | std::filesystem::perms::others_all | std::filesystem::perms::group_all;
        std::filesystem::permissions(path, allPermissions, std::filesystem::perm_options::remove);

        NodeId nodeId;
        CPPUNIT_ASSERT(IoHelper::getNodeId(path, nodeId));

        std::filesystem::permissions(path, allPermissions, std::filesystem::perm_options::add);

        CPPUNIT_ASSERT(!nodeId.empty());
    }

    // A regular file within a subdirectory that misses owner read permission (no error expected)
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath subdir = temporaryDirectory.path() / "permission_less_subdirectory";
        std::filesystem::create_directory(subdir);
        const SyncPath path = subdir / "file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
            ofs.close();
        }
        std::filesystem::permissions(subdir, std::filesystem::perms::owner_read, std::filesystem::perm_options::remove);

        NodeId nodeId;
        CPPUNIT_ASSERT(IoHelper::getNodeId(path, nodeId));

        // Restore permission to allow subdir removal
        std::filesystem::permissions(subdir, std::filesystem::perms::owner_read, std::filesystem::perm_options::add);

        CPPUNIT_ASSERT(!nodeId.empty());
    }

    // A regular file within a subdirectory that misses owner exec permission:
    // - access denied expected on MacOSX
    // - no error on Windows
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath subdir = temporaryDirectory.path() / "permission_less_subdirectory";
        std::filesystem::create_directory(subdir);
        const SyncPath path = subdir / "file.txt";
        { std::ofstream ofs(path); }
        std::filesystem::permissions(subdir, std::filesystem::perms::owner_exec, std::filesystem::perm_options::remove);

        NodeId nodeId;
#if defined(KD_WINDOWS)
        CPPUNIT_ASSERT(IoHelper::getNodeId(path, nodeId));
#else
        CPPUNIT_ASSERT(!IoHelper::getNodeId(path, nodeId));
#endif

        // Restore permission to allow subdir removal
        std::filesystem::permissions(subdir, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);

#if defined(KD_WINDOWS)
        CPPUNIT_ASSERT(!nodeId.empty());
#else
        CPPUNIT_ASSERT(nodeId.empty());
#endif
    }
}


} // namespace KDC
