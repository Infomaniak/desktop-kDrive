
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

#include "testfolderwatchermac.h"

#include "update_detection/file_system_observer/folderwatcher_mac.h"

#include <CoreServices/CoreServices.h>
#include <codecvt>

namespace KDC {

void TestFolderWatcher_mac::testGetOpType() {
    CPPUNIT_ASSERT_EQUAL(OperationType::Create, FolderWatcher_mac::getOpType(kFSEventStreamEventFlagItemCreated));
    CPPUNIT_ASSERT_EQUAL(OperationType::Delete, FolderWatcher_mac::getOpType(kFSEventStreamEventFlagItemRemoved));
    CPPUNIT_ASSERT_EQUAL(OperationType::Edit, FolderWatcher_mac::getOpType(kFSEventStreamEventFlagItemModified));
    CPPUNIT_ASSERT_EQUAL(OperationType::Edit, FolderWatcher_mac::getOpType(kFSEventStreamEventFlagItemInodeMetaMod));
    CPPUNIT_ASSERT_EQUAL(OperationType::Move, FolderWatcher_mac::getOpType(kFSEventStreamEventFlagItemRenamed));
    CPPUNIT_ASSERT_EQUAL(OperationType::Rights, FolderWatcher_mac::getOpType(kFSEventStreamEventFlagItemChangeOwner));
    CPPUNIT_ASSERT_EQUAL(OperationType::None, FolderWatcher_mac::getOpType(kFSEventStreamEventFlagNone));
    CPPUNIT_ASSERT_EQUAL(OperationType::None, FolderWatcher_mac::getOpType(kFSEventStreamEventFlagItemXattrMod));
}

} // namespace KDC
