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

#include "testlitesyncextconnector.h"

#include "io/filestat.h"
#include "io/iohelper.h"
#include "server/vfs/mac/litesyncextconnector.h"

using namespace CppUnit;

namespace KDC {

TestLiteSyncExtConnector::TestLiteSyncExtConnector() : CppUnit::TestFixture() {}

void TestLiteSyncExtConnector::setUp() {}

void TestLiteSyncExtConnector::tearDown() {}

void TestLiteSyncExtConnector::testGetVfsStatus() {
    // Get status of a non existing file
    {
        bool isPlaceholder = false;
        bool isHydrated = false;
        bool isSyncing = false;
        int progress = 0;

        // vfsGetStatus returns `true` if the file path indicates a non-existing item.
        log4cplus::Logger logger;
        CPPUNIT_ASSERT(LiteSyncExtConnector::vfsGetStatus("this_file_does_not_exist.txt", isPlaceholder, isHydrated, isSyncing,
                                                          progress, logger));
        CPPUNIT_ASSERT(!isPlaceholder);
        CPPUNIT_ASSERT(!isHydrated);
    }
    // Get status of an existing file that is not yet a placeholder
    {
        bool isPlaceholder = false;
        bool isHydrated = false;
        bool isSyncing = false;
        int progress = 0;

        // vfsGetStatus returns `true` if the file path indicates a non-existing item.
        log4cplus::Logger logger;
        CPPUNIT_ASSERT(
            LiteSyncExtConnector::vfsGetStatus("/Users/clement/Projects/desktop-kDrive/test/test_ci/test_pictures/picture-1.jpg",
                                               isPlaceholder, isHydrated, isSyncing, progress, logger));
        CPPUNIT_ASSERT(!isPlaceholder);
        CPPUNIT_ASSERT(!isHydrated);
    }
    {
        FileStat buf;
        IoError ioError = IoErrorUnknown;
        IoHelper::getFileStat("/Users/clement/kDrive2/Move-Delete_tree.pdf", &buf, ioError);
        int64_t testSize = buf.size;
    }
}

}  // namespace KDC
