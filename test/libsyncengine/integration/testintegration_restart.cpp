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

} // namespace KDC
