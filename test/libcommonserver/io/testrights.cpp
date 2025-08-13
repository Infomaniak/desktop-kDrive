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
#include "libcommonserver/io/permissionsholder.h"
#include "test_utility/testhelpers.h"

#include <filesystem>

using namespace CppUnit;

namespace KDC {

struct RightsSet {
        RightsSet(int rights) :
            read(rights & 4),
            write(rights & 2),
            execute(rights & 1) {};
        RightsSet(bool read, bool write, bool execute) :
            read(read),
            write(write),
            execute(execute) {};
        bool read;
        bool write;
        bool execute;
};

void TestIo::testCheckSetAndGetRights() {
    // Test if the rights are correctly set and get on a directory
    {
        const LocalTemporaryDirectory temporaryDirectory("io_rights");
        const SyncPath path = temporaryDirectory.path() / "changePerm";

        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::createDirectory(path, false, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

        bool isReadable = false;
        bool isWritable = false;
        bool isExecutable = false;

#if defined(KD_WINDOWS)
        // Test for a directory without any Explicit ACE (ie no inherited rights)
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::getRights(path, isReadable, isWritable, isExecutable, ioError));
        CPPUNIT_ASSERT(ioError == IoError::Success && isReadable && isWritable && isExecutable);
#endif

        /* Test all the possible rights and all the possible order of rights modification. ie:
         *  | READ | WRITE | EXECUTE | |
         *  |  0   |   0   |    0    | v
         *  |  0   |   0   |    1    | |
         *  |  0   |   0   |    0    | v
         *  |  0   |   1   |    0    | |
         *  |  0   |   0   |    0    | v
         *  |  0   |   1   |    1    | |
         *  |  0   |   0   |    0    | v
         *  |  1   |   0   |    0    | |
         *  |  0   |   0   |    0    | v
         *  |  1   |   0   |    1    | |
         *  |  0   |   0   |    0    | v
         *  |  1   |   1   |    0    | |
         *  |  0   |   0   |    0    | v
         *  |  1   |   1   |    1    | |
         *  |  0   |   0   |    0    | v
         *  ...
         */
#if defined(KD_WINDOWS)
        IoHelper::_getAndSetRightsMethod = 0; // Set the method to use the windows API
        for (int i = 0; i < 2; i++) { // Test Windows API and fallback method
#endif
            for (int baseRights = 0; baseRights < 7; baseRights++) {
                for (int targetRights = baseRights + 1; targetRights < 8; targetRights++) {
                    auto rightsSet = RightsSet(baseRights);
#if defined(KD_WINDOWS)
                    if (IoHelper::_getAndSetRightsMethod == 1 && (!rightsSet.execute || !rightsSet.read)) {
                        continue; // Skip the test if the rights are not supported by the current method
                    }
#endif
                    bool result = IoHelper::setRights(path, rightsSet.read, rightsSet.write, rightsSet.execute, ioError);
                    result &= ioError == IoError::Success;
                    if (!result) {
                        IoHelper::setRights(path, true, true, true, ioError);
                        CPPUNIT_ASSERT(false /* Failed to set base rights */);
                    }

                    result = IoHelper::getRights(path, isReadable, isWritable, isExecutable, ioError);
                    result &= ioError == IoError::Success;
                    if (!result) {
                        IoHelper::setRights(path, true, true, true, ioError);
                        CPPUNIT_ASSERT(false /* Failed to get base rights */);
                    }

                    if (!(isReadable == rightsSet.read && isWritable == rightsSet.write && isExecutable == rightsSet.execute)) {
                        IoHelper::setRights(path, true, true, true, ioError);
                        CPPUNIT_ASSERT(false /* Set base rights mismatch  with get base rights */);
                    }

                    rightsSet = RightsSet(targetRights);
#if defined(KD_WINDOWS)
                    if (IoHelper::_getAndSetRightsMethod == 1 && (!rightsSet.execute || !rightsSet.read)) {
                        continue; // Skip the test if the rights are not supported by the current method
                    }
#endif
                    result = IoHelper::setRights(path, rightsSet.read, rightsSet.write, rightsSet.execute, ioError);
                    result &= ioError == IoError::Success;
                    if (!result) {
                        IoHelper::setRights(path, true, true, true, ioError);
                        CPPUNIT_ASSERT(false /* Failed to set target rights */);
                    }

                    result = IoHelper::getRights(path, isReadable, isWritable, isExecutable, ioError);
                    result &= ioError == IoError::Success;
                    if (!result) {
                        IoHelper::setRights(path, true, true, true, ioError);
                        CPPUNIT_ASSERT(false /* Failed to get target rights */);
                    }

                    if (!(isReadable == rightsSet.read && isWritable == rightsSet.write && isExecutable == rightsSet.execute)) {
                        IoHelper::setRights(path, true, true, true, ioError);
                        CPPUNIT_ASSERT(false /* Set target rights mismatch with get target rights */);
                    }
                }
            }
#if defined(KD_WINDOWS)
            if (i == 0) {
                CPPUNIT_ASSERT_EQUAL(0, IoHelper::_getAndSetRightsMethod); // Check that no error occurred with the windows API
            }
            IoHelper::_getAndSetRightsMethod = 1; // Set the method to use std::filesystem method (fallback)
        }
        IoHelper::_getAndSetRightsMethod = 0; // Set the method to use the windows API
#endif

        // Restore the rights
        IoHelper::setRights(path, true, true, true, ioError);
    }

    // Test if the rights are correctly set and if they can be successfully retrieved from a file
    {
        const LocalTemporaryDirectory temporaryDirectory("io_rights");
        const SyncPath filepath = temporaryDirectory.path() / "changePerm.txt";

        IoError ioError = IoError::Unknown;

        std::ofstream file(filepath);
        CPPUNIT_ASSERT(file.is_open());
        file << "testCheckSetAndGetRights";
        file.close();

        bool isReadable = false;
        bool isWritable = false;
        bool isExecutable = false;

#if defined(KD_WINDOWS)
        // Test for a file without any Explicit ACE (ie no inherited rights)
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::getRights(filepath, isReadable, isWritable, isExecutable, ioError));
        CPPUNIT_ASSERT(ioError == IoError::Success && isReadable && isWritable && isExecutable);
#endif

        /* Test all the possible rights and all the possible order of rights modification. ie:
         *  | READ | WRITE | EXECUTE | |
         *  |  0   |   0   |    0    | v
         *  |  0   |   0   |    1    | |
         *  |  0   |   0   |    0    | v
         *  |  0   |   1   |    0    | |
         *  |  0   |   0   |    0    | v
         *  |  0   |   1   |    1    | |
         *  |  0   |   0   |    0    | v
         *  |  1   |   0   |    0    | |
         *  |  0   |   0   |    0    | v
         *  |  1   |   0   |    1    | |
         *  |  0   |   0   |    0    | v
         *  |  1   |   1   |    0    | |
         *  |  0   |   0   |    0    | v
         *  |  1   |   1   |    1    | |
         *  |  0   |   0   |    0    | v
         *  ...
         */
#if defined(KD_WINDOWS)

        IoHelper::_getAndSetRightsMethod = 0; // Set the method to use the windows API
        for (int i = 0; i < 2; i++) { // Test Windows API and fallback method
#endif
            for (int baseRights = 0; baseRights < 7;
                 baseRights++) { // Test all the possible rights and the all the possible order of rights modification
                for (int targetRights = baseRights + 1; targetRights < 8; targetRights++) {
                    auto rightsSet = RightsSet(baseRights);
#if defined(KD_WINDOWS)
                    if (IoHelper::_getAndSetRightsMethod == 1 && (!rightsSet.execute || !rightsSet.read)) {
                        continue; // Skip the test if the rights are not supported by the current method
                    }
#endif
                    bool result = IoHelper::setRights(filepath, rightsSet.read, rightsSet.write, rightsSet.execute, ioError);
                    result &= ioError == IoError::Success;
                    if (!result) {
                        IoHelper::setRights(filepath, true, true, true, ioError);
                        CPPUNIT_ASSERT(false /* Failed to set base rights */);
                    }

                    result = IoHelper::getRights(filepath, isReadable, isWritable, isExecutable, ioError);
                    result &= ioError == IoError::Success;
                    if (!result) {
                        IoHelper::setRights(filepath, true, true, true, ioError);
                        CPPUNIT_ASSERT(false /* Failed to get base rights */);
                    }
                    if (!(isReadable == rightsSet.read && isWritable == rightsSet.write && isExecutable == rightsSet.execute)) {
                        IoHelper::setRights(filepath, true, true, true, ioError);
                        CPPUNIT_ASSERT(false /* Set base rights mismatch  with get base rights */);
                    }

                    rightsSet = RightsSet(targetRights);
#if defined(KD_WINDOWS)
                    if (IoHelper::_getAndSetRightsMethod == 1 && (!rightsSet.execute || !rightsSet.read)) {
                        continue; // Skip the test if the rights are not supported by the current method
                    }
#endif
                    result = IoHelper::setRights(filepath, rightsSet.read, rightsSet.write, rightsSet.execute, ioError);
                    result &= ioError == IoError::Success;
                    if (!result) {
                        IoHelper::setRights(filepath, true, true, true, ioError);
                        CPPUNIT_ASSERT(false /* Failed to set target rights */);
                    }
                    result = IoHelper::getRights(filepath, isReadable, isWritable, isExecutable, ioError);
                    result &= ioError == IoError::Success;
                    if (!result) {
                        IoHelper::setRights(filepath, true, true, true, ioError);
                        CPPUNIT_ASSERT(false /* Failed to get target rights */);
                    }

                    if (!(isReadable == rightsSet.read && isWritable == rightsSet.write && isExecutable == rightsSet.execute)) {
                        IoHelper::setRights(filepath, true, true, true, ioError);
                        CPPUNIT_ASSERT(false /* Set target rights mismatch with get target rights */);
                    }
                }
            }
#if defined(KD_WINDOWS)
            if (i == 0) {
                CPPUNIT_ASSERT_EQUAL(0, IoHelper::_getAndSetRightsMethod); // Check that no error occurred with the windows API
            }
            IoHelper::_getAndSetRightsMethod = 1; // Set the method to use the std::filesystem method (fallback)
        }
        IoHelper::_getAndSetRightsMethod = 0; // Set the method to use the windows API
#endif

        // Restore the rights
        IoHelper::setRights(filepath, true, true, true, ioError);
    }

    // Check permissions are not set recursively on a folder
    {
        const LocalTemporaryDirectory temporaryDirectory("io_rights");
        const SyncPath path = temporaryDirectory.path() / "testCheckSetAndGetRights";
        const SyncPath subFolderPath = path / "subFolder";
        const SyncPath subFilePath = path / "subFile.txt";


        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::createDirectory(path, false, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::createDirectory(subFolderPath, false, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

        std::ofstream file(subFilePath);
        CPPUNIT_ASSERT(file.is_open());
        file << "testCheckSetAndGetRights";
        file.close();


        bool isReadable = false;
        bool isWritable = false;
        bool isExecutable = false;

        bool result = IoHelper::setRights(path, true, true, true, ioError);
        result = IoHelper::setRights(subFolderPath, true, true, true, ioError);
        result = IoHelper::setRights(subFilePath, true, true, true, ioError);
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::getRights(path, isReadable, isWritable, isExecutable, ioError));
        CPPUNIT_ASSERT(ioError == IoError::Success && isReadable && isWritable && isExecutable);

        CPPUNIT_ASSERT_MESSAGE(toString(ioError),
                               IoHelper::getRights(subFolderPath, isReadable, isWritable, isExecutable, ioError));
        CPPUNIT_ASSERT(ioError == IoError::Success && isReadable && isWritable && isExecutable);

        CPPUNIT_ASSERT_MESSAGE(toString(ioError),
                               IoHelper::getRights(subFilePath, isReadable, isWritable, isExecutable, ioError));
        CPPUNIT_ASSERT(ioError == IoError::Success && isReadable && isWritable && isExecutable);

        result = IoHelper::setRights(path, true, false, true, ioError);
        result &= ioError == IoError::Success;
        if (!result) {
            IoHelper::setRights(path, true, true, true, ioError); // Restore the rights for delete
            CPPUNIT_ASSERT(false /* Failed to set base rights */);
        }

        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::getRights(path, isReadable, isWritable, isExecutable, ioError));
        CPPUNIT_ASSERT(ioError == IoError::Success && isReadable && !isWritable && isExecutable);
        CPPUNIT_ASSERT_MESSAGE(toString(ioError),
                               IoHelper::getRights(subFolderPath, isReadable, isWritable, isExecutable, ioError));
        CPPUNIT_ASSERT(ioError == IoError::Success && isReadable && isWritable && isExecutable);
        CPPUNIT_ASSERT_MESSAGE(toString(ioError),
                               IoHelper::getRights(subFilePath, isReadable, isWritable, isExecutable, ioError));
        CPPUNIT_ASSERT(ioError == IoError::Success && isReadable && isWritable && isExecutable);

        // Restore the rights
        IoHelper::setRights(path, true, true, true, ioError); // Restore the rights for delete
#if defined(KD_WINDOWS)
        CPPUNIT_ASSERT_EQUAL(0, IoHelper::_getAndSetRightsMethod); // Check that no error occurred with the windows API
#endif
    }

    // Test with inherited permissions on a directory
    {
#if defined(KD_WINDOWS)
        const LocalTemporaryDirectory temporaryDirectory("io_rights");
        const SyncPath path = temporaryDirectory.path() / "testCheckSetAndGetRights";
        const SyncPath subFolderPath = path / "subFolder";

        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::createDirectory(path, false, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::createDirectory(subFolderPath, false, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

        bool isReadable = false;
        bool isWritable = false;
        bool isExecutable = false;

        /* Test all the possible rights and all the possible order of rights modification. ie:
         *  | READ | WRITE | EXECUTE | |
         *  |  0   |   0   |    0    | v
         *  |  0   |   0   |    1    | |
         *  |  0   |   0   |    0    | v
         *  |  0   |   1   |    0    | |
         *  |  0   |   0   |    0    | v
         *  |  0   |   1   |    1    | |
         *  |  0   |   0   |    0    | v
         *  |  1   |   0   |    0    | |
         *  |  0   |   0   |    0    | v
         *  |  1   |   0   |    1    | |
         *  |  0   |   0   |    0    | v
         *  |  1   |   1   |    0    | |
         *  |  0   |   0   |    0    | v
         *  |  1   |   1   |    1    | |
         *  |  0   |   0   |    0    | v
         *  ...
         */
        IoHelper::_setRightsWindowsApiInheritance = true;
        for (int baseRights = 0; baseRights < 7;
             baseRights++) { // Test all the possible rights and the all the possible order of rights modification
            for (int targetRights = baseRights + 1; targetRights < 8; targetRights++) {
                auto rightsSet = RightsSet(baseRights);
                bool result = IoHelper::setRights(path, rightsSet.read, rightsSet.write, rightsSet.execute, ioError);
                result &= ioError == IoError::Success;
                if (!result) {
                    IoHelper::setRights(path, true, true, true, ioError);
                    IoHelper::_setRightsWindowsApiInheritance = false;
                    CPPUNIT_ASSERT(false /* Failed to set base rights */);
                }

                result = IoHelper::getRights(subFolderPath, isReadable, isWritable, isExecutable, ioError);
                result &= ioError == IoError::Success;
                if (!result) {
                    IoHelper::setRights(path, true, true, true, ioError);
                    IoHelper::_setRightsWindowsApiInheritance = false;
                    CPPUNIT_ASSERT(false /* Failed to get base rights */);
                }

                if (!(isReadable == rightsSet.read && isWritable == rightsSet.write && isExecutable == rightsSet.execute)) {
                    IoHelper::setRights(path, true, true, true, ioError);
                    IoHelper::_setRightsWindowsApiInheritance = false;
                    CPPUNIT_ASSERT(false /* Set base rights mismatch  with get base rights */);
                }

                rightsSet = RightsSet(targetRights);
                result = IoHelper::setRights(path, rightsSet.read, rightsSet.write, rightsSet.execute, ioError);
                result &= ioError == IoError::Success;
                if (!result) {
                    IoHelper::setRights(path, true, true, true, ioError);
                    IoHelper::_setRightsWindowsApiInheritance = false;
                    CPPUNIT_ASSERT(false /* Failed to set target rights */);
                }
                result = IoHelper::getRights(subFolderPath, isReadable, isWritable, isExecutable, ioError);
                result &= ioError == IoError::Success;
                if (!result) {
                    IoHelper::setRights(path, true, true, true, ioError);
                    IoHelper::_setRightsWindowsApiInheritance = false;
                    CPPUNIT_ASSERT(false /* Failed to get target rights */);
                }

                if (!(isReadable == rightsSet.read && isWritable == rightsSet.write && isExecutable == rightsSet.execute)) {
                    IoHelper::setRights(path, true, true, true, ioError);
                    IoHelper::_setRightsWindowsApiInheritance = false;
                    CPPUNIT_ASSERT(false /* Set target rights mismatch with get target rights */);
                }
            }
        }

        // Restore the rights
        IoHelper::setRights(path, true, true, true, ioError);
        IoHelper::_setRightsWindowsApiInheritance = false;
        CPPUNIT_ASSERT_EQUAL(0, IoHelper::_getAndSetRightsMethod); // Check that no error occurred with the windows API
#endif
    }

    // Test with inherited permissions on a file
    {
#if defined(KD_WINDOWS)
        const LocalTemporaryDirectory temporaryDirectory("io_rights");
        const SyncPath path = temporaryDirectory.path() / "testCheckSetAndGetRights";
        const SyncPath filePath = path / "file.txt";

        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::createDirectory(path, false, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

        std::ofstream file(filePath);
        CPPUNIT_ASSERT(file.is_open());
        file << "testCheckSetAndGetRights";
        file.close();

        bool isReadable = false;
        bool isWritable = false;
        bool isExecutable = false;

        /* Test all the possible rights and all the possible order of rights modification. ie:
         *  | READ | WRITE | EXECUTE | |
         *  |  0   |   0   |    0    | v
         *  |  0   |   0   |    1    | |
         *  |  0   |   0   |    0    | v
         *  |  0   |   1   |    0    | |
         *  |  0   |   0   |    0    | v
         *  |  0   |   1   |    1    | |
         *  |  0   |   0   |    0    | v
         *  |  1   |   0   |    0    | |
         *  |  0   |   0   |    0    | v
         *  |  1   |   0   |    1    | |
         *  |  0   |   0   |    0    | v
         *  |  1   |   1   |    0    | |
         *  |  0   |   0   |    0    | v
         *  |  1   |   1   |    1    | |
         *  |  0   |   0   |    0    | v
         *  ...
         */
        IoHelper::_setRightsWindowsApiInheritance = true;
        for (int baseRights = 0; baseRights < 7;
             baseRights++) { // Test all the possible rights and the all the possible order of rights modification
            for (int targetRights = baseRights + 1; targetRights < 8; targetRights++) {
                auto rightsSet = RightsSet(baseRights);
                bool result = IoHelper::setRights(path, rightsSet.read, rightsSet.write, rightsSet.execute, ioError);
                result &= ioError == IoError::Success;
                if (!result) {
                    IoHelper::setRights(path, true, true, true, ioError);
                    IoHelper::_setRightsWindowsApiInheritance = false;
                    CPPUNIT_ASSERT(false /* Failed to set base rights */);
                }

                result = IoHelper::getRights(filePath, isReadable, isWritable, isExecutable, ioError);
                result &= ioError == IoError::Success;
                if (!result) {
                    IoHelper::setRights(path, true, true, true, ioError);
                    IoHelper::_setRightsWindowsApiInheritance = false;
                    CPPUNIT_ASSERT(false /* Failed to get base rights */);
                }
                if (!(isReadable == rightsSet.read && isWritable == rightsSet.write && isExecutable == rightsSet.execute)) {
                    IoHelper::setRights(path, true, true, true, ioError);
                    IoHelper::_setRightsWindowsApiInheritance = false;
                    CPPUNIT_ASSERT(false /* Set base rights mismatch  with get base rights */);
                }

                rightsSet = RightsSet(targetRights);
                result = IoHelper::setRights(path, rightsSet.read, rightsSet.write, rightsSet.execute, ioError);
                result &= ioError == IoError::Success;
                if (!result) {
                    IoHelper::setRights(path, true, true, true, ioError);
                    IoHelper::_setRightsWindowsApiInheritance = false;
                    CPPUNIT_ASSERT(false /* Failed to set target rights */);
                }
                result = IoHelper::getRights(filePath, isReadable, isWritable, isExecutable, ioError);
                result &= ioError == IoError::Success;
                if (!result) {
                    IoHelper::setRights(path, true, true, true, ioError);
                    IoHelper::_setRightsWindowsApiInheritance = false;
                    CPPUNIT_ASSERT(false /* Failed to get target rights */);
                }

                if (!(isReadable == rightsSet.read && isWritable == rightsSet.write && isExecutable == rightsSet.execute)) {
                    IoHelper::setRights(path, true, true, true, ioError);
                    IoHelper::_setRightsWindowsApiInheritance = false;
                    CPPUNIT_ASSERT(false /* Set target rights mismatch with get target rights */);
                }
            }
        }

        // Restore the rights
        IoHelper::setRights(path, true, true, true, ioError);
        IoHelper::_setRightsWindowsApiInheritance = false;
        CPPUNIT_ASSERT_EQUAL(0, IoHelper::_getAndSetRightsMethod); // Check that no error occurred with the wndows API
#endif
    }

    // Test on a non existing file
    {
        const LocalTemporaryDirectory temporaryDirectory("io_rights");
        const SyncPath path = temporaryDirectory.path() / "testCheckSetAndGetRights/nonExistingFile.txt";
        bool isReadable = false;
        bool isWritable = false;
        bool isExecutable = false;
        IoError ioError = IoError::Unknown;

        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::getRights(path, isReadable, isWritable, isExecutable, ioError));
        CPPUNIT_ASSERT_EQUAL(ioError, IoError::NoSuchFileOrDirectory);

        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::setRights(path, true, true, true, ioError));
        CPPUNIT_ASSERT(ioError == IoError::NoSuchFileOrDirectory);
#if defined(KD_WINDOWS)
        CPPUNIT_ASSERT_EQUAL(0, IoHelper::_getAndSetRightsMethod); // Check that no error occurred with the wndows API
        IoHelper::_getAndSetRightsMethod = 1; // Set the method to use the std::filesystem method (fallback)
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::getRights(path, isReadable, isWritable, isExecutable, ioError));
        CPPUNIT_ASSERT_EQUAL(ioError, IoError::NoSuchFileOrDirectory);

        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::setRights(path, true, true, true, ioError));
        CPPUNIT_ASSERT(ioError == IoError::NoSuchFileOrDirectory);

#endif
    }
#if defined(KD_WINDOWS)
    IoHelper::_getAndSetRightsMethod = 0; // Set the method to use the std::filesystem method (fallback)
#endif
}

void TestIo::testLock() {
    const LocalTemporaryDirectory tempDir("testLockDir");
    const auto filePath = tempDir.path() / "testLockFile";
    testhelpers::generateOrEditTestFile(filePath);
    bool isLocked = false;

    // Lock a directory.
    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::lock(tempDir.path()));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::isLocked(tempDir.path(), isLocked));
#if defined(KD_MACOS)
    CPPUNIT_ASSERT(isLocked);
#else
    CPPUNIT_ASSERT(!isLocked);
#endif
    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::isLocked(filePath, isLocked));
    CPPUNIT_ASSERT(!isLocked);

    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::unlock(tempDir.path()));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::isLocked(tempDir.path(), isLocked));
    CPPUNIT_ASSERT(!isLocked);
    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::isLocked(filePath, isLocked));
    CPPUNIT_ASSERT(!isLocked);

    // Lock a file.
    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::lock(filePath));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::isLocked(filePath, isLocked));
#if defined(KD_MACOS)
    CPPUNIT_ASSERT(isLocked);
#else
    CPPUNIT_ASSERT(!isLocked);
#endif

    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::unlock(filePath));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::isLocked(filePath, isLocked));
    CPPUNIT_ASSERT(!isLocked);
}

void TestIo::testReadOnly() {
    const LocalTemporaryDirectory tempDir("testSetReadOnlyDir");
    const auto filePath = tempDir.path() / "testSetReadOnlyFile";
    testhelpers::generateOrEditTestFile(filePath);

    // Set read-only access to a directory.
    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::setReadOnly(tempDir.path()));
    bool read = false;
    bool write = false;
    bool dummyExec = false;
    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::getRights(tempDir.path(), read, write, dummyExec));
    CPPUNIT_ASSERT_EQUAL(true, read);
    CPPUNIT_ASSERT_EQUAL(false, write);
    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::getRights(filePath, read, write, dummyExec));
    CPPUNIT_ASSERT_EQUAL(true, read);
    CPPUNIT_ASSERT_EQUAL(true, write);
    bool isLocked = false;
    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::isLocked(tempDir.path(), isLocked));
#if defined(KD_MACOS)
    CPPUNIT_ASSERT(isLocked);
#else
    CPPUNIT_ASSERT(!isLocked);
#endif
    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::isLocked(filePath, isLocked));
    CPPUNIT_ASSERT(!isLocked);

    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::setFullAccess(tempDir.path()));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::getRights(tempDir.path(), read, write, dummyExec));
    CPPUNIT_ASSERT_EQUAL(true, read);
    CPPUNIT_ASSERT_EQUAL(true, write);
    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::getRights(filePath, read, write, dummyExec));
    CPPUNIT_ASSERT_EQUAL(true, read);
    CPPUNIT_ASSERT_EQUAL(true, write);
    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::isLocked(tempDir.path(), isLocked));
    CPPUNIT_ASSERT(!isLocked);
    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::isLocked(filePath, isLocked));
    CPPUNIT_ASSERT(!isLocked);

    // Set read-only access to a file.
    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::setReadOnly(filePath));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::getRights(filePath, read, write, dummyExec));
    CPPUNIT_ASSERT_EQUAL(true, read);
    CPPUNIT_ASSERT_EQUAL(false, write);
    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::isLocked(filePath, isLocked));
#if defined(KD_MACOS)
    CPPUNIT_ASSERT(isLocked);
#else
    CPPUNIT_ASSERT(!isLocked);
#endif

    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::setFullAccess(filePath));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::getRights(filePath, read, write, dummyExec));
    CPPUNIT_ASSERT_EQUAL(true, read);
    CPPUNIT_ASSERT_EQUAL(true, write);
    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::isLocked(filePath, isLocked));
    CPPUNIT_ASSERT(!isLocked);
}

void TestIo::testPermissionsHolder() {
    const LocalTemporaryDirectory tempDir("testPermissionsHolderDir");
    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::setReadOnly(tempDir.path()));

    const auto filePath = tempDir.path() / "testPermissionsHolderFile";
    testhelpers::generateOrEditTestFile(filePath);
    CPPUNIT_ASSERT(!std::filesystem::exists(filePath));

    {
        const PermissionsHolder permsHolder(tempDir.path(), Log::instance()->getLogger());
        testhelpers::generateOrEditTestFile(filePath);
        CPPUNIT_ASSERT(std::filesystem::exists(filePath));
    }

    const auto filePath2 = tempDir.path() / "testPermissionsHolderFile2";
    testhelpers::generateOrEditTestFile(filePath2);
    CPPUNIT_ASSERT(!std::filesystem::exists(filePath2));

    // Test with several instance of perms holder on the same folder
    bool dummyRead = false;
    bool write = false;
    bool dummyExec = false;
    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::getRights(tempDir.path(), dummyRead, write, dummyExec));
    CPPUNIT_ASSERT_EQUAL(false, write);
    {
        const PermissionsHolder permsHolder(tempDir.path(), Log::instance()->getLogger());
        CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::getRights(tempDir.path(), dummyRead, write, dummyExec));
        CPPUNIT_ASSERT_EQUAL(true, write);
        {
            const PermissionsHolder permsHolder2(tempDir.path(), Log::instance()->getLogger());
            CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::getRights(tempDir.path(), dummyRead, write, dummyExec));
            CPPUNIT_ASSERT_EQUAL(true, write);
            {
                const PermissionsHolder permsHolder3(tempDir.path(), Log::instance()->getLogger());
                CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::getRights(tempDir.path(), dummyRead, write, dummyExec));
                CPPUNIT_ASSERT_EQUAL(true, write);
            }
            CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::getRights(tempDir.path(), dummyRead, write, dummyExec));
            CPPUNIT_ASSERT_EQUAL(true, write);
        }
        CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::getRights(tempDir.path(), dummyRead, write, dummyExec));
        CPPUNIT_ASSERT_EQUAL(true, write);
    }
    CPPUNIT_ASSERT_EQUAL(IoError::Success, IoHelper::getRights(tempDir.path(), dummyRead, write, dummyExec));
    CPPUNIT_ASSERT_EQUAL(false, write);
}

} // namespace KDC
