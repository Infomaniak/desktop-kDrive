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
#include "test_utility/localtemporarydirectory.h"

namespace KDC {

class TestServerRequests : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestServerRequests);
        CPPUNIT_TEST(testFixProxyConfig);
        CPPUNIT_TEST(testGetPublicLink);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() final;
        void tearDown() override;

        void testFixProxyConfig();
        void testGetPublicLink();

    private:
        int _driveDbId{0};
        LocalTemporaryDirectory _localTempDir{"testServerRequests"};
};

} // namespace KDC
