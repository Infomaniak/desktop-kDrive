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

#include <log4cplus/logger.h>

using namespace CppUnit;

namespace KDC {

class TestLogArchiver : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(TestLogArchiver);
        CPPUNIT_TEST(testGetLogEstimatedSize);
        CPPUNIT_TEST(testCopyLogsTo);
        CPPUNIT_TEST(testCopyParmsDbTo);
        CPPUNIT_TEST(testCompressLogs);
        CPPUNIT_TEST(testGenerateUserDescriptionFile);
        CPPUNIT_TEST(testGenerateLogsSupportArchive);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp(void) final;

    protected:
        log4cplus::Logger _logger;

        void testGetLogEstimatedSize(void);
        void testCopyLogsTo(void);
        void testCopyParmsDbTo(void);
        void testCompressLogs(void);
        void testGenerateUserDescriptionFile(void);
        void testGenerateLogsSupportArchive(void);

    private:
        bool parmsDbFileExist();
};

} // namespace KDC
