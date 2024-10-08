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

using namespace CppUnit;

namespace KDC {

void TestIo::testCreateJunction() {
    // A Windows junction on a regular target directory.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const SyncPath path = temporaryDirectory.path() / "regular_dir_junction";

        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(_testObj->createJunctionFromPath(targetPath, path, ioError));
        CPPUNIT_ASSERT(ioError == IoError::Success);

        bool isJunction = false;
        CPPUNIT_ASSERT(_testObj->checkIfIsJunction(path, isJunction, ioError));
        CPPUNIT_ASSERT(isJunction);

        NodeId nodeId;
        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(!nodeId.empty());

        ItemType itemType;
        CPPUNIT_ASSERT(_testObj->getItemType(path, itemType));
        CPPUNIT_ASSERT(itemType.linkType == LinkType::Junction);
    }

    // A Windows junction on a non-existing target directory: no error expected
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "non_existing_dir"; // It doesn't exist.
        const SyncPath path = temporaryDirectory.path() / "dir_junction";

        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(_testObj->createJunctionFromPath(targetPath, path, ioError));
        CPPUNIT_ASSERT(ioError == IoError::Success);

        bool isJunction = false;
        CPPUNIT_ASSERT(_testObj->checkIfIsJunction(path, isJunction, ioError));
        CPPUNIT_ASSERT(isJunction);
    }

    // A Windows junction on a regular target file: no error expected
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = temporaryDirectory.path() / "dir_junction";

        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(_testObj->createJunctionFromPath(targetPath, path, ioError));
        CPPUNIT_ASSERT(ioError == IoError::Success);

        bool isJunction = false;
        CPPUNIT_ASSERT(_testObj->checkIfIsJunction(path, isJunction, ioError));
        CPPUNIT_ASSERT(isJunction);
    }

    // The junction name is very long: failure with ad hoc error
    {
        const std::string veryLongfileName(1000, 'a'); // Exceeds the max allowed name length on every file system of interest.
        const SyncPath path = _localTestDirPath / veryLongfileName; // This file doesn't exist.
        const SyncPath targetPath = _localTestDirPath / "test_pictures";

        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(!_testObj->createJunctionFromPath(targetPath, path, ioError));
        CPPUNIT_ASSERT(ioError == IoError::NoSuchFileOrDirectory);
    }

    // The junction path is very long: failure with ad hoc error
    {
        const std::string pathSegment(50, 'a');
        SyncPath path = _localTestDirPath;
        for (int i = 0; i < 1000; ++i) {
            path /= pathSegment; // Eventually exceeds the max allowed path length on every file system of interest.
        }
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        IoError ioError = IoError::Unknown;

        CPPUNIT_ASSERT(!_testObj->createJunctionFromPath(targetPath, path, ioError));
        CPPUNIT_ASSERT(ioError == IoError::NoSuchFileOrDirectory);
    }
}

} // namespace KDC
