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

void TestIo::testGetRights() {
    // Getting the rights of a regular symbolic link on a file
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path() / "regular_file";
        const SyncPath path = temporaryDirectory.path() / "regular_file_symbolic_link";

        { std::ofstream ofs(targetPath); }
        std::error_code ec;
        CPPUNIT_ASSERT(std::filesystem::exists(targetPath, ec) && ec.value() == 0);

        std::filesystem::create_symlink(targetPath, path, ec);
        CPPUNIT_ASSERT_MESSAGE(ec.message(), ec.value() == 0);

        bool readPermission = false;
        bool writePermission = false;
        bool execPermission = false;
        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError),
                               IoHelper::getRights(path, readPermission, writePermission, execPermission, ioError));
        CPPUNIT_ASSERT(readPermission == true);
        CPPUNIT_ASSERT(writePermission == true);
        CPPUNIT_ASSERT(execPermission == true);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // Getting the rights of a dangling symbolic link on a file
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path() / "dummy";
        const SyncPath path = temporaryDirectory.path() / "dangling_file_symbolic_link";

        std::error_code ec;
        CPPUNIT_ASSERT(!std::filesystem::exists(targetPath, ec));

        std::filesystem::create_symlink(targetPath, path, ec);
        CPPUNIT_ASSERT_MESSAGE(ec.message(), ec.value() == 0);

        bool readPermission = false;
        bool writePermission = false;
        bool execPermission = false;
        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError),
                               IoHelper::getRights(path, readPermission, writePermission, execPermission, ioError));
        CPPUNIT_ASSERT(readPermission == true);
        CPPUNIT_ASSERT(writePermission == true);
        CPPUNIT_ASSERT(execPermission == true);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // Getting the rights of a regular symbolic link on a folder
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path() / "regular_dir";
        const SyncPath path = temporaryDirectory.path() / "regular_dir_symbolic_link";

        std::error_code ec;
        CPPUNIT_ASSERT(std::filesystem::create_directory(targetPath, ec) && ec.value() == 0);
        CPPUNIT_ASSERT(std::filesystem::exists(targetPath, ec));

        std::filesystem::create_directory_symlink(targetPath, path, ec);
        CPPUNIT_ASSERT_MESSAGE(ec.message(), ec.value() == 0);

        bool readPermission = false;
        bool writePermission = false;
        bool execPermission = false;
        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError),
                               IoHelper::getRights(path, readPermission, writePermission, execPermission, ioError));
        CPPUNIT_ASSERT(readPermission == true);
        CPPUNIT_ASSERT(writePermission == true);
        CPPUNIT_ASSERT(execPermission == true);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // Getting the rights of a dangling symbolic link on a folder
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path() / "dummy";
        const SyncPath path = temporaryDirectory.path() / "dangling_dir_symbolic_link";

        std::error_code ec;
        CPPUNIT_ASSERT(!std::filesystem::exists(targetPath, ec));

        std::filesystem::create_directory_symlink(targetPath, path, ec);
        CPPUNIT_ASSERT_MESSAGE(ec.message(), ec.value() == 0);

        bool readPermission = false;
        bool writePermission = false;
        bool execPermission = false;
        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError),
                               IoHelper::getRights(path, readPermission, writePermission, execPermission, ioError));
        CPPUNIT_ASSERT(readPermission == true);
        CPPUNIT_ASSERT(writePermission == true);
        CPPUNIT_ASSERT(execPermission == true);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

#if defined(KD_MACOS)
    // Getting the rights of a MacOSX Finder alias on a regular file.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path() / "regular_file";
        const SyncPath path = temporaryDirectory.path() / "regular_file_alias";

        { std::ofstream ofs(targetPath); }

        std::error_code ec;
        CPPUNIT_ASSERT(std::filesystem::exists(targetPath, ec) && ec.value() == 0);

        IoError aliasError;
        CPPUNIT_ASSERT(IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoError::Success);

        bool readPermission = false;
        bool writePermission = false;
        bool execPermission = false;
        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError),
                               IoHelper::getRights(path, readPermission, writePermission, execPermission, ioError));
        CPPUNIT_ASSERT(readPermission == true);
        CPPUNIT_ASSERT(writePermission == true);
        CPPUNIT_ASSERT(execPermission == false);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // Getting the rights of a MacOSX Finder alias on a non existing file.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path() / "dummy";
        const SyncPath path = temporaryDirectory.path() / "dangling_file_alias";

        { std::ofstream ofs(targetPath); }

        IoError aliasError;
        CPPUNIT_ASSERT(IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoError::Success);

        std::error_code ec;
        CPPUNIT_ASSERT(std::filesystem::remove(targetPath, ec) && ec.value() == 0);
        CPPUNIT_ASSERT(!std::filesystem::exists(targetPath, ec));

        bool readPermission = false;
        bool writePermission = false;
        bool execPermission = false;
        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError),
                               IoHelper::getRights(path, readPermission, writePermission, execPermission, ioError));
        CPPUNIT_ASSERT(readPermission == true);
        CPPUNIT_ASSERT(writePermission == true);
        CPPUNIT_ASSERT(execPermission == false);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // Getting the rights of a MacOSX Finder alias on a regular directory.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path() / "regular_dir";
        const SyncPath path = temporaryDirectory.path() / "regular_dir_alias";

        std::error_code ec;
        CPPUNIT_ASSERT(std::filesystem::create_directory(targetPath, ec) && ec.value() == 0);
        CPPUNIT_ASSERT(std::filesystem::exists(targetPath, ec) && ec.value() == 0);

        IoError aliasError;
        CPPUNIT_ASSERT(IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoError::Success);

        bool readPermission = false;
        bool writePermission = false;
        bool execPermission = false;
        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError),
                               IoHelper::getRights(path, readPermission, writePermission, execPermission, ioError));
        CPPUNIT_ASSERT(readPermission == true);
        CPPUNIT_ASSERT(writePermission == true);
        CPPUNIT_ASSERT(execPermission == false);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // Getting the rights of a MacOSX Finder alias on a non existing directory.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path() / "dummy";
        const SyncPath path = temporaryDirectory.path() / "dummy_dir_alias";

        std::error_code ec;
        CPPUNIT_ASSERT(std::filesystem::create_directory(targetPath, ec) && ec.value() == 0);

        IoError aliasError;
        CPPUNIT_ASSERT(IoHelper::createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoError::Success);

        CPPUNIT_ASSERT(std::filesystem::remove(targetPath, ec) && ec.value() == 0);
        CPPUNIT_ASSERT(!std::filesystem::exists(targetPath, ec));

        bool readPermission = false;
        bool writePermission = false;
        bool execPermission = false;
        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError),
                               IoHelper::getRights(path, readPermission, writePermission, execPermission, ioError));
        CPPUNIT_ASSERT(readPermission == true);
        CPPUNIT_ASSERT(writePermission == true);
        CPPUNIT_ASSERT(execPermission == false);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }
#elif defined(KD_WINDOWS)
    // Getting the rights of a junction on a regular folder.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path() / "regular_dir";
        const SyncPath path = temporaryDirectory.path() / "dangling_junction";

        std::error_code ec;
        CPPUNIT_ASSERT(std::filesystem::create_directory(targetPath, ec) && ec.value() == 0);
        CPPUNIT_ASSERT(std::filesystem::exists(targetPath, ec) && ec.value() == 0);

        IoError junctionError{IoError::Unknown};
        CPPUNIT_ASSERT(IoHelper::createJunctionFromPath(targetPath, path, junctionError));
        CPPUNIT_ASSERT(junctionError == IoError::Success);

        bool readPermission = false;
        bool writePermission = false;
        bool execPermission = false;
        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError),
                               IoHelper::getRights(path, readPermission, writePermission, execPermission, ioError));
        CPPUNIT_ASSERT(readPermission == true);
        CPPUNIT_ASSERT(writePermission == true);
        CPPUNIT_ASSERT(execPermission == true);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // Getting the rights of a junction on a non existing folder.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path() / "dummy";
        const SyncPath path = temporaryDirectory.path() / "dangling_junction";

        std::error_code ec;
        CPPUNIT_ASSERT(!std::filesystem::exists(targetPath, ec));

        IoError junctionError{IoError::Unknown};
        CPPUNIT_ASSERT(IoHelper::createJunctionFromPath(targetPath, path, junctionError));
        CPPUNIT_ASSERT(junctionError == IoError::Success);

        bool readPermission = false;
        bool writePermission = false;
        bool execPermission = false;
        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError),
                               IoHelper::getRights(path, readPermission, writePermission, execPermission, ioError));
        CPPUNIT_ASSERT(readPermission == true);
        CPPUNIT_ASSERT(writePermission == true);
        CPPUNIT_ASSERT(execPermission == true);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }
#endif
}

} // namespace KDC
