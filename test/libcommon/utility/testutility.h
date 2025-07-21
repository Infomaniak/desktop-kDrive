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

#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "test_utility/testbase.h"
#include "libcommon/utility/types.h"

namespace KDC {

class TestUtility : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestUtility);
        CPPUNIT_TEST(testGetAppSupportDir);
        CPPUNIT_TEST(testIsVersionLower);
        CPPUNIT_TEST(testStringToAppStateValue);
        CPPUNIT_TEST(testArgsWriter);
        CPPUNIT_TEST(testCompressFile);
        CPPUNIT_TEST(testCurrentVersion);
        CPPUNIT_TEST(testSourceLocation);
        CPPUNIT_TEST(testGenerateRandomStringAlphaNum);
        CPPUNIT_TEST(testLanguageCode);
        CPPUNIT_TEST(testIsSupportedLanguage);
        CPPUNIT_TEST(testTruncateLongLogMessage);
        CPPUNIT_TEST(testLogIfFail);
        CPPUNIT_TEST(testRelativePath);
        CPPUNIT_TEST(testSplitSyncName);
        CPPUNIT_TEST(testSplitSyncPath);
        CPPUNIT_TEST(testSplitPathFromSyncName);
        CPPUNIT_TEST(testComputeSyncNameNormalizations);
        CPPUNIT_TEST(testComputePathNormalizations);
#if defined(KD_WINDOWS)
        CPPUNIT_TEST(testGetLastErrorMessage);
#endif
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp(void) override { TestBase::start(); }
        void tearDown(void) override { TestBase::stop(); }

    protected:
        void testGetAppSupportDir();
        void testIsVersionLower();
        void testStringToAppStateValue();
        void testArgsWriter();
        void testCompressFile();
        void testCurrentVersion();
        void testSourceLocation();
        void testGenerateRandomStringAlphaNum();
        void testLanguageCode();
        void testIsSupportedLanguage();
        void testTruncateLongLogMessage();
        void testLogIfFail();
        void testRelativePath();
        void testSplitSyncName();
        void testSplitSyncPath();
        void testSplitPathFromSyncName();
        void testComputeSyncNameNormalizations();
        void testComputePathNormalizations();

#if defined(KD_WINDOWS)
        void testGetLastErrorMessage();
#endif

    private:
        /* Generate all the possible path for a set of items and separators
         * result vector will contain all the generated paths, eg:
         *  itemsNames = {"Dir1", "Dir2", "Dir3"}
         *  separators = {'/', '\\'}
         *  result = {"Dir1/Dir2/Dir3/", "Dir1\\Dir2/Dir3/", "Dir1/Dir2\\Dir3", "Dir1/Dir2/Dir3\\", "Dir1\\Dir2\\Dir3",
         * "Dir1\\Dir2\\Dir3\\", ... }
         *
         * @param itemsNames: the list of items
         * @param separators: the list of separators
         * @param startWithSeparator: if the path should start with a separator
         * @param result: the list of generated paths
         */
        void generatePaths(const std::vector<std::string> &itemsNames, const std::vector<char> &separators,
                           bool startWithSeparator, std::vector<SyncPath> &result, const std::string &start = "", size_t pos = 0);
};


} // namespace KDC
