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

#include "config.h"
#include "testutility.h"
#include "libcommon/utility/utility.h"
#include "testtypes.h"

namespace KDC {

void KDC::TestTypes::testOtherSide() {
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, otherSide(ReplicaSide::Remote));
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Remote, otherSide(ReplicaSide::Local));
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Unknown, otherSide(ReplicaSide::Unknown));
}
}  // namespace KDC