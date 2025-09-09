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

#if defined(KD_WINDOWS)
#include <windows.h>
#endif

using namespace CppUnit;

namespace KDC {

void TestIo::testCheckIfFileIsDehydrated() {
    // A regular file without any extended attribute
    {
        const SyncPath path = _localTestDirPath / "test_pictures/picture-1.jpg";
        IoError ioError = IoError::Unknown;
        bool isDehydrated = false;
        CPPUNIT_ASSERT(IoHelper::checkIfFileIsDehydrated(path, isDehydrated, ioError));
#if defined(KD_MACOS)
        CPPUNIT_ASSERT_EQUAL(IoError::AttrNotFound, ioError);
#elif defined(KD_LINUX) || defined(KD_WINDOWS)
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(IoError::Success) + "!=" + toString(ioError), IoError::Success, ioError);
#endif
        CPPUNIT_ASSERT(!isDehydrated);
    }
#if defined(KD_MACOS)
    // A dehydrated file
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "dehydrated_file";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
        }

        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::setDehydratedPlaceholderStatus(path, ioError));
        bool isDehydrated = false;
        CPPUNIT_ASSERT(IoHelper::checkIfFileIsDehydrated(path, isDehydrated, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(isDehydrated);
    }
    // A hydrated file
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "hydrated_file";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
        }

        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::setXAttrValue(path, "com.infomaniak.drive.desktopclient.litesync.status", "F", ioError));
        bool isDehydrated = true;
        CPPUNIT_ASSERT(IoHelper::checkIfFileIsDehydrated(path, isDehydrated, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(!isDehydrated);
    }
#endif

#if defined(KD_WINDOWS)
    // A dehydrated file
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "dehydrated_file";
        { std::ofstream ofs(path); }

        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::setDehydratedPlaceholderStatus(path, ioError));
        bool isDehydrated = false;
        CPPUNIT_ASSERT(IoHelper::checkIfFileIsDehydrated(path, isDehydrated, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(isDehydrated);
    }
    // A hydrated file
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "hydrated_file";
        { std::ofstream ofs(path); }

        IoError ioError = IoError::Unknown;
        bool isDehydrated = true;
        CPPUNIT_ASSERT(IoHelper::checkIfFileIsDehydrated(path, isDehydrated, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(!isDehydrated);
    }
#endif
    // A non-existing file
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "non_existing_file.txt";

        IoError ioError = IoError::Success;
        bool isDehydrated = true;
        CPPUNIT_ASSERT(IoHelper::checkIfFileIsDehydrated(path, isDehydrated, ioError));
#if defined(KD_LINUX)
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
#else
        CPPUNIT_ASSERT_EQUAL(IoError::NoSuchFileOrDirectory, ioError);
#endif
        CPPUNIT_ASSERT(!isDehydrated);
    }

    // A file missing owner read permission
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "permission_less_file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
        }
        std::filesystem::permissions(path, std::filesystem::perms::owner_read, std::filesystem::perm_options::remove);

        IoError ioError = IoError::Unknown;
        bool isDehydrated = true;
        CPPUNIT_ASSERT(IoHelper::checkIfFileIsDehydrated(path, isDehydrated, ioError));

        std::filesystem::permissions(path, std::filesystem::perms::owner_read, std::filesystem::perm_options::add);

#if defined(KD_MACOS)
        CPPUNIT_ASSERT(ioError == IoError::AccessDenied);
#else
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
#endif
        CPPUNIT_ASSERT(!isDehydrated);
    }
}

} // namespace KDC
