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
#include "test_utility/testhelpers.h"

#include <filesystem>

using namespace CppUnit;

namespace KDC {

using namespace testhelpers;


void TestIo::testCopyFileOrDirectory() {
    const LocalTemporaryDirectory tempDir("testCopyFileOrDirectory");

    // Regular file and target doesn't exist
    const auto sourceFilePath = tempDir.path() / "file.txt";
    testhelpers::generateOrEditTestFile(sourceFilePath);
    const auto destFilePath = tempDir.path() / "file_copy.txt";
    IoError ioError = IoError::Success;
    CPPUNIT_ASSERT(IoHelper::copyFileOrDirectory(sourceFilePath, destFilePath, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    std::error_code ec;
    CPPUNIT_ASSERT(std::filesystem::exists(destFilePath, ec) && !ec.value());

    // Regular file and target does exist
    ioError = IoError::Success;
    CPPUNIT_ASSERT(IoHelper::copyFileOrDirectory(sourceFilePath, destFilePath, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    // Regular empty folder and target doesn't exist
    const auto sourceFolderPath = tempDir.path() / "folder";
    testhelpers::generateTestFolder(sourceFolderPath);
    const auto destFolderPath = tempDir.path() / "folder_copy";
    ioError = IoError::Success;
    CPPUNIT_ASSERT(IoHelper::copyFileOrDirectory(sourceFolderPath, destFolderPath, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(std::filesystem::exists(destFolderPath, ec) && !ec.value());

    // Regular empty folder and target does exist
    ioError = IoError::Success;
    CPPUNIT_ASSERT(IoHelper::copyFileOrDirectory(sourceFolderPath, destFolderPath, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    // Regular non empty folder and target does exist and is empty
    testhelpers::generateOrEditTestFile(sourceFolderPath / "file.txt");
    ioError = IoError::Success;
    CPPUNIT_ASSERT(IoHelper::copyFileOrDirectory(sourceFolderPath, destFolderPath, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(std::filesystem::exists(destFolderPath / "file.txt", ec) && !ec.value());

    // Regular non empty folder and target does exist and contains a file that exists in the source folder
    ioError = IoError::Success;
    CPPUNIT_ASSERT(IoHelper::copyFileOrDirectory(sourceFolderPath, destFolderPath, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    // Copy a file to a folder target
    ioError = IoError::Success;
    CPPUNIT_ASSERT(IoHelper::copyFileOrDirectory(sourceFilePath, destFolderPath, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    // Copy a folder to a file target
    ioError = IoError::Success;
    CPPUNIT_ASSERT(!IoHelper::copyFileOrDirectory(sourceFolderPath, destFilePath, ioError));
#if defined(KD_MACOS)
    CPPUNIT_ASSERT_EQUAL(IoError::Unknown, ioError); // std::errc::function_not_supported
#else
    CPPUNIT_ASSERT_EQUAL(IoError::IsADirectory, ioError);
#endif

    // Regular symlink and target doesn't exist
    const auto sourceSymlinkPath = tempDir.path() / "symlink.txt";
    std::filesystem::create_symlink("dummy/target", sourceSymlinkPath, ec);
    const auto destSymlinkPath = tempDir.path() / "symlink_copy.txt";
    ioError = IoError::Success;
    CPPUNIT_ASSERT(IoHelper::copyFileOrDirectory(sourceSymlinkPath, destSymlinkPath, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(std::filesystem::is_symlink(destSymlinkPath, ec) && !ec.value());

    // Regular symlink and target does exist
#if defined(KD_MACOS) || defined(KD_WINDOWS)
    NodeId symlinkNodeIdBeforeCopy;
    CPPUNIT_ASSERT(IoHelper::getNodeId(destSymlinkPath, symlinkNodeIdBeforeCopy));
#endif
    ioError = IoError::Success;
    CPPUNIT_ASSERT(IoHelper::copyFileOrDirectory(sourceSymlinkPath, destSymlinkPath, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(std::filesystem::is_symlink(destSymlinkPath, ec) && !ec.value());
#if defined(KD_MACOS) || defined(KD_WINDOWS)
    // Check that the symlink has been replaced by a new one with a different inode number
    // Note: on Linux, the inode number of a symlink is not unique and can be reused by another symlink, so we cannot check that
    // the inode number has changed
    NodeId symlinkNodeIdAfterCopy;
    CPPUNIT_ASSERT(IoHelper::getNodeId(destSymlinkPath, symlinkNodeIdAfterCopy));
    CPPUNIT_ASSERT(symlinkNodeIdBeforeCopy != symlinkNodeIdAfterCopy);
#endif

    // Copy a file to a symlink target
    ioError = IoError::Success;
    CPPUNIT_ASSERT(IoHelper::copyFileOrDirectory(sourceFilePath, destSymlinkPath, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(destSymlinkPath, ec) &&
                   !ec.value()); // The symlink has been replaced by a file

    // Recreate the dest symlink for the next tests
    CPPUNIT_ASSERT(std::filesystem::remove(destSymlinkPath, ec) && !ec.value());
    CPPUNIT_ASSERT(IoHelper::copyFileOrDirectory(sourceSymlinkPath, destSymlinkPath, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    // Copy a folder to a symlink target
    ioError = IoError::Success;
    CPPUNIT_ASSERT(IoHelper::copyFileOrDirectory(sourceFolderPath, destSymlinkPath, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(std::filesystem::is_directory(destSymlinkPath, ec) &&
                   !ec.value()); // The symlink has been replaced by a folder

    // Copy a symlink to a file target
    ioError = IoError::Success;
    CPPUNIT_ASSERT(!IoHelper::copyFileOrDirectory(sourceSymlinkPath, destFilePath, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::FileExists, ioError);

    // Copy a symlink to a folder target
    ioError = IoError::Success;
    CPPUNIT_ASSERT(!IoHelper::copyFileOrDirectory(sourceSymlinkPath, destFolderPath, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::FileExists, ioError); // std::errc::function_not_supported
}

} // namespace KDC
