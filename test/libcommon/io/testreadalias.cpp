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


void TestIo::testReadAlias() {
    // A MacOSX Finder alias on a regular file.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = temporaryDirectory.path() / "regular_file_alias";
        IoError createAliasError = IoError::Unknown;
        CPPUNIT_ASSERT_MESSAGE(toString(createAliasError), IoHelper::createAliasFromPath(targetPath, path, createAliasError));


        IoError readAliasError = IoError::Unknown;
        std::string data;
        SyncPath actualTargetPath;
        CPPUNIT_ASSERT_MESSAGE(toString(readAliasError), IoHelper::readAlias(path, data, actualTargetPath, readAliasError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, readAliasError);
        CPPUNIT_ASSERT_EQUAL(targetPath, actualTargetPath);
        CPPUNIT_ASSERT(!data.empty());
    }

    // A MacOSX Finder alias on a regular directory.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const SyncPath path = temporaryDirectory.path() / "regular_dir_alias";

        IoError createAliasError = IoError::Unknown;
        CPPUNIT_ASSERT_MESSAGE(toString(createAliasError), IoHelper::createAliasFromPath(targetPath, path, createAliasError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, createAliasError);

        IoError readAliasError = IoError::Unknown;
        std::string data;
        SyncPath actualTargetPath;
        CPPUNIT_ASSERT_MESSAGE(toString(readAliasError), IoHelper::readAlias(path, data, actualTargetPath, readAliasError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, readAliasError);
        CPPUNIT_ASSERT_EQUAL(targetPath, actualTargetPath);
        CPPUNIT_ASSERT(!data.empty());
    }

    // The alias file does not exist: success with empty data, empty target path and IoError::NoSuchFileOrDirectory output error.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "non-existing.jpg"; // This file does not exist.
        const SyncPath path = temporaryDirectory.path() / "non_existing_dir_alias"; // This file does not exist.

        IoError readAliasError = IoError::Unknown;
        std::string data;
        SyncPath actualTargetPath;
        CPPUNIT_ASSERT_MESSAGE(toString(readAliasError), IoHelper::readAlias(path, data, actualTargetPath, readAliasError));
        CPPUNIT_ASSERT_EQUAL(IoError::NoSuchFileOrDirectory, readAliasError);
        CPPUNIT_ASSERT_EQUAL(SyncPath{}, actualTargetPath);
        CPPUNIT_ASSERT_EQUAL(std::string{}, data);
    }


    // The alias path is the target path (of an existing file): success
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "file.txt";
        { std::ofstream ofs(path); }
        const SyncPath targetPath = path;

        IoError createAliasError = IoError::Unknown;
        CPPUNIT_ASSERT_MESSAGE(toString(createAliasError), IoHelper::createAliasFromPath(targetPath, path, createAliasError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, createAliasError);

        IoError readAliasError = IoError::Unknown;
        std::string data;
        SyncPath actualTargetPath;
        CPPUNIT_ASSERT_MESSAGE(toString(readAliasError), IoHelper::readAlias(path, data, actualTargetPath, readAliasError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, readAliasError);
        CPPUNIT_ASSERT_EQUAL(targetPath, actualTargetPath);
        CPPUNIT_ASSERT(!data.empty());
    }

    // The alias file name is very long: failure
    {
        const std::string veryLongfileName(1000,
                                           'a'); // Exceeds the max allowed name length on every file system of interest.
        const SyncPath path = _localTestDirPath / veryLongfileName; // This file doesn't exist.
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";

        IoError readAliasError = IoError::Unknown;
        std::string data;
        SyncPath actualTargetPath;
        CPPUNIT_ASSERT_MESSAGE(toString(readAliasError), IoHelper::readAlias(path, data, actualTargetPath, readAliasError));
        CPPUNIT_ASSERT_EQUAL(IoError::InvalidFileName, readAliasError);
        CPPUNIT_ASSERT_EQUAL(SyncPath{}, actualTargetPath);
        CPPUNIT_ASSERT(data.empty());
    }

    // The alias file path is very long: failure
    {
        const std::string pathSegment(50, 'a');
        const SyncPath path = makeVeryLonPath(_localTestDirPath);
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";

        IoError readAliasError = IoError::Unknown;
        std::string data;
        SyncPath actualTargetPath;
        CPPUNIT_ASSERT_MESSAGE(toString(readAliasError), IoHelper::readAlias(path, data, actualTargetPath, readAliasError));
        CPPUNIT_ASSERT_EQUAL(IoError::InvalidFileName, readAliasError);
        CPPUNIT_ASSERT_EQUAL(SyncPath{}, actualTargetPath);
        CPPUNIT_ASSERT(data.empty());
    }

    // The alias file name contains emojis: success
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / makeFileNameWithEmojis();
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";

        IoError aliasError = IoError::Unknown;
        CPPUNIT_ASSERT_MESSAGE(toString(aliasError), IoHelper::createAliasFromPath(targetPath, path, aliasError));

        IoError readAliasError = IoError::Unknown;
        std::string data;
        SyncPath actualTargetPath;
        CPPUNIT_ASSERT_MESSAGE(toString(readAliasError), IoHelper::readAlias(path, data, actualTargetPath, readAliasError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, readAliasError);
        CPPUNIT_ASSERT_EQUAL(targetPath, actualTargetPath);
        CPPUNIT_ASSERT(!data.empty());
    }
}


} // namespace KDC
