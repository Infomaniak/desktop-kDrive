/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#ifdef _WIN32
#include <windows.h>
#endif

using namespace CppUnit;

namespace KDC {

void TestIo::testSetXAttrValue() {
#ifdef __APPLE__
    // A regular file
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
            ofs.close();
        }
        IoError ioError = IoErrorSuccess;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, "status", "corrupted", ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        std::string value;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(value == "corrupted");

        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, "status", "sound", ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(value == "sound");
    }

    // A non-existing file
    {
        const SyncPath path = _localTestDirPath / "non-existing.jpg";  // This file does not exist.
        IoError ioError = IoErrorSuccess;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, "status", "on-fire", ioError));
        CPPUNIT_ASSERT(ioError == IoErrorNoSuchFileOrDirectory);
    }

    // A non-existing file with a very long name
    {
        const std::string veryLongfileName(1000, 'a');  // Exceeds the max allowed name length on every file system of interest.
        const SyncPath path = _localTestDirPath / veryLongfileName;  // This file doesn't exist.
        IoError ioError = IoErrorSuccess;
        CPPUNIT_ASSERT(!_testObj->setXAttrValue(path, "status", "water-proof", ioError));
        CPPUNIT_ASSERT(ioError == IoErrorFileNameTooLong);
    }

    // A regular file missing owner write permission: access denied expected
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "permission_less_file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
        }
        std::filesystem::permissions(path, std::filesystem::perms::owner_write, std::filesystem::perm_options::remove);

        IoError ioError = IoErrorSuccess;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, "status", "locked", ioError));

        std::filesystem::permissions(path, std::filesystem::perms::owner_read, std::filesystem::perm_options::add);

        CPPUNIT_ASSERT(ioError == IoErrorAccessDenied);

        std::string value;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorAttrNotFound);
        CPPUNIT_ASSERT(value.empty());
    }
#endif

#ifdef _WIN32
    // A regular file
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "file.txt";
        {
            std::ofstream ofs(path);
            ofs.close();
        }
        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, FILE_ATTRIBUTE_HIDDEN, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        bool value = false;
        ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, FILE_ATTRIBUTE_HIDDEN, value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(value);

        ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, FILE_ATTRIBUTE_NORMAL, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, FILE_ATTRIBUTE_HIDDEN, value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(!value);
    }

    // A non-existing file
    {
        const SyncPath path = _localTestDirPath / "non-existing.jpg";  // This file does not exist.
        IoError ioError = IoErrorSuccess;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, FILE_ATTRIBUTE_NORMAL, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorNoSuchFileOrDirectory);
    }
#endif
}

}  // namespace KDC
