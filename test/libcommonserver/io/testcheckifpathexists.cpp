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

#include "test_utility/testhelpers.h"

#include <filesystem>

using namespace CppUnit;

namespace KDC {

void TestIo::testCheckIfPathExistsSimpleCases() {
    // A regular file
    {
        const SyncPath path = _localTestDirPath / "test_pictures/picture-1.jpg";
        bool exists = false;
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // A regular directory
    {
        const SyncPath path = _localTestDirPath / "test_pictures";
        bool exists = false;
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // A regular symbolic link on a file
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath path = temporaryDirectory.path() / "regular_file_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        bool exists = false;
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // A regular symbolic link on a folder
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath path = temporaryDirectory.path() / "regular_dir_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        bool exists = false;
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // A non-existing file
    {
        const SyncPath path = _localTestDirPath / "non_existing.jpg";
        bool exists = false;
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(!exists);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // A non-existing file whose name is too long for the OS.
    {
        const SyncPath path = std::string(1000, 'a');
        bool exists = false;
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(!exists);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::FileNameTooLong), IoError::FileNameTooLong,
                                     ioError);
    }

    // A dangling symbolic link
    {
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath targetPath = temporaryDirectory.path() / "non_existing_test_file.txt"; // This file does not exist.
        const SyncPath path = temporaryDirectory.path() / "dangling_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        bool exists = false;
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // A regular file without read/write permission
    {
        LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath path = temporaryDirectory.path() / "test.txt";
        { std::ofstream ofs(path); }
        IoError ioError = IoError::Unknown;
        const bool setRightResults = IoHelper::setRights(path, false, false, false, ioError) && ioError == IoError::Success;
        if (!setRightResults) {
            IoHelper::setRights(path, true, true, true, ioError);
            CPPUNIT_FAIL("Failed to set rights on the file");
        }
        bool exists = false;
        const bool checkIfPathExistsResult = IoHelper::checkIfPathExists(path, exists, ioError);
        IoHelper::setRights(path, true, true, true, ioError);

        CPPUNIT_ASSERT(checkIfPathExistsResult);
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // Checking existence of a subdirectory inside a directory that has been deleted and replaced with a file with the same name.
    // Example: the conversion of a bundle into a single file (macOS).
    {
        const SyncPath path = _localTestDirPath / "test_pictures" / "picture-1.jpg" / "A";
        bool exists = false;
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(!exists);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    }

#if defined(KD_MACOS)
    // A MacOSX Finder alias on a regular file.
    {
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = temporaryDirectory.path() / "regular_file_alias";

        IoError aliasError;
        CPPUNIT_ASSERT_MESSAGE(toString(aliasError), IoHelper::createAliasFromPath(targetPath, path, aliasError));

        bool exists = false;
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // A dangling MacOSX Finder alias on a non-existing file.
    {
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath targetPath = temporaryDirectory.path() / "file_to_be_deleted.png"; // This file will be deleted.
        const SyncPath path = temporaryDirectory.path() / "dangling_file_alias";
        {
            std::ofstream ofs(targetPath);
            ofs << "Some content.\n";
            ofs.close();
        }

        IoError aliasError;
        CPPUNIT_ASSERT_MESSAGE(toString(aliasError), IoHelper::createAliasFromPath(targetPath, path, aliasError));

        std::filesystem::remove(targetPath);

        bool exists = false;
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }
#endif

#if defined(KD_WINDOWS)
    // A Windows junction on a regular target directory.
    {
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const SyncPath path = temporaryDirectory.path() / "regular_dir_junction";

        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(_testObj->createJunctionFromPath(targetPath, path, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        bool exists = false;
        CPPUNIT_ASSERT(IoHelper::checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // A Windows junction on a non-existing target directory.
    {
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath targetPath = _localTestDirPath / "non_existing_dir"; // It doesn't exist.
        const SyncPath path = temporaryDirectory.path() / "dir_junction";

        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(_testObj->createJunctionFromPath(targetPath, path, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        bool exists = false;
        CPPUNIT_ASSERT(IoHelper::checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // A Windows junction on a regular target file.
    {
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = temporaryDirectory.path() / "dir_junction";

        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(_testObj->createJunctionFromPath(targetPath, path, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        bool exists = false;

        CPPUNIT_ASSERT(IoHelper::checkIfPathExists(path, exists, ioError));
        CPPUNIT_ASSERT(exists);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }
#endif
}

void TestIo::testCheckIfPathExistsWithSameNodeIdSimpleCases() {
    // A regular file
    {
        const SyncPath path = _localTestDirPath / "test_pictures/picture-1.jpg";
        bool existsWithSameId = false;
        NodeId otherNodeId;
        IoError ioError = IoError::Unknown;
        NodeId nodeId;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(IoHelper::checkIfPathExistsWithSameNodeId(path, nodeId, existsWithSameId, otherNodeId, ioError));
        CPPUNIT_ASSERT(existsWithSameId);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // A regular directory
    {
        const SyncPath path = _localTestDirPath / "test_pictures";
        bool existsWithSameId = false;
        NodeId otherNodeId;
        IoError ioError = IoError::Unknown;
        NodeId nodeId;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(IoHelper::checkIfPathExistsWithSameNodeId(path, nodeId, existsWithSameId, otherNodeId, ioError));
        CPPUNIT_ASSERT(existsWithSameId);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // A regular symbolic link on a file
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath path = temporaryDirectory.path() / "regular_file_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        bool existsWithSameId = false;
        NodeId otherNodeId;
        IoError ioError = IoError::Unknown;
        NodeId nodeId;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(IoHelper::checkIfPathExistsWithSameNodeId(path, nodeId, existsWithSameId, otherNodeId, ioError));
        CPPUNIT_ASSERT(existsWithSameId);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // A regular symbolic link on a folder
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath path = temporaryDirectory.path() / "regular_dir_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        bool existsWithSameId = false;
        NodeId otherNodeId;
        IoError ioError = IoError::Unknown;
        NodeId nodeId;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(IoHelper::checkIfPathExistsWithSameNodeId(path, nodeId, existsWithSameId, otherNodeId, ioError));
        CPPUNIT_ASSERT(existsWithSameId);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // A non-existing file
    {
        const SyncPath path = _localTestDirPath / "non_existing.jpg";
        bool existsWithSameId = false;
        NodeId otherNodeId;
        IoError ioError = IoError::Success;
        NodeId nodeId;

        CPPUNIT_ASSERT(!_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(IoHelper::checkIfPathExistsWithSameNodeId(path, nodeId, existsWithSameId, otherNodeId, ioError));
        CPPUNIT_ASSERT(!existsWithSameId);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // A dangling symbolic link
    {
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath targetPath = temporaryDirectory.path() / "non_existing_test_file.txt"; // This file does not exist.
        const SyncPath path = temporaryDirectory.path() / "dangling_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        bool existsWithSameId = false;
        NodeId otherNodeId;
        IoError ioError = IoError::Unknown;
        NodeId nodeId;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(IoHelper::checkIfPathExistsWithSameNodeId(path, nodeId, existsWithSameId, otherNodeId, ioError));
        CPPUNIT_ASSERT(existsWithSameId);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // A regular file but the function is given a wrong node identifier
    {
        const SyncPath path = _localTestDirPath / "test_pictures/picture-1.jpg";
        bool existsWithSameId = false;
        NodeId otherNodeId;
        IoError ioError = IoError::Unknown;
        NodeId nodeId = "wrong_node_id";

        CPPUNIT_ASSERT(IoHelper::checkIfPathExistsWithSameNodeId(path, nodeId, existsWithSameId, otherNodeId, ioError));
        CPPUNIT_ASSERT(!existsWithSameId);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // A regular file without read/write permission
    {
        LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath path = temporaryDirectory.path() / "test.txt";
        { std::ofstream ofs(path); }

        IoError ioError = IoError::Unknown;
        bool existsWithSameId = false;
        NodeId otherNodeId;
        NodeId nodeId;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));

        const bool setRightResults = IoHelper::setRights(path, false, false, false, ioError) && ioError == IoError::Success;
        if (!setRightResults) {
            IoHelper::setRights(path, true, true, true, ioError);
            CPPUNIT_FAIL("Failed to set rights on the file");
        }
        bool checkIfPathExistsResult =
                IoHelper::checkIfPathExistsWithSameNodeId(path, nodeId, existsWithSameId, otherNodeId, ioError);
        IoHelper::setRights(path, true, true, true, ioError);
        CPPUNIT_ASSERT(checkIfPathExistsResult);
        CPPUNIT_ASSERT(existsWithSameId);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

#if defined(KD_MACOS)
    // A MacOSX Finder alias on a regular file.
    {
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = temporaryDirectory.path() / "regular_file_alias";

        IoError aliasError;
        CPPUNIT_ASSERT_MESSAGE(toString(aliasError), IoHelper::createAliasFromPath(targetPath, path, aliasError));

        bool existsWithSameId = false;
        NodeId otherNodeId;
        NodeId nodeId;
        IoError ioError = IoError::Unknown;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(IoHelper::checkIfPathExistsWithSameNodeId(path, nodeId, existsWithSameId, otherNodeId, ioError));
        CPPUNIT_ASSERT(existsWithSameId);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // A dangling MacOSX Finder alias on a non-existing file.
    {
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath targetPath = temporaryDirectory.path() / "file_to_be_deleted.png"; // This file will be deleted.
        const SyncPath path = temporaryDirectory.path() / "dangling_file_alias";
        { std::ofstream ofs(targetPath); }

        IoError aliasError;
        CPPUNIT_ASSERT_MESSAGE(toString(aliasError), IoHelper::createAliasFromPath(targetPath, path, aliasError));

        std::filesystem::remove(targetPath);
        CPPUNIT_ASSERT(!std::filesystem::exists(targetPath));

        bool existsWithSameId = false;
        NodeId otherNodeId;
        NodeId nodeId;
        IoError ioError = IoError::Unknown;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(IoHelper::checkIfPathExistsWithSameNodeId(path, nodeId, existsWithSameId, otherNodeId, ioError));
        CPPUNIT_ASSERT(existsWithSameId);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }
#endif

#if defined(KD_WINDOWS)
    // A Windows junction on a regular target directory.
    {
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const SyncPath path = temporaryDirectory.path() / "regular_dir_junction";

        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(_testObj->createJunctionFromPath(targetPath, path, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        bool existsWithSameId = false;
        NodeId otherNodeId;
        NodeId nodeId;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(IoHelper::checkIfPathExistsWithSameNodeId(path, nodeId, existsWithSameId, otherNodeId, ioError));
        CPPUNIT_ASSERT(existsWithSameId);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // A Windows junction on a non-existing target directory.
    {
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath targetPath = _localTestDirPath / "non_existing_dir"; // It doesn't exist.
        const SyncPath path = temporaryDirectory.path() / "dir_junction";

        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(_testObj->createJunctionFromPath(targetPath, path, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        bool existsWithSameId = false;
        NodeId otherNodeId;
        NodeId nodeId;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(IoHelper::checkIfPathExistsWithSameNodeId(path, nodeId, existsWithSameId, otherNodeId, ioError));
        CPPUNIT_ASSERT(existsWithSameId);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // A Windows junction on a regular target file.
    {
        const LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath targetPath = _localTestDirPath / "test_pictures" / "picture-1.jpg";
        const SyncPath path = temporaryDirectory.path() / "dir_junction";

        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(_testObj->createJunctionFromPath(targetPath, path, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        bool existsWithSameId = false;
        NodeId otherNodeId;
        NodeId nodeId;

        CPPUNIT_ASSERT(_testObj->getNodeId(path, nodeId));
        CPPUNIT_ASSERT(IoHelper::checkIfPathExistsWithSameNodeId(path, nodeId, existsWithSameId, otherNodeId, ioError));
        CPPUNIT_ASSERT(existsWithSameId);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }
#endif
}

void TestIo::testCheckIfPathExistWithDistinctEncodings() {
    // Create two files in the same directory, with the same name, up-to to encoding. First NFC, then NFD.
    // Two distinct files should exist, except on MacOSX.
    {
        LocalTemporaryDirectory temporaryDirectory("TestIo");
        const auto nfc = testhelpers::makeNfcSyncName();
        const auto nfd = testhelpers::makeNfdSyncName();
        const SyncPath nfcPath = temporaryDirectory.path() / nfc;
        const SyncPath nfdPath = temporaryDirectory.path() / nfd;

        {
            std::ofstream{nfcPath};
            std::ofstream{nfdPath};
        }

        CPPUNIT_ASSERT(std::filesystem::exists(nfcPath));
        CPPUNIT_ASSERT(std::filesystem::exists(nfdPath));

        bool exists = false;
        IoError ioError = IoError::Unknown;

        CPPUNIT_ASSERT(IoHelper::checkIfPathExists(nfcPath, exists, ioError) && exists);
        CPPUNIT_ASSERT(IoHelper::checkIfPathExists(nfdPath, exists, ioError) && exists);

        NodeId nfcNodeId, nfdNodeId;
        IoHelper::getNodeId(nfcPath, nfcNodeId);
        IoHelper::getNodeId(nfdPath, nfdNodeId);

#if defined(KD_MACOS)
        CPPUNIT_ASSERT_EQUAL(nfcNodeId, nfdNodeId);
#else
        CPPUNIT_ASSERT(nfcNodeId != nfdNodeId);
#endif
    }

    // Create two files in the same directory, with the same name, up-to to encoding. First NFD, then NFC.
    // Both files should exist, except on MacOSX.
    {
        LocalTemporaryDirectory temporaryDirectory("TestIo");
        const SyncPath nfcPath = temporaryDirectory.path() / testhelpers::makeNfcSyncName();
        const SyncPath nfdPath = temporaryDirectory.path() / testhelpers::makeNfdSyncName();

        {
            std::ofstream{nfdPath};
            std::ofstream{nfcPath};
        }

        CPPUNIT_ASSERT(std::filesystem::exists(nfcPath));
        CPPUNIT_ASSERT(std::filesystem::exists(nfdPath));

        bool exists = false;
        IoError ioError = IoError::Unknown;

        CPPUNIT_ASSERT(IoHelper::checkIfPathExists(nfcPath, exists, ioError) && exists);
        CPPUNIT_ASSERT(IoHelper::checkIfPathExists(nfdPath, exists, ioError) && exists);

        NodeId nfcNodeId, nfdNodeId;
        IoHelper::getNodeId(nfcPath, nfcNodeId);
        IoHelper::getNodeId(nfdPath, nfdNodeId);

#if defined(KD_MACOS)
        CPPUNIT_ASSERT_EQUAL(nfcNodeId, nfdNodeId);
#else
        CPPUNIT_ASSERT(nfcNodeId != nfdNodeId);
#endif
    }
}

void TestIo::testCheckIfPathExistsMixedSeparators(void) {
    // Run only on Windows
    // On Unix systems, '\' is not considered a path separator, it can be used like any other character in a file name.

    const LocalTemporaryDirectory temporaryDirectory("TestIo_checkIfPathExistsMixedSeparators"); // The separ
    const SyncPath subDirForward = temporaryDirectory.path().string() + "/sub_dir";
    const SyncPath subFileForward = subDirForward.string() + "/sub_file.txt";
    const SyncPath subDirBackward = temporaryDirectory.path().string() + "\\sub_dir";
    const SyncPath subFileBackward = subDirBackward.string() + "\\sub_file.txt";

    // Ensure the separators are not automatically converted
    CPPUNIT_ASSERT_EQUAL(temporaryDirectory.path().string() + "/" + "sub_dir", subDirForward.string());
    CPPUNIT_ASSERT_EQUAL(subDirForward.string() + "/" + "sub_file.txt", subFileForward.string());

    CPPUNIT_ASSERT_EQUAL(temporaryDirectory.path().string() + "\\" + "sub_dir", subDirBackward.string());
    CPPUNIT_ASSERT_EQUAL(subDirBackward.string() + "\\" + "sub_file.txt", subFileBackward.string());

    // Create subDir and subFile
    IoError ioError = IoError::Success;
    CPPUNIT_ASSERT(IoHelper::createDirectory(subDirForward.lexically_normal(), false, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    testhelpers::generateOrEditTestFile(subFileForward.lexically_normal());

    // Checked that CheckIfPathExist can handle all the directory separators
    bool exist = false;
    CPPUNIT_ASSERT(IoHelper::checkIfPathExists(subDirForward, exist, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(exist);

    CPPUNIT_ASSERT(IoHelper::checkIfPathExists(subFileForward, exist, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(exist);

    CPPUNIT_ASSERT(IoHelper::checkIfPathExists(subDirBackward, exist, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(exist);

    CPPUNIT_ASSERT(IoHelper::checkIfPathExists(subFileBackward, exist, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(exist);
}


void TestIo::testCheckIfPathExists() {
    testCheckIfPathExistsSimpleCases();
    testCheckIfPathExistsWithSameNodeIdSimpleCases();
    testCheckIfPathExistWithDistinctEncodings();
#ifdef WIN32
    // On Unix systems, '\' is not considered a path separator, it can be used like any other character in a file name.
    testCheckIfPathExistsMixedSeparators();
#endif // WIN32
}

} // namespace KDC
