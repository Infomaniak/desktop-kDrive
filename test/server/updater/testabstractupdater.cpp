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

#include "testabstractupdater.h"

#include "db/parmsdb.h"
#include "requests/parameterscache.h"

namespace KDC {

void TestAbstractUpdater::setUp() {
    // Init parmsDb
    bool alreadyExists = false;
    const std::filesystem::path parmsDbPath = ParmsDb::makeDbName(alreadyExists, true);
    ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);

    // Setup parameters cache in test mode
    ParametersCache::instance(true);
}

void TestAbstractUpdater::testSkipUnskipVersion() {
    const std::string testStr("1.1.1.20210101");
    _testObj.skipVersion(testStr);

    CPPUNIT_ASSERT_EQUAL(testStr, ParametersCache::instance()->parameters().seenVersion());

    bool found = false;
    Parameters parameters;
    ParmsDb::instance()->selectParameters(parameters, found);
    CPPUNIT_ASSERT(parameters.seenVersion() == testStr);

    _testObj.unskipVersion();

    CPPUNIT_ASSERT(ParametersCache::instance()->parameters().seenVersion().empty());

    ParmsDb::instance()->selectParameters(parameters, found);
    CPPUNIT_ASSERT(parameters.seenVersion().empty());
}

void TestAbstractUpdater::testIsVersionSkipped() {
    const auto skippedVersion("3.3.3.20210101");

    CPPUNIT_ASSERT(!_testObj.isVersionSkipped(skippedVersion));

    CPPUNIT_ASSERT(!_testObj.isVersionSkipped("4.3.3.20210101"));
    CPPUNIT_ASSERT(!_testObj.isVersionSkipped("3.5.3.20210101"));
    CPPUNIT_ASSERT(!_testObj.isVersionSkipped("3.3.6.20210101"));
    CPPUNIT_ASSERT(!_testObj.isVersionSkipped("3.3.3.20210109"));

    CPPUNIT_ASSERT(!_testObj.isVersionSkipped("2.3.3.20210101"));
    CPPUNIT_ASSERT(!_testObj.isVersionSkipped("3.1.3.20210101"));
    CPPUNIT_ASSERT(!_testObj.isVersionSkipped("3.3.0.20210101"));
    CPPUNIT_ASSERT(!_testObj.isVersionSkipped("3.3.3.20200101"));

    _testObj.skipVersion(skippedVersion);

    CPPUNIT_ASSERT(_testObj.isVersionSkipped(skippedVersion));

    CPPUNIT_ASSERT(!_testObj.isVersionSkipped("4.3.3.20210101"));
    CPPUNIT_ASSERT(!_testObj.isVersionSkipped("3.5.3.20210101"));
    CPPUNIT_ASSERT(!_testObj.isVersionSkipped("3.3.6.20210101"));
    CPPUNIT_ASSERT(!_testObj.isVersionSkipped("3.3.3.20210109"));

    CPPUNIT_ASSERT(_testObj.isVersionSkipped("2.3.3.20210101"));
    CPPUNIT_ASSERT(_testObj.isVersionSkipped("3.1.3.20210101"));
    CPPUNIT_ASSERT(_testObj.isVersionSkipped("3.3.0.20210101"));
    CPPUNIT_ASSERT(_testObj.isVersionSkipped("3.3.3.20200101"));

    _testObj.unskipVersion();

    CPPUNIT_ASSERT(!_testObj.isVersionSkipped(skippedVersion));

    CPPUNIT_ASSERT(!_testObj.isVersionSkipped("4.3.3.20210101"));
    CPPUNIT_ASSERT(!_testObj.isVersionSkipped("3.5.3.20210101"));
    CPPUNIT_ASSERT(!_testObj.isVersionSkipped("3.3.6.20210101"));
    CPPUNIT_ASSERT(!_testObj.isVersionSkipped("3.3.3.20210109"));

    CPPUNIT_ASSERT(!_testObj.isVersionSkipped("2.3.3.20210101"));
    CPPUNIT_ASSERT(!_testObj.isVersionSkipped("3.1.3.20210101"));
    CPPUNIT_ASSERT(!_testObj.isVersionSkipped("3.3.0.20210101"));
    CPPUNIT_ASSERT(!_testObj.isVersionSkipped("3.3.3.20200101"));
}

} // namespace KDC
