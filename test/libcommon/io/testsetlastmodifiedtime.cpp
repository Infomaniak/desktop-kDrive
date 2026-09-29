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

#include "libcommonserver/io/fileStat.h"
#include "test_utility/testhelpers.h"

#include <filesystem>

using namespace CppUnit;

namespace KDC {

void TestIo::testSetLastModifiedTime() {
    const auto timestamp = testhelpers::defaultTime;

    // Test on a regular file.
    {
        const LocalTemporaryDirectory tempDir("testSetLastModifiedTime");
        const SyncPath filepath = tempDir.path() / "test.txt";
        testhelpers::generateOrEditTestFile(filepath);

        // Set a known creation date, later used to check that setLastModifiedTime leaves it unchanged.
        IoError ioError = IoHelper::setFileDates(filepath, timestamp, timestamp, false);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

        FileStat fileStat;
        CPPUNIT_ASSERT(IoHelper::getFileStat(filepath, &fileStat, ioError, IoHelper::PathCheckOption::Insensitive));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
#if defined(KD_MACOS) || defined(KD_WINDOWS)
        CPPUNIT_ASSERT_EQUAL(timestamp, fileStat.creationTime);
#endif

        CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::setLastModifiedTime(filepath, timestamp + 10));

        CPPUNIT_ASSERT(IoHelper::getFileStat(filepath, &fileStat, ioError, IoHelper::PathCheckOption::Insensitive));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
#if defined(KD_MACOS) || defined(KD_WINDOWS)
        CPPUNIT_ASSERT_EQUAL(timestamp, fileStat.creationTime); // The creation date is left unchanged
#endif
        CPPUNIT_ASSERT_EQUAL(timestamp + 10, fileStat.modificationTime);
    }

    // Test on a regular folder.
    {
        const LocalTemporaryDirectory tempDir("testSetLastModifiedTime");
        const SyncPath folderPath = tempDir.path() / "test_dir";
        std::error_code ec;
        CPPUNIT_ASSERT(std::filesystem::create_directory(folderPath, ec));

        // Set a known creation date, later used to check that setLastModifiedTime leaves it unchanged.
        IoError ioError = IoHelper::setFileDates(folderPath, timestamp, timestamp, false);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

        FileStat fileStat;
        (void) IoHelper::getFileStat(folderPath, &fileStat, ioError, IoHelper::PathCheckOption::Insensitive);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
#if defined(KD_MACOS) || defined(KD_WINDOWS)
        CPPUNIT_ASSERT_EQUAL(timestamp, fileStat.creationTime);
#endif

        CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::setLastModifiedTime(folderPath, timestamp + 10));

        (void) IoHelper::getFileStat(folderPath, &fileStat, ioError, IoHelper::PathCheckOption::Insensitive);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
#if defined(KD_MACOS) || defined(KD_WINDOWS)
        CPPUNIT_ASSERT_EQUAL(timestamp, fileStat.creationTime); // The creation date is left unchanged
#endif
        CPPUNIT_ASSERT_EQUAL(timestamp + 10, fileStat.modificationTime);
    }

    // Test on a symlink on a file. `IoHelper::setLastModifiedTime` should not follow symlinks, so the modification date of the
    // target file should be left unchanged.
    {
        const LocalTemporaryDirectory tempDir("testSetLastModifiedTime");
        const SyncPath filepath = tempDir.path() / "test.txt";
        testhelpers::generateOrEditTestFile(filepath);
        const SyncPath linkPath = tempDir.path() / "test_link_file";

        auto ioError = IoError::Success;
        CPPUNIT_ASSERT(IoHelper::createSymlink(filepath, linkPath, false, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

        // Set a known modification date for the target file, later used to check that it is left unchanged.
        ioError = IoHelper::setFileDates(filepath, timestamp, timestamp, false);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

        CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::setLastModifiedTime(linkPath, timestamp + 10));

        FileStat fileStat;
        (void) IoHelper::getFileStat(linkPath, &fileStat, ioError, IoHelper::PathCheckOption::Insensitive);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        CPPUNIT_ASSERT_EQUAL(timestamp + 10, fileStat.modificationTime); // The symlink modification date is updated

        // The target file is left unchanged.
        (void) IoHelper::getFileStat(filepath, &fileStat, ioError, IoHelper::PathCheckOption::Insensitive);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
#if defined(KD_MACOS) || defined(KD_WINDOWS)
        CPPUNIT_ASSERT_EQUAL(timestamp, fileStat.creationTime);
#endif
        CPPUNIT_ASSERT_EQUAL(timestamp, fileStat.modificationTime);
    }

    // Test on a non-existing item.
    {
        const LocalTemporaryDirectory tempDir("testSetLastModifiedTime");
        const SyncPath filepath = tempDir.path() / "does_not_exist.txt";
        CPPUNIT_ASSERT_EQUAL(IoError::NoSuchFileOrDirectory, IoHelper::setLastModifiedTime(filepath, timestamp));
    }
}

} // namespace KDC
