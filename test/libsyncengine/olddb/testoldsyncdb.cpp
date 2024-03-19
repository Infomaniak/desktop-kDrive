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

#include "testoldsyncdb.h"
#include "libcommonserver/utility/asserts.h"


using namespace CppUnit;

namespace KDC {

const std::string oldSyncDbPath = "/Users/chrilarc/kDrive2/.sync_accaa3d05def.db";

void TestOldSyncDb::setUp() {
    // Create DB
    _testObj = new OldSyncDb(oldSyncDbPath);
}

void TestOldSyncDb::tearDown() {
    // Close and delete DB
    _testObj->close();
    delete _testObj;
}

void TestOldSyncDb::testSelectiveSync() {
    CPPUNIT_ASSERT(_testObj->exists());

    std::list<std::pair<std::string, int>> selectiveSyncList;
    CPPUNIT_ASSERT(_testObj->selectAllSelectiveSync(selectiveSyncList));
    CPPUNIT_ASSERT(selectiveSyncList.size() > 0);
}

}  // namespace KDC
