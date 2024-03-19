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

void TestIo::testCreateAlias() {
    // A MacOSX Finder alias on a regular file.
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = temporaryDirectory.path / "regular_file_alias";

        IoError aliasError = IoErrorUnknown;
        CPPUNIT_ASSERT(IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoErrorSuccess);
        CPPUNIT_ASSERT(std::filesystem::exists(path));

        ItemType itemType;
        CPPUNIT_ASSERT(IoHelper::getItemType(path, itemType));
        CPPUNIT_ASSERT(itemType.ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(itemType.nodeType == NodeTypeFile);
        CPPUNIT_ASSERT(itemType.linkType == LinkTypeFinderAlias);
        CPPUNIT_ASSERT(itemType.targetType == NodeTypeFile);
    }

    // A MacOSX Finder alias on a regular directory.
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const SyncPath path = temporaryDirectory.path / "regular_dir_alias";

        IoError aliasError = IoErrorUnknown;
        CPPUNIT_ASSERT(IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoErrorSuccess);
        CPPUNIT_ASSERT(std::filesystem::exists(path));

        ItemType itemType;
        CPPUNIT_ASSERT(IoHelper::getItemType(path, itemType));
        CPPUNIT_ASSERT(itemType.ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(itemType.nodeType == NodeTypeFile);
        CPPUNIT_ASSERT(itemType.linkType == LinkTypeFinderAlias);
        CPPUNIT_ASSERT(itemType.targetType == NodeTypeDirectory);
    }

    // The target file does not exist: failure
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "non-existing.jpg";  // This file does not exist.
        const SyncPath path = temporaryDirectory.path / "regular_dir_alias";

        IoError aliasError = IoErrorSuccess;
        CPPUNIT_ASSERT(!IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoErrorNoSuchFileOrDirectory);
        CPPUNIT_ASSERT(!std::filesystem::exists(path));
    }

    // The alias path is the path of an existing file: overwriting
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = temporaryDirectory.path / "file.txt";
        { std::ofstream ofs(path); }

        IoError aliasError = IoErrorUnknown;
        CPPUNIT_ASSERT(IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoErrorSuccess);
        CPPUNIT_ASSERT(std::filesystem::exists(path));

        ItemType itemType;
        CPPUNIT_ASSERT(IoHelper::getItemType(path, itemType));
        CPPUNIT_ASSERT(itemType.ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(itemType.nodeType == NodeTypeFile);
        CPPUNIT_ASSERT(itemType.linkType == LinkTypeFinderAlias);
        CPPUNIT_ASSERT(itemType.targetType == NodeTypeFile);
    }

    // The alias path is the path of an existing directory: failure
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = temporaryDirectory.path;

        IoError aliasError = IoErrorSuccess;
        CPPUNIT_ASSERT(!IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoErrorUnknown);
        CPPUNIT_ASSERT(std::filesystem::exists(path));

        ItemType itemType;
        CPPUNIT_ASSERT(IoHelper::getItemType(path, itemType));
        CPPUNIT_ASSERT(itemType.ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(itemType.nodeType == NodeTypeDirectory);
        CPPUNIT_ASSERT(itemType.linkType == LinkTypeNone);
        CPPUNIT_ASSERT(itemType.targetType == NodeTypeUnknown);
    }

    // The alias path is the target path (of an existing file): no error!
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "file.txt";
        { std::ofstream ofs(path); }
        const SyncPath targetPath = path;

        IoError aliasError = IoErrorUnknown;
        CPPUNIT_ASSERT(IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoErrorSuccess);
        CPPUNIT_ASSERT(std::filesystem::exists(path));

        ItemType itemType;
        CPPUNIT_ASSERT(IoHelper::getItemType(path, itemType));
        CPPUNIT_ASSERT(itemType.ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(itemType.nodeType == NodeTypeFile);
        CPPUNIT_ASSERT(itemType.linkType == LinkTypeFinderAlias);
        CPPUNIT_ASSERT(itemType.nodeType == NodeTypeFile);
    }

    // The alias file name is very long: failure
    {
        const std::string veryLongfileName(1000, 'a');  // Exceeds the max allowed name length on every file system of interest.
        const SyncPath path = _localTestDirPath / veryLongfileName;  // This file doesn't exist.
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";

        IoError aliasError = IoErrorSuccess;
        CPPUNIT_ASSERT(!IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoErrorNoSuchFileOrDirectory);
        // The test CPPUNIT_ASSERT(!std::filesystem::exists(path)) throws because a filesystem error.
    }

    // The target file name is very long: failure
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "alias.txt";  // This file doesn't exist.

        const std::string veryLongfileName(1000, 'a');  // Exceeds the max allowed name length on every file system of interest.
        const SyncPath targetPath = _localTestDirPath / veryLongfileName;  // This file doesn't exist.

        IoError aliasError = IoErrorSuccess;
        CPPUNIT_ASSERT(!IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoErrorNoSuchFileOrDirectory);
        // The test CPPUNIT_ASSERT(!std::filesystem::exists(path)) throws because a filesystem error.
    }

    // The alias file path is very long: failure
    {
        const std::string pathSegment(50, 'a');
        SyncPath path = _localTestDirPath;
        for (int i = 0; i < 1000; ++i) {
            path /= pathSegment;  // Eventually exceeds the max allowed path length on every file system of interest.
        }

        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        IoError aliasError = IoErrorSuccess;
        CPPUNIT_ASSERT(!IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoErrorNoSuchFileOrDirectory);
        // The test CPPUNIT_ASSERT(!std::filesystem::exists(path)) throws because a filesystem error.
    }

    // The target file path is very long: failure
    {
        const std::string pathSegment(50, 'a');
        SyncPath targetPath = _localTestDirPath;
        for (int i = 0; i < 1000; ++i) {
            targetPath /= pathSegment;  // Eventually exceeds the max allowed path length on every file system of interest.
        }

        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "alias.txt";  // This file doesn't exist.

        IoError aliasError = IoErrorSuccess;
        CPPUNIT_ASSERT(!IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoErrorNoSuchFileOrDirectory);
        // The test CPPUNIT_ASSERT(!std::filesystem::exists(path)) throws because a filesystem error.
    }

    // The alias file name contains emojis: success
    {
        using namespace std::string_literals;  // operator ""s
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / std::string{u8"🫃😋🌲👣🍔🕉️⛎"s};
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";

        IoError aliasError = IoErrorUnknown;
        CPPUNIT_ASSERT(IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoErrorSuccess);
        CPPUNIT_ASSERT(std::filesystem::exists(path));

        ItemType itemType;
        CPPUNIT_ASSERT(IoHelper::getItemType(path, itemType));
        CPPUNIT_ASSERT(itemType.ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(itemType.nodeType == NodeTypeFile);
        CPPUNIT_ASSERT(itemType.linkType == LinkTypeFinderAlias);
    }
}


}  // namespace KDC
