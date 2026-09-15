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

    // Regular file and destination doesn't exist
    const auto sourceFilePath = tempDir.path() / "file.txt";
    uint64_t sourceFileSize = 10;
    testhelpers::generateTestFile(sourceFilePath, sourceFileSize);
    const auto destFilePath = tempDir.path() / "file_copy.txt";
    std::error_code ec;
    CPPUNIT_ASSERT(!std::filesystem::exists(destFilePath, ec) && !ec.value());
    IoError ioError = IoError::Success;
    CPPUNIT_ASSERT(IoHelper::copyFileOrDirectory(sourceFilePath, destFilePath, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(std::filesystem::exists(destFilePath, ec) && !ec.value());
    uint64_t destFileSize = std::filesystem::file_size(destFilePath, ec);
    CPPUNIT_ASSERT(destFileSize == sourceFileSize);

    // Regular file and destination does exist
    // => The target file is overwritten by the source file (the node id of the target file is not changed)
    NodeId fileNodeIdBeforeCopy;
    CPPUNIT_ASSERT(IoHelper::getNodeId(destFilePath, fileNodeIdBeforeCopy));
    sourceFileSize = 20;
    testhelpers::setTestFileSize(sourceFilePath, sourceFileSize);
    CPPUNIT_ASSERT(destFileSize != sourceFileSize);
    ioError = IoError::Success;
    CPPUNIT_ASSERT(IoHelper::copyFileOrDirectory(sourceFilePath, destFilePath, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    NodeId fileNodeIdAfterCopy;
    CPPUNIT_ASSERT(IoHelper::getNodeId(destFilePath, fileNodeIdAfterCopy));
    CPPUNIT_ASSERT(fileNodeIdBeforeCopy == fileNodeIdAfterCopy);
    destFileSize = std::filesystem::file_size(destFilePath, ec);
    CPPUNIT_ASSERT(destFileSize == sourceFileSize);

    // Regular empty folder and destination doesn't exist
    const auto sourceFolderPath = tempDir.path() / "folder";
    CPPUNIT_ASSERT(testhelpers::generateTestFolder(sourceFolderPath));
    const auto destFolderPath = tempDir.path() / "folder_copy";
    CPPUNIT_ASSERT(!std::filesystem::exists(destFolderPath, ec) && !ec.value());
    ioError = IoError::Success;
    CPPUNIT_ASSERT(IoHelper::copyFileOrDirectory(sourceFolderPath, destFolderPath, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(std::filesystem::exists(destFolderPath, ec) && !ec.value());

    // Regular empty folder and destination does exist
    // => The destination folder is overwritten by the source folder (the node id of the target folder is not changed)
    NodeId folderNodeIdBeforeCopy;
    CPPUNIT_ASSERT(IoHelper::getNodeId(destFolderPath, folderNodeIdBeforeCopy));
    ioError = IoError::Success;
    CPPUNIT_ASSERT(IoHelper::copyFileOrDirectory(sourceFolderPath, destFolderPath, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    NodeId folderNodeIdAfterCopy;
    CPPUNIT_ASSERT(IoHelper::getNodeId(destFolderPath, folderNodeIdAfterCopy));
    CPPUNIT_ASSERT(folderNodeIdBeforeCopy == folderNodeIdAfterCopy);

    // Regular non empty folder and destination folder does exist and is empty
    sourceFileSize = 30;
    testhelpers::generateTestFile(sourceFolderPath / "file.txt", sourceFileSize);
    ioError = IoError::Success;
    CPPUNIT_ASSERT(IoHelper::copyFileOrDirectory(sourceFolderPath, destFolderPath, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(std::filesystem::exists(destFolderPath / "file.txt", ec) && !ec.value());
    destFileSize = std::filesystem::file_size(destFolderPath / "file.txt", ec);
    CPPUNIT_ASSERT(destFileSize == sourceFileSize);

    // Regular non empty folder and destination folder does exist and contains a file that exists in the source folder
    // => The destination folder is overwritten by the source folder (the node id of the target file is not changed)
    fileNodeIdBeforeCopy.clear();
    CPPUNIT_ASSERT(IoHelper::getNodeId(destFolderPath / "file.txt", fileNodeIdBeforeCopy));
    sourceFileSize = 40;
    testhelpers::setTestFileSize(sourceFolderPath / "file.txt", sourceFileSize);
    CPPUNIT_ASSERT(destFileSize != sourceFileSize);
    ioError = IoError::Success;
    CPPUNIT_ASSERT(IoHelper::copyFileOrDirectory(sourceFolderPath, destFolderPath, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    fileNodeIdAfterCopy.clear();
    CPPUNIT_ASSERT(IoHelper::getNodeId(destFolderPath / "file.txt", fileNodeIdAfterCopy));
    CPPUNIT_ASSERT(fileNodeIdBeforeCopy == fileNodeIdAfterCopy);
    destFileSize = std::filesystem::file_size(destFolderPath / "file.txt", ec);
    CPPUNIT_ASSERT(destFileSize == sourceFileSize);

    // Copy a file to a destination which is a folder
    // => The file is copied into the folder
    CPPUNIT_ASSERT(std::filesystem::is_directory(destFolderPath, ec) && !ec.value());
    CPPUNIT_ASSERT(std::filesystem::remove(destFolderPath / "file.txt", ec) && !ec.value());
    ioError = IoError::Success;
    CPPUNIT_ASSERT(IoHelper::copyFileOrDirectory(sourceFilePath, destFolderPath, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(std::filesystem::exists(destFolderPath / "file.txt", ec) && !ec.value());

    // Copy a folder to destination which is a file
    // => The copy fails
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(destFilePath, ec) && !ec.value());
    ioError = IoError::Success;
    CPPUNIT_ASSERT(!IoHelper::copyFileOrDirectory(sourceFolderPath, destFilePath, ioError));
#if defined(KD_MACOS)
    CPPUNIT_ASSERT_EQUAL(IoError::Unknown, ioError); // std::errc::function_not_supported
#elif defined(KD_WINDOWS)
    CPPUNIT_ASSERT_EQUAL(IoError::CrossDeviceLink, ioError);
#else
    CPPUNIT_ASSERT_EQUAL(IoError::IsADirectory, ioError);
#endif

    // Regular symlink and destination doesn't exist
    const auto sourceSymlinkPath = tempDir.path() / "symlink.txt";
    std::filesystem::create_symlink("dummy/target", sourceSymlinkPath, ec);
    const auto destSymlinkPath = tempDir.path() / "symlink_copy.txt";
    CPPUNIT_ASSERT(!std::filesystem::exists(destSymlinkPath, ec) && !ec.value());
    ioError = IoError::Success;
    CPPUNIT_ASSERT(IoHelper::copyFileOrDirectory(sourceSymlinkPath, destSymlinkPath, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(std::filesystem::is_symlink(destSymlinkPath, ec) && !ec.value());

    // Regular symlink and destination does exist
    // => The existing symlink is deleted before the copy
    auto destTargetPathBeforeCopy = std::filesystem::read_symlink(destSymlinkPath, ec);
    ioError = IoError::Success;
    const auto sourceSymlink2Path = tempDir.path() / "symlink2.txt";
    std::filesystem::create_symlink("dummy/target2", sourceSymlink2Path, ec);
    CPPUNIT_ASSERT(IoHelper::copyFileOrDirectory(sourceSymlink2Path, destSymlinkPath, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(std::filesystem::is_symlink(destSymlinkPath, ec) && !ec.value());
    auto destTargetPathAfterCopy = std::filesystem::read_symlink(destSymlinkPath, ec);
    CPPUNIT_ASSERT(destTargetPathBeforeCopy != destTargetPathAfterCopy);

    // Copy a file to a destination which is a symlink
    // => The symlink is replaced by the file
    CPPUNIT_ASSERT(std::filesystem::is_symlink(destSymlinkPath, ec) && !ec.value());
    ioError = IoError::Success;
    CPPUNIT_ASSERT(IoHelper::copyFileOrDirectory(sourceFilePath, destSymlinkPath, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(destSymlinkPath, ec) &&
                   !ec.value()); // The symlink has been replaced by a file

    // Recreate the dest symlink for the next tests
    CPPUNIT_ASSERT(std::filesystem::remove(destSymlinkPath, ec) && !ec.value());
    CPPUNIT_ASSERT(IoHelper::copyFileOrDirectory(sourceSymlinkPath, destSymlinkPath, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    // Copy a folder to a destination which is a symlink
    // => The symlink is replaced by the folder
    CPPUNIT_ASSERT(std::filesystem::is_symlink(destSymlinkPath, ec) && !ec.value());
    ioError = IoError::Success;
    CPPUNIT_ASSERT(IoHelper::copyFileOrDirectory(sourceFolderPath, destSymlinkPath, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(std::filesystem::is_directory(destSymlinkPath, ec) && !ec.value());

    // Copy a symlink to a destination which is a file
    // => The copy fails
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(destFilePath, ec) && !ec.value());
    ioError = IoError::Success;
    CPPUNIT_ASSERT(!IoHelper::copyFileOrDirectory(sourceSymlinkPath, destFilePath, ioError));
#if defined(KD_MACOS)
    CPPUNIT_ASSERT_EQUAL(IoError::FileExists, ioError);
#elif defined(KD_WINDOWS)
    CPPUNIT_ASSERT_EQUAL(IoError::NoSuchFileOrDirectory, ioError);
#else
    CPPUNIT_ASSERT_EQUAL(IoError::InvalidArgument, ioError);
#endif

    // Copy a symlink to a destination which is a folder
    // => The copy fails
    CPPUNIT_ASSERT(std::filesystem::is_directory(destFolderPath, ec) && !ec.value());
    ioError = IoError::Success;
    CPPUNIT_ASSERT(!IoHelper::copyFileOrDirectory(sourceSymlinkPath, destFolderPath, ioError));
#if defined(KD_MACOS)
    CPPUNIT_ASSERT_EQUAL(IoError::FileExists, ioError);
#elif defined(KD_WINDOWS)
    CPPUNIT_ASSERT_EQUAL(IoError::NoSuchFileOrDirectory, ioError);
#else
    CPPUNIT_ASSERT_EQUAL(IoError::InvalidArgument, ioError);
#endif
}

} // namespace KDC
