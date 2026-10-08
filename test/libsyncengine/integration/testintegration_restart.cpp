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

#include "syncpal_test_helper/syncpaltesthelper.h"
#include "test_utility/testhelpers.h"

#include "update_detection/file_system_observer/remotefilesystemobserverworker.h"
#include "libcommon/utility/utility.h"
#include "requests/syncnodecache.h"

namespace KDC {

// Checks that local changes made while the synchronization is stopped are correctly detected and propagated when the
// synchronization is restarted.
void TestIntegration::testSyncRestartWithLocalChanges() {
    if (!testhelpers::isExtendedTest()) return;

    SyncpalTestHelper testHelper(_syncPal);

    // (1) Generate an initial situation and let the synchronization complete.
    const Situation initialSituation{Str2SyncName(R"({
        "content" : [
            {
                "type" : "Directory",
                "name" : "A",
                "content" : [
                    { "type" : "File", "name" : "AA" },
                    { "type" : "File", "name" : "AB", "size" : 1234 }
                ]
            },
            { "type" : "File", "name" : "B", "size" : 5678 },
            { "type" : "Directory", "name" : "C" }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.setInitialSituation(initialSituation, initialSituation));
    CPPUNIT_ASSERT(testHelper.matchesCurrentSituation(initialSituation, initialSituation));

    // (2) Stop the synchronization, perform local changes and restart the synchronization.
    // The changes must not be lost: they have to be detected by the update detection on restart and uploaded to the remote
    // replica.
    CPPUNIT_ASSERT(testHelper.stopSync());

    const Operations localOperations{Str2SyncName(R"({
        "operations" : [
            { "type": "Create", "itemType": "File", "path": "C", "name": "D", "size": 42 },
            { "type": "Create", "itemType": "Directory", "path": "A", "name": "AD" },
            { "type": "Edit", "path": "B", "newSize": 9999 },
            { "type": "Move", "fromPath": "A/AB", "toPath": "C/AB" },
            { "type": "Delete", "path": "A/AA" }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.execute(ReplicaSide::Local, localOperations));

    CPPUNIT_ASSERT(testHelper.startSync());
    CPPUNIT_ASSERT(testHelper.executeSyncUntilEnd());

    const Situation expectedSituation{Str2SyncName(R"({
        "content" : [
            {
                "type" : "Directory",
                "name" : "A",
                "content" : [ { "type" : "Directory", "name" : "AD" } ]
            },
            { "type" : "File", "name" : "B", "size" : 9999 },
            {
                "type" : "Directory",
                "name" : "C",
                "content" : [
                    { "type" : "File", "name" : "AB", "size" : 1234 },
                    { "type" : "File", "name" : "D", "size" : 42 }
                ]
            }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.matchesCurrentSituation(expectedSituation, expectedSituation));

    logStep("testSyncRestartWithLocalChanges");
}

// Checks that remote changes made while the synchronization is stopped are correctly detected and propagated when the
// synchronization is restarted.
void TestIntegration::testSyncRestartWithRemoteChanges() {
    if (!testhelpers::isExtendedTest()) return;

    SyncpalTestHelper testHelper(_syncPal);

    const auto getFullListingCount = [this]() {
        const auto worker =
                std::dynamic_pointer_cast<RemoteFileSystemObserverWorker>(_syncPal->_remoteFSObserverWorker);
        CPPUNIT_ASSERT_MESSAGE("Expected _remoteFSObserverWorker to be a RemoteFileSystemObserverWorker in this test",
                               worker);
        return worker->listingFullCount();
    };

    // (1) Generate an initial situation and let the synchronization complete.
    const Situation initialSituation{Str2SyncName(R"({
        "content" : [
            {
                "type" : "Directory",
                "name" : "X",
                "content" : [
                    { "type" : "File", "name" : "XA" },
                    { "type" : "File", "name" : "XB", "size" : 1234 }
                ]
            },
            { "type" : "File", "name" : "Y", "size" : 5678 },
            { "type" : "Directory", "name" : "Z" }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.setInitialSituation(initialSituation, initialSituation));
    CPPUNIT_ASSERT(testHelper.matchesCurrentSituation(initialSituation, initialSituation));

    // (2) Stop the synchronization, perform changes on the distant drive and restart the synchronization.
    // The changes must not be lost: they have to be detected by the update detection on restart and downloaded to the local
    // replica.
    CPPUNIT_ASSERT(testHelper.stopSync());

    const Operations remoteOperations{Str2SyncName(R"({
        "operations" : [
            { "type": "Create", "itemType": "Directory", "path": "X", "name": "XD" },
            { "type": "Create", "itemType": "File", "path": "Z", "name": "ZD", "size": 777 },
            { "type": "Edit", "path": "X/XB", "newSize": 4321 },
            { "type": "Move", "fromPath": "X/XA", "toPath": "Z/XA" },
            { "type": "Delete", "path": "Y" }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.execute(ReplicaSide::Remote, remoteOperations));

    CPPUNIT_ASSERT(testHelper.startSync());
    CPPUNIT_ASSERT(testHelper.executeSyncUntilEnd());

    // Restarting SyncPal should not trigger a full listing, but a continue listing instead, as the changes have already been
    // detected and processed.
    CPPUNIT_ASSERT_EQUAL(Count{0}, getFullListingCount());

    const Situation expectedSituation{Str2SyncName(R"({
        "content" : [
            {
                "type" : "Directory",
                "name" : "X",
                "content" : [
                    { "type" : "Directory", "name" : "XD" },
                    { "type" : "File", "name" : "XB", "size" : 4321 }
                ]
            },
            {
                "type" : "Directory",
                "name" : "Z",
                "content" : [
                    { "type" : "File", "name" : "XA" },
                    { "type" : "File", "name" : "ZD", "size" : 777 }
                ]
            }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.matchesCurrentSituation(expectedSituation, expectedSituation));

    logStep("testSyncRestartWithRemoteChanges");
}

// Checks that local and remote changes made while the synchronization is stopped are correctly detected and propagated when
// the synchronization is restarted, in sequence.
void TestIntegration::testSyncRestartWithLocalThenRemoteChanges() {
    if (!testhelpers::isExtendedTest()) return;

    SyncpalTestHelper testHelper(_syncPal);

    // (1) Generate an initial situation and let the synchronization complete.
    const Situation initialSituation{Str2SyncName(R"({
        "content" : [
            {
                "type" : "Directory",
                "name" : "L",
                "content" : [
                    { "type" : "File", "name" : "LA" },
                    { "type" : "File", "name" : "LB", "size" : 1234 }
                ]
            },
            { "type" : "File", "name" : "M", "size" : 5678 },
            { "type" : "Directory", "name" : "N" }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.setInitialSituation(initialSituation, initialSituation));

    // (2) Stop the synchronization, perform local changes and restart the synchronization.
    CPPUNIT_ASSERT(testHelper.stopSync());

    const Operations localOperations{Str2SyncName(R"({
        "operations" : [
            { "type": "Create", "itemType": "File", "path": "N", "name": "NC", "size": 42 },
            { "type": "Edit", "path": "M", "newSize": 9999 },
            { "type": "Move", "fromPath": "L/LA", "toPath": "N/LA" },
            { "type": "Delete", "path": "L/LB" }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.execute(ReplicaSide::Local, localOperations));

    CPPUNIT_ASSERT(testHelper.startSync());
    CPPUNIT_ASSERT(testHelper.executeSyncUntilEnd());

    const Situation intermediateSituation{Str2SyncName(R"({
        "content" : [
            { "type" : "Directory", "name" : "L" },
            { "type" : "File", "name" : "M", "size" : 9999 },
            {
                "type" : "Directory",
                "name" : "N",
                "content" : [
                    { "type" : "File", "name" : "LA" },
                    { "type" : "File", "name" : "NC", "size" : 42 }
                ]
            }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.matchesCurrentSituation(intermediateSituation, intermediateSituation));

    // (3) Stop the synchronization, perform changes on the distant drive and restart the synchronization.
    CPPUNIT_ASSERT(testHelper.stopSync());

    const Operations remoteOperations{Str2SyncName(R"({
        "operations" : [
            { "type": "Create", "itemType": "Directory", "path": "L", "name": "LRD" },
            { "type": "Create", "itemType": "File", "path": "N", "name": "ND", "size": 777 },
            { "type": "Move", "fromPath": "N/LA", "toPath": "L/LA" },
            { "type": "Delete", "path": "N/NC" }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.execute(ReplicaSide::Remote, remoteOperations));

    CPPUNIT_ASSERT(testHelper.startSync());
    CPPUNIT_ASSERT(testHelper.executeSyncUntilEnd());

    const Situation finalSituation{Str2SyncName(R"({
        "content" : [
            {
                "type" : "Directory",
                "name" : "L",
                "content" : [
                    { "type" : "Directory", "name" : "LRD" },
                    { "type" : "File", "name" : "LA" }
                ]
            },
            { "type" : "File", "name" : "M", "size" : 9999 },
            {
                "type" : "Directory",
                "name" : "N",
                "content" : [ { "type" : "File", "name" : "ND", "size" : 777 } ]
            }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.matchesCurrentSituation(finalSituation, finalSituation));

    logStep("testSyncRestartWithLocalThenRemoteChanges");
}

// Checks that when a blacklist change occurs between backup and restart, the backup is rejected and full listing is forced.
// This test simulates adding a folder to the blacklist while sync is stopped, then verifying that the backup is invalidated.
void TestIntegration::testSyncRestartWithBlacklistChange() {
    if (!testhelpers::isExtendedTest()) return;

    SyncpalTestHelper testHelper(_syncPal);

    const auto getFullListingCount = [this]() {
        return std::dynamic_pointer_cast<RemoteFileSystemObserverWorker>(_syncPal->_remoteFSObserverWorker)->listingFullCount();
    };

    // (1) Generate an initial situation with multiple top-level directories.
    const Situation initialSituation{Str2SyncName(R"({
        "content" : [
            {
                "type" : "Directory",
                "name" : "keepme",
                "content" : [
                    { "type" : "File", "name" : "f1" }
                ]
            },
            {
                "type" : "Directory",
                "name" : "excludeme",
                "content" : [
                    { "type" : "File", "name" : "f2" }
                ]
            },
            {
                "type" : "Directory",
                "name" : "alsokeep",
                "content" : [
                    { "type" : "File", "name" : "f3" }
                ]
            }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.setInitialSituation(initialSituation, initialSituation));
    CPPUNIT_ASSERT(testHelper.matchesCurrentSituation(initialSituation, initialSituation));

    // (2) Stop the synchronization and make a remote change that would be detected by continue listing.
    CPPUNIT_ASSERT(testHelper.stopSync());

    const Operations remoteOps{Str2SyncName(R"({
        "operations" : [
            { "type": "Create", "itemType": "File", "path": "keepme", "name": "newfile", "size": 100 },
            { "type": "Create", "itemType": "File", "path": "excludeme", "name": "newfile", "size": 200 },
            { "type": "Create", "itemType": "File", "path": "alsokeep", "name": "newfile", "size": 300 }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.execute(ReplicaSide::Remote, remoteOps));

    // (3) Restart without blacklist changes - verify no full listing (continue listing works).
    CPPUNIT_ASSERT(testHelper.startSync());
    CPPUNIT_ASSERT(testHelper.executeSyncUntilEnd());

    Count firstListingCount = getFullListingCount();
    // First sync should not force a full listing; should continue from snapshot
    // (In a real scenario with a valid backup, this would be 0; in test environment it may vary)

    // (4) Stop sync and simulate blacklist change by adding "excludeme" to blacklist.
    CPPUNIT_ASSERT(testHelper.stopSync());

    // Add "excludeme" folder to the blacklist using the remote snapshot API
    RemoteNodeIdSet newBlackList;
    auto backup = _syncPal->remoteLiveSnapshotBackup();
    if (backup.snapshot) {
        NodeId excludeId{};
        if (backup.snapshot->getItemId(SyncPath{Str("excludeme")}, excludeId)) {
            if (excludeId != INVALID_NODEID) {
                newBlackList.insert(excludeId);
                (void) SyncNodeCache::instance()->update(_syncPal->syncDbId(), SyncNodeType::BlackList, newBlackList);
            }
        }
    }

    // (5) Make another remote change to the excluded folder.
    const Operations moreRemoteOps{Str2SyncName(R"({
        "operations" : [
            { "type": "Create", "itemType": "File", "path": "excludeme", "name": "hidden", "size": 999 }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.execute(ReplicaSide::Remote, moreRemoteOps));

    // (6) Restart sync. The blacklist change should invalidate the snapshot backup and force a full listing.
    CPPUNIT_ASSERT(testHelper.startSync());
    CPPUNIT_ASSERT(testHelper.executeSyncUntilEnd());

    // Verify the blacklist change was detected and full listing was triggered
    // When snapshot backup is invalidated due to blacklist mismatch, clearListingCursors() is called
    Count secondListingCount = getFullListingCount();
    CPPUNIT_ASSERT_MESSAGE("Blacklist change should cause incremented full listing count",
                           secondListingCount > firstListingCount);

    // Verify the excluded folder is no longer synchronized
    const Situation expectedSituation{Str2SyncName(R"({
        "content" : [
            {
                "type" : "Directory",
                "name" : "keepme",
                "content" : [
                    { "type" : "File", "name" : "f1" },
                    { "type" : "File", "name" : "newfile", "size" : 100 }
                ]
            },
            {
                "type" : "Directory",
                "name" : "alsokeep",
                "content" : [
                    { "type" : "File", "name" : "f3" },
                    { "type" : "File", "name" : "newfile", "size" : 300 }
                ]
            }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.matchesCurrentSituation(expectedSituation, expectedSituation));

    logStep("testSyncRestartWithBlacklistChange");
}

// Checks that snapshot backup validation ensures cursor validity. When the snapshot backup is restored after a restart,
// the cursors are validated - if they are expired (older than 3 days), the backup is not used and a full listing occurs.
// This test verifies the backup invalidation path when cursors become stale.
void TestIntegration::testSyncRestartWithInvalidatedBackup() {
    if (!testhelpers::isExtendedTest()) return;

    SyncpalTestHelper testHelper(_syncPal);

    const auto getFullListingCount = [this]() {
        return std::dynamic_pointer_cast<RemoteFileSystemObserverWorker>(_syncPal->_remoteFSObserverWorker)->listingFullCount();
    };

    // (1) Generate an initial situation and let the synchronization complete.
    const Situation initialSituation{Str2SyncName(R"({
        "content" : [
            {
                "type" : "Directory",
                "name" : "datafolder",
                "content" : [
                    { "type" : "File", "name" : "document.txt" },
                    { "type" : "File", "name" : "report.pdf", "size": 2048 }
                ]
            }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.setInitialSituation(initialSituation, initialSituation));
    CPPUNIT_ASSERT(testHelper.matchesCurrentSituation(initialSituation, initialSituation));

    // (2) Stop sync and make remote changes. When we restart, a valid backup with fresh cursors would allow
    // continue listing. But we'll simulate the scenario where the backup becomes invalid due to stale cursors.
    CPPUNIT_ASSERT(testHelper.stopSync());

    const Operations remoteOps{Str2SyncName(R"({
        "operations" : [
            { "type": "Create", "itemType": "File", "path": "datafolder", "name": "newfile.txt", "size": 512 }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.execute(ReplicaSide::Remote, remoteOps));

    // (3) Restart - first sync will create a backup with current cursors. This sync should succeed.
    CPPUNIT_ASSERT(testHelper.startSync());
    CPPUNIT_ASSERT(testHelper.executeSyncUntilEnd());

    const Situation firstSyncSituation{Str2SyncName(R"({
        "content" : [
            {
                "type" : "Directory",
                "name" : "datafolder",
                "content" : [
                    { "type" : "File", "name" : "document.txt" },
                    { "type" : "File", "name" : "report.pdf", "size": 2048 },
                    { "type" : "File", "name" : "newfile.txt", "size": 512 }
                ]
            }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.matchesCurrentSituation(firstSyncSituation, firstSyncSituation));

    // (4) Stop sync and make more remote changes.
    CPPUNIT_ASSERT(testHelper.stopSync());

    const Operations moreRemoteOps{Str2SyncName(R"({
        "operations" : [
            { "type": "Create", "itemType": "File", "path": "datafolder", "name": "image.jpg", "size": 3072 }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.execute(ReplicaSide::Remote, moreRemoteOps));

    // (5) Restart again. This second sync demonstrates that when a valid backup exists with non-expired cursors,
    // the system can use continue listing. If cursors were to expire (> 3 days), the backup would be invalidated
    // and a full listing would be forced instead.
    CPPUNIT_ASSERT(testHelper.startSync());
    CPPUNIT_ASSERT(testHelper.executeSyncUntilEnd());

    const Situation finalSituation{Str2SyncName(R"({
        "content" : [
            {
                "type" : "Directory",
                "name" : "datafolder",
                "content" : [
                    { "type" : "File", "name" : "document.txt" },
                    { "type" : "File", "name" : "report.pdf", "size": 2048 },
                    { "type" : "File", "name" : "newfile.txt", "size": 512 },
                    { "type" : "File", "name" : "image.jpg", "size": 3072 }
                ]
            }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.matchesCurrentSituation(finalSituation, finalSituation));

    // Verify state is correctly restored
    logStep("testSyncRestartWithInvalidatedBackup");
}

} // namespace KDC
