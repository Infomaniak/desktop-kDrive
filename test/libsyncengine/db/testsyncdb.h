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
        CPPUNIT_TEST(testUpgradeTo3_6_5CheckNodeMap);
        CPPUNIT_TEST(testUpgradeTo3_6_5);
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
        void testUpgradeTo3_6_5CheckNodeMap();

    private:
        class SyncDbMock : public SyncDb {
            public:
                SyncDbMock(const std::string &dbPath, const std::string &version, const std::string &targetNodeId = {}) :
                    SyncDb(dbPath, version, targetNodeId) {};
                void setNormalizationEnabled(bool enabled) { _normalizeNames = enabled; };

            protected:
                void updateNames(const char *requestId, const SyncName &localName, const SyncName &remoteName) override {
                    SyncName inputLocalName = localName, inputRemoteName = remoteName;
                    if (_normalizeNames) {
                        inputLocalName = Utility::normalizedSyncName(inputLocalName);
                        inputRemoteName = Utility::normalizedSyncName(inputRemoteName);
                    }
                    queryBindValue(requestId, 2, inputLocalName);
                    queryBindValue(requestId, 3, inputRemoteName);
                };

            private:
                bool _normalizeNames = true;
        };

        SyncDbMock *_testObj;
        // Note: the node ID value "1" is reserved for the root node of any synchronisation for both local and remote sides.
        std::vector<DbNode> setupSyncDb3_6_5(const std::vector<NodeId> &localNodeIds = {"2", "3", "4", "5", "6"});
};

} // namespace KDC
