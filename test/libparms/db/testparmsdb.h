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

#include "testincludes.h"
#include "libparms/db/parmsdb.h"

#include <filesystem>

using namespace CppUnit;

namespace KDC {

class TestParmsDb : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(TestParmsDb);
        CPPUNIT_TEST(testParameters);
        CPPUNIT_TEST(testUser);
        CPPUNIT_TEST(testAccount);
        CPPUNIT_TEST(testDrive);
        CPPUNIT_TEST(testSync);
        CPPUNIT_TEST(testExclusionTemplate);
#ifdef __APPLE__
        CPPUNIT_TEST(testExclusionApp);
#endif
        CPPUNIT_TEST(testError);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp(void);
        void tearDown(void);

    protected:
        void testParameters(void);
        void testUser(void);
        void testAccount(void);
        void testDrive(void);
        void testSync(void);
        void testExclusionTemplate(void);
#ifdef __APPLE__
        void testExclusionApp(void);
#endif
        void testError(void);
};

}  // namespace KDC
