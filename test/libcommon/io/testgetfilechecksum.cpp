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
#include <fstream>

using namespace CppUnit;

namespace KDC {

void TestIo::testGetFileChecksum() {
    constexpr std::string_view hash1 = "xxh3:5dcc477e35136516";
    constexpr std::string_view hash2 = "xxh3:91f9d1732ca53515";
    // A regular file
    {
        const SyncPath path = _localTestDirPath / "test_pictures/picture-1.jpg";
        std::ifstream ifs;
        std::string checksum;
        const IoError ioError = IoHelper::getFileChecksum(path, ifs, checksum);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        CPPUNIT_ASSERT(!checksum.empty());
        CPPUNIT_ASSERT_EQUAL(std::string(hash1), checksum);
    }

    // A regular file whose name exists with a different capitalization
    {
        const SyncPath path = _localTestDirPath / "test_pictures/Picture-1.jpg";
        std::ifstream ifs;
        std::string checksum;
        const IoError ioError = IoHelper::getFileChecksum(path, ifs, checksum);
#if defined(KD_MACOS) || defined(KD_WINDOWS)
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        CPPUNIT_ASSERT(!checksum.empty());
        CPPUNIT_ASSERT_EQUAL(std::string(hash1), checksum);
#else
        CPPUNIT_ASSERT_EQUAL(IoError::NoSuchFileOrDirectory, ioError);
        CPPUNIT_ASSERT(checksum.empty());
#endif
    }

    // A regular symbolic link on a file
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "regular_file_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);
        std::ifstream ifs;
        std::string checksum;
        const IoError ioError = IoHelper::getFileChecksum(path, ifs, checksum);
        CPPUNIT_ASSERT_EQUAL(IoError::InvalidArgument, ioError);
        CPPUNIT_ASSERT(checksum.empty());
    }

    // A non-existing file
    {
        const SyncPath path = _localTestDirPath / "non-existing.jpg"; // This file does not exist.
        std::ifstream ifs;
        std::string checksum;
        const IoError ioError = IoHelper::getFileChecksum(path, ifs, checksum);
        CPPUNIT_ASSERT_EQUAL(IoError::NoSuchFileOrDirectory, ioError);
        CPPUNIT_ASSERT(checksum.empty());
    }

    // A non-existing file with a very long name
    {
        const std::string veryLongfileName(1000, 'a'); // Exceeds the max allowed name length on every file system of interest.
        const SyncPath path = _localTestDirPath / veryLongfileName; // This file doesn't exist.
        std::ifstream ifs;
        std::string checksum;
        const IoError ioError = IoHelper::getFileChecksum(path, ifs, checksum);
        CPPUNIT_ASSERT(checksum.empty());
#if defined(KD_WINDOWS)
        CPPUNIT_ASSERT_EQUAL(IoError::NoSuchFileOrDirectory, ioError);
#else
        CPPUNIT_ASSERT_EQUAL(IoError::FileNameTooLong, ioError);
#endif
    }

    // A non-existing file with a very long path
    {
        const std::string pathSegment(50, 'a');
        SyncPath path = _localTestDirPath;
        for (auto i = 0; i < 1000; ++i) {
            path /= pathSegment; // Eventually exceeds the max allowed path length on every file system of interest.
        }
        std::ifstream ifs;
        std::string checksum;
        const IoError ioError = IoHelper::getFileChecksum(path, ifs, checksum);
        CPPUNIT_ASSERT(checksum.empty());
#if defined(KD_WINDOWS)
        CPPUNIT_ASSERT_EQUAL(IoError::NoSuchFileOrDirectory, ioError);
#else
        CPPUNIT_ASSERT_EQUAL(IoError::FileNameTooLong, ioError);
#endif
    }

    // A hidden file
    {
        const LocalTemporaryDirectory temporaryDirectory;
#if defined(KD_MACOS) || defined(KD_WINDOWS)
        const SyncPath path = temporaryDirectory.path() / "hidden_file.txt";
#else
        const SyncPath path = temporaryDirectory.path() / ".hidden_file.txt";
#endif
        {
            std::ofstream ofs(path);
            ofs << "Some content.";
        }
#if defined(KD_MACOS) || defined(KD_WINDOWS)
        IoHelper::setFileHidden(path, true);
#endif
        std::ifstream ifs;
        std::string checksum;
        const IoError ioError = IoHelper::getFileChecksum(path, ifs, checksum);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        CPPUNIT_ASSERT(!checksum.empty());
        CPPUNIT_ASSERT_EQUAL(std::string(hash2), checksum);
    }

    // A file with dots and colons in its name
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / ":.file.::.name.:";
        {
            std::ofstream ofs(path);
            ofs << "Some content.";
        }
        std::ifstream ifs;
        std::string checksum;
        const IoError ioError = IoHelper::getFileChecksum(path, ifs, checksum);
#if defined(KD_WINDOWS)
        CPPUNIT_ASSERT_EQUAL(IoError::NoSuchFileOrDirectory, ioError);
        CPPUNIT_ASSERT(checksum.empty());
#else
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        CPPUNIT_ASSERT_EQUAL(std::string(hash2), checksum);
#endif
    }

    // An existing file with emojis
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / makeFileNameWithEmojis();
        {
            std::ofstream ofs(path);
            ofs << "Some content.";
        }
        std::ifstream ifs;
        std::string checksum;
        const IoError ioError = IoHelper::getFileChecksum(path, ifs, checksum);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        CPPUNIT_ASSERT(!checksum.empty());
        CPPUNIT_ASSERT_EQUAL(std::string(hash2), checksum);
    }

    // A dangling symbolic link
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path() / "non_existing_test_file.txt"; // This file does not exist.
        const SyncPath path = temporaryDirectory.path() / "dangling_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);
        std::ifstream ifs;
        std::string checksum;
        const IoError ioError = IoHelper::getFileChecksum(path, ifs, checksum);
        CPPUNIT_ASSERT_EQUAL(IoError::InvalidArgument, ioError);
        CPPUNIT_ASSERT(checksum.empty());
    }

#if defined(KD_MACOS)
    // A macOS Finder alias on a regular file: not computed, returns empty checksum.
    {
        using enum IoError;
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = temporaryDirectory.path() / "regular_file_alias";
        IoError aliasError = Unknown;
        CPPUNIT_ASSERT(IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT_EQUAL(Success, aliasError);
        std::ifstream ifs;
        std::string checksum;
        const IoError ioError = IoHelper::getFileChecksum(path, ifs, checksum);
        CPPUNIT_ASSERT_EQUAL(InvalidArgument, ioError);
        CPPUNIT_ASSERT(checksum.empty());
    }

    // A dangling macOS Finder alias
    {
        using enum IoError;
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path() / "file_to_be_deleted.png"; // This file will be deleted.
        const SyncPath path = temporaryDirectory.path() / "dangling_file_alias";
        {
            std::ofstream ofs(targetPath);
            ofs << "Some content.";
        }
        IoError aliasError = Unknown;
        CPPUNIT_ASSERT(IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT_EQUAL(Success, aliasError);
        IoError deleteError = Unknown;
        CPPUNIT_ASSERT(IoHelper::deleteItem(targetPath, deleteError));
        CPPUNIT_ASSERT_EQUAL(Success, deleteError);
        std::ifstream ifs;
        std::string checksum;
        const IoError ioError = IoHelper::getFileChecksum(path, ifs, checksum);
        CPPUNIT_ASSERT_EQUAL(InvalidArgument, ioError);
        CPPUNIT_ASSERT(checksum.empty());
    }
#endif

    // A regular file missing all permissions (no error expected on Windows, error expected on Unix)
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "permission_less_file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.";
        }
        std::filesystem::permissions(path, std::filesystem::perms::all, std::filesystem::perm_options::remove);
        {
            std::ifstream ifs;
            std::string checksum;
            const IoError ioError = IoHelper::getFileChecksum(path, ifs, checksum);
#if defined(KD_WINDOWS)
            CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
            CPPUNIT_ASSERT(!checksum.empty());
            CPPUNIT_ASSERT_EQUAL(std::string(hash2), checksum);
#else
            CPPUNIT_ASSERT_EQUAL(IoError::AccessDenied, ioError);
            CPPUNIT_ASSERT(checksum.empty());
#endif
        }
        std::filesystem::permissions(path, std::filesystem::perms::all, std::filesystem::perm_options::add);
        {
            std::ifstream ifs;
            std::string checksum;
            const IoError ioError = IoHelper::getFileChecksum(path, ifs, checksum);
            CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
            CPPUNIT_ASSERT(!checksum.empty());
            CPPUNIT_ASSERT_EQUAL(std::string(hash2), checksum);
        }
    }

    // A regular file within a subdirectory that misses owner read permission (no error expected)
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath subdir = temporaryDirectory.path() / "permission_less_subdirectory";
        CPPUNIT_ASSERT(std::filesystem::create_directory(subdir));
        const SyncPath path = subdir / "file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.";
        }
        std::filesystem::permissions(subdir, std::filesystem::perms::owner_read, std::filesystem::perm_options::remove);
        {
            std::ifstream ifs;
            std::string checksum;
            const IoError ioError = IoHelper::getFileChecksum(path, ifs, checksum);
            CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
            CPPUNIT_ASSERT(!checksum.empty());
            CPPUNIT_ASSERT_EQUAL(std::string(hash2), checksum);
        }
        // Restore permission to allow subdir removal
        std::filesystem::permissions(subdir, std::filesystem::perms::owner_read, std::filesystem::perm_options::add);
        {
            std::ifstream ifs;
            std::string checksum;
            const IoError ioError = IoHelper::getFileChecksum(path, ifs, checksum);
            CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
            CPPUNIT_ASSERT(!checksum.empty());
            CPPUNIT_ASSERT_EQUAL(std::string(hash2), checksum);
        }
    }

    // A regular file within a subdirectory that misses owner search/exec permission:
    // - access denied expected on macOS/Linux
    // - no error expected on Windows
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath subdir = temporaryDirectory.path() / "permission_less_subdirectory";
        CPPUNIT_ASSERT(std::filesystem::create_directory(subdir));
        const SyncPath path = subdir / "file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.";
        }
        std::filesystem::permissions(subdir, std::filesystem::perms::owner_exec, std::filesystem::perm_options::remove);
        std::ifstream ifs;
        std::string checksum;
        const IoError ioError = IoHelper::getFileChecksum(path, ifs, checksum);
        // Restore permission to allow subdir removal
        std::filesystem::permissions(subdir, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);
#if defined(KD_WINDOWS)
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        CPPUNIT_ASSERT(!checksum.empty());
        CPPUNIT_ASSERT_EQUAL(std::string(hash2), checksum);
#else
        CPPUNIT_ASSERT_EQUAL(IoError::AccessDenied, ioError);
        CPPUNIT_ASSERT(checksum.empty());
#endif
    }
}

} // namespace KDC
