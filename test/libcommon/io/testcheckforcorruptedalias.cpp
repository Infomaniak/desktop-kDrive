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

#include <filesystem>

using namespace CppUnit;

namespace KDC {


void TestIo::testCheckForCorruptedAlias(void) {
    // A regular file.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "regular_file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
        }

        auto ioError = IoError::Unknown;
        auto isCorruptedAlias = false;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::checkForCorruptedAlias(path, isCorruptedAlias, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        CPPUNIT_ASSERT(!isCorruptedAlias);

        // Remove exec rights on the file parent
        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::setRights(temporaryDirectory.path(), true, true, false, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

        ioError = IoError::Unknown;
        isCorruptedAlias = false;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::checkForCorruptedAlias(path, isCorruptedAlias, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::AccessDenied, ioError);

        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::setRights(temporaryDirectory.path(), true, true, true, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

        // Delete the file
        std::error_code ec;
        CPPUNIT_ASSERT(std::filesystem::remove(path, ec));
        CPPUNIT_ASSERT(!ec);

        CPPUNIT_ASSERT(!std::filesystem::exists(path, ec));
        CPPUNIT_ASSERT(!ec);

        ioError = IoError::Unknown;
        isCorruptedAlias = false;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::checkForCorruptedAlias(path, isCorruptedAlias, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::NoSuchFileOrDirectory, ioError);
    }

    // A symlink.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "regular_symlink.jpg";
        const SyncPath targetPath = temporaryDirectory.path() / "dummy.txt";
        {
            std::ofstream ofs(targetPath);
            ofs << "Some content.\n";
        }

        auto ioError = IoError::Unknown;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::createSymlink(targetPath, path, false, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

        ioError = IoError::Unknown;
        auto isCorruptedAlias = false;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::checkForCorruptedAlias(path, isCorruptedAlias, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        CPPUNIT_ASSERT(!isCorruptedAlias);

        // Delete the target
        std::error_code ec;
        CPPUNIT_ASSERT(std::filesystem::remove(targetPath, ec));
        CPPUNIT_ASSERT(!ec);

        CPPUNIT_ASSERT(!std::filesystem::exists(targetPath, ec));
        CPPUNIT_ASSERT(!ec);

        ioError = IoError::Unknown;
        isCorruptedAlias = false;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::checkForCorruptedAlias(path, isCorruptedAlias, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        CPPUNIT_ASSERT(!isCorruptedAlias);
    }

    // A MacOSX Finder alias.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "regular_alias.jpg";
        const SyncPath targetPath = temporaryDirectory.path() / "dummy.txt";
        {
            std::ofstream ofs(targetPath);
            ofs << "Some content.\n";
        }

        auto ioError = IoError::Unknown;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::createAliasFromPath(targetPath, path, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

        ioError = IoError::Unknown;
        auto isCorruptedAlias = false;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::checkForCorruptedAlias(path, isCorruptedAlias, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        CPPUNIT_ASSERT(!isCorruptedAlias);

        // Delete the target
        std::error_code ec;
        CPPUNIT_ASSERT(std::filesystem::remove(targetPath, ec));
        CPPUNIT_ASSERT(!ec);

        CPPUNIT_ASSERT(!std::filesystem::exists(targetPath, ec));
        CPPUNIT_ASSERT(!ec);

        ioError = IoError::Unknown;
        isCorruptedAlias = false;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::checkForCorruptedAlias(path, isCorruptedAlias, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        CPPUNIT_ASSERT(!isCorruptedAlias);

        // Corrupt the alias
        {
            std::ofstream ofs(path);
            ofs << "qwertz";
        }

        ioError = IoError::Unknown;
        isCorruptedAlias = false;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::checkForCorruptedAlias(path, isCorruptedAlias, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        CPPUNIT_ASSERT(isCorruptedAlias);
    }
}

} // namespace KDC
