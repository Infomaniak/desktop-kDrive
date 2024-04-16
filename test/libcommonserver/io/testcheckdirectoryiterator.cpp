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
#include "libcommonserver/utility/testutility.h"

using namespace CppUnit;

namespace KDC {

void TestIo::testCheckDirectoryIterator() {
    testCheckDirectoryIteratorNonExistingPath();
    testCheckDirectoryIteratorExistingPath();
    testCheckDirectoryIteratotNextAfterEndOfDir();
    testCheckDirectoryIteratorPermission();
    testCheckDirectoryIteratorUnexpectedDelete();
#ifndef _WIN32
    // This test is not valid on Windows because the permission is not checked
    // on the directory iterator
    testCheckDirectoryPermissionLost();
#endif  

}

void TestIo::testCheckDirectoryIteratorNonExistingPath() {
    // Check that the directory iterator returns an error when the path does not exist
    {
        IoError error;
        const DirectoryIterator it("C:\\nonexistingpath", false, error);

        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorNoSuchFileOrDirectory, error);
    }

    // Check that the directory iterator returns an error when the path syntax is invalid
    {
        IoError error;
        const DirectoryIterator it("C:\\nonexistingpath\\*\\", false, error);

        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorNoSuchFileOrDirectory, error);
    }
}


void TestIo::testCheckDirectoryIteratorExistingPath() {
    // Check that the directory iterator is valid when the path is an empty directory and return EOF on first call
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "chekDirIt/empty_dir";
        std::filesystem::create_directories(path);

        IoError error;

        DirectoryIterator it(path, false, error);
        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorSuccess, error);

        DirectoryEntry entry;
        CPPUNIT_ASSERT(!it.next(entry, error));
        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorEndOfDirectory, error);
    }

    // Check that the directory iterator is valid when the path is a directory with one file and return the file on first call
    {
        const SyncPath directoryWithOneFile = _localTestDirPath / "test_dir_iterator/oneFile_dir";

        IoError error;
        DirectoryIterator it(directoryWithOneFile, false, error);
        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorSuccess, error);

        DirectoryEntry entry;
        CPPUNIT_ASSERT(it.next(entry, error));


        CPPUNIT_ASSERT_EQUAL(directoryWithOneFile / "testFile1.txt", entry.path());

        CPPUNIT_ASSERT(!it.next(entry, error));
        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorEndOfDirectory, error);
    }

    // Check that the directory iterator is valid when the path is a directory with one child directory
    {
        const SyncPath directoryWithOneChildDirectory = _localTestDirPath / "test_dir_iterator/oneDir_dir";
        IoError error;
        DirectoryIterator it(directoryWithOneChildDirectory, false, error);
        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorSuccess, error);

        DirectoryEntry entry;
        CPPUNIT_ASSERT(it.next(entry, error));

        CPPUNIT_ASSERT_EQUAL(directoryWithOneChildDirectory / "testDir1", entry.path());

        CPPUNIT_ASSERT(!it.next(entry, error));
    }


    // Check that the directory iterator do not search recursively when the recursive flag is false
    {
        const SyncPath directoryWithMultipleSubDirectories = _localTestDirPath / "test_dir_iterator/recursive_dir";

        IoError error;
        DirectoryIterator it(directoryWithMultipleSubDirectories, false, error);
        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorSuccess, error);

        DirectoryEntry entry;
        for (int i = 0; i < 4; i++) {
            CPPUNIT_ASSERT(it.next(entry, error));
        }

        CPPUNIT_ASSERT(!it.next(entry, error));
        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorEndOfDirectory, error);
    }

    // Check that the directory iterator searches recursively when the recursive flag is true
    {
        const SyncPath directoryWithMultipleSubDirectories = _localTestDirPath / "test_dir_iterator/recursive_dir";

        IoError error;
        DirectoryIterator it(directoryWithMultipleSubDirectories, true, error);
        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorSuccess, error);

        DirectoryEntry entry;
        for (int i = 0; i < 8; i++) {
            CPPUNIT_ASSERT(it.next(entry, error));
        }

        CPPUNIT_ASSERT(!it.next(entry, error));
        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorEndOfDirectory, error);
    }
}

void TestIo::testCheckDirectoryIteratotNextAfterEndOfDir() {
    // Check that the directory iterator returns an EOF on everycall to next after EOF
    {
        const SyncPath oneFileDirectory = _localTestDirPath / "test_dir_iterator/oneFile_dir";

        IoError error;
        DirectoryIterator it(oneFileDirectory, false, error);
        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorSuccess, error);

        // Read the only file in the directory
        DirectoryEntry entry;

        CPPUNIT_ASSERT(it.next(entry, error));

        // Expecting EOF
        for (int i = 0; i < 3; i++) {
            CPPUNIT_ASSERT(!it.next(entry, error));
            CPPUNIT_ASSERT_EQUAL(IoError::IoErrorEndOfDirectory, error);
        }
    }
}

void TestIo::testCheckDirectoryIteratorPermission() {
    // Check that the directory iterator shows a directory with no permission when `skip_permission_denied` is false
    const TemporaryDirectory temporaryDirectory;
    const SyncPath path = temporaryDirectory.path / "chekDirIt/noPermissionFolder";
    std::filesystem::create_directories(path);
    // create a file in the directory
    const SyncPath testFilePathNoPerm = path / "testFile.txt";
    std::ofstream(testFilePathNoPerm, std::ios::out).close();

    // Remove permission from the file
    std::error_code ec;
    std::filesystem::permissions(
        testFilePathNoPerm,
        std::filesystem::perms::group_write | std::filesystem::perms::others_write | std::filesystem::perms::owner_write,
        std::filesystem::perm_options::remove, ec);
    {
        IoError ioError = IoErrorSuccess;
        DirectoryIterator it(path, false, ioError);
        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorSuccess, ioError);

        DirectoryEntry entry;
        CPPUNIT_ASSERT(it.next(entry, ioError));
        CPPUNIT_ASSERT_EQUAL(testFilePathNoPerm, entry.path());
    }

    // Check that the directory iterator skips each directory with no permission when `skip_permission_denied` is true
    {
        IoError ioError = IoErrorSuccess;
        DirectoryIterator it(path, false, ioError, DirectoryOptions::skip_permission_denied);
        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorSuccess, ioError);

        DirectoryEntry entry;
        CPPUNIT_ASSERT(!it.next(entry, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorEndOfDirectory, ioError);
    }
    // Restore permission to allow subdir removal
    std::filesystem::permissions(
        testFilePathNoPerm,
        std::filesystem::perms::group_write | std::filesystem::perms::others_write | std::filesystem::perms::owner_write,
        std::filesystem::perm_options::add);
}

void TestIo::testCheckDirectoryIteratorUnexpectedDelete() {
    // Check that the directory iterator is consistent when a parent directory is deleted
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path.string() + "\\chekDirIt\\subDirDel";
        std::string subDir = path.string();

        for (int i = 0; i < 5; i++) {
            subDir += "/subDir" + std::to_string(i);
        }

        std::filesystem::create_directories(subDir);

        IoError ioError = IoErrorSuccess;
        DirectoryIterator it(path, true, ioError);
        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorSuccess, ioError);

        DirectoryEntry entry;
        CPPUNIT_ASSERT(it.next(entry, ioError));

        std::filesystem::remove_all(path);

        CPPUNIT_ASSERT(!it.next(entry, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorNoSuchFileOrDirectory, ioError);

        CPPUNIT_ASSERT(!it.next(entry, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorInvalidDirectoryIterator, ioError);
    }
}

void TestIo::testCheckDirectoryPermissionLost(void) {
    // Check that the directory iterator is consistent when a parent directory loses permission
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path.string() + "\\chekDirIt\\subDirDel";
        std::string subDir = path.string();

        for (int i = 0; i < 7; i++) {
            subDir += "/subDir" + std::to_string(i);
        }

        std::filesystem::create_directories(subDir);

        IoError ioError = IoErrorSuccess;
        DirectoryIterator it(path, true, ioError);
        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorSuccess, ioError);

        DirectoryEntry entry;
        CPPUNIT_ASSERT(it.next(entry, ioError));

        
        for (int i = 0; i < 2; i++) {
            CPPUNIT_ASSERT(it.next(entry, ioError));
        }

        std::filesystem::permissions(
            path,
            std::filesystem::perms::group_write | std::filesystem::perms::others_write | std::filesystem::perms::owner_write,
            std::filesystem::perm_options::remove);


        for (int i = 0; i < 3; i++) {
            bool error = it.next(entry, ioError);
            CPPUNIT_ASSERT(!it.next(entry, ioError));
            std::cout << "Error: " << IoHelper::ioError2StdString(ioError) << " | " << error << std::endl;
            CPPUNIT_ASSERT_EQUAL(IoError::IoErrorNoSuchFileOrDirectory, ioError);
        }

        CPPUNIT_ASSERT(!it.next(entry, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorInvalidDirectoryIterator, ioError);

        // Restore permission to allow subdir removal
        std::filesystem::permissions(
            path,
            std::filesystem::perms::group_write | std::filesystem::perms::others_write | std::filesystem::perms::owner_write,
            std::filesystem::perm_options::add);

        std::filesystem::remove_all(path);

    }
}


}  // namespace KDC
