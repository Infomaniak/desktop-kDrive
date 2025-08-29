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


void TestIo::testGetDirectorySize() {
    // Getting the size of a directory containing a regular file
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "file.txt";
        {
            std::ofstream ofs(path);
            ofs << std::string(1000, 'a');
        }
        uint64_t dirSize = 0u;
        IoError ioError = IoError::Success;

        CPPUNIT_ASSERT(IoHelper::getDirectorySize(temporaryDirectory.path(), dirSize, ioError));
        CPPUNIT_ASSERT(uint64_t(1000) <= dirSize);
        CPPUNIT_ASSERT(uint64_t(2000) >= dirSize);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // Getting the size of a directory containing a regular file missing all permissions: no error expected
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "permission_less_file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
            ofs.close();
        }

        std::filesystem::permissions(path, std::filesystem::perms::all, std::filesystem::perm_options::remove);

        uint64_t dirSize = 0u;
        IoError ioError = IoError::Success;

        CPPUNIT_ASSERT(IoHelper::getDirectorySize(temporaryDirectory.path(), dirSize, ioError));

        std::filesystem::permissions(path, std::filesystem::perms::all, std::filesystem::perm_options::add);

        CPPUNIT_ASSERT(uint64_t(14) <= dirSize);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // Attempting to get the size of a file
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "file.txt";
        { std::ofstream ofs(path); }
        uint64_t dirSize = 0u;
        IoError ioError = IoError::Success;

        CPPUNIT_ASSERT(IoHelper::getDirectorySize(temporaryDirectory.path(), dirSize, ioError));
        CPPUNIT_ASSERT(uint64_t(0) == dirSize);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // Attempting to get the size of a non-existing directory
    {
        uint64_t dirSize = 0u;
        IoError ioError = IoError::Success;

        const LocalTemporaryDirectory temporaryDirectory;
        CPPUNIT_ASSERT(IoHelper::getDirectorySize(temporaryDirectory.path() / "does_not_exist", dirSize, ioError));
        CPPUNIT_ASSERT(uint64_t(0) == dirSize);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::NoSuchFileOrDirectory),
                                     IoError::NoSuchFileOrDirectory, ioError);
    }
}

} // namespace KDC
