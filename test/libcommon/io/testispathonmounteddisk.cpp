/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

using namespace CppUnit;

namespace KDC {

void TestIo::testIsPathOnMountedDisk() {
    LocalTemporaryDirectory tempDir;
    bool isMounted = false;
    IoError ioError = IoError::Unknown;

    CPPUNIT_ASSERT(IoHelper::isPathOnMountedDisk(tempDir.path(), isMounted, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(isMounted);

    const SyncPath nonExistingPath = tempDir.path() / "missing_path";
    ioError = IoError::Unknown;
    isMounted = false;
    CPPUNIT_ASSERT(IoHelper::isPathOnMountedDisk(nonExistingPath, isMounted, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(isMounted);

#if defined(KD_WINDOWS)
    const SyncPath unmountedPath = SyncPath("X:\\") / "kdrive_unmounted";
#elif defined(KD_MACOS) || defined(KD_LINUX)
    const SyncPath unmountedPath = SyncPath("/kdrive_unmounted_disk") / "path";
#endif

    ioError = IoError::Unknown;
    isMounted = true;
    CPPUNIT_ASSERT(!IoHelper::isPathOnMountedDisk(unmountedPath, isMounted, ioError));
    CPPUNIT_ASSERT(ioError != IoError::Success);
    CPPUNIT_ASSERT(!isMounted);
}

} // namespace KDC
