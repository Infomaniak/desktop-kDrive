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

#include "testfsoperation.h"
#include "test_utility/testhelpers.h"

#include "update_detection/file_system_observer/fsoperation.h"


using namespace CppUnit;

namespace KDC {

void TestFsOperation::testConstructor() {
    const SyncPath syncPath = testhelpers::makeNfdSyncName();
    const SyncPath destPath = testhelpers::makeNfdSyncName();
    const auto op = FSOperation(OperationType::Create, "node_1", NodeType::File, 10, -10, 0, syncPath, destPath);

    CPPUNIT_ASSERT(op.path() == syncPath);
    CPPUNIT_ASSERT(op.destinationPath() == destPath);
}


} // namespace KDC
