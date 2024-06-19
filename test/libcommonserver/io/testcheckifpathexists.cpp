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
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(_testObj->checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT(ioError == IoError::Success);
    }

    // A regular directory
    {
        const SyncPath path = _localTestDirPath / "test_pictures";
        bool exists = false;
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(_testObj->checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT(ioError == IoError::Success);
    }

    // A regular symbolic link on a file
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "regular_file_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        bool exists = false;
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(_testObj->checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT(ioError == IoError::Success);
    }

    // A regular symbolic link on a folder
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "regular_dir_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        bool exists = false;
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(_testObj->checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT(ioError == IoError::Success);
    }

    // A non-existing file
    {
        const SyncPath path = _localTestDirPath / "non_existing.jpg";
        bool exists = false;
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(_testObj->checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(!exists);
        CPPUNIT_ASSERT(ioError == IoError::NoSuchFileOrDirectory);
    }

    // A dangling symbolic link
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path / "non_existing_test_file.txt";  // This file does not exist.
        const SyncPath path = temporaryDirectory.path / "dangling_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        bool exists = false;
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(_testObj->checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT(ioError == IoError::Success);
    }
#if defined(__APPLE__)
    // A MacOSX Finder alias on a regular file.
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = temporaryDirectory.path / "regular_file_alias";

        IoError aliasError;
        CPPUNIT_ASSERT(IoHelper::createAliasFromPath(targetPath, path, aliasError));

        bool exists = false;
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(_testObj->checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT(ioError == IoError::Success);
    }

    // A dangling MacOSX Finder alias on a non-existing file.
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path / "file_to_be_deleted.png";  // This file will be deleted.
        const SyncPath path = temporaryDirectory.path / "dangling_file_alias";
        {
            std::ofstream ofs(targetPath);
            ofs << "Some content.\n";
            ofs.close();
        }

        IoError aliasError;
        CPPUNIT_ASSERT(IoHelper::createAliasFromPath(targetPath, path, aliasError));

        std::filesystem::remove(targetPath);

        bool exists = false;
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(_testObj->checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT(ioError == IoError::Success);
    }
#endif
}

void TestIo::testCheckIfPathExistsAllBranches() {
    // Failing to read a regular symbolic link because of an unexpected error.
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "regular_dir_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        _testObj->setIsSymlinkFunction([](const SyncPath &, std::error_code &ec) -> bool {
            ec = std::make_error_code(std::errc::state_not_recoverable);  // Not handled -> IoError::Unknown.
            return false;
        });

        bool exists = false;
        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(!_testObj->checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(!exists);
        CPPUNIT_ASSERT(ioError == IoError::Unknown);

        _testObj->resetFunctions();
    }

    // Reading a regular symbolic link that is removed after `filesystem::is_simlink` was called.
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "regular_dir_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        _testObj->setReadSymlinkFunction([](const SyncPath &path, std::error_code &ec) -> SyncPath {
            std::filesystem::remove(path);
            return std::filesystem::read_symlink(path, ec);
        });

        bool exists = false;
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(_testObj->checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(!exists);
        CPPUNIT_ASSERT(ioError == IoError::NoSuchFileOrDirectory);

        _testObj->resetFunctions();
    }

    // Reading a symlink within a subdirectory whose owner exec permission is removed
    // after `filesystem::is_simlink` was called.
    // No error on Windows. Access denied on MacOSX and Linux.
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath subdir = temporaryDirectory.path / "permission_less_subdirectory";
        std::filesystem::create_directory(subdir);

        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = subdir / "regular_file_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        _testObj->setReadSymlinkFunction([&subdir](const SyncPath &path, std::error_code &ec) -> SyncPath {
            std::filesystem::permissions(subdir, std::filesystem::perms::owner_exec, std::filesystem::perm_options::remove);
            return std::filesystem::read_symlink(path, ec);
        });

        bool exists = false;
        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(_testObj->checkIfPathExists(path, exists, ioError));

        // Restore permission to allow subdir removal
        std::filesystem::permissions(subdir, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);

        CPPUNIT_ASSERT(exists);
#ifdef _WIN32
        CPPUNIT_ASSERT(ioError == IoError::Success);
#else
        CPPUNIT_ASSERT(ioError == IoError::AccessDenied);
#endif
        _testObj->resetFunctions();
    }
}

void TestIo::testCheckIfPathExists() {
    testCheckIfPathExistsSimpleCases();
    testCheckIfPathExistsAllBranches();

    testCheckIfPathExistsWithSameNodeIdSimpleCases();
    testCheckIfPathExistsWithSameNodeIdAllBranches();
}

void TestIo::testCheckIfPathExistsWithSameNodeIdSimpleCases() {
    // A regular file
    {
        const SyncPath path = _localTestDirPath / "test_pictures/picture-1.jpg";
        bool exists = false;
        IoError ioError = IoError::Unknown;
        NodeId nodeId;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(_testObj->checkIfPathExistsWithSameNodeId(path, nodeId, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT(ioError == IoError::Success);
    }

    // A regular directory
    {
        const SyncPath path = _localTestDirPath / "test_pictures";
        bool exists = false;
        IoError ioError = IoError::Unknown;
        NodeId nodeId;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(_testObj->checkIfPathExistsWithSameNodeId(path, nodeId, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT(ioError == IoError::Success);
    }

    // A regular symbolic link on a file
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "regular_file_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        bool exists = false;
        IoError ioError = IoError::Unknown;
        NodeId nodeId;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(_testObj->checkIfPathExistsWithSameNodeId(path, nodeId, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT(ioError == IoError::Success);
    }

    // A regular symbolic link on a folder
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "regular_dir_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        bool exists = false;
        IoError ioError = IoError::Unknown;
        NodeId nodeId;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(_testObj->checkIfPathExistsWithSameNodeId(path, nodeId, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT(ioError == IoError::Success);
    }

    // A non-existing file
    {
        const SyncPath path = _localTestDirPath / "non_existing.jpg";
        bool exists = false;
        IoError ioError = IoError::Success;
        NodeId nodeId;

        CPPUNIT_ASSERT(!_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(_testObj->checkIfPathExistsWithSameNodeId(path, nodeId, exists, ioError));
        CPPUNIT_ASSERT(!exists);
        CPPUNIT_ASSERT(ioError == IoError::NoSuchFileOrDirectory);
    }

    // A dangling symbolic link
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path / "non_existing_test_file.txt";  // This file does not exist.
        const SyncPath path = temporaryDirectory.path / "dangling_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        bool exists = false;
        IoError ioError = IoError::Unknown;
        NodeId nodeId;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(_testObj->checkIfPathExistsWithSameNodeId(path, nodeId, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT(ioError == IoError::Success);
    }

    // A regular file but the function is given a wrong node identifier
    {
        const SyncPath path = _localTestDirPath / "test_pictures/picture-1.jpg";
        bool exists = false;
        IoError ioError = IoError::Unknown;
        NodeId nodeId = "wrong_node_id";

        CPPUNIT_ASSERT(_testObj->checkIfPathExistsWithSameNodeId(path, nodeId, exists, ioError));
        CPPUNIT_ASSERT(!exists);
        CPPUNIT_ASSERT(ioError == IoError::Success);
    }

#if defined(__APPLE__)
    // A MacOSX Finder alias on a regular file.
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = temporaryDirectory.path / "regular_file_alias";

        IoError aliasError;
        CPPUNIT_ASSERT(IoHelper::createAliasFromPath(targetPath, path, aliasError));

        bool exists = false;
        NodeId nodeId;
        IoError ioError = IoError::Unknown;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(_testObj->checkIfPathExistsWithSameNodeId(path, nodeId, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT(ioError == IoError::Success);
    }

    // A dangling MacOSX Finder alias on a non-existing file.
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path / "file_to_be_deleted.png";  // This file will be deleted.
        const SyncPath path = temporaryDirectory.path / "dangling_file_alias";
        { std::ofstream ofs(targetPath); }

        IoError aliasError;
        CPPUNIT_ASSERT(IoHelper::createAliasFromPath(targetPath, path, aliasError));

        std::filesystem::remove(targetPath);
        CPPUNIT_ASSERT(!std::filesystem::exists(targetPath));

        bool exists = false;
        NodeId nodeId;
        IoError ioError = IoError::Unknown;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(_testObj->checkIfPathExistsWithSameNodeId(path, nodeId, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT(ioError == IoError::Success);
    }
#endif
}

void TestIo::testCheckIfPathExistsWithSameNodeIdAllBranches() {
    // Failing to read a regular symbolic link because of an unexpected error.
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "regular_dir_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        _testObj->setIsSymlinkFunction([](const SyncPath &, std::error_code &ec) -> bool {
            ec = std::make_error_code(std::errc::state_not_recoverable);  // Not handled -> IoError::Unknown.
            return false;
        });

        bool exists = false;
        IoError ioError = IoError::Success;
        NodeId nodeId;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(!_testObj->checkIfPathExistsWithSameNodeId(path, nodeId, exists, ioError));
        CPPUNIT_ASSERT(!exists);
        CPPUNIT_ASSERT(ioError == IoError::Unknown);

        _testObj->resetFunctions();
    }

    // Reading a symlink within a subdirectory whose owner exec permission is removed
    // right before `std::filesystem::read_symlink` is called.
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath subdir = temporaryDirectory.path / "permission_less_subdirectory";
        std::filesystem::create_directory(subdir);

        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = subdir / "regular_file_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        _testObj->setReadSymlinkFunction([&subdir](const SyncPath &path, std::error_code &ec) -> SyncPath {
            std::filesystem::permissions(subdir, std::filesystem::perms::owner_exec, std::filesystem::perm_options::remove);
            return std::filesystem::read_symlink(path, ec);
        });

        bool exists = false;
        IoError ioError = IoError::Unknown;
        NodeId nodeId;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(_testObj->checkIfPathExistsWithSameNodeId(path, nodeId, exists, ioError));

        // Restore permission to allow subdir removal
        std::filesystem::permissions(subdir, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);
#ifdef _WIN32
        CPPUNIT_ASSERT(exists);
#else
        CPPUNIT_ASSERT(!exists);  // getLocalNodeId returns an empty string because of the denied access.
        CPPUNIT_ASSERT(ioError == IoError::AccessDenied);
#endif

        _testObj->resetFunctions();
    }

    // Reading a regular symbolic link that is removed right after `filesystem::is_simlink` was called.
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "regular_dir_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        _testObj->setReadSymlinkFunction([](const SyncPath &path, std::error_code &ec) -> SyncPath {
            std::filesystem::remove(path);
            return std::filesystem::read_symlink(path, ec);
        });

        bool exists = false;
        IoError ioError = IoError::Success;
        NodeId nodeId;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(_testObj->checkIfPathExistsWithSameNodeId(path, nodeId, exists, ioError));
        CPPUNIT_ASSERT(!exists);
        CPPUNIT_ASSERT(ioError == IoError::NoSuchFileOrDirectory);

        _testObj->resetFunctions();
    }

    // Checking existence of a subdirectory inside a directory that have been deleted and replaced with a file with the same name
    // ex: conversion of a bundle into a single file (macOS)
    {
        const SyncPath path = _localTestDirPath / "test_pictures/picture-1.jpg/A";
        bool exists = false;
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(_testObj->checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(!exists);
        CPPUNIT_ASSERT_EQUAL(IoError::NoSuchFileOrDirectory, ioError);
    }
}


}  // namespace KDC
