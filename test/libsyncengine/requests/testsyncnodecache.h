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
#include "db/syncdb.h"

using namespace CppUnit;

namespace KDC {

class TestSyncNodeCache : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestSyncNodeCache);
        CPPUNIT_TEST(testContainsSyncNode);
        CPPUNIT_TEST(testDeleteSyncNode);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    protected:
        void testContainsSyncNode();
        void testDeleteSyncNode();

    private:
        std::shared_ptr<SyncDb> _testObj;
};
} // namespace KDC
