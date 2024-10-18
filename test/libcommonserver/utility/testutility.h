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
#include "libcommonserver/utility/utility.h"

using namespace CppUnit;

namespace KDC {

class TestUtility : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(TestUtility);
        CPPUNIT_TEST(testFreeDiskSpace);
        CPPUNIT_TEST(testIsCreationDateValid);
        CPPUNIT_TEST(testS2ws);
        CPPUNIT_TEST(testWs2s);
        CPPUNIT_TEST(testLtrim);
        CPPUNIT_TEST(testRtrim);
        CPPUNIT_TEST(testTrim);
        CPPUNIT_TEST(testMsSleep);
        CPPUNIT_TEST(testV2ws);
        CPPUNIT_TEST(testFormatStdError);
        CPPUNIT_TEST(testFormatIoError);
        CPPUNIT_TEST(testFormatSyncName);
        CPPUNIT_TEST(testFormatPath);
        CPPUNIT_TEST(testFormatSyncPath);
        CPPUNIT_TEST(testFormatRequest);
        CPPUNIT_TEST(testFileSystemName);
        CPPUNIT_TEST(testStartsWith);
        CPPUNIT_TEST(testStartsWithInsensitive);
        CPPUNIT_TEST(testEndsWith);
        CPPUNIT_TEST(testEndsWithInsensitive);
        CPPUNIT_TEST(testIsEqualInsensitive);
        CPPUNIT_TEST(testMoveItemToTrash);
        CPPUNIT_TEST(testStr2HexStr);
        CPPUNIT_TEST(testStrHex2Str);
        CPPUNIT_TEST(testSplitStr);
        CPPUNIT_TEST(testJoinStr);
        CPPUNIT_TEST(testPathDepth);
        CPPUNIT_TEST(testComputeMd5Hash);
        CPPUNIT_TEST(testXxHash);
        CPPUNIT_TEST(testToUpper);
        CPPUNIT_TEST(testErrId);
        CPPUNIT_TEST(testcheckIfDirEntryIsManaged);
        CPPUNIT_TEST(isSubDir);
        CPPUNIT_TEST(testNormalizedSyncName);
        CPPUNIT_TEST(testNormalizedSyncPath);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp();
        void tearDown();

    protected:
        void testFreeDiskSpace();
        void testIsCreationDateValid();
        void testS2ws();
        void testWs2s();
        void testLtrim();
        void testRtrim();
        void testTrim();
        void testMsSleep();
        void testV2ws();
        void testFormatStdError();
        void testFormatIoError();
        void testFormatPath();
        void testFormatSyncName();
        void testFormatSyncPath();
        void testFormatRequest();
        void testFileSystemName();
        void testStartsWith();
        void testStartsWithInsensitive();
        void testEndsWith();
        void testEndsWithInsensitive();
        void testIsEqualInsensitive();
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
        void testToUpper();
        void testErrId();
        void isSubDir();
        void testcheckIfDirEntryIsManaged();
        void testNormalizedSyncName();
        void testNormalizedSyncPath();

    private:
        std::unique_ptr<Utility> _testObj;

        bool checkNfcAndNfdNamesEqual(const SyncName &name, bool &equal);
};

} // namespace KDC
