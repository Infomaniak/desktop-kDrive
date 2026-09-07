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

#pragma once

#include "testincludes.h"
#include "test_utility/localtemporarydirectory.h"

#include "utility/types.h"

using namespace CppUnit;

namespace KDC {

class TestSearchJob : public CppUnit::TestFixture, public TestBase {
    public:
        CPPUNIT_TEST_SUITE(TestSearchJob);
        CPPUNIT_TEST(testHandleResponsePrivatePath);
        CPPUNIT_TEST(testHandleResponseSharedPath);
        CPPUNIT_TEST(testHandleResponseLeadingSlash);
        CPPUNIT_TEST(testHandleResponseIsAvailableLocally);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    protected:
        void testHandleResponsePrivatePath();
        void testHandleResponseSharedPath();
        void testHandleResponseLeadingSlash();
        void testHandleResponseIsAvailableLocally();

    private:
        DriveDbId _driveDbId = 1;
        LocalTemporaryDirectory _localTempDir{"testSearchJob"};
};

} // namespace KDC
