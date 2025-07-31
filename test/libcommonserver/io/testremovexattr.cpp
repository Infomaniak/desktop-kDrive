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

void TestIo::testRemoveXAttr() {
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
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, "status1", "to-be-deleted", ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, "status2", "to-be-deleted", ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        CPPUNIT_ASSERT(_testObj->removeXAttrs(path, {"status1", "status2"}, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        std::string value;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, "status1", value, ioError));
        CPPUNIT_ASSERT(ioError == IoError::AttrNotFound);
        CPPUNIT_ASSERT(value.empty());

        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, "status2", value, ioError));
        CPPUNIT_ASSERT(ioError == IoError::AttrNotFound);
        CPPUNIT_ASSERT(value.empty());
    }

    // A symlink
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path() / "file.txt";
        {
            std::ofstream ofs(targetPath);
            ofs << "Some content.\n";
            ofs.close();
        }

        const SyncPath linkPath = temporaryDirectory.path() / "link.txt";
        std::error_code ec;
        std::filesystem::create_symlink(targetPath, linkPath, ec);
        CPPUNIT_ASSERT(!ec);

        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(linkPath, "link-status", "to-be-deleted", ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        CPPUNIT_ASSERT(_testObj->removeXAttrs(linkPath, {"link-status"}, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        std::string value;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(linkPath, "link-status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoError::AttrNotFound);
        CPPUNIT_ASSERT(value.empty());
    }

    // An alias
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path() / "file.txt";
        {
            std::ofstream ofs(targetPath);
            ofs << "Some content.\n";
            ofs.close();
        }

        const SyncPath aliasPath = temporaryDirectory.path() / "alias.txt";
        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::createAliasFromPath(targetPath, aliasPath, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        CPPUNIT_ASSERT(_testObj->setXAttrValue(aliasPath, "alias-status", "to-be-deleted", ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        CPPUNIT_ASSERT(_testObj->removeXAttrs(aliasPath, {"alias-status"}, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        std::string value;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(aliasPath, "alias-status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoError::AttrNotFound);
        CPPUNIT_ASSERT(value.empty());
    }

    // A non-existing file
    {
        const SyncPath path = _localTestDirPath / "non-existing.jpg"; // This file does not exist.
        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(_testObj->removeXAttrs(path, {"status"}, ioError));
        CPPUNIT_ASSERT(ioError == IoError::NoSuchFileOrDirectory);
    }

    // A regular file missing owner write permission: access denied expected
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "permission_less_file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
        }

        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, "status", "to-be-deleted", ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        std::filesystem::permissions(path, std::filesystem::perms::owner_write, std::filesystem::perm_options::remove);

        CPPUNIT_ASSERT(_testObj->removeXAttrs(path, {"status"}, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError); // `removeXAttrs` grant write permissions temporarily

        std::filesystem::permissions(path, std::filesystem::perms::owner_read, std::filesystem::perm_options::add);
    }
}

} // namespace KDC
