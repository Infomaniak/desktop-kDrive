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

#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

namespace KDC {

class TestTypes : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(TestTypes);
        CPPUNIT_TEST(testOtherSide);
        CPPUNIT_TEST(testStreamConversion);
        CPPUNIT_TEST(testExitInfo);
        CPPUNIT_TEST_SUITE_END();

    protected:
        void testOtherSide();
        void testStreamConversion();
        void testExitInfo();
};
} // namespace KDC
