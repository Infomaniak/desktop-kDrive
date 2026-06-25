/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "libcommon/utility/types.h"
#include "db/migration/v3migration.h"

using namespace CppUnit;

namespace KDC {

class V3MigrationMock : public V3Migration {
    public:
        V3MigrationMock(std::shared_ptr<SyncDb> synDbPtr) :
            V3Migration(synDbPtr){};
        virtual ~V3MigrationMock() = default;

    private:
        bool getPrivateDirRemoteNodeId(const DriveDbId, RemoteNodeId &remoteNodeId) override {
            remoteNodeId = RemoteNodeId{"666"};
            return true;
        }

        friend class TestV3Migration;
};

class TestV3Migration : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestV3Migration);
        CPPUNIT_TEST(testMigrateLocalItemsToPrivateDir);
        CPPUNIT_TEST(testMigrationOfNonRootAdvancedSync);
        CPPUNIT_TEST(testNoOpMigration);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    protected:
        void testMigrateLocalItemsToPrivateDir();
        void testMigrationOfNonRootAdvancedSync();
        void testNoOpMigration();

    private:
        std::unique_ptr<V3MigrationMock> _testObj{nullptr};

        DriveDbId _driveDbId{1};
        enum class SyncType {
            Advanced,
            Standard
        };
        void createParmsDb(const SyncPath &syncDbPath, const SyncPath &localPath, SyncType syncType = SyncType::Standard) const;


        struct MigrationFileSetup {
                std::vector<SyncPath> movedItems;
                std::vector<SyncPath> remainingItems;
        };

        // Initial setup to test the migration to local Private folder in keeping with the backend API v3.
        MigrationFileSetup setupSyncMigrationToLocalPrivateDir(const SyncPath &localPath);
};
} // namespace KDC
