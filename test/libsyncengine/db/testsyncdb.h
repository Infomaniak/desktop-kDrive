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

class SyncDbMock : public SyncDb {
    public:
        SyncDbMock(const std::string &dbPath, const std::string &version, const std::string &targetNodeId = std::string());
        bool prepare() override;
        void freeRequest(const char *requestId);
        void enablePrepare(bool enabled);

    private:
        bool _isPrepareEnabled{false};
};

class TestSyncDb : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestSyncDb);
        CPPUNIT_TEST(testNodes);
        CPPUNIT_TEST(testSyncNodes);
        CPPUNIT_TEST(testCorrespondingNodeId);
        CPPUNIT_TEST(testUpdateLocalName);
        CPPUNIT_TEST(testUpgradeTo3_6_7);
        CPPUNIT_TEST(testUpgradeTo3_6_5CheckNodeMap);
        CPPUNIT_TEST(testUpgradeTo3_6_5);
        CPPUNIT_TEST(testInit3_6_4);
        CPPUNIT_TEST(testDummyUpgrade);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    protected:
        void testNodes();
        void testSyncNodes();
        void testCorrespondingNodeId();
        void testUpdateLocalName();
        void testUpgradeTo3_6_7();
        void testUpgradeTo3_6_5();
        void testUpgradeTo3_6_5CheckNodeMap();
        void testInit3_6_4();
        void testDummyUpgrade();

    private:
        SyncDbMock *_testObj;
        // Note: the node ID value "1" is reserved for the root node of any synchronisation for both local and remote sides.
        std::vector<DbNode> setupSyncDb3_6_5(const std::vector<NodeId> &localNodeIds = {"2", "3", "4", "5", "6"});
};

} // namespace KDC
