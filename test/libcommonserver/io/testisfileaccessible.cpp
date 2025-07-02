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

#include "libcommonserver/utility/utility.h"

#include <thread>

using namespace CppUnit;

namespace KDC {

void TestIo::testIsFileAccessible() {
    const LocalTemporaryDirectory temporaryDirectory;
    const SyncPath sourcePath = temporaryDirectory.path() / "test_big_file.txt";
    const SyncPath destPath = temporaryDirectory.path() / "test_big_file_copy.txt";

    // Create a 100 MB file
    {
        std::ofstream sourceFile(sourcePath);
        sourceFile << std::string(100000000, 'z');
    }

    // Copy file
    std::thread copyFile([](const SyncPath &fromPath, const SyncPath &toPath) { std::filesystem::copy(fromPath, toPath); },
                         sourcePath, destPath);
    copyFile.detach();

    Utility::msleep(10);

    IoError ioError = IoError::Unknown;
    bool res = IoHelper::isFileAccessible(destPath, ioError);
    // IoHelper::isFileAccessible returns instantly `true` on MacOSX and Linux.
#if defined(KD_WINDOWS)
    CPPUNIT_ASSERT(!res);
#else
    (void) res;
#endif

    // File copy not finished yet.
    Utility::msleep(1000);

    // File copy is now finished.
    res = IoHelper::isFileAccessible(destPath, ioError);
    CPPUNIT_ASSERT(res);
}

} // namespace KDC
