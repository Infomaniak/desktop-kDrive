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
#include "libcommonserver/utility/utility.h"

#include <filesystem>

using namespace CppUnit;

namespace KDC {

static struct rightsSet {
        bool read;
        bool write;
        bool execute;
};

static void rightsSetFromInt(int rights, rightsSet& rightsSet) {
    rightsSet.read = rights & 4;
    rightsSet.write = rights & 2;
    rightsSet.execute = rights & 1;
}

static int rightsSetToInt(rightsSet rightsSet) {
    int rights = 0;
    if (rightsSet.read) {
        rights += 4;
    }
    if (rightsSet.write) {
        rights += 2;
    }
    if (rightsSet.execute) {
        rights += 1;
    }

    return rights;
}

void TestIo::testCheckSetAndGetRights() {
    #ifdef _WIN32
        CPPUNIT_ASSERT(Utility::init());  // Initialize the utility library, needed to access/change the permissions on Windows
    #endif

    const TemporaryDirectory temporaryDirectory;

    // Test if the rights are correctly set and get on a directory
    {
        const SyncPath path = temporaryDirectory.path / "changePerm";

        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(IoHelper::createDirectory(path, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        bool isReadable = false;
        bool isWritable = false;
        bool isExecutable = false;
        bool exists = false;

        rightsSet rightsSet = {false, false, false};

        // For a directory

        /* Test all the possible rights and the all the possible order of rights modification. ie:
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

        for (int baseRigths = 0; baseRigths < 7;
             baseRigths++) {  // Test all the possible rights and the all the possible order of rights modification
            for (int targetRigths = baseRigths + 1; targetRigths < 8; targetRigths++) {
                rightsSetFromInt(baseRigths, rightsSet);

                bool result = IoHelper::setRights(path, rightsSet.read, rightsSet.write, rightsSet.execute, ioError);
                result &= ioError == IoErrorSuccess;
                if (!result) {
                    IoHelper::setRights(path, true, true, true, ioError);
                    CPPUNIT_ASSERT(false /* Failed to set base rights */);
                }

                result = IoHelper::getRights(path, isReadable, isWritable, isExecutable, exists);
                if (!result) {
                    IoHelper::setRights(path, true, true, true, ioError);
                    CPPUNIT_ASSERT(false /* Failed to get base rights */);
                }

                if (!(exists && isReadable == rightsSet.read && isWritable == rightsSet.write &&
                      isExecutable == rightsSet.execute)) {
                    IoHelper::setRights(path, true, true, true, ioError);
                    std::cout << "setted rigths (RWX): " << rightsSet.read << rightsSet.write << rightsSet.execute
                              << " | readed rigths(RWX): " << isReadable << isWritable << isExecutable << std::endl;
                    CPPUNIT_ASSERT(false /* Set base rights mismatch  with get base rights */);
                }

                rightsSetFromInt(targetRigths, rightsSet);

                result = IoHelper::setRights(path, rightsSet.read, rightsSet.write, rightsSet.execute, ioError);
                result &= ioError == IoErrorSuccess;
                if (!result) {
                    IoHelper::setRights(path, true, true, true, ioError);
                    CPPUNIT_ASSERT(false /* Failed to set target rights */);
                }

                result = IoHelper::getRights(path, isReadable, isWritable, isExecutable, exists);
                if (!result) {
                    IoHelper::setRights(path, true, true, true, ioError);
                    CPPUNIT_ASSERT(false /* Failed to get target rights */);
                }

                if (!(exists && isReadable == rightsSet.read && isWritable == rightsSet.write &&
                      isExecutable == rightsSet.execute)) {
                    IoHelper::setRights(path, true, true, true, ioError);
                    std::cout << "setted rigths (RWX): " << rightsSet.read << rightsSet.write << rightsSet.execute
                              << " | readed rigths(RWX): " << isReadable << isWritable << isExecutable << std::endl;
                    CPPUNIT_ASSERT(false /* Set target rights mismatch with get target rights */);
                }
            }
        }
        // Restore the rights
        IoHelper::setRights(path, true, true, true, ioError);
    }

    // Test if the rights are correctly set and get on a file
    {
        const SyncPath filepath = temporaryDirectory.path / "changePerm.txt";

        IoError ioError = IoErrorUnknown;

        std::ofstream file(filepath.string());
        CPPUNIT_ASSERT(file.is_open());
        file << "testCheckSetAndGetRights";
        file.close();

        bool isReadable = false;
        bool isWritable = false;
        bool isExecutable = false;
        bool exists = false;

        rightsSet rightsSet = {false, false, false};

        // For a directory
        for (int baseRigths = 0; baseRigths < 7;
             baseRigths++) {  // Test all the possible rights and the all the possible order of rights modification
            for (int targetRigths = baseRigths + 1; targetRigths < 8; targetRigths++) {
                rightsSetFromInt(baseRigths, rightsSet);

                bool result = IoHelper::setRights(filepath, rightsSet.read, rightsSet.write, rightsSet.execute, ioError);
                result &= ioError == IoErrorSuccess;
                if (!result) {
                    IoHelper::setRights(filepath, true, true, true, ioError);
                    CPPUNIT_ASSERT(false /* Failed to set base rights */);
                }

                result = IoHelper::getRights(filepath, isReadable, isWritable, isExecutable, exists);
                if (!result) {
                    IoHelper::setRights(filepath, true, true, true, ioError);
                    CPPUNIT_ASSERT(false /* Failed to get base rights */);
                }

                if (!(exists && isReadable == rightsSet.read && isWritable == rightsSet.write &&
                      isExecutable == rightsSet.execute)) {
                    IoHelper::setRights(filepath, true, true, true, ioError);
                    CPPUNIT_ASSERT(false /* Set base rights mismatch  with get base rights */);
                }

                rightsSetFromInt(targetRigths, rightsSet);

                result = IoHelper::setRights(filepath, rightsSet.read, rightsSet.write, rightsSet.execute, ioError);
                result &= ioError == IoErrorSuccess;
                if (!result) {
                    IoHelper::setRights(filepath, true, true, true, ioError);
                    CPPUNIT_ASSERT(false /* Failed to set target rights */);
                }

                result = IoHelper::getRights(filepath, isReadable, isWritable, isExecutable, exists);
                if (!result) {
                    IoHelper::setRights(filepath, true, true, true, ioError);
                    CPPUNIT_ASSERT(false /* Failed to get target rights */);
                }

                if (!(exists && isReadable == rightsSet.read && isWritable == rightsSet.write &&
                      isExecutable == rightsSet.execute)) {
                    IoHelper::setRights(filepath, true, true, true, ioError);
                    CPPUNIT_ASSERT(false /* Set target rights mismatch with get target rights */);
                }
            }
        }

        // Restore the rights
        IoHelper::setRights(filepath, true, true, true, ioError);
    }

    // Check permission is not recursive on folder
    {
        const SyncPath path = temporaryDirectory.path / "testCheckSetAndGetRights";
        const SyncPath subFolderPath = path / "subFolder";
        const SyncPath subFilePath = path / "subFile.txt";


        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(IoHelper::createDirectory(path, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        CPPUNIT_ASSERT(IoHelper::createDirectory(subFolderPath, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        std::ofstream file(subFilePath.string());
        CPPUNIT_ASSERT(file.is_open());
        file << "testCheckSetAndGetRights";
        file.close();


        bool isReadable = false;
        bool isWritable = false;
        bool isExecutable = false;
        bool exists = false;

        bool result = IoHelper::setRights(path, true, true , true, ioError);
        bool result = IoHelper::setRights(subFolderPath, true, true, true, ioError);
        bool result = IoHelper::setRights(subFilePath, true, true, true, ioError);

        CPPUNIT_ASSERT(IoHelper::getRights(path, isReadable, isWritable, isExecutable, exists));
        CPPUNIT_ASSERT(exists && isReadable && isWritable && isExecutable);

        CPPUNIT_ASSERT(IoHelper::getRights(subFolderPath, isReadable, isWritable, isExecutable, exists));
        CPPUNIT_ASSERT(exists && isReadable && isWritable && isExecutable);

        CPPUNIT_ASSERT(IoHelper::getRights(subFilePath, isReadable, isWritable, isExecutable, exists));
        CPPUNIT_ASSERT(exists && isReadable && isWritable && isExecutable);

        result = IoHelper::setRights(path, true, false, true, ioError);
        result &= ioError == IoErrorSuccess;
        if (!result) {
            IoHelper::setRights(path, true, true, true, ioError);  // Restore the rights for delete
            CPPUNIT_ASSERT(false /* Failed to set base rights */);
        }

        CPPUNIT_ASSERT(IoHelper::getRights(path, isReadable, isWritable, isExecutable, exists));
        CPPUNIT_ASSERT(exists && isReadable && !isWritable && isExecutable);
        CPPUNIT_ASSERT(IoHelper::getRights(subFolderPath, isReadable, isWritable, isExecutable, exists));
        CPPUNIT_ASSERT(exists && isReadable && isWritable && isExecutable);
        CPPUNIT_ASSERT(IoHelper::getRights(subFilePath, isReadable, isWritable, isExecutable, exists));
        CPPUNIT_ASSERT(exists && isReadable && isWritable && isExecutable);

        // Restore the rights
        IoHelper::setRights(path, true, true, true, ioError);  // Restore the rights for delete
    }

    // Test on a non existing file
    {
        const SyncPath path = temporaryDirectory.path / "testCheckSetAndGetRights/nonExistingFile.txt";
        bool isReadable = false;
        bool isWritable = false;
        bool isExecutable = false;
        bool exists = false;
        IoError ioError = IoErrorUnknown;

        CPPUNIT_ASSERT(IoHelper::getRights(path, isReadable, isWritable, isExecutable, exists));
        CPPUNIT_ASSERT(!exists);

        CPPUNIT_ASSERT(IoHelper::setRights(path, true, true, true, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorNoSuchFileOrDirectory);
    }
}
}  // namespace KDC
