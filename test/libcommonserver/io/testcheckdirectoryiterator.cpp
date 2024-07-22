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
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/utility/testutility.h"


using namespace CppUnit;

namespace KDC {

void TestIo::testCheckDirectoryIterator() {
    testCheckDirectoryIteratorNonExistingPath();
    testCheckDirectoryIteratorExistingPath();
    testCheckDirectoryRecursive();
    testCheckDirectoryIteratotNextAfterEndOfDir();
    testCheckDirectoryIteratorUnexpectedDelete();
    testCheckDirectoryIteratorPermission();
    testCheckDirectoryPermissionLost();
}

void TestIo::testCheckDirectoryIteratorNonExistingPath() {
    // Check that the directory iterator returns an error when the path does not exist
    {
        IoError error;
        const IoHelper::DirectoryIterator it("C:\\nonexistingpath", false, error);

        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorNoSuchFileOrDirectory, error);
    }

    // Check that the directory iterator returns an error when the path syntax is invalid
    {
        IoError error;
        const IoHelper::DirectoryIterator it("C:\\nonexistingpath\\*\\", false, error);

        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorNoSuchFileOrDirectory, error);
    }
}

void TestIo::testCheckDirectoryIteratorExistingPath() {
    LocalTemporaryDirectory tempDir;

    // Create test empty directory
    const SyncPath emptyDir = tempDir.path() / "chekDirIt/empty_dir";
    std::filesystem::create_directories(emptyDir);

    // Create test directory with one file
    SyncPath oneFileDir = tempDir.path() / "chekDirIt/oneFile_dir";
    std::filesystem::create_directories(oneFileDir);
    std::ofstream file(oneFileDir / "oneFile.txt");
    file << "oneFile";
    file.close();

    // Create test directory with one directory
    std::filesystem::create_directories(tempDir.path() / "chekDirIt/oneDir_dir/testDir1");

    // Check that the directory iterator is valid when the path is an empty directory and return EOF
    {
        IoError error;

        IoHelper::DirectoryIterator it(emptyDir, false, error);
        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorSuccess, error);

        DirectoryEntry entry;
        bool endOfDirectory = false;
        CPPUNIT_ASSERT(it.next(entry, endOfDirectory, error));
        CPPUNIT_ASSERT(endOfDirectory);
    }

    // Check that the directory iterator is valid when the path is a directory with one file and return the file on first call
    {
        const SyncPath directoryWithOneFile = tempDir.path() / "chekDirIt/oneFile_dir";

        IoError error;
        IoHelper::DirectoryIterator it(directoryWithOneFile, false, error);
        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorSuccess, error);

        DirectoryEntry entry;
        bool endOfDirectory = false;
        CPPUNIT_ASSERT(it.next(entry, endOfDirectory, error));
        CPPUNIT_ASSERT(!endOfDirectory);

        CPPUNIT_ASSERT_EQUAL(directoryWithOneFile / "oneFile.txt", entry.path());

        CPPUNIT_ASSERT(it.next(entry, endOfDirectory, error));
        CPPUNIT_ASSERT(endOfDirectory);
    }

    // Check that the directory iterator is valid when the path is a directory with one child directory
    {
        const SyncPath directoryWithOneChildDirectory = tempDir.path() / "chekDirIt/oneDir_dir";

        IoError error;
        IoHelper::DirectoryIterator it(directoryWithOneChildDirectory, false, error);
        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorSuccess, error);

        DirectoryEntry entry;
        bool endOfDirectory = false;
        CPPUNIT_ASSERT(it.next(entry, endOfDirectory, error));
        CPPUNIT_ASSERT(!endOfDirectory);

        CPPUNIT_ASSERT_EQUAL(directoryWithOneChildDirectory / "testDir1", entry.path());
    }
}

void TestIo::testCheckDirectoryRecursive(void) {
    LocalTemporaryDirectory tempDir;

    // Create test directory with 4 directories with 1 file each
    SyncPath recursiveDir = tempDir.path() / "chekDirIt/recursive_dir";
    for (int i = 0; i < 4; ++i) {
        SyncPath childDir = recursiveDir / ("childDir_" + std::to_string(i));
        std::filesystem::create_directories(childDir);
        std::ofstream file(childDir / "file.txt");
        file << "file";
        file.close();
    }

    // Check that the directory iterator do not search recursively when the recursive flag is false
    {
        IoError error;
        IoHelper::DirectoryIterator it(recursiveDir, false, error);
        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorSuccess, error);

        DirectoryEntry entry;
        bool endOfDirectory = false;

        for (int i = 0; i < 4; i++) {
            CPPUNIT_ASSERT(it.next(entry, endOfDirectory, error));
            CPPUNIT_ASSERT(!endOfDirectory);
        }

        CPPUNIT_ASSERT(it.next(entry, endOfDirectory, error));
        CPPUNIT_ASSERT(endOfDirectory);
    }

    // Check that the directory iterator searches recursively when the recursive flag is true
    {
        IoError error;
        IoHelper::DirectoryIterator it(recursiveDir, true, error);
        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorSuccess, error);

        bool endOfDirectory = false;
        DirectoryEntry entry;

        for (int i = 0; i < 8; i++) {
            CPPUNIT_ASSERT(it.next(entry, endOfDirectory, error));
            CPPUNIT_ASSERT(!endOfDirectory);
        }

        CPPUNIT_ASSERT(it.next(entry, endOfDirectory, error));
        CPPUNIT_ASSERT(endOfDirectory);
    }
}

void TestIo::testCheckDirectoryIteratotNextAfterEndOfDir() {
    // Create test directory with one file
    LocalTemporaryDirectory tempDir;
    SyncPath oneFileDir = tempDir.path() / "chekDirIt/oneFile_dir";
    std::filesystem::create_directories(oneFileDir);
    std::ofstream file(oneFileDir / "oneFile.txt");
    file << "oneFile";
    file.close();

    // Check that the directory iterator returns an EOF on everycall to next after EOF
    {
        IoError error;
        IoHelper::DirectoryIterator it(oneFileDir, false, error);
        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorSuccess, error);

        // Read the only file in the directory
        bool endOfDirectory = false;
        DirectoryEntry entry;

        CPPUNIT_ASSERT(it.next(entry, endOfDirectory, error));
        CPPUNIT_ASSERT(!endOfDirectory);

        // Expecting EOF
        for (int i = 0; i < 3; i++) {
            CPPUNIT_ASSERT(it.next(entry, endOfDirectory, error));
            CPPUNIT_ASSERT(endOfDirectory);
        }
    }
}

void TestIo::testCheckDirectoryIteratorPermission() {
    {
        // Check that the directory iterator skips each directory with no permission
        LocalTemporaryDirectory tempDir;
        const SyncPath noPermissionDir = tempDir.path() / "chekDirIt/noPermission";
        const SyncPath noPermissionFile = noPermissionDir / "file.txt";

        IoError ioError = IoErrorSuccess;

        CPPUNIT_ASSERT(IoHelper::createDirectory(noPermissionDir.parent_path(), ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorSuccess, ioError);
        CPPUNIT_ASSERT(IoHelper::createDirectory(noPermissionDir, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorSuccess, ioError);

        std::ofstream file(noPermissionFile);
        file << "file";
        file.close();


        bool result = IoHelper::setRights(noPermissionDir, false, false, false, ioError);
        result &= ioError == IoErrorSuccess;
        if (!result) {
            IoHelper::setRights(noPermissionDir, true, true, true, ioError);
            CPPUNIT_ASSERT(false /*setRights failed*/);
        }

        IoHelper::DirectoryIterator it(noPermissionDir, false, ioError);

        DirectoryEntry entry;
        bool endOfDirectory = false;
        bool success = it.next(entry, endOfDirectory, ioError);

        IoHelper::setRights(noPermissionDir, true, true, true, ioError);


        CPPUNIT_ASSERT(success);
        CPPUNIT_ASSERT(endOfDirectory);
    }
}

void TestIo::testCheckDirectoryIteratorUnexpectedDelete() {
    LocalTemporaryDirectory tempDir;

    // Create test directory with 5 subdirectories
    const SyncPath path = tempDir.path().string() + "\\chekDirIt\\IteratorUnexpectedDelete";
    std::string subDir = path.string();

    for (int i = 0; i < 5; i++) {
        subDir += "/subDir" + std::to_string(i);
    }

    std::filesystem::create_directories(subDir);

    // Check that the directory iterator is consistent when a parent directory is deleted
    {
        IoError ioError = IoErrorSuccess;
        IoHelper::DirectoryIterator it(path, true, ioError);
        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorSuccess, ioError);

        DirectoryEntry entry;
        bool endOfDirectory = false;
        CPPUNIT_ASSERT(it.next(entry, endOfDirectory, ioError));
        CPPUNIT_ASSERT(!endOfDirectory);

        std::filesystem::remove_all(path);

        CPPUNIT_ASSERT(it.next(entry, endOfDirectory, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorNoSuchFileOrDirectory, ioError);

        CPPUNIT_ASSERT(it.next(entry, endOfDirectory, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::IoErrorInvalidDirectoryIterator, ioError);
    }
}

void TestIo::testCheckDirectoryPermissionLost() {
    const LocalTemporaryDirectory temporaryDirectory;
    const SyncPath chekDirItDir = temporaryDirectory.path() / "chekDirIt";
    const SyncPath permLostRoot = chekDirItDir / "permissionLost";
    const SyncPath subDir = permLostRoot / "subDir1";
    const SyncPath filePath = subDir / "file.txt";

    std::filesystem::create_directories(subDir);
    std::ofstream file(filePath);
    file << "file";
    file.close();

    // Check that the directory iterator is consistent when a parent directory lose permission
    {
        IoError ioError = IoErrorSuccess;
        IoHelper::DirectoryIterator it(permLostRoot, true, ioError);

        // Remove permission (after iterator is created)
        bool result = IoHelper::setRights(subDir, false, false, false, ioError);
        result &= ioError == IoErrorSuccess;
        if (!result) {
            IoHelper::setRights(subDir, true, true, true, ioError);
            CPPUNIT_ASSERT(false /*setRights failed*/);
        }

        DirectoryEntry entry;
        bool endOfDirectory = false;

        result = it.next(entry, endOfDirectory, ioError);
        result &= ioError == IoErrorSuccess;

        if (!result) {
            IoHelper::setRights(subDir, true, true, true, ioError);
            IoHelper::setRights(filePath, true, true, true, ioError);
        }

        CPPUNIT_ASSERT(result /*result = it.next(entry, endOfDirectory, ioError);*/);

        result = it.next(entry, endOfDirectory, ioError);
        result &= ioError == IoErrorSuccess;

        IoHelper::setRights(subDir, true, true, true, ioError);
        IoHelper::setRights(filePath, true, true, true, ioError);

        CPPUNIT_ASSERT(result /*result = it.next(entry, endOfDirectory, ioError);*/);
        CPPUNIT_ASSERT(endOfDirectory);
    }
}

}  // namespace KDC
