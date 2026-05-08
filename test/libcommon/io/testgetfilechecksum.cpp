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
    // A regular file
    {
        const SyncPath path = _localTestDirPath / "test_pictures/picture-1.jpg";
        IoError ioError = IoError::Unknown;
        std::ifstream ifs;

        auto checksum = IoHelper::getFileChecksum(path, ifs, ioError);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        CPPUNIT_ASSERT(!checksum.empty());
        CPPUNIT_ASSERT_EQUAL(std::string("5dcc477e35136516"), checksum);
    }

    // A regular file whose name exists with a different capitalization
    {
        const SyncPath path = _localTestDirPath / "test_pictures/Picture-1.jpg";
        IoError ioError = IoError::Unknown;
        std::ifstream ifs;

        auto checksum = IoHelper::getFileChecksum(path, ifs, ioError);
#if defined(KD_MACOS) || defined(KD_WINDOWS)
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
#else
        CPPUNIT_ASSERT_EQUAL(IoError::NoSuchFileOrDirectory, ioError);
#endif
        CPPUNIT_ASSERT(!checksum.empty());
        CPPUNIT_ASSERT_EQUAL(std::string("5dcc477e35136516"), checksum);
    }

    // A regular symbolic link on a file
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "regular_file_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);
        IoError ioError = IoError::Unknown;
        std::ifstream ifs;

        auto checksum = IoHelper::getFileChecksum(path, ifs, ioError);
        CPPUNIT_ASSERT(checksum.empty());
        CPPUNIT_ASSERT_EQUAL(std::string(""), checksum);
        CPPUNIT_ASSERT_EQUAL(IoError::InvalidArgument, ioError);
    }

    // A non-existing file
    {
        const SyncPath path = _localTestDirPath / "non-existing.jpg"; // This file does not exist.
        IoError ioError = IoError::Success;
        std::ifstream ifs;

        auto checksum = IoHelper::getFileChecksum(path, ifs, ioError);
        CPPUNIT_ASSERT(checksum.empty());
        CPPUNIT_ASSERT_EQUAL(std::string(""), checksum);
        CPPUNIT_ASSERT_EQUAL(IoError::NoSuchFileOrDirectory, ioError);
    }

    // A non-existing file with a very long name
    {
        const std::string veryLongfileName(1000, 'a'); // Exceeds the max allowed name length on every file system of interest.
        const SyncPath path = _localTestDirPath / veryLongfileName; // This file doesn't exist.
        IoError ioError = IoError::Success;
        std::ifstream ifs;

        auto checksum = IoHelper::getFileChecksum(path, ifs, ioError);
        CPPUNIT_ASSERT(checksum.empty());
        CPPUNIT_ASSERT_EQUAL(std::string(""), checksum);
        CPPUNIT_ASSERT_EQUAL(IoError::NoSuchFileOrDirectory, ioError);
    }
    // A non-existing file with a very long path
    {
        const std::string pathSegment(50, 'a');
        SyncPath path = _localTestDirPath;
        for (auto i = 0; i < 1000; ++i) {
            path /= pathSegment; // Eventually exceeds the max allowed path length on every file system of interest.
        }
        IoError ioError = IoError::Success;
        std::ifstream ifs;

        auto checksum = IoHelper::getFileChecksum(path, ifs, ioError);
        CPPUNIT_ASSERT(checksum.empty());
        CPPUNIT_ASSERT_EQUAL(std::string(""), checksum);

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
            ofs << "Some content.\n";
        }

#if defined(KD_MACOS) || defined(KD_WINDOWS)
        IoHelper::setFileHidden(path, true);
#endif

        IoError ioError = IoError::Unknown;
        std::ifstream ifs;

        auto checksum = IoHelper::getFileChecksum(path, ifs, ioError);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        CPPUNIT_ASSERT(!checksum.empty());
        CPPUNIT_ASSERT_EQUAL(std::string("3d48b0e057db6f01"), checksum);
    }

    // A file with dots and colons in its name
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / ":.file.::.name.:";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
        }

        IoError ioError = IoError::Unknown;
        std::ifstream ifs;

        auto checksum = IoHelper::getFileChecksum(path, ifs, ioError);
#if defined(KD_WINDOWS)
        CPPUNIT_ASSERT_EQUAL(IoError::NoSuchFileOrDirectory, ioError);
        CPPUNIT_ASSERT(checksum.empty());
#else
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        CPPUNIT_ASSERT_EQUAL(std::string("3d48b0e057db6f01"), checksum);
#endif
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

        IoError ioError = IoError::Unknown;
        std::ifstream ifs;

        auto checksum = IoHelper::getFileChecksum(path, ifs, ioError);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        CPPUNIT_ASSERT(!checksum.empty());
        CPPUNIT_ASSERT_EQUAL(std::string("3d48b0e057db6f01"), checksum);
    }

    // A dangling symbolic link
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path() / "non_existing_test_file.txt"; // This file does not exist.
        const SyncPath path = temporaryDirectory.path() / "dangling_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        IoError ioError = IoError::Unknown;
        std::ifstream ifs;

        auto checksum = IoHelper::getFileChecksum(path, ifs, ioError);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        CPPUNIT_ASSERT(checksum.empty());
        CPPUNIT_ASSERT_EQUAL(std::string(""), checksum);
    }
#if defined(KD_MACOS)
    // A macOS Finder alias on a regular file: not computed, returns empty checksum.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = temporaryDirectory.path() / "regular_file_alias";

        IoError aliasError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, aliasError);

        IoError ioError = IoError::Unknown;
        std::ifstream ifs;

        auto checksum = IoHelper::getFileChecksum(path, ifs, ioError);
        CPPUNIT_ASSERT_EQUAL(IoError::InvalidArgument, ioError);
        CPPUNIT_ASSERT(checksum.empty());
        CPPUNIT_ASSERT_EQUAL(std::string(""), checksum);
    }

    // A dangling macOS Finder alias (target deleted): not computed, returns empty checksum.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path() / "file_to_be_deleted.png"; // This file will be deleted.
        const SyncPath path = temporaryDirectory.path() / "dangling_file_alias";
        {
            std::ofstream ofs(targetPath);
            ofs << "Some content.\n";
        }

        IoError aliasError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, aliasError);

        auto ioErr = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::deleteItem(targetPath, ioErr));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioErr);

        IoError ioError = IoError::Unknown;
        std::ifstream ifs;

        auto checksum = IoHelper::getFileChecksum(path, ifs, ioError);
        CPPUNIT_ASSERT_EQUAL(IoError::InvalidArgument, ioError);
        CPPUNIT_ASSERT(checksum.empty());
    } /*
#elif defined(KD_WINDOWS)
    // A junction on a regular directory.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path() / "regular_dir";
        const SyncPath path = temporaryDirectory.path() / "regular_dir_alias";

        std::error_code ec;
        CPPUNIT_ASSERT(std::filesystem::create_directory(targetPath, ec) && ec.value() == 0);

        IoError junctionError{IoError::Unknown};
        IoHelper::createJunctionFromPath(targetPath, path, junctionError);
        CPPUNIT_ASSERT(junctionError == IoError::Success);

        FileStat fileStat;
        IoError ioError = IoError::Unknown;

        CPPUNIT_ASSERT(IoHelper::getFileStat(path, &fileStat, ioError, IoHelper::PathCheckOption::Insensitive));
        CPPUNIT_ASSERT(!fileStat.isHidden);
        CPPUNIT_ASSERT_EQUAL(int64_t{0},
                             fileStat.size); // `fileStat.size` is 0 for a junction`
        CPPUNIT_ASSERT_GREATEREQUAL(fileStat.creationTime, fileStat.modificationTime);
        CPPUNIT_ASSERT_EQUAL(NodeType::Directory, fileStat.nodeType);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    }

    // A dangling junction on a non-existing directory.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "dummy"; // Non existing directory
        const SyncPath path = temporaryDirectory.path() / "regular_dir_alias";

        IoError junctionError{IoError::Unknown};
        IoHelper::createJunctionFromPath(targetPath, path, junctionError);
        CPPUNIT_ASSERT(junctionError == IoError::Success);

        FileStat fileStat;
        IoError ioError = IoError::Unknown;

        CPPUNIT_ASSERT(IoHelper::getFileStat(path, &fileStat, ioError, IoHelper::PathCheckOption::Insensitive));
        CPPUNIT_ASSERT(!fileStat.isHidden);
        CPPUNIT_ASSERT_EQUAL(int64_t{0},
                             fileStat.size); // `fileStat.size` is 0 for a junction`
        CPPUNIT_ASSERT_GREATEREQUAL(fileStat.creationTime, fileStat.modificationTime);
        CPPUNIT_ASSERT_EQUAL(NodeType::Directory, fileStat.nodeType);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    }
#endif

    // A regular file missing all permissions (no error expected)
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "permission_less_file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
        }

        std::filesystem::permissions(path, std::filesystem::perms::all, std::filesystem::perm_options::remove);

        FileStat fileStat;
        IoError ioError = IoError::Unknown;

        CPPUNIT_ASSERT(IoHelper::getFileStat(path, &fileStat, ioError, IoHelper::PathCheckOption::Insensitive));

        std::filesystem::permissions(path, std::filesystem::perms::all, std::filesystem::perm_options::add);

        CPPUNIT_ASSERT(!fileStat.isHidden);
        CPPUNIT_ASSERT_GREATER(int64_t{0}, fileStat.size);
        CPPUNIT_ASSERT_GREATER(SyncTime{0}, fileStat.creationTime);
        CPPUNIT_ASSERT_GREATEREQUAL(fileStat.creationTime, fileStat.modificationTime);
        CPPUNIT_ASSERT_EQUAL(NodeType::File, fileStat.nodeType);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    }

    // A regular directory missing all permissions (no error expected)
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "permission_less_directory";
        std::filesystem::create_directory(path);

        std::filesystem::permissions(path, std::filesystem::perms::all, std::filesystem::perm_options::remove);

        FileStat fileStat;
        IoError ioError = IoError::Unknown;

        CPPUNIT_ASSERT(IoHelper::getFileStat(path, &fileStat, ioError, IoHelper::PathCheckOption::Insensitive));

        std::filesystem::permissions(path, std::filesystem::perms::all, std::filesystem::perm_options::add);

        CPPUNIT_ASSERT(!fileStat.isHidden);
#if defined(KD_WINDOWS)
        CPPUNIT_ASSERT_EQUAL(int64_t{0}, fileStat.size);
#else
        CPPUNIT_ASSERT(fileStat.size > 0u);
#endif
        CPPUNIT_ASSERT_GREATER(SyncTime{0}, fileStat.creationTime);
        CPPUNIT_ASSERT_GREATEREQUAL(fileStat.creationTime, fileStat.modificationTime);
        CPPUNIT_ASSERT_EQUAL(NodeType::Directory, fileStat.nodeType);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
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
        }
        std::filesystem::permissions(subdir, std::filesystem::perms::owner_read, std::filesystem::perm_options::remove);

        FileStat fileStat;
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::getFileStat(path, &fileStat, ioError, IoHelper::PathCheckOption::Insensitive));

        // Restore permission to allow subdir removal
        std::filesystem::permissions(subdir, std::filesystem::perms::owner_read, std::filesystem::perm_options::add);

        CPPUNIT_ASSERT(!fileStat.isHidden);
        CPPUNIT_ASSERT_GREATER(int64_t{0}, fileStat.size);
        CPPUNIT_ASSERT_GREATER(SyncTime{0}, fileStat.creationTime);
        CPPUNIT_ASSERT_GREATEREQUAL(fileStat.creationTime, fileStat.modificationTime);
        CPPUNIT_ASSERT_EQUAL(NodeType::File, fileStat.nodeType);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    }

    // A regular file within a subdirectory that misses owner search/exec permission:
    // - access denied expected on MacOSX
    // - no error expected on Windows
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath subdir = temporaryDirectory.path() / "permission_less_subdirectory";
        std::filesystem::create_directory(subdir);
        const SyncPath path = subdir / "file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
        }
        std::filesystem::permissions(subdir, std::filesystem::perms::owner_exec, std::filesystem::perm_options::remove);

        FileStat fileStat;
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::getFileStat(path, &fileStat, ioError, IoHelper::PathCheckOption::Insensitive));

        // Restore permission to allow subdir removal
        std::filesystem::permissions(subdir, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);

        CPPUNIT_ASSERT(!fileStat.isHidden);
#if defined(KD_WINDOWS)
        CPPUNIT_ASSERT_GREATER(int64_t{0}, fileStat.size);
        CPPUNIT_ASSERT_GREATER(SyncTime{0}, fileStat.modificationTime);
        CPPUNIT_ASSERT_GREATEREQUAL(fileStat.creationTime, fileStat.modificationTime);
        CPPUNIT_ASSERT_EQUAL(NodeType::File, fileStat.nodeType);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
#else
        CPPUNIT_ASSERT_EQUAL(int64_t{0}, fileStat.size);
        CPPUNIT_ASSERT_EQUAL(SyncTime{0}, fileStat.modificationTime);
        CPPUNIT_ASSERT_EQUAL(SyncTime{0}, fileStat.creationTime);
        CPPUNIT_ASSERT_EQUAL(NodeType::Unknown, fileStat.nodeType);
        CPPUNIT_ASSERT_EQUAL(IoError::AccessDenied, ioError);
    }*/
#endif
}

} // namespace KDC
