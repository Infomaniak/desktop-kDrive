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

#include "testintegration.h"

#include "requests/syncnodecache.h"
#include "syncpal_test_helper/syncpaltesthelper.h"
#include "test_utility/testhelpers.h"
#include "update_detection/blacklist_changes_propagator/blacklistpropagator.h"

namespace KDC {
namespace {

DbNode dbNodeForRemotePath(const std::shared_ptr<MockSyncPal> &syncPal, const SyncPath &path) {
    DbNodeId dbId = 0;
    bool found = false;
    CPPUNIT_ASSERT(syncPal->syncDb()->dbId(ReplicaSide::Remote, path, dbId, found) && found);

    DbNode dbNode;
    CPPUNIT_ASSERT(syncPal->syncDb()->node(dbId, dbNode, found) && found);
    return dbNode;
}

void assertNodeMissing(const std::shared_ptr<MockSyncPal> &syncPal, const ReplicaSide side, const NodeId &nodeId) {
    DbNodeId dbId = 0;
    bool found = false;
    CPPUNIT_ASSERT(syncPal->syncDb()->dbId(side, nodeId, dbId, found));
    CPPUNIT_ASSERT(!found);
}

void runBlacklistPropagator(const std::shared_ptr<MockSyncPal> &syncPal, const NodeId &remoteNodeId) {
    NodeSet blacklist{remoteNodeId};
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok,
                         SyncNodeCache::instance()->update(syncPal->syncDbId(), SyncNodeType::BlackList, blacklist));
    CPPUNIT_ASSERT(BlacklistPropagator(syncPal).runSynchronously());
}

} // namespace

static const auto maxNbBlacklistedFiles = 3000;

void TestIntegration::testBlacklist() {
    waitForSyncToBeIdle(std::source_location::current());
    const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id(), "testBlacklistDir");
    const auto filename = Str("testBlacklist");
    const auto fileId = testhelpers::duplicateRemoteItem(_driveDbId, _testFileRemoteId, filename);
    waitForSyncToBeIdle(std::source_location::current());

    const auto dirpath = _syncPal->localPath() / tmpRemoteDir.name();
    CPPUNIT_ASSERT(std::filesystem::exists(dirpath));

    (void) SyncNodeCache::instance()->update(_syncPal->syncDbId(), SyncNodeType::BlackList, {tmpRemoteDir.id()});
    _syncPal->stop();
    (void) BlacklistPropagator(_syncPal).runSynchronously();
    _syncPal->start();
    waitForSyncToBeIdle(std::source_location::current());

    CPPUNIT_ASSERT(!std::filesystem::exists(dirpath));
#if defined(KD_LINUX)
    CPPUNIT_ASSERT(testhelpers::hasTrashInfo());
    CPPUNIT_ASSERT(testhelpers::isInTrash(dirpath));
#else
    CPPUNIT_ASSERT(testhelpers::isInTrash(dirpath.filename()));
#endif

#if defined(KD_MACOS) || defined(KD_LINUX)
    testhelpers::eraseFromTrash(dirpath.filename());
#endif

    testhelpers::moveRemoteItem(_driveDbId, fileId, tmpRemoteDir.id());
    _syncPal->_remoteFSObserverWorker->forceUpdate();
    waitForSyncToBeIdle(std::source_location::current());

    CPPUNIT_ASSERT(!std::filesystem::exists(_syncPal->localPath() / filename));
#if defined(KD_LINUX)
    CPPUNIT_ASSERT(testhelpers::hasTrashInfo());
    CPPUNIT_ASSERT(testhelpers::isInTrash(_syncPal->localPath() / filename));
#else
    CPPUNIT_ASSERT(testhelpers::isInTrash(filename));
#endif

#if defined(KD_MACOS) || defined(KD_LINUX)
    testhelpers::eraseFromTrash(filename);
#endif

    testhelpers::moveRemoteItem(_driveDbId, fileId, _remoteSyncDir.id());
    _syncPal->_remoteFSObserverWorker->forceUpdate();
    waitForSyncToBeIdle(std::source_location::current());

    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / filename));

    (void) SyncNodeCache::instance()->update(_syncPal->syncDbId(), SyncNodeType::BlackList, {});
    _syncPal->stop();
    (void) BlacklistPropagator(_syncPal).runSynchronously();
    _syncPal->start();

    waitForSyncToBeIdle(std::source_location::current());
    CPPUNIT_ASSERT(std::filesystem::exists(dirpath));

    const auto idInt = static_cast<uint64_t>(std::stoll(_remoteSyncDir.id()));
    NodeSet blacklist;
    for (auto i = idInt + 1; i < idInt + maxNbBlacklistedFiles; i++) {
        (void) blacklist.emplace(std::to_string(i));
    }
    (void) SyncNodeCache::instance()->update(_syncPal->syncDbId(), SyncNodeType::BlackList, blacklist);
    _syncPal->stop();
    (void) BlacklistPropagator(_syncPal).runSynchronously();
    _syncPal->start();

    waitForSyncToBeIdle(std::source_location::current());
    CPPUNIT_ASSERT(!std::filesystem::exists(dirpath));

    (void) blacklist.emplace(std::to_string(idInt + maxNbBlacklistedFiles + 1));
    (void) blacklist.emplace(std::to_string(idInt + maxNbBlacklistedFiles + 2));
    (void) SyncNodeCache::instance()->update(_syncPal->syncDbId(), SyncNodeType::BlackList, blacklist);
    _syncPal->stop();
    (void) BlacklistPropagator(_syncPal).runSynchronously();
    _syncPal->start();

    const TimerUtility timer;
    while (timer.elapsed<std::chrono::seconds>() < std::chrono::seconds(10)) {
        if (_syncPal->isPaused()) break;
        Utility::msleep(100);
    }
    CPPUNIT_ASSERT(_syncPal->isPaused());

    (void) SyncNodeCache::instance()->update(_syncPal->syncDbId(), SyncNodeType::BlackList, {});
    _syncPal->stop();
    (void) BlacklistPropagator(_syncPal).runSynchronously();
    _syncPal->start();

    logStep("testBlacklist");
}

void TestIntegration::testBlacklistPropagatorWithMissingLocalDirectory() {
    SyncpalTestHelper testHelper(_syncPal);

    const SyncPath blacklistedDir = "blacklisted_dir";
    const SyncPath blacklistedFile = blacklistedDir / "blacklisted_file.txt";
    const Situation situation{Str2SyncName(R"({
        "content" : [
            {
                "type" : "Directory",
                "name" : "blacklisted_dir",
                "content" : [ {"type" : "File", "name" : "blacklisted_file.txt", "size" : 1234} ]
            }
        ]
    })")};

    CPPUNIT_ASSERT(testHelper.setInitialSituation(situation, situation));

    const DbNode dirDbNode = dbNodeForRemotePath(_syncPal, blacklistedDir);
    const DbNode fileDbNode = dbNodeForRemotePath(_syncPal, blacklistedFile);
    const SyncPath absoluteLocalPath = _syncPal->localPath() / blacklistedDir;

    CPPUNIT_ASSERT(IoHelper::deleteItem(absoluteLocalPath));
    CPPUNIT_ASSERT(!std::filesystem::exists(absoluteLocalPath));
    CPPUNIT_ASSERT(testHelper.stopSync());

    runBlacklistPropagator(_syncPal, *dirDbNode.nodeIdRemote());

    CPPUNIT_ASSERT(!std::filesystem::exists(absoluteLocalPath));
    assertNodeMissing(_syncPal, ReplicaSide::Local, *dirDbNode.nodeIdLocal());
    assertNodeMissing(_syncPal, ReplicaSide::Remote, *dirDbNode.nodeIdRemote());
    assertNodeMissing(_syncPal, ReplicaSide::Local, *fileDbNode.nodeIdLocal());
    assertNodeMissing(_syncPal, ReplicaSide::Remote, *fileDbNode.nodeIdRemote());

    CPPUNIT_ASSERT(testHelper.startSync());
    CPPUNIT_ASSERT(testHelper.executeSyncUntilEnd());
    CPPUNIT_ASSERT(!std::filesystem::exists(absoluteLocalPath));
    assertNodeMissing(_syncPal, ReplicaSide::Local, *dirDbNode.nodeIdLocal());
    assertNodeMissing(_syncPal, ReplicaSide::Remote, *dirDbNode.nodeIdRemote());
    assertNodeMissing(_syncPal, ReplicaSide::Local, *fileDbNode.nodeIdLocal());
    assertNodeMissing(_syncPal, ReplicaSide::Remote, *fileDbNode.nodeIdRemote());
}

void TestIntegration::testBlacklistPropagatorWithHydrationCancellationFailure() {
    SyncpalTestHelper testHelper(_syncPal);

    const SyncPath blacklistedFile = "blacklisted_file.txt";
    const Situation situation{Str2SyncName(R"({
        "content" : [
            {"type" : "File", "name" : "blacklisted_file.txt", "size" : 1234}
        ]
    })")};

    CPPUNIT_ASSERT(testHelper.setInitialSituation(situation, situation));

    const DbNode fileDbNode = dbNodeForRemotePath(_syncPal, blacklistedFile);
    const SyncPath absoluteLocalPath = _syncPal->localPath() / blacklistedFile;

#if defined(KD_MACOS)
    _syncPal->setVfsMode(VirtualFileMode::Mac);
#elif defined(KD_WINDOWS)
    _syncPal->setVfsMode(VirtualFileMode::Win);
#else
    CPPUNIT_SKIP();
#endif
    CPPUNIT_ASSERT(testHelper.stopSync());

    runBlacklistPropagator(_syncPal, *fileDbNode.nodeIdRemote());

    CPPUNIT_ASSERT(!std::filesystem::exists(absoluteLocalPath));
    assertNodeMissing(_syncPal, ReplicaSide::Local, *fileDbNode.nodeIdLocal());
    assertNodeMissing(_syncPal, ReplicaSide::Remote, *fileDbNode.nodeIdRemote());

    CPPUNIT_ASSERT(testHelper.startSync());
    CPPUNIT_ASSERT(testHelper.executeSyncUntilEnd());
    CPPUNIT_ASSERT(!std::filesystem::exists(absoluteLocalPath));
    assertNodeMissing(_syncPal, ReplicaSide::Local, *fileDbNode.nodeIdLocal());
    assertNodeMissing(_syncPal, ReplicaSide::Remote, *fileDbNode.nodeIdRemote());
}

} // namespace KDC
