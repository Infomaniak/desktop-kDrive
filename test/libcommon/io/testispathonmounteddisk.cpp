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

#if defined(KD_WINDOWS)
#include <Windows.h>
#endif

using namespace CppUnit;

namespace KDC {

void TestIo::testIsPathOnMountedDisk() {
    LocalTemporaryDirectory tempDir;
    bool isMounted = false;
    IoError ioError = IoError::Unknown;

    // Check that an existing path on a mounted disk is correctly detected as such.
    CPPUNIT_ASSERT(IoHelper::isPathOnMountedDisk(tempDir.path(), isMounted, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(isMounted);

    // Check that a path on a mounted disk is correctly detected as such, even if it does not exist.
    const SyncPath nonExistingPath = tempDir.path() / "missing_path";
    ioError = IoError::Unknown;
    isMounted = false;
    CPPUNIT_ASSERT(IoHelper::isPathOnMountedDisk(nonExistingPath, isMounted, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(isMounted);

    // Check that an unmounted path is correctly detected as such. We test with a path that is very likely to not exist on the
    // system, to avoid any false positive.
#if defined(KD_WINDOWS)
    // Find a drive letter that is not currently used by the system to test an unmounted path. We start from Z: and go down to D:
    DWORD driveMask = GetLogicalDrives();
    wchar_t driveLetter = 0;
    for (wchar_t letter = L'Z'; letter >= L'D'; --letter) {
        const DWORD bit = 1u << static_cast<DWORD>(letter - L'A');
        if ((driveMask & bit) == 0) {
            driveLetter = letter;
            break;
        }
    }

    if (driveLetter == 0) {
        return;
    }

    const SyncPath unmountedPath = SyncPath(std::wstring(1, driveLetter) + L":\\") / "kdrive_unmounted";
#elif defined(KD_MACOS)
    const SyncPath unmountedPath = SyncPath("/Volumes/kdrive_unmounted") / "path";
#elif defined(KD_LINUX)
    const SyncPath unmountedPath = SyncPath("/mnt/kdrive_unmounted") / "path";
#endif

    ioError = IoError::Unknown;
    isMounted = true;
    CPPUNIT_ASSERT(IoHelper::isPathOnMountedDisk(unmountedPath, isMounted, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(!isMounted);
}

} // namespace KDC
