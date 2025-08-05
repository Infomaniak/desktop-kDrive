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

using namespace CppUnit;

namespace KDC {

class TestLiteSyncExtConnector : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestLiteSyncExtConnector);
#if defined(KD_MACOS)
        CPPUNIT_TEST(testGetVfsStatus);
#endif
        CPPUNIT_TEST_SUITE_END();

    public:
        TestLiteSyncExtConnector();
        void setUp(void);
        void tearDown(void);

    protected:
#if defined(KD_MACOS)
        void testGetVfsStatus(void);
#endif
};

} // namespace KDC
