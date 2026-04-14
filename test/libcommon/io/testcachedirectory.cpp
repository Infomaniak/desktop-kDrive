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

#include "testcachedirectory.h"

#include "io/cachedirectory.h"
#include "io/iohelper.h"
#include "test_utility/localtemporarydirectory.h"
#include "test_utility/testhelpers.h"

#include <regex>

namespace KDC {

void TestCacheDirectory::testCacheDir() {
    // Create cache directory
    const LocalTemporaryDirectory tmpDir;
    const auto cacheDirectory = std::make_shared<CacheDirectory>(tmpDir.path());

    SyncPath cacheDirectoryPath;
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, cacheDirectory->path(cacheDirectoryPath).code());
    CPPUNIT_ASSERT(!cacheDirectoryPath.empty());

    // Delete the tmp directory and make sure it is re-created
    auto ioError = IoError::Unknown;
    CPPUNIT_ASSERT(IoHelper::deleteItem(cacheDirectoryPath, ioError));

    bool exists = false;
    CPPUNIT_ASSERT(IoHelper::checkIfPathExists(cacheDirectoryPath, exists, ioError, IoHelper::PathCheckOption::Insensitive));
    CPPUNIT_ASSERT(!exists);

    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, cacheDirectory->path(cacheDirectoryPath).code());
    CPPUNIT_ASSERT(!cacheDirectoryPath.empty());

    CPPUNIT_ASSERT(IoHelper::checkIfPathExists(cacheDirectoryPath, exists, ioError, IoHelper::PathCheckOption::Insensitive));
    CPPUNIT_ASSERT(exists);
}

void TestCacheDirectory::testCleanUpCacheDir() {
    const LocalTemporaryDirectory tmpDir;
    SyncPath cacheDirectoryPath;
    {
        CacheDirectory cacheDirectory(tmpDir.path());
        CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, cacheDirectory.path(cacheDirectoryPath).code());
        CPPUNIT_ASSERT(!cacheDirectoryPath.empty());

        // Generate test files
        testhelpers::generateTestFile(cacheDirectoryPath / Str("kdrive_1234567890"));
        testhelpers::generateTestFile(cacheDirectoryPath / Str("kdrive_0987654321"));
        testhelpers::generateTestFile(cacheDirectoryPath / Str("kdrive_123456789"));
        testhelpers::generateTestFile(cacheDirectoryPath / Str("drive_1234567890"));
        testhelpers::generateTestFile(cacheDirectoryPath / Str("myFile.txt"));
    }

    // Check the file been correctly cleaned
    auto ioError = IoError::Unknown;
    IoHelper::DirectoryIterator dirIt(cacheDirectoryPath, false, ioError);
    bool endOfDir = false;
    DirectoryEntry entry;
    auto count = 0;
    while (dirIt.next(entry, endOfDir, ioError) && !endOfDir) {
        if (std::regex_match(SyncName2Str(entry.path().filename().native()), std::regex(R"(kdrive_.{10})"))) {
            CPPUNIT_ASSERT(false);
        }
        count++;
    }
    CPPUNIT_ASSERT_EQUAL(3, count);
}

} // namespace KDC
