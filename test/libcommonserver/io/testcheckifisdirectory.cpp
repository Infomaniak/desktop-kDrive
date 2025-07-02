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

void TestIo::testCheckIfIsDirectory() {
    // A regular file
    {
        const SyncPath path = _localTestDirPath / "test_pictures/picture-1.jpg";
        bool isDirectory = true;
        IoError ioError = IoError::Unknown;

        CPPUNIT_ASSERT(IoHelper::checkIfIsDirectory(path, isDirectory, ioError));
        CPPUNIT_ASSERT(!isDirectory);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // A regular directory
    {
        const SyncPath path = _localTestDirPath / "test_pictures";
        IoError ioError = IoError::Unknown;
        bool isDirectory = false;

        CPPUNIT_ASSERT(IoHelper::checkIfIsDirectory(path, isDirectory, ioError));
        CPPUNIT_ASSERT(isDirectory);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // A regular symbolic link on a file
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "regular_file_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        IoError ioError = IoError::Unknown;
        bool isDirectory = false;

        CPPUNIT_ASSERT(IoHelper::checkIfIsDirectory(path, isDirectory, ioError));
        CPPUNIT_ASSERT(!isDirectory);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    }

    // A regular symbolic link on a symlink
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path_ = temporaryDirectory.path() / "symbolic_link";
        std::filesystem::create_symlink(targetPath, path_);
        const SyncPath path = temporaryDirectory.path() / "symbolic_link_link";
        std::filesystem::create_symlink(path_, path);

        IoError ioError = IoError::Unknown;
        bool isDirectory = false;

        CPPUNIT_ASSERT(IoHelper::checkIfIsDirectory(path, isDirectory, ioError));
        CPPUNIT_ASSERT(!isDirectory);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // A regular symbolic link on a folder
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "regular_dir_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        IoError ioError = IoError::Unknown;
        bool isDirectory = false;


        CPPUNIT_ASSERT(IoHelper::checkIfIsDirectory(path, isDirectory, ioError));
        CPPUNIT_ASSERT(!isDirectory);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        /* For comparison with the standard implementation, the following test passes:
        // std::error_code ec;
        // CPPUNIT_ASSERT(std::filesystem::is_directory(path, ec));
        // CPPUNIT_ASSERT_MESSAGE(ec.message(), ec.value() == 0);
        */
    }

    // A non-existing folder
    {
        const SyncPath path = _localTestDirPath / "non_existing";
        IoError ioError = IoError::Success;
        bool isDirectory = true;

        CPPUNIT_ASSERT(IoHelper::checkIfIsDirectory(path, isDirectory, ioError));
        CPPUNIT_ASSERT(!isDirectory);
        CPPUNIT_ASSERT(ioError == IoError::NoSuchFileOrDirectory);
    }

    // A dangling symbolic link
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path() / "non_existing_file.txt"; // This file does not exist.
        const SyncPath path = temporaryDirectory.path() / "dangling_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        IoError ioError = IoError::Success;
        bool isDirectory = true;

        CPPUNIT_ASSERT(IoHelper::checkIfIsDirectory(path, isDirectory, ioError));
        CPPUNIT_ASSERT(!isDirectory);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success,
                                     ioError); // Although the target path is invalid.
    }

    // A regular directory missing all permissions: no error expected
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "permission_less_directory";
        std::filesystem::create_directory(path);

        std::filesystem::permissions(path, std::filesystem::perms::all, std::filesystem::perm_options::remove);

        IoError ioError = IoError::Unknown;
        bool isDirectory = false;

        CPPUNIT_ASSERT(IoHelper::checkIfIsDirectory(path, isDirectory, ioError));
        CPPUNIT_ASSERT(isDirectory);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        std::filesystem::permissions(path, std::filesystem::perms::all, std::filesystem::perm_options::add);
    }

    // A non-existing file with a very long name
    // - IoError::NoSuchFileOrDirectory on Windows (expected error).
    // - IoError::FileNameTooLong error for MacOSX and Linux (unexpected error).
    {
        const std::string veryLongfileName(1000,
                                           'a'); // Exceeds the max allowed name length on every file system of interest.
        const SyncPath path = _localTestDirPath / veryLongfileName; // This file doesn't exist.

        IoError ioError = IoError::Success;
        bool isDirectory = true;
#if defined(KD_WINDOWS)
        CPPUNIT_ASSERT(IoHelper::checkIfIsDirectory(path, isDirectory, ioError));
        CPPUNIT_ASSERT(ioError == IoError::NoSuchFileOrDirectory);
#else
        CPPUNIT_ASSERT(!IoHelper::checkIfIsDirectory(path, isDirectory, ioError));
        CPPUNIT_ASSERT(ioError == IoError::FileNameTooLong);
#endif
        CPPUNIT_ASSERT(!isDirectory);
    }

    // A regular directory within a subdirectory that misses owner exec permission.
    // - No error on Windows.
    // - Access denied expected on MacOSX and Linux
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "subdir";
        std::filesystem::create_directory(path);

        std::filesystem::permissions(temporaryDirectory.path(), std::filesystem::perms::owner_exec,
                                     std::filesystem::perm_options::remove);

        IoError ioError = IoError::Success;
        bool isDirectory = true;

        CPPUNIT_ASSERT(IoHelper::checkIfIsDirectory(path, isDirectory, ioError));
#if defined(KD_WINDOWS)
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(isDirectory);
#else
        CPPUNIT_ASSERT(ioError == IoError::AccessDenied);
        CPPUNIT_ASSERT(!isDirectory);
#endif
        // Restore permission to allow subdir removal
        std::filesystem::permissions(temporaryDirectory.path(), std::filesystem::perms::owner_exec,
                                     std::filesystem::perm_options::add);
    }

#if defined(KD_MACOS)
    // A MacOSX Finder alias on a regular file.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = temporaryDirectory.path() / "regular_file_alias";

        IoError aliasError;
        IoHelper::createAliasFromPath(targetPath, path, aliasError);

        IoError ioError = IoError::Unknown;
        bool isDirectory = true;

        CPPUNIT_ASSERT(IoHelper::checkIfIsDirectory(path, isDirectory, ioError));
        CPPUNIT_ASSERT(!isDirectory);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // A MacOSX Finder alias on a regular folder.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const SyncPath path = temporaryDirectory.path() / "regular_dir_alias";

        IoError aliasError;
        CPPUNIT_ASSERT_MESSAGE(toString(aliasError), IoHelper::createAliasFromPath(targetPath, path, aliasError));

        IoError ioError = IoError::Unknown;
        bool isDirectory = true;

        CPPUNIT_ASSERT(IoHelper::checkIfIsDirectory(path, isDirectory, ioError));
        CPPUNIT_ASSERT(!isDirectory);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // A dangling MacOSX Finder alias on a non-existing directory.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path() / "directory_to_be_deleted"; // This directory will be deleted.
        std::filesystem::create_directory(targetPath);

        const SyncPath path = temporaryDirectory.path() / "dangling_directory_alias";

        IoError aliasError;
        IoHelper::createAliasFromPath(targetPath, path, aliasError);
        std::filesystem::remove_all(targetPath);

        IoError ioError = IoError::Unknown;
        bool isDirectory = true;

        CPPUNIT_ASSERT(IoHelper::checkIfIsDirectory(path, isDirectory, ioError));
        CPPUNIT_ASSERT(!isDirectory);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }
#endif
}

void TestIo::testCreateDirectory() {
    // Creates successfully a directory
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "regular_directory";

        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::createDirectory(path, true, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        ioError = IoError::Unknown;
        bool isDirectory = false;

        CPPUNIT_ASSERT(IoHelper::checkIfIsDirectory(path, isDirectory, ioError));
        CPPUNIT_ASSERT(isDirectory);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // Fails to create a directory because the dir path indicates an existing directory
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path();

        IoError ioError = IoError::Success;

        CPPUNIT_ASSERT(!IoHelper::createDirectory(path, false, ioError));
        CPPUNIT_ASSERT(ioError == IoError::DirectoryExists);
    }

    // Fails to create a directory because the dir path indicates an existing file
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "file.txt";
        { std::ofstream ofs(path); }

        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(!IoHelper::createDirectory(path, false, ioError));
        CPPUNIT_ASSERT(ioError == IoError::FileExists);
    }

    // Fails to create a directory within a subdirectory that misses owner exec permission.
    // - No error on Windows.
    // - Access denied on MacOSX and Linux.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "subdir";

        std::filesystem::permissions(temporaryDirectory.path(), std::filesystem::perms::owner_exec,
                                     std::filesystem::perm_options::remove);

        IoError ioError = IoError::Success;
#if defined(KD_WINDOWS)
        CPPUNIT_ASSERT(IoHelper::createDirectory(path, false, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
#else
        CPPUNIT_ASSERT(!IoHelper::createDirectory(path, false, ioError));
        CPPUNIT_ASSERT(ioError == IoError::AccessDenied);
#endif
        // Restore permission to allow subdir removal
        std::filesystem::permissions(temporaryDirectory.path(), std::filesystem::perms::owner_exec,
                                     std::filesystem::perm_options::add);
    }

    // Fails to create a directory with a very long name
    {
        const std::string veryLongDirName(1000,
                                          'a'); // Exceeds the max allowed name length on every file system of interest.
        const SyncPath path = _localTestDirPath / veryLongDirName; // This directory doesn't exist.

        IoError ioError = IoError::Success;
#if defined(KD_WINDOWS)
        CPPUNIT_ASSERT(!IoHelper::createDirectory(path, false, ioError));
        CPPUNIT_ASSERT(ioError == IoError::NoSuchFileOrDirectory);
#else
        CPPUNIT_ASSERT(!IoHelper::createDirectory(path, false, ioError));
        CPPUNIT_ASSERT(ioError == IoError::FileNameTooLong);
#endif
    }
}

} // namespace KDC
