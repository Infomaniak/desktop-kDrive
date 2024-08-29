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
#include "db/syncdb.h"

using namespace CppUnit;

namespace KDC {

class TestSyncDb : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(TestSyncDb);
        CPPUNIT_TEST(testNodes);
        CPPUNIT_TEST(testSyncNodes);
        CPPUNIT_TEST(testCorrespondingNodeId);
        CPPUNIT_TEST(testUpdateLocalName);
        CPPUNIT_TEST(testUpgradeTo3_6_5);
        CPPUNIT_TEST(testUpgradeTo3_6_5_checkNodeMap);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    protected:
        void testNodes();
        void testSyncNodes();
        void testCorrespondingNodeId();
        void testUpdateLocalName();
        void testUpgradeTo3_6_5();
        void testUpgradeTo3_6_5_checkNodeMap();

    private:
        SyncDb *_testObj;
        std::vector<DbNode> setupSyncDb_3_6_5();
};

}  // namespace KDC
