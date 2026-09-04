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

#include "testdeleteitematomically.h"

#include "test_utility/localtemporarydirectory.h"
#include "test_utility/testhelpers.h"
#include "test_utility/iohelpertestutilities.h"

#include "io/cachedirectory.h"
#include "io/iohelper.h"

namespace KDC {

void TestDeleteItemAtomically::testDeleteRegularFile() {
    const LocalTemporaryDirectory temporaryDirectory("testDeleteItemAtomically_regularFile");
    const auto cacheDirectory = std::make_shared<CacheDirectory>(temporaryDirectory.path());
    SyncPath cacheDirectoryPath;
    CPPUNIT_ASSERT(cacheDirectory->path(cacheDirectoryPath));

    // A regular file is first atomically moved to the cache directory and is then deleted from it.
    const SyncPath filePath = temporaryDirectory.path() / "test_file.txt";
    { std::ofstream ofs(filePath); }

    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), IoHelper::deleteItemAtomically(filePath, cacheDirectory));
    CPPUNIT_ASSERT(!std::filesystem::exists(filePath));
    CPPUNIT_ASSERT(std::filesystem::exists(cacheDirectoryPath)); // The cache directory is left in place.
    CPPUNIT_ASSERT(!std::filesystem::exists(cacheDirectoryPath / filePath.filename()));
}

void TestDeleteItemAtomically::testDeleteDirectory() {
    const LocalTemporaryDirectory temporaryDirectory("testDeleteItemAtomically_directory");
    const auto cacheDirectory = std::make_shared<CacheDirectory>(temporaryDirectory.path());
    SyncPath cacheDirectoryPath;
    CPPUNIT_ASSERT(cacheDirectory->path(cacheDirectoryPath));

    // A directory and its content are first atomically moved to the cache directory and are then deleted from it.
    const SyncPath dirPath = temporaryDirectory.path() / "test_dir";
    CPPUNIT_ASSERT(std::filesystem::create_directory(dirPath));
    { std::ofstream ofs(dirPath / "test_file.txt"); }

    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), IoHelper::deleteItemAtomically(dirPath, cacheDirectory));
    CPPUNIT_ASSERT(!std::filesystem::exists(dirPath));
    CPPUNIT_ASSERT(std::filesystem::is_empty(cacheDirectoryPath));
}

void TestDeleteItemAtomically::testDeleteNonExistingItem() {
    const LocalTemporaryDirectory temporaryDirectory("testDeleteItemAtomically_nonExistingItem");
    const auto cacheDirectory = std::make_shared<CacheDirectory>(temporaryDirectory.path());
    SyncPath cacheDirectoryPath;
    CPPUNIT_ASSERT(cacheDirectory->path(cacheDirectoryPath));

    // Non-existing items do not raise any deletion error.
    const SyncPath nonExistingPath = temporaryDirectory.path() / "non-existing-item.txt";
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), IoHelper::deleteItemAtomically(nonExistingPath, cacheDirectory));

    // The cache directory has not been modified.
    CPPUNIT_ASSERT(std::filesystem::is_empty(cacheDirectoryPath));
}

void TestDeleteItemAtomically::testDeleteItemWithoutRights() {
    const LocalTemporaryDirectory temporaryDirectory("testDeleteItemAtomically_withoutRights");
    const auto cacheDirectory = std::make_shared<CacheDirectory>(temporaryDirectory.path());
    SyncPath cacheDirectoryPath;
    CPPUNIT_ASSERT(cacheDirectory->path(cacheDirectoryPath));

    // A file within a directory that misses owner write and execute permissions cannot be moved to the cache directory.
    // It is left unmodified.
    const SyncPath permissionLessSubdir = temporaryDirectory.path() / "permission_less_subdirectory";
    CPPUNIT_ASSERT(std::filesystem::create_directory(permissionLessSubdir));
    const SyncPath filePathInSubdir = permissionLessSubdir / "test_file.txt";
    { std::ofstream ofs(filePathInSubdir); }
    const testhelpers::RightsSet rightSet(true, true, false);
    auto rightsError = IoError::Unknown;
    CPPUNIT_ASSERT(IoHelper::setRights(permissionLessSubdir, rightSet.read, rightSet.write, rightSet.execute, rightsError));

#if defined(KD_MACOS) || defined(KD_LINUX)
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::SystemError, ExitCause::FileAccessError),
                         IoHelper::deleteItemAtomically(filePathInSubdir, cacheDirectory));
#elif defined(KD_WINDOWS)
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), IoHelper::deleteItemAtomically(filePathInSubdir, cacheDirectory));
#endif
    // Restore the rights so that the temporary directory can be inspected and then deleted.
    CPPUNIT_ASSERT(IoHelper::setRights(permissionLessSubdir, true, true, true, rightsError));

#if defined(KD_MACOS) || defined(KD_LINUX)
    CPPUNIT_ASSERT(std::filesystem::exists(filePathInSubdir));
    CPPUNIT_ASSERT(std::filesystem::is_empty(cacheDirectoryPath));
#elif defined(KD_WINDOWS)
    CPPUNIT_ASSERT(!std::filesystem::exists(filePathInSubdir));
    CPPUNIT_ASSERT(!std::filesystem::is_empty(cacheDirectoryPath));
#endif
}

} // namespace KDC
