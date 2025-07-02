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

void TestIo::testGetXAttrValue() {
#if defined(KD_MACOS)
    // A regular file without any extended attributes
    {
        const SyncPath path = _localTestDirPath / "test_pictures/picture-1.jpg";
        IoError ioError = IoError::Success;
        std::string value;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoError::AttrNotFound);
        CPPUNIT_ASSERT(value.empty());
    }

    // A regular directory without any extended attributes
    {
        const SyncPath path = _localTestDirPath / "test_pictures";
        IoError ioError = IoError::Success;
        std::string value;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoError::AttrNotFound);
        CPPUNIT_ASSERT(value.empty());
    }

    // A regular symbolic link on a file without any extended attributes
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "regular_file_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        IoError ioError = IoError::Success;
        std::string value;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoError::AttrNotFound);
        CPPUNIT_ASSERT(value.empty());
    }

    // A non-existing file
    {
        const SyncPath path = _localTestDirPath / "non-existing.jpg"; // This file does not exist.
        IoError ioError = IoError::Success;
        std::string value;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoError::NoSuchFileOrDirectory);
        CPPUNIT_ASSERT(value.empty());
    }

    // A non-existing file with a very long name
    {
        const std::string veryLongfileName(1000, 'a'); // Exceeds the max allowed name length on every file system of interest.
        const SyncPath path = _localTestDirPath / veryLongfileName; // This file doesn't exist.
        std::string value;
        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(!IoHelper::getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoError::FileNameTooLong);
        CPPUNIT_ASSERT(value.empty());
    }

    // A regular file missing owner read permission and without extended attribute: access denied expected
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "permission_less_file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
        }
        std::filesystem::permissions(path, std::filesystem::perms::owner_read, std::filesystem::perm_options::remove);

        std::string value;
        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, "status", value, ioError));

        std::filesystem::permissions(path, std::filesystem::perms::owner_read, std::filesystem::perm_options::add);
        CPPUNIT_ASSERT(ioError == IoError::AccessDenied);
        CPPUNIT_ASSERT(value.empty());
    }

    // An existing file with an extended attribute
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
        }

        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(IoHelper::setXAttrValue(path, "status", "sugar-free", ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        std::string value;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(value == "sugar-free");
    }

    // An existing directory with an extended attribute
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path();

        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(IoHelper::setXAttrValue(path, "status", "super-dry", ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        std::string value;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(value == "super-dry");
    }

    // A regular symbolic link on a file, with an extended attribute set for the link
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "regular_file_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(IoHelper::setXAttrValue(path, "status", "regular-file-symlink", ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        std::string value;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(value == "regular-file-symlink");

        CPPUNIT_ASSERT(IoHelper::getXAttrValue(targetPath, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoError::AttrNotFound);
        CPPUNIT_ASSERT(value.empty());
    }

    // A regular symbolic link on a folder, with an extended attribute for the link
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "regular_dir_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(IoHelper::setXAttrValue(path, "status", "regular-dir-symlink", ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        std::string value;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(value == "regular-dir-symlink");

        CPPUNIT_ASSERT(IoHelper::getXAttrValue(targetPath, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoError::AttrNotFound);
        CPPUNIT_ASSERT(value.empty());
    }

    // A dangling symbolic link on a file, with an extended attribute set for the link
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path() / "non_existing_test_file.txt"; // This file does not exist.
        const SyncPath path = temporaryDirectory.path() / "dangling_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(IoHelper::setXAttrValue(path, "status", "dangling-symbolic-link", ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        std::string value;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(value == "dangling-symbolic-link");
    }

    // A MacOSX Finder alias on a regular file, with an extended attribute set for the alias.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = temporaryDirectory.path() / "regular_file_alias";

        IoError aliasError;
        CPPUNIT_ASSERT_MESSAGE(toString(aliasError), IoHelper::createAliasFromPath(targetPath, path, aliasError));

        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(IoHelper::setXAttrValue(path, "status", "sane-alias", ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        std::string value;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(value == "sane-alias");

        CPPUNIT_ASSERT(IoHelper::getXAttrValue(targetPath, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoError::AttrNotFound);
        CPPUNIT_ASSERT(value.empty());
    }

    // An existing file with an extended attribute
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
        }

        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(IoHelper::setXAttrValue(path, "status", "sugar-free", ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        std::string value;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(value == "sugar-free");
    }

    // An existing directory with an extended attribute
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path();

        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(IoHelper::setXAttrValue(path, "status", "super-dry", ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        std::string value;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(value == "super-dry");
    }

    // A regular symbolic link on a file, with an extended attribute set for the link
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "regular_file_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(IoHelper::setXAttrValue(path, "status", "regular-file-symlink", ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        std::string value;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(value == "regular-file-symlink");

        CPPUNIT_ASSERT(IoHelper::getXAttrValue(targetPath, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoError::AttrNotFound);
        CPPUNIT_ASSERT(value.empty());
    }

    // A regular symbolic link on a folder, with an extended attribute for the link
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "regular_dir_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(IoHelper::setXAttrValue(path, "status", "regular-dir-symlink", ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        std::string value;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(value == "regular-dir-symlink");

        CPPUNIT_ASSERT(IoHelper::getXAttrValue(targetPath, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoError::AttrNotFound);
        CPPUNIT_ASSERT(value.empty());
    }

    // A dangling symbolic link on a file, with an extended attribute set for the link
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path() / "non_existing_test_file.txt"; // This file does not exist.
        const SyncPath path = temporaryDirectory.path() / "dangling_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(IoHelper::setXAttrValue(path, "status", "dangling-symbolic-link", ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        std::string value;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(value == "dangling-symbolic-link");
    }

    // A MacOSX Finder alias on a regular file, with an extended attribute set for the alias.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = temporaryDirectory.path() / "regular_file_alias";

        IoError aliasError;
        CPPUNIT_ASSERT_MESSAGE(toString(aliasError), IoHelper::createAliasFromPath(targetPath, path, aliasError));

        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(IoHelper::setXAttrValue(path, "status", "sane-alias", ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        std::string value;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(value == "sane-alias");

        CPPUNIT_ASSERT(IoHelper::getXAttrValue(targetPath, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoError::AttrNotFound);
        CPPUNIT_ASSERT(value.empty());
    }
#elif defined(KD_WINDOWS)
    // A regular file
    {
        const SyncPath path = _localTestDirPath / "test_pictures/picture-1.jpg";
        IoError ioError = IoError::Unknown;
        bool value = true;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, FILE_ATTRIBUTE_OFFLINE, value, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(!value);

        ioError = IoError::Unknown;
        value = true;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, FILE_ATTRIBUTE_DIRECTORY, value, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(!value);

        ioError = IoError::Unknown;
        value = true;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, FILE_ATTRIBUTE_NORMAL, value, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(!value);
    }

    // A regular directory
    {
        const SyncPath path = _localTestDirPath / "test_pictures";
        IoError ioError = IoError::Unknown;
        bool value = true;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, FILE_ATTRIBUTE_OFFLINE, value, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(!value);

        ioError = IoError::Unknown;
        value = false;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, FILE_ATTRIBUTE_DIRECTORY, value, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(value);
    }

    // A regular symbolic link on a file without any extended attributes
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "regular_file_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        IoError ioError = IoError::Success;
        bool value = true;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, FILE_ATTRIBUTE_OFFLINE, value, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(!value);
    }

    // A non-existing file
    {
        const SyncPath path = _localTestDirPath / "non-existing.jpg"; // This file does not exist.
        IoError ioError = IoError::Success;
        bool value = true;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, FILE_ATTRIBUTE_OFFLINE, value, ioError));
        CPPUNIT_ASSERT(ioError == IoError::NoSuchFileOrDirectory);
        CPPUNIT_ASSERT(!value);
    }

    // A non-existing file with a very long name
    {
        const std::string veryLongfileName(1000, 'a'); // Exceeds the max allowed name length on every file system of interest.
        const SyncPath path = _localTestDirPath / veryLongfileName; // This file doesn't exist.
        bool value = true;
        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, FILE_ATTRIBUTE_OFFLINE, value, ioError));
        CPPUNIT_ASSERT(ioError == IoError::NoSuchFileOrDirectory);
        CPPUNIT_ASSERT(!value);
    }

    // A regular file missing owner read permission and without extended attribute: no error expected
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "permission_less_file.txt";
        { std::ofstream ofs(path); }
        std::filesystem::permissions(path, std::filesystem::perms::owner_read, std::filesystem::perm_options::remove);

        bool value = true;
        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, FILE_ATTRIBUTE_OFFLINE, value, ioError));

        std::filesystem::permissions(path, std::filesystem::perms::owner_read, std::filesystem::perm_options::add);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(!value);
    }

    // An existing file with an extended attribute
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "file.txt";
        { std::ofstream ofs(path); }

        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::setXAttrValue(path, FILE_ATTRIBUTE_OFFLINE, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        bool value = false;
        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, FILE_ATTRIBUTE_OFFLINE, value, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(value);
    }

    // An existing directory with an extended attribute
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path();

        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::setXAttrValue(path, FILE_ATTRIBUTE_OFFLINE, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        bool value = false;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, FILE_ATTRIBUTE_OFFLINE, value, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(value);
    }

    // A regular symbolic link on a file, with an extended attribute set for the link
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "regular_file_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(IoHelper::setXAttrValue(path, FILE_ATTRIBUTE_READONLY, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        bool value = false;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, FILE_ATTRIBUTE_READONLY, value, ioError));

        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(value);

        CPPUNIT_ASSERT(IoHelper::getXAttrValue(targetPath, FILE_ATTRIBUTE_READONLY, value, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(!value);

        // Restore permission to allow automatic removal of the temporary directory
        IoHelper::setXAttrValue(path, FILE_ATTRIBUTE_NORMAL, ioError);
    }

    // A regular symbolic link on a folder, with an extended attribute for the link
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "regular_dir_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::setXAttrValue(path, FILE_ATTRIBUTE_HIDDEN, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        bool value = false;
        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, FILE_ATTRIBUTE_HIDDEN, value, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(value);

        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(targetPath, FILE_ATTRIBUTE_HIDDEN, value, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(!value);

        // Restore permission to allow automatic removal of the temporary directory
        IoHelper::setXAttrValue(path, FILE_ATTRIBUTE_NORMAL, ioError);
    }

    // A dangling symbolic link on a file, with an extended attribute set for the link
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path() / "non_existing_test_file.txt"; // This file does not exist.
        const SyncPath path = temporaryDirectory.path() / "dangling_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::setXAttrValue(path, FILE_ATTRIBUTE_OFFLINE, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        bool value = false;
        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::getXAttrValue(path, FILE_ATTRIBUTE_OFFLINE, value, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(value);
    }
#endif
}
} // namespace KDC
