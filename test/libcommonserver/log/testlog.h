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
#include <utility/types.h>

namespace KDC {

class TestLog : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(TestLog);
        CPPUNIT_TEST(testLog);
        CPPUNIT_TEST(testExpiredLogFiles);
        CPPUNIT_TEST(testLargeLogRolling);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp(void) final;

        void testLargeLogRolling(void);
        void testExpiredLogFiles(void);

    private:
        log4cplus::Logger _logger;
        void testLog(void);

        int countFilesInDirectory(const SyncPath& directory) const; // return -1 if error
        void clearLogDirectory(void) const; // remove all files in log directory except the current log file
        SyncPath _logDir;
};
} // namespace KDC
