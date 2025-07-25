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
#include "test_utility/testhelpers.h"
#include "libcommonserver/utility/utility.h"

using namespace CppUnit;

namespace KDC {

class TestUtility : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestUtility);
        CPPUNIT_TEST(testFreeDiskSpace);
        CPPUNIT_TEST(testIsCreationDateValid);
        CPPUNIT_TEST(testMsSleep);
        CPPUNIT_TEST(testV2ws);
        CPPUNIT_TEST(testFormatStdError);
        CPPUNIT_TEST(testFormatIoError);
        CPPUNIT_TEST(testFormatSyncName);
        CPPUNIT_TEST(testFormatPath);
        CPPUNIT_TEST(testFormatSyncPath);
        CPPUNIT_TEST(testFormatRequest);
        CPPUNIT_TEST(testIsEqualUpToCaseAndEnc);
        CPPUNIT_TEST(testMoveItemToTrash);
        CPPUNIT_TEST(testStr2HexStr);
        CPPUNIT_TEST(testStrHex2Str);
        CPPUNIT_TEST(testSplitStr);
        CPPUNIT_TEST(testJoinStr);
        CPPUNIT_TEST(testPathDepth);
        CPPUNIT_TEST(testComputeMd5Hash);
        CPPUNIT_TEST(testXxHash);
        CPPUNIT_TEST(testErrId);
        CPPUNIT_TEST(testCheckIfDirEntryIsManaged);
        CPPUNIT_TEST(testIsSubDir);
        CPPUNIT_TEST(testIsDiskRootFolder);
        CPPUNIT_TEST(testNormalizedSyncName);
        CPPUNIT_TEST(testNormalizedSyncPath);
        CPPUNIT_TEST(testUserName);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override { TestBase::start(); }
        void tearDown() override { TestBase::stop(); }

    protected:
        void testFreeDiskSpace();
        void testIsCreationDateValid();
        void testMsSleep();
        void testV2ws();
        void testFormatStdError();
        void testFormatIoError();
        void testFormatPath();
        void testFormatSyncName();
        void testFormatSyncPath();
        void testFormatRequest();
        void testIsEqualUpToCaseAndEnc();
        void testMoveItemToTrash();
        void testGetLinuxDesktopType();
        void testGetAppSupportDir();
        void testStr2HexStr();
        void testStrHex2Str();
        void testSplitStr();
        void testJoinStr();
        void testPathDepth();
        void testComputeMd5Hash();
        void testXxHash();
        void testErrId();
        void testIsSubDir();
        void testIsDiskRootFolder();
        void testCheckIfDirEntryIsManaged();
        void testNormalizedSyncName();
        void testNormalizedSyncPath();
        void testUserName();

    private:
        bool checkNfcAndNfdNamesEqual(const SyncName &name, bool &equal);
};

} // namespace KDC
