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
#include "test_utility/testhelpers.h"

#include <filesystem>

#if defined(KD_WINDOWS)
#include <windows.h>
#endif

using namespace CppUnit;

namespace KDC {

void TestIo::testMoveItemToTrash() {
    // !!! Linux - Move to trash fails on tmpfs
    if (Utility::userName() == "docker") return;
    LocalTemporaryDirectory tempDir;
    SyncPath path = tempDir.path() / "test.txt";
    std::ofstream file(path);
    file << "test";
    file.close();

    CPPUNIT_ASSERT(IoHelper::moveItemToTrash(path));
    CPPUNIT_ASSERT(!std::filesystem::exists(path));

    // Test with a non-existing file
    CPPUNIT_ASSERT(!IoHelper::moveItemToTrash(tempDir.path() / "test2.txt"));

    // Test with a directory
    SyncPath dirPath = tempDir.path() / "testDir";
    std::filesystem::create_directory(dirPath);
    CPPUNIT_ASSERT(IoHelper::moveItemToTrash(dirPath));
    CPPUNIT_ASSERT(!std::filesystem::exists(dirPath));

    // A regular file within a subdirectory that misses owner exec permission:
    const SyncPath subdir = tempDir.path() / "permission_less_subdirectory";
    (void) std::filesystem::create_directory(subdir);
    path = subdir / "file.txt";
    { std::ofstream ofs(path); }
    const testhelpers::RightsSet rightSet(true, true, false);
    auto rightsError = IoError::Unknown;
    CPPUNIT_ASSERT(IoHelper::setRights(subdir, rightSet.read, rightSet.write, rightSet.execute, rightsError));

    std::error_code ec;
#if defined(KD_WINDOWS)
    CPPUNIT_ASSERT(IoHelper::moveItemToTrash(path));
    CPPUNIT_ASSERT(!std::filesystem::exists(path, ec));
    CPPUNIT_ASSERT_EQUAL(0, ec.value());
#elif defined(KD_MACOS) || defined(KD_LINUX)
    CPPUNIT_ASSERT(!IoHelper::moveItemToTrash(path));
    CPPUNIT_ASSERT(!std::filesystem::exists(path, ec));
    CPPUNIT_ASSERT_EQUAL(13, ec.value()); // EACCES 13 Permission denied
#endif

    // A regular directory that misses owner exec permission:
    CPPUNIT_ASSERT(IoHelper::moveItemToTrash(subdir));
    CPPUNIT_ASSERT(!std::filesystem::exists(subdir, ec));
    CPPUNIT_ASSERT(!ec);

    // A regular directory that misses all permissions:
    const testhelpers::RightsSet noPermission(false, false, false);
    CPPUNIT_ASSERT(IoHelper::setRights(subdir, noPermission.read, noPermission.write, noPermission.execute, rightsError));
    CPPUNIT_ASSERT(!IoHelper::moveItemToTrash(subdir));
    CPPUNIT_ASSERT(!std::filesystem::exists(subdir, ec));
    CPPUNIT_ASSERT(!ec);
    CPPUNIT_ASSERT(IoHelper::setRights(subdir, true, true, true, rightsError));
}

} // namespace KDC
