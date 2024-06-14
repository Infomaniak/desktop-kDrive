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

#include "testio.h"

#include <filesystem>

using namespace CppUnit;

namespace KDC {

void TestIo::testCreateSymlink() {
    // Successfully creates a symlink on a regular file.
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures" / "picture-1.jpg";
        const SyncPath path = temporaryDirectory.path / "regular_file_alias";

        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(IoHelper::createSymlink(targetPath, path, false, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(std::filesystem::is_symlink(path));

        ItemType itemType;
        CPPUNIT_ASSERT(IoHelper::getItemType(path, itemType));
        CPPUNIT_ASSERT(itemType.ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(itemType.nodeType == NodeType::File);
        CPPUNIT_ASSERT(itemType.linkType == LinkTypeSymlink);
        CPPUNIT_ASSERT(itemType.targetType == NodeType::File);
    }

    // Successfully creates a symlink on a regular directory.
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const SyncPath path = temporaryDirectory.path / "regular_dir_alias";

        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(IoHelper::createSymlink(targetPath, path, true, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(std::filesystem::is_symlink(path));

        ItemType itemType;
        CPPUNIT_ASSERT(IoHelper::getItemType(path, itemType));
        CPPUNIT_ASSERT(itemType.ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(itemType.nodeType == NodeType::File);
        CPPUNIT_ASSERT(itemType.linkType == LinkTypeSymlink);
        CPPUNIT_ASSERT(itemType.targetType == NodeType::Directory);
    }

    // Successfully creates a symlink whose target path indicates a non-existing item.
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "non-existing.jpg";
        const SyncPath path = temporaryDirectory.path / "file_symlink";

        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(IoHelper::createSymlink(targetPath, path, false, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(std::filesystem::is_symlink(path));  // Dangling link created.

        ItemType itemType;
        CPPUNIT_ASSERT(IoHelper::getItemType(path, itemType));
        CPPUNIT_ASSERT(itemType.ioError == IoErrorSuccess);  // Although the target path is invalid.
        CPPUNIT_ASSERT(itemType.nodeType == NodeType::File);
        CPPUNIT_ASSERT(itemType.linkType == LinkTypeSymlink);
#ifdef _WIN32
        CPPUNIT_ASSERT(itemType.targetType == NodeType::File);
#else
        CPPUNIT_ASSERT(itemType.targetType == NodeType::Unknown);
#endif
    }

    // Fails to create a symlink whose path indicates an existing file: no overwriting
    // Warning: this doesn't match IoHelper::createAliasFromPath behaviour.
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = temporaryDirectory.path / "file.txt";
        { std::ofstream ofs(path); }

        IoError ioError = IoErrorSuccess;
        CPPUNIT_ASSERT(!IoHelper::createSymlink(targetPath, path, false, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorFileExists);
        CPPUNIT_ASSERT(!std::filesystem::is_symlink(path));
    }

    // Fails to create a symlink whose path is the path of an existing directory
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = temporaryDirectory.path;

        IoError ioError = IoErrorSuccess;
        CPPUNIT_ASSERT(!IoHelper::createSymlink(targetPath, path, true, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorFileExists);
        CPPUNIT_ASSERT(std::filesystem::exists(path));

        ItemType itemType;
        CPPUNIT_ASSERT(IoHelper::getItemType(path, itemType));
        CPPUNIT_ASSERT(itemType.ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(itemType.nodeType == NodeType::Directory);
        CPPUNIT_ASSERT(itemType.linkType == LinkTypeNone);
        CPPUNIT_ASSERT(itemType.targetType == NodeType::Unknown);
    }

    // Fails to create a symlink whose path is the target path (of an existing file)
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "file.txt";
        { std::ofstream ofs(path); }
        const SyncPath targetPath = path;

        IoError aliasError;
        CPPUNIT_ASSERT(!IoHelper::createSymlink(targetPath, path, false, aliasError));
        CPPUNIT_ASSERT(aliasError == IoErrorInvalidArgument);
        CPPUNIT_ASSERT(std::filesystem::exists(path));

        ItemType itemType;
        CPPUNIT_ASSERT(IoHelper::getItemType(path, itemType));
        CPPUNIT_ASSERT(itemType.ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(itemType.nodeType == NodeType::File);
        CPPUNIT_ASSERT(itemType.linkType == LinkTypeNone);
        CPPUNIT_ASSERT(itemType.targetType == NodeType::Unknown);
    }

    // Fails to create a symlink whose name is very long
    {
        const std::string veryLongfileName(1000, 'a');  // Exceeds the max allowed name length on every file system of interest.
        const SyncPath path = _localTestDirPath / veryLongfileName;  // This file doesn't exist.
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";

        IoError ioError;
        CPPUNIT_ASSERT(!IoHelper::createSymlink(targetPath, path, false, ioError));
#ifdef _WIN32
        CPPUNIT_ASSERT(ioError == IoErrorNoSuchFileOrDirectory);
#else
        CPPUNIT_ASSERT(ioError == IoErrorFileNameTooLong);
#endif
        // The test CPPUNIT_ASSERT(!std::filesystem::exists(path)) throws because a filesystem error.
    }

    // Successfully creates a symlink whose file name contains emojis
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / makeFileNameWithEmojis();
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";

        IoError aliasError;
        CPPUNIT_ASSERT(IoHelper::createSymlink(targetPath, path, false, aliasError));
        CPPUNIT_ASSERT(aliasError == IoErrorSuccess);

        CPPUNIT_ASSERT(std::filesystem::exists(path));

        ItemType itemType;
        CPPUNIT_ASSERT(IoHelper::getItemType(path, itemType));
        CPPUNIT_ASSERT(itemType.ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(itemType.nodeType == NodeType::File);
        CPPUNIT_ASSERT(itemType.linkType == LinkTypeSymlink);
        CPPUNIT_ASSERT(itemType.targetType == NodeType::File);
    }
}

}  // namespace KDC
