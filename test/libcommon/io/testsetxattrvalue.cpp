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

void TestIo::testSetXAttrValue() {
#if defined(KD_MACOS)
    // A regular file
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
            ofs.close();
        }
        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, "status", "corrupted", ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        std::string value;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(value == "corrupted");

        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, "status", "sound", ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(value == "sound");
    }

    // A non-existing file
    {
        const SyncPath path = _localTestDirPath / "non-existing.jpg"; // This file does not exist.
        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, "status", "on-fire", ioError));
        CPPUNIT_ASSERT(ioError == IoError::NoSuchFileOrDirectory);
    }

    // A non-existing file with a very long name
    {
        const std::string veryLongfileName(1000, 'a'); // Exceeds the max allowed name length on every file system of interest.
        const SyncPath path = _localTestDirPath / veryLongfileName; // This file doesn't exist.
        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(!_testObj->setXAttrValue(path, "status", "water-proof", ioError));
        CPPUNIT_ASSERT(ioError == IoError::FileNameTooLong);
    }

    // A regular file missing owner write permission: access denied expected
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "permission_less_file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
        }
        std::filesystem::permissions(path, std::filesystem::perms::owner_write, std::filesystem::perm_options::remove);

        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, "status", "locked", ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError); // `setXAttrValue` grant write permissions temporarily

        std::string value;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        CPPUNIT_ASSERT_EQUAL(std::string("locked"), value);
    }
#endif

#if defined(KD_WINDOWS)
    // A regular file
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "file.txt";
        {
            std::ofstream ofs(path);
            ofs.close();
        }
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, FILE_ATTRIBUTE_HIDDEN, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        bool value = false;
        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, FILE_ATTRIBUTE_HIDDEN, value, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(value);

        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, FILE_ATTRIBUTE_NORMAL, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, FILE_ATTRIBUTE_HIDDEN, value, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(!value);
    }

    // A non-existing file
    {
        const SyncPath path = _localTestDirPath / "non-existing.jpg"; // This file does not exist.
        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, FILE_ATTRIBUTE_NORMAL, ioError));
        CPPUNIT_ASSERT(ioError == IoError::NoSuchFileOrDirectory);
    }
#endif
}

} // namespace KDC
