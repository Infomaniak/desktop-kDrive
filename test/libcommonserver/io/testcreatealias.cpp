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


void TestIo::testCreateAlias() {
    GetItemChecker checker{_testObj};

    // A MacOSX Finder alias on a regular file.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = temporaryDirectory.path() / "regular_file_alias";

        IoError aliasError = IoError::Unknown;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoError::Success);
        CPPUNIT_ASSERT(std::filesystem::exists(path));

        const auto result = checker.checkSuccessfulLinkRetrieval(path, targetPath, LinkType::FinderAlias, NodeType::File);
        CPPUNIT_ASSERT_MESSAGE(result.message, result.success);
    }

    // A MacOSX Finder alias on a regular directory.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const SyncPath path = temporaryDirectory.path() / "regular_dir_alias";

        IoError aliasError = IoError::Unknown;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoError::Success);
        CPPUNIT_ASSERT(std::filesystem::exists(path));

        const auto result = checker.checkSuccessfulLinkRetrieval(path, targetPath, LinkType::FinderAlias, NodeType::Directory);
        CPPUNIT_ASSERT_MESSAGE(result.message, result.success);
    }

    // The target file does not exist: failure
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "non-existing.jpg"; // This file does not exist.
        const SyncPath path = temporaryDirectory.path() / "regular_dir_alias";

        IoError aliasError = IoError::Success;
        CPPUNIT_ASSERT(!IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoError::NoSuchFileOrDirectory);
        CPPUNIT_ASSERT(!std::filesystem::exists(path));
    }

    // The alias path is the path of an existing file: overwriting
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = temporaryDirectory.path() / "file.txt";
        { std::ofstream ofs(path); }

        IoError aliasError = IoError::Unknown;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoError::Success);
        CPPUNIT_ASSERT(std::filesystem::exists(path));

        const auto result = checker.checkSuccessfulLinkRetrieval(path, targetPath, LinkType::FinderAlias, NodeType::File);
        CPPUNIT_ASSERT_MESSAGE(result.message, result.success);
    }

    // The alias path is the path of an existing directory: failure
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = temporaryDirectory.path();

        IoError aliasError = IoError::Success;
        CPPUNIT_ASSERT(!IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoError::Unknown);
        CPPUNIT_ASSERT(std::filesystem::exists(path));

        ItemType itemType;
        CPPUNIT_ASSERT_MESSAGE(toString(itemType.ioError), IoHelper::getItemType(path, itemType));
        CPPUNIT_ASSERT(itemType.ioError == IoError::Success);
        CPPUNIT_ASSERT(itemType.nodeType == NodeType::Directory);
        CPPUNIT_ASSERT(itemType.linkType == LinkType::None);
        CPPUNIT_ASSERT(itemType.targetType == NodeType::Unknown);
    }

    // The alias path is the target path (of an existing file): no error!
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "file.txt";
        { std::ofstream ofs(path); }
        const SyncPath targetPath = path;

        IoError aliasError = IoError::Unknown;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoError::Success);
        CPPUNIT_ASSERT(std::filesystem::exists(path));

        const auto result = checker.checkSuccessfulLinkRetrieval(path, targetPath, LinkType::FinderAlias, NodeType::File);
        CPPUNIT_ASSERT_MESSAGE(result.message, result.success);
    }

    // The alias file name is very long: failure
    {
        const std::string veryLongfileName(1000,
                                           'a'); // Exceeds the max allowed name length on every file system of interest.
        const SyncPath path = _localTestDirPath / veryLongfileName; // This file doesn't exist.
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";

        IoError aliasError = IoError::Success;
        CPPUNIT_ASSERT(!IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT_EQUAL(IoError::NoSuchFileOrDirectory, aliasError);
        // The test CPPUNIT_ASSERT(!std::filesystem::exists(path)) throws because a filesystem error.
    }

    // The target file name is very long: failure
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "alias.txt"; // This file doesn't exist.

        const std::string veryLongfileName(1000,
                                           'a'); // Exceeds the max allowed name length on every file system of interest.
        const SyncPath targetPath = _localTestDirPath / veryLongfileName; // This file doesn't exist.

        IoError aliasError = IoError::Success;
        CPPUNIT_ASSERT(!IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoError::NoSuchFileOrDirectory);
        // The test CPPUNIT_ASSERT(!std::filesystem::exists(path)) throws because of a filesystem error.
    }

    // The alias file path is very long: failure
    {
        const std::string pathSegment(50, 'a');
        const SyncPath path = makeVeryLonPath(_localTestDirPath);
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";

        IoError aliasError = IoError::Success;
        CPPUNIT_ASSERT(!IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT_EQUAL(IoError::NoSuchFileOrDirectory, aliasError);
        // The test CPPUNIT_ASSERT(!std::filesystem::exists(path)) throws because a filesystem error.
    }

    // The target file path is very long: failure
    {
        const std::string pathSegment(50, 'a');
        const SyncPath targetPath = makeVeryLonPath(_localTestDirPath);

        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "alias.txt"; // This file doesn't exist.

        IoError aliasError = IoError::Success;
        CPPUNIT_ASSERT(!IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoError::NoSuchFileOrDirectory);
        // The test CPPUNIT_ASSERT(!std::filesystem::exists(path)) throws because a filesystem error.
    }

    // The alias file name contains emojis: success
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / makeFileNameWithEmojis();
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";

        IoError aliasError = IoError::Unknown;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoError::Success);
        CPPUNIT_ASSERT(std::filesystem::exists(path));

        const auto result = checker.checkSuccessfulLinkRetrieval(path, targetPath, LinkType::FinderAlias, NodeType::File);
        CPPUNIT_ASSERT_MESSAGE(result.message, result.success);
    }
}


} // namespace KDC
