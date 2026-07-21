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
#include "test_utility/localtemporarydirectory.h"

namespace KDC {

class TestServerRequests : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestServerRequests);
        CPPUNIT_TEST(testFixProxyConfig);
        CPPUNIT_TEST(testGetPublicLink);
        CPPUNIT_TEST(testFindGoodPathForNewSync);
        CPPUNIT_TEST(testDeleteUser);
        CPPUNIT_TEST(testDeleteUserNotFound);
        CPPUNIT_TEST(testDeleteAccount);
        CPPUNIT_TEST(testDeleteDrive);
        CPPUNIT_TEST(testFolderContainsNonExcludedItemInvalidPath);
        CPPUNIT_TEST(testFolderContainsNonExcludedItemEmptyDir);
        CPPUNIT_TEST(testFolderContainsNonExcludedItemOnlyExcludedFiles);
        CPPUNIT_TEST(testFolderContainsNonExcludedItemWithNonExcludedFile);
        CPPUNIT_TEST(testFolderContainsNonExcludedItemMixed);
        CPPUNIT_TEST(isSyncFolderAllowedByRules_allowsAnyPathWhenNoRulesExist);
        CPPUNIT_TEST(isSyncFolderAllowedByRules_deniesPathNotMatchingAnyRule);
        CPPUNIT_TEST(isSyncFolderAllowedByRules_allowsPathMatchingWhiteListRule);
        CPPUNIT_TEST(isSyncFolderAllowedByRules_allowsSubfolderOfWhiteListRule);
        CPPUNIT_TEST(isSyncFolderAllowedByRules_deniesPathMatchingBlackListRule);
        CPPUNIT_TEST(isSyncFolderAllowedByRules_deniesSubfolderOfBlackListRule);
        CPPUNIT_TEST(isSyncFolderAllowedByRules_allowsSubfolderOfWhiteListSubFolderRule);
        CPPUNIT_TEST(isSyncFolderAllowedByRules_deniesExactPathOfWhiteListSubFolderRule);
        CPPUNIT_TEST(isSyncFolderAllowedByRules_deeperRuleWinsOverShallowerRule);
        CPPUNIT_TEST(isSyncFolderAllowedByRules_blackListSubfolderInsideWhiteListSubFolderParent);
        CPPUNIT_TEST(isSyncFolderAllowedByRules_expandsHomeDirVariable);

        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() final;
        void tearDown() override;

        void testFixProxyConfig();
        void testGetPublicLink();
        void testFindGoodPathForNewSync();
        void testDeleteUser();
        void testDeleteUserNotFound();
        void testDeleteAccount();
        void testDeleteDrive();
        void testFolderContainsNonExcludedItemInvalidPath();
        void testFolderContainsNonExcludedItemEmptyDir();
        void testFolderContainsNonExcludedItemOnlyExcludedFiles();
        void testFolderContainsNonExcludedItemWithNonExcludedFile();
        void testFolderContainsNonExcludedItemMixed();

        void isSyncFolderAllowedByRules_allowsAnyPathWhenNoRulesExist();
        void isSyncFolderAllowedByRules_allowsPathNotMatchingAnyRule();
        void isSyncFolderAllowedByRules_allowsPathMatchingWhiteListRule();
        void isSyncFolderAllowedByRules_allowsSubfolderOfWhiteListRule();
        void isSyncFolderAllowedByRules_deniesPathMatchingBlackListRule();
        void isSyncFolderAllowedByRules_deniesSubfolderOfBlackListRule();
        void isSyncFolderAllowedByRules_allowsSubfolderOfWhiteListSubFolderRule();
        void isSyncFolderAllowedByRules_deniesExactPathOfWhiteListSubFolderRule();
        void isSyncFolderAllowedByRules_deeperRuleWinsOverShallowerRule();
        void isSyncFolderAllowedByRules_blackListSubfolderInsideWhiteListSubFolderParent();
        void isSyncFolderAllowedByRules_expandsHomeDirVariable();

    private:
        int _driveDbId{0};
        std::string _keychainKey{"123"};
        LocalTemporaryDirectory _localTempDir{"testServerRequests"};
};

} // namespace KDC
