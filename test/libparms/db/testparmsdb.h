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

#include "testincludes.h"
#include "libparms/db/parmsdb.h"

#include <filesystem>

using namespace CppUnit;

namespace KDC {

class TestParmsDb : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestParmsDb);
        CPPUNIT_TEST(testParameters);
        CPPUNIT_TEST(testUser);
        CPPUNIT_TEST(testAccount);
        CPPUNIT_TEST(testDrive);
        CPPUNIT_TEST(testSync);
        CPPUNIT_TEST(testExclusionTemplate);
        CPPUNIT_TEST(testUpdateExclusionTemplates);
        CPPUNIT_TEST(testUpgrade);
#ifdef __APPLE__
        CPPUNIT_TEST(testExclusionApp);
#endif
        CPPUNIT_TEST(testError);
        CPPUNIT_TEST(testAppState);
#ifdef _WIN32
        CPPUNIT_TEST(testUpgradeOfShortPathNames);
#endif
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp();
        void tearDown();

    protected:
        void testParameters();
        void testUser();
        void testAccount();
        void testDrive();
        void testSync();
        void testExclusionTemplate();
        void testAppState();
        void testUpdateExclusionTemplates();
        void testUpgrade();
#ifdef __APPLE__
        void testExclusionApp();
#endif
        void testError();
#ifdef _WIN32
        void testUpgradeOfShortPathNames();
#endif
};

} // namespace KDC
