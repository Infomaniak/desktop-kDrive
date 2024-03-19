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
#include "config.h"

#include <filesystem>

using namespace CppUnit;

namespace KDC {


void TestIo::testGetFileSizeSimpleCases() {
    // Getting the size of a regular file
    {
        const SyncPath path = _localTestDirPath / "test_pictures/picture-1.jpg";
        uint64_t fileSize = 0u;
        IoError ioError = IoErrorSuccess;

        CPPUNIT_ASSERT(_testObj->getFileSize(path, fileSize, ioError));
        CPPUNIT_ASSERT(fileSize == 408278u);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // Getting the size of a regular file missing all permissions: no error expected
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "permission_less_file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
            ofs.close();
        }
        const auto allPermissions =
            std::filesystem::perms::owner_all | std::filesystem::perms::others_all | std::filesystem::perms::group_all;
        std::filesystem::permissions(path, allPermissions, std::filesystem::perm_options::remove);

        uint64_t fileSize = 0u;
        IoError ioError = IoErrorSuccess;

        CPPUNIT_ASSERT(_testObj->getFileSize(path, fileSize, ioError));
        std::filesystem::permissions(path, allPermissions, std::filesystem::perm_options::add);

        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(fileSize == std::filesystem::file_size(path));
    }


    // Attempting to get the file size of a regular directory
    {
        const SyncPath path = _localTestDirPath / "test_pictures";
        uint64_t fileSize = 0u;
        IoError ioError = IoErrorSuccess;

        CPPUNIT_ASSERT(!_testObj->getFileSize(path, fileSize, ioError));
        CPPUNIT_ASSERT(fileSize == 0u);
        CPPUNIT_ASSERT(ioError == IoErrorIsADirectory);
    }

    // Getting the size of regular symbolic link on a file
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "regular_file_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        uint64_t fileSize = 0u;
        IoError ioError = IoErrorSuccess;

        CPPUNIT_ASSERT(_testObj->getFileSize(path, fileSize, ioError));
        CPPUNIT_ASSERT(fileSize == targetPath.native().size());

        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // Getting the size of a regular symbolic link on a folder
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "regular_dir_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        uint64_t fileSize = 0u;
        IoError ioError = IoErrorSuccess;

        CPPUNIT_ASSERT(_testObj->getFileSize(path, fileSize, ioError));
        CPPUNIT_ASSERT(fileSize == targetPath.native().size());
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);


        // Getting the size of a dangling symbolic link: `getFileSize` returns `true` but `itemType.ioError`
        // is set with `IoErrorNoSuchFileOrDirectory`.
        {
            const TemporaryDirectory temporaryDirectory;
            const SyncPath targetPath = temporaryDirectory.path / "non_existing_test_file.txt";  // This file does not exist.
            const SyncPath path = temporaryDirectory.path / "dangling_symbolic_link";
            std::filesystem::create_symlink(targetPath, path);

            uint64_t fileSize = 0u;
            IoError ioError = IoErrorSuccess;

            CPPUNIT_ASSERT(_testObj->getFileSize(path, fileSize, ioError));
            CPPUNIT_ASSERT(fileSize == targetPath.native().size());
            CPPUNIT_ASSERT(ioError == IoErrorNoSuchFileOrDirectory);
        }
    }

#if defined(__APPLE__)
    // Getting the size of a MacOSX Finder alias on a regular file.
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = temporaryDirectory.path / "regular_file_alias";

        IoError aliasError;
        CPPUNIT_ASSERT(_testObj->createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoErrorSuccess);

        uint64_t fileSize = 0u;
        IoError ioError = IoErrorSuccess;

        CPPUNIT_ASSERT(_testObj->getFileSize(path, fileSize, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(fileSize == std::filesystem::file_size(path));
    }

    // Getting the size of a MacOSX Finder alias on a regular directory.
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const SyncPath path = temporaryDirectory.path / "regular_dir_alias";

        IoError aliasError;
        CPPUNIT_ASSERT(_testObj->createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoErrorSuccess);

        uint64_t fileSize = 0u;
        IoError ioError = IoErrorSuccess;

        CPPUNIT_ASSERT(_testObj->getFileSize(path, fileSize, ioError));
        CPPUNIT_ASSERT(fileSize == std::filesystem::file_size(path));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // Getting the size of a dangling MacOSX Finder alias on a non-existing file.
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path / "file_to_be_deleted.png";  // This file will be deleted.
        const SyncPath path = temporaryDirectory.path / "dangling_file_alias";
        {
            std::ofstream ofs(targetPath);
            ofs << "Some content.\n";
            ofs.close();
        }

        IoError aliasError;
        CPPUNIT_ASSERT(_testObj->createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoErrorSuccess);

        std::filesystem::remove(targetPath);
        CPPUNIT_ASSERT(!std::filesystem::exists(targetPath));

        uint64_t fileSize = 0u;
        IoError ioError = IoErrorSuccess;

        CPPUNIT_ASSERT(_testObj->getFileSize(path, fileSize, ioError));
        CPPUNIT_ASSERT(fileSize == std::filesystem::file_size(path));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }
#endif
}

void TestIo::testGetFileSizeAllBranches() {
    // Access denied for a regular file after `getItemType` was called successfully.
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath subdir = temporaryDirectory.path / "permission_less_subdirectory";
        std::filesystem::create_directory(subdir);
        const SyncPath path = subdir / "file.txt";
        { std::ofstream ofs(path); }

        _testObj->setFileSizeFunction([&subdir](const SyncPath &path, std::error_code &ec) -> std::uintmax_t {
            std::filesystem::permissions(subdir, std::filesystem::perms::owner_exec, std::filesystem::perm_options::remove);
            return std::filesystem::file_size(path, ec);
        });

        uint64_t fileSize = 0u;
        IoError ioError = IoErrorSuccess;
        CPPUNIT_ASSERT(_testObj->getFileSize(path, fileSize, ioError));

        // Restore permission to allow subdir removal
        std::filesystem::permissions(subdir, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);
        std::filesystem::remove_all(subdir);  // required to allow automated deletion of `temporaryDirectory`

        // Remark: the test CPPUNIT_ASSERT(fileSize == 0u) fails on MacOSX.
#ifdef _WIN32
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
#else
        CPPUNIT_ASSERT(ioError == IoErrorAccessDenied);
#endif
    }
}

void TestIo::testGetFileSize() {
    testGetFileSizeSimpleCases();
    testGetFileSizeAllBranches();
}

}  // namespace KDC
