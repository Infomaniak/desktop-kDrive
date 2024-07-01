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

#include <log4cplus/logger.h>
#include <QApplication>
#include "server/updater/kdcupdater.h"
#ifdef __APPLE__
#include "server/updater/sparkleupdater.h
#endif

using namespace CppUnit;

namespace KDC {

class TestUpdater : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(TestUpdater);
        CPPUNIT_TEST(testUpdateInfoVersionParseString);
        CPPUNIT_TEST(testIsKDCorSparkleUpdater);
        CPPUNIT_TEST(testClientVersion);
        CPPUNIT_TEST(testUpdateSucceeded);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp(void) final;
        void testUpdateInfoVersionParseString(void);
        void testIsKDCorSparkleUpdater(void);
        void testClientVersion(void);
        void testUpdateSucceeded(void);

    protected:
        log4cplus::Logger _logger;
#ifdef __APPLE__
        SparkleUpdater* _updater;
#else
        KDCUpdater* _updater;
#endif
};

}  // namespace KDC
