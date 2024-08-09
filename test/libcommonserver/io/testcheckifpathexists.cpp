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

void TestIo::testCheckIfPathExistsSimpleCases() {
    // A regular file
    {
        const SyncPath path = _localTestDirPath / "test_pictures/picture-1.jpg";
        bool exists = false;
        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A regular directory
    {
        const SyncPath path = _localTestDirPath / "test_pictures";
        bool exists = false;
        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A regular symbolic link on a file
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath path = temporaryDirectory.path() / "regular_file_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        bool exists = false;
        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A regular symbolic link on a folder
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath path = temporaryDirectory.path() / "regular_dir_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        bool exists = false;
        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A non-existing file
    {
        const SyncPath path = _localTestDirPath / "non_existing.jpg";
        bool exists = false;
        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(!exists);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A dangling symbolic link
    {
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath targetPath = temporaryDirectory.path() / "non_existing_test_file.txt";  // This file does not exist.
        const SyncPath path = temporaryDirectory.path() / "dangling_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        bool exists = false;
        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A regular file without read/write permission
    {
        LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath path = temporaryDirectory.path() / "test.txt";
        std::ofstream ofs(path);
        ofs << "Some content.\n";
        ofs.close();

        IoError ioError = IoErrorUnknown;
        const bool setRightResults = IoHelper::setRights(path, false, false, false, ioError) && ioError == IoErrorSuccess;
        if (!setRightResults) {
            IoHelper::setRights(path, true, true, true, ioError);
            CPPUNIT_FAIL("Failed to set rights on the file");
        }
        bool exists = false;
        const bool checkIfPathExistsResult = _testObj->checkIfPathExists(path, exists, ioError);
        IoHelper::setRights(path, true, true, true, ioError);

        CPPUNIT_ASSERT(checkIfPathExistsResult);
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // Checking existence of a subdirectory inside a directory that have been deleted and replaced with a file with the same name
    // ex: conversion of a bundle into a single file (macOS)
    {
        const SyncPath path = _localTestDirPath / "test_pictures" / "picture-1.jpg" / "A";
        bool exists = false;
        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(!exists);
        CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, ioError);
    }

#if defined(__APPLE__)
    // A MacOSX Finder alias on a regular file.
    {
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = temporaryDirectory.path() / "regular_file_alias";

        IoError aliasError;
        CPPUNIT_ASSERT(IoHelper::createAliasFromPath(targetPath, path, aliasError));

        bool exists = false;
        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A dangling MacOSX Finder alias on a non-existing file.
    {
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath targetPath = temporaryDirectory.path() / "file_to_be_deleted.png";  // This file will be deleted.
        const SyncPath path = temporaryDirectory.path() / "dangling_file_alias";
        {
            std::ofstream ofs(targetPath);
            ofs << "Some content.\n";
            ofs.close();
        }

        IoError aliasError;
        CPPUNIT_ASSERT(IoHelper::createAliasFromPath(targetPath, path, aliasError));

        std::filesystem::remove(targetPath);

        bool exists = false;
        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }
#endif

#if defined(_WIN32)
    // A Windows junction on a regular target directory.
    {
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const SyncPath path = temporaryDirectory.path() / "regular_dir_junction";

        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->createJunctionFromPath(targetPath, path, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        bool exists = false;
        CPPUNIT_ASSERT(_testObj->checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A Windows junction on a non-existing target directory.
    {
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath targetPath = _localTestDirPath / "non_existing_dir";  // It doesn't exist.
        const SyncPath path = temporaryDirectory.path() / "dir_junction";

        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->createJunctionFromPath(targetPath, path, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        bool exists = false;
        CPPUNIT_ASSERT(_testObj->checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A Windows junction on a regular target file.
    {
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = temporaryDirectory.path() / "dir_junction";

        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->createJunctionFromPath(targetPath, path, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        bool exists = false;

        CPPUNIT_ASSERT(_testObj->checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }
#endif
}

void TestIo::testCheckIfPathExists() {
    testCheckIfPathExistsSimpleCases();
    testCheckIfPathExistsWithSameNodeIdSimpleCases();
}

void TestIo::testCheckIfPathExistsWithSameNodeIdSimpleCases() {
    // A regular file
    {
        const SyncPath path = _localTestDirPath / "test_pictures/picture-1.jpg";
        bool existsWithSameId = false;
        NodeId otherNodeId;
        IoError ioError = IoErrorUnknown;
        NodeId nodeId;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(_testObj->checkIfPathExistsWithSameNodeId(path, nodeId, existsWithSameId, otherNodeId, ioError));
        CPPUNIT_ASSERT(existsWithSameId);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A regular directory
    {
        const SyncPath path = _localTestDirPath / "test_pictures";
        bool existsWithSameId = false;
        NodeId otherNodeId;
        IoError ioError = IoErrorUnknown;
        NodeId nodeId;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(_testObj->checkIfPathExistsWithSameNodeId(path, nodeId, existsWithSameId, otherNodeId, ioError));
        CPPUNIT_ASSERT(existsWithSameId);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A regular symbolic link on a file
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath path = temporaryDirectory.path() / "regular_file_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        bool existsWithSameId = false;
        NodeId otherNodeId;
        IoError ioError = IoErrorUnknown;
        NodeId nodeId;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(_testObj->checkIfPathExistsWithSameNodeId(path, nodeId, existsWithSameId, otherNodeId, ioError));
        CPPUNIT_ASSERT(existsWithSameId);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A regular symbolic link on a folder
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath path = temporaryDirectory.path() / "regular_dir_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        bool existsWithSameId = false;
        NodeId otherNodeId;
        IoError ioError = IoErrorUnknown;
        NodeId nodeId;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(_testObj->checkIfPathExistsWithSameNodeId(path, nodeId, existsWithSameId, otherNodeId, ioError));
        CPPUNIT_ASSERT(existsWithSameId);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A non-existing file
    {
        const SyncPath path = _localTestDirPath / "non_existing.jpg";
        bool existsWithSameId = false;
        NodeId otherNodeId;
        IoError ioError = IoErrorSuccess;
        NodeId nodeId;

        CPPUNIT_ASSERT(!_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(_testObj->checkIfPathExistsWithSameNodeId(path, nodeId, existsWithSameId, otherNodeId, ioError));
        CPPUNIT_ASSERT(!existsWithSameId);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A dangling symbolic link
    {
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath targetPath = temporaryDirectory.path() / "non_existing_test_file.txt";  // This file does not exist.
        const SyncPath path = temporaryDirectory.path() / "dangling_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        bool existsWithSameId = false;
        NodeId otherNodeId;
        IoError ioError = IoErrorUnknown;
        NodeId nodeId;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(_testObj->checkIfPathExistsWithSameNodeId(path, nodeId, existsWithSameId, otherNodeId, ioError));
        CPPUNIT_ASSERT(existsWithSameId);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A regular file but the function is given a wrong node identifier
    {
        const SyncPath path = _localTestDirPath / "test_pictures/picture-1.jpg";
        bool existsWithSameId = false;
        NodeId otherNodeId;
        IoError ioError = IoErrorUnknown;
        NodeId nodeId = "wrong_node_id";

        CPPUNIT_ASSERT(_testObj->checkIfPathExistsWithSameNodeId(path, nodeId, existsWithSameId, otherNodeId, ioError));
        CPPUNIT_ASSERT(!existsWithSameId);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A regular file without read/write permission
    {
        LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath path = temporaryDirectory.path() / "test.txt";
        std::ofstream ofs(path);
        ofs << "Some content.\n";
        ofs.close();

        IoError ioError = IoErrorUnknown;
        bool existsWithSameId = false;
        NodeId otherNodeId;
        NodeId nodeId;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));

        const bool setRightResults = IoHelper::setRights(path, false, false, false, ioError) && ioError == IoErrorSuccess;
        if (!setRightResults) {
            IoHelper::setRights(path, true, true, true, ioError);
            CPPUNIT_FAIL("Failed to set rights on the file");
        }
        bool checkIfPathExistsResult =
            _testObj->checkIfPathExistsWithSameNodeId(path, nodeId, existsWithSameId, otherNodeId, ioError);
        IoHelper::setRights(path, true, true, true, ioError);
        CPPUNIT_ASSERT(checkIfPathExistsResult);
        CPPUNIT_ASSERT(existsWithSameId);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

#if defined(__APPLE__)
    // A MacOSX Finder alias on a regular file.
    {
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = temporaryDirectory.path() / "regular_file_alias";

        IoError aliasError;
        CPPUNIT_ASSERT(IoHelper::createAliasFromPath(targetPath, path, aliasError));

        bool existsWithSameId = false;
        NodeId otherNodeId;
        NodeId nodeId;
        IoError ioError = IoErrorUnknown;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(_testObj->checkIfPathExistsWithSameNodeId(path, nodeId, existsWithSameId, otherNodeId, ioError));
        CPPUNIT_ASSERT(existsWithSameId);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A dangling MacOSX Finder alias on a non-existing file.
    {
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath targetPath = temporaryDirectory.path() / "file_to_be_deleted.png";  // This file will be deleted.
        const SyncPath path = temporaryDirectory.path() / "dangling_file_alias";
        { std::ofstream ofs(targetPath); }

        IoError aliasError;
        CPPUNIT_ASSERT(IoHelper::createAliasFromPath(targetPath, path, aliasError));

        std::filesystem::remove(targetPath);
        CPPUNIT_ASSERT(!std::filesystem::exists(targetPath));

        bool existsWithSameId = false;
        NodeId otherNodeId;
        NodeId nodeId;
        IoError ioError = IoErrorUnknown;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(_testObj->checkIfPathExistsWithSameNodeId(path, nodeId, existsWithSameId, otherNodeId, ioError));
        CPPUNIT_ASSERT(existsWithSameId);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }
#endif

#if defined(_WIN32)
    // A Windows junction on a regular target directory.
    {
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const SyncPath path = temporaryDirectory.path() / "regular_dir_junction";

        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->createJunctionFromPath(targetPath, path, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        bool existsWithSameId = false;
        NodeId otherNodeId;
        NodeId nodeId;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(_testObj->checkIfPathExistsWithSameNodeId(path, nodeId, existsWithSameId, otherNodeId, ioError));
        CPPUNIT_ASSERT(existsWithSameId);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A Windows junction on a non-existing target directory.
    {
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath targetPath = _localTestDirPath / "non_existing_dir";  // It doesn't exist.
        const SyncPath path = temporaryDirectory.path() / "dir_junction";

        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->createJunctionFromPath(targetPath, path, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        bool existsWithSameId = false;
        NodeId otherNodeId;
        NodeId nodeId;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(_testObj->checkIfPathExistsWithSameNodeId(path, nodeId, existsWithSameId, otherNodeId, ioError));
        CPPUNIT_ASSERT(existsWithSameId);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A Windows junction on a regular target file.
    {
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath targetPath = _localTestDirPath / "test_pictures" / "picture-1.jpg";
        const SyncPath path = temporaryDirectory.path() / "dir_junction";

        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->createJunctionFromPath(targetPath, path, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        bool existsWithSameId = false;
        NodeId otherNodeId;
        NodeId nodeId;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(_testObj->checkIfPathExistsWithSameNodeId(path, nodeId, existsWithSameId, otherNodeId, ioError));
        CPPUNIT_ASSERT(existsWithSameId);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }
#endif
}
}  // namespace KDC
