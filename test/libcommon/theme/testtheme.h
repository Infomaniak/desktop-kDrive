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
#include "test_utility/testbase.h"

namespace KDC {

class TestTheme : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestTheme);
        CPPUNIT_TEST(testSingleton);
        CPPUNIT_TEST(testAppNames);
        CPPUNIT_TEST(testVersion);
        CPPUNIT_TEST(testUrls);
        CPPUNIT_TEST(testSystrayIcons);
        CPPUNIT_TEST(testSharingConfig);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override { TestBase::start(); }
        void tearDown() override { TestBase::stop(); }

    protected:
        void testSingleton();
        void testAppNames();
        void testVersion();
        void testUrls();
        void testSystrayIcons();
        void testSharingConfig();
};

} // namespace KDC
