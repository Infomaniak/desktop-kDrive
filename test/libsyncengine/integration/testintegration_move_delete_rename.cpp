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

#include "testintegration_move_delete_rename.h"

#include "syncpal_test_helper/initialsituationsetter.h"
#include "syncpal_test_helper/operationsexecutor.h"
#include "mocks/libcommonserver/db/mockdb.h"

#include "test_utility/testhelpers.h"

#include "libcommonserver/keychainmanager/keychainmanager.h"
#include "mocks/mockkeychainstorage.h"
#include "libcommonserver/utility/utility.h"

using namespace CppUnit;

namespace KDC {

Situation TestMoveDeleteRename::getInitialSituation() {
    // Initial situation:
    // .
    // ├── A
    // │   └── AA
    // │       └── AAA
    // └── B
    return Situation(Str2SyncName(R"({
            "content": [
                { "type": "Directory", "name": "A", "content": [
                    { "type": "Directory", "name": "AA", "content": [
                        { "type": "File", "name": "AAA" }
                    ]}
                ]},
                { "type": "Directory", "name": "B" }
            ]
        })"));
}

Situation TestMoveDeleteRename::getExpectedFinalSituation() {
    // Expected final situation:
    // .
    // └── A
    //     └── AA
    //         └── AAA
    return Situation(Str2SyncName(R"({
        "content": [
            { "type": "Directory", "name": "A", "content": [
                { "type": "Directory", "name": "AA", "content": [
                    { "type": "File", "name": "AAA" }
                ]}
            ]}
        ]
    })"));
}

Operations TestMoveDeleteRename::getOperations() {
    return Operations(Str2SyncName(R"({
        "operations": [
            { "type": "Move", "fromPath": "A/AA", "toPath": "B/AA" },
            { "type": "Delete", "path": "A" },
            { "type": "Move", "fromPath": "B", "toPath": "A" }
        ]
    })"));
}

void TestMoveDeleteRename::setUp() {
    TestBase::start();

    const testhelpers::TestVariables testVariables;

    ApiToken apiToken;
    apiToken.setAccessToken(testVariables.apiToken);

    const std::string keychainKey("123");
    (void) KeyChainManager::instance(std::make_shared<MockKeyChainStorage>());
    (void) KeyChainManager::instance()->writeData(keychainKey, apiToken.reconstructJsonString());

    bool alreadyExists = false;
    const std::filesystem::path parmsDbPath = MockDb::makeDbName(alreadyExists);
    (void) ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);

    const User user(1, 12321, keychainKey);
    (void) ParmsDb::instance()->insertUser(user);

    const Account account(1, atoi(testVariables.accountId.c_str()), user.dbId(), "account1");
    (void) ParmsDb::instance()->insertAccount(account);

    _driveDbId = 1;
    const Drive drive(_driveDbId, atoi(testVariables.driveId.c_str()), account.dbId(), std::string(), 0, std::string());
    (void) ParmsDb::instance()->insertDrive(drive);

    FileStat fileStat;
    bool found = false;
    (void) IoHelper::getFileStat(_localSyncDir.path(), &fileStat, found, IoHelper::PathCheckOption::Insensitive);

    const SyncPath remotePath = _remoteSyncDir.name();
    Sync sync(1, drive.dbId(), _localSyncDir.path(), std::to_string(fileStat.inode), remotePath, _remoteSyncDir.id());
    const auto syncDbPath = MockDb::makeDbName(user.userId(), account.accountId(), drive.driveId(), sync.dbId());
    sync.setDbPath(syncDbPath);
    (void) ParmsDb::instance()->insertSync(sync);

    _syncPal = std::make_shared<MockSyncPal>(std::make_shared<VfsOff>(VfsSetupParams(Log::instance()->getLogger())), sync.dbId(),
                                             KDRIVE_VERSION_STRING);
    _syncPal->createSharedObjects();
    _syncPal->syncDb()->setAutoDelete(true);

    _testHelper.setSyncpal(_syncPal);
}

void TestMoveDeleteRename::tearDown() {
    if (_syncPal) {
        _syncPal->stop(SyncPal::PauseCaller::Sync, SyncPal::DbBehaviorAfterStop::Remove);
    }
    ParmsDb::instance()->close();
    ParmsDb::reset();
    TestBase::stop();
}

void TestMoveDeleteRename::testRemoteOperations() {
    // Initial situation:
    // .
    // ├── A
    // │   └── AA
    // │       └── AAA
    // └── B

    if (const Situation initialSituation = getInitialSituation();
        !_testHelper.setInitialSituation(initialSituation, initialSituation)) {
        CPPUNIT_FAIL("Failed to set initial situation");
    }

    // Remote operations:
    // Move AA into B
    // Delete A
    // Rename B with A (Move with same parent = rename)
    if (const Operations remoteOperations = getOperations(); !_testHelper.execute(ReplicaSide::Remote, remoteOperations)) {
        CPPUNIT_FAIL("Failed to execute remote operations");
    }

    if (!_testHelper.executeSyncUntilEnd()) {
        CPPUNIT_FAIL("Sync did not complete successfully");
    }

    // Expected final situation:
    // .
    // └── A
    //     └── AA
    //         └── AAA
    const Situation expectedSituation = getExpectedFinalSituation();
    CPPUNIT_ASSERT(_testHelper.matchesCurrentSituation(expectedSituation, expectedSituation));
}

void TestMoveDeleteRename::testLocalOperations() {
    // Initial situation:
    // .
    // ├── A
    // │   └── AA
    // │       └── AAA
    // └── B

    if (const Situation initialSituation = getInitialSituation();
        !_testHelper.setInitialSituation(initialSituation, initialSituation)) {
        CPPUNIT_FAIL("Failed to set initial situation.");
    }

    // Local operations:
    // Move AA
    // Delete A
    // Rename B with A (Move with same parent = rename)
    if (const Operations localOperations = getOperations(); !_testHelper.execute(ReplicaSide::Local, localOperations)) {
        CPPUNIT_FAIL("Failed to execute local operations.");
    }

    if (!_testHelper.executeSyncUntilEnd()) {
        CPPUNIT_FAIL("Sync did not complete successfully.");
    }

    // Expected final situation:
    // .
    // └── A
    //     └── AA
    //         └── AAA
    const Situation expectedSituation = getExpectedFinalSituation();
    CPPUNIT_ASSERT(_testHelper.matchesCurrentSituation(expectedSituation, expectedSituation));
}

} // namespace KDC
