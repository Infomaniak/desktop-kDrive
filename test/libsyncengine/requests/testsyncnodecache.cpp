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

#include "testsyncnodecache.h"
#include "test_utility/testhelpers.h"
#include "test_utility/localtemporarydirectory.h"

#include "libsyncengine/requests/syncnodecache.h"

#include "libcommon/utility/logiffail.h"

#include <algorithm>

using namespace CppUnit;

namespace KDC {


void TestSyncNodeCache::setUp() {
    TestBase::start();
    bool alreadyExists = false;
    const std::filesystem::path syncDbPath = Db::makeDbName(1, 1, 1, 1, alreadyExists);

    // Delete previous DB
    std::filesystem::remove(syncDbPath);

    // Create DB
    _testObj = std::make_shared<SyncDb>(syncDbPath.string(), KDRIVE_VERSION_STRING);
    _testObj->init(KDRIVE_VERSION_STRING);
    _testObj->setAutoDelete(true);

    NodeSet nodeIdSet;
    nodeIdSet.emplace("1");
    nodeIdSet.emplace("2");

    CPPUNIT_ASSERT(_testObj->updateAllSyncNodes(SyncNodeType::BlackList, nodeIdSet));
}

void TestSyncNodeCache::tearDown() {
    // Close and delete DB
    _testObj->close();
    _testObj.reset();
    TestBase::stop();
    LogIfFailSettings::assertEnabled = true;
}

void TestSyncNodeCache::testContainsSyncNode() {
    const int nonExistingSyncDbId = -1;
    CPPUNIT_ASSERT(!SyncNodeCache::instance()->contains(nonExistingSyncDbId, "non-existing-sync-node-id"));

    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, SyncNodeCache::instance()->initCache(1, _testObj));
    CPPUNIT_ASSERT(!SyncNodeCache::instance()->contains(1, "non-existing-sync-node-id"));
    CPPUNIT_ASSERT(!SyncNodeCache::instance()->contains(1, SyncNodeType::BlackList, "non-existing-sync-node-id"));

    CPPUNIT_ASSERT(SyncNodeCache::instance()->contains(1, "1"));
    CPPUNIT_ASSERT(SyncNodeCache::instance()->contains(1, SyncNodeType::BlackList, "1"));
    CPPUNIT_ASSERT(!SyncNodeCache::instance()->contains(1, SyncNodeType::WhiteList, "1"));
}


void TestSyncNodeCache::testDeleteSyncNode() {
    const int nonExistingSyncDbId = -1;
    CPPUNIT_ASSERT_EQUAL(ExitInfo{ExitCode::DataError},
                         SyncNodeCache::instance()->deleteSyncNode(nonExistingSyncDbId, "non-existing-sync-node-id"));

    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, SyncNodeCache::instance()->initCache(1, _testObj));
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok, ExitCause::NotFound),
                         SyncNodeCache::instance()->deleteSyncNode(1, "non-existing-sync-node-id"));

    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), SyncNodeCache::instance()->deleteSyncNode(1, "1"));

    NodeSet nodeIdSet;
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, SyncNodeCache::instance()->syncNodes(1, SyncNodeType::BlackList, nodeIdSet));
    CPPUNIT_ASSERT_EQUAL(size_t{1}, nodeIdSet.size());
    CPPUNIT_ASSERT(nodeIdSet.contains("2"));

    nodeIdSet.clear();

    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), SyncNodeCache::instance()->deleteSyncNode(1, "2"));
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, SyncNodeCache::instance()->syncNodes(1, SyncNodeType::BlackList, nodeIdSet));
    CPPUNIT_ASSERT_EQUAL(size_t{0}, nodeIdSet.size());
}

} // namespace KDC
