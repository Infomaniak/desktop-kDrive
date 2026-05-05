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
#include "appserver.h"
#include "test_utility/localtemporarydirectory.h"

namespace KDC {

class MockAppServer : public AppServer {
    public:
        explicit MockAppServer(int &argc, char **argv) :
            AppServer(argc, argv) {}
        std::filesystem::path makeDbName() override;
        std::shared_ptr<ParmsDb> initParmsDB(const std::filesystem::path &dbPath, const std::string &version) override;
        bool startClient() override { return true; }
        void cleanup() override;

        void setParmsDbPath(const std::filesystem::path &path) { _parmsDbPath = path; }

        void setLoadUserInfoFunction(const std::function<ExitInfo(User &user, bool &updated)> &f) { _loadUserInfo = f; };
        void setLoadAccountInfoFunction(const std::function<ExitInfo(Account &account, bool &updated)> &f) {
            _loadAccountInfo = f;
        };
        void setLoadDriveInfoFunction(
                const std::function<ExitInfo(Drive &drive, const AccountId previousAccountId, AccountId &newAccountId,
                                             bool &updated, bool &quotaUpdated)> &f) {
            _loadDriveInfo = f;
        };

    private:
        void sendUserUpdated(const UserInfo &) const override { /* Do not try to notify the client */ };
        void sendAccountAdded(const AccountInfo &) const override { /* Do not try to notify the client */ };
        void sendAccountUpdated(const AccountInfo &) const override { /* Do not try to notify the client */ };
        void sendDriveUpdated(const DriveInfo &) const override { /* Do not try to notify the client */ };

        std::filesystem::path _parmsDbPath;
};

class TestAppServer : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestAppServer);
        CPPUNIT_TEST(testInitAndStopSyncPal);
        CPPUNIT_TEST(testStartAndStopSync);
        CPPUNIT_TEST(testUpdateUserInfo);
        CPPUNIT_TEST(testCleanup); // Must be the last test
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() final;
        void tearDown() override;

        void testInitAndStopSyncPal();
        void testStartAndStopSync();
        void testCleanup();
        void testUpdateUserInfo();

    private:
        MockAppServer *_appPtr;
        LocalTemporaryDirectory _localTempDir = LocalTemporaryDirectory("TestAppServer");

        bool waitForSyncStatus(int syncDbId, SyncStatus targetStatus) const;
        bool syncIsActive(int syncDbId) const;
};

} // namespace KDC
