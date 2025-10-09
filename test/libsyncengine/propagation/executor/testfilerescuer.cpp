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

#include "testfilerescuer.h"
#include "db/parmsdb.h"
#include "keychainmanager/keychainmanager.h"
#include "network/proxy.h"
#include "propagation/executor/filerescuer.h"

#include "mocks/libcommonserver/db/mockdb.h"
#include "test_classes/testsituationgenerator.h"
#include "test_utility/testhelpers.h"

namespace KDC {

void TestFileRescuer::setUp() {
    TestBase::start();
    const testhelpers::TestVariables testVariables;

    const std::string localPathStr = _localTempDir.path().string();

    // Insert api token into keystore
    std::string keychainKey("123");
    (void) KeyChainManager::instance(true);
    KeyChainManager::instance()->writeToken(keychainKey, testVariables.apiToken);

    // Create parmsDb
    (void) ParmsDb::instance(_localTempDir.path() / MockDb::makeDbMockFileName(), KDRIVE_VERSION_STRING, true, true);

    // Insert user, account, drive & sync
    int userId(12321);
    User user(1, userId, keychainKey);
    (void) ParmsDb::instance()->insertUser(user);

    int accountId(atoi(testVariables.accountId.c_str()));
    Account account(1, accountId, user.dbId());
    (void) ParmsDb::instance()->insertAccount(account);

    int driveDbId = 1;
    int driveId = atoi(testVariables.driveId.c_str());
    Drive drive(driveDbId, driveId, account.dbId(), std::string(), 0, std::string());
    (void) ParmsDb::instance()->insertDrive(drive);

    const auto sync = Sync(1, drive.dbId(), localPathStr, "", testVariables.remotePath);
    (void) ParmsDb::instance()->insertSync(sync);

    // Setup proxy
    Parameters parameters;
    if (bool found = false; ParmsDb::instance()->selectParameters(parameters, found) && found) {
        Proxy::instance(parameters.proxyConfig());
    }

    _syncPal = std::make_shared<SyncPal>(std::make_shared<VfsOff>(VfsSetupParams(Log::instance()->getLogger())), sync.dbId(),
                                         KDRIVE_VERSION_STRING);
    _syncPal->syncDb()->setAutoDelete(true);
    _syncPal->createSharedObjects();
    _syncPal->createWorkers();
}

void TestFileRescuer::tearDown() {
    ParmsDb::instance()->close();
    ParmsDb::reset();
    if (_syncPal && _syncPal->syncDb()) {
        _syncPal->syncDb()->close();
    }
    TestBase::stop();
}

void TestFileRescuer::testFileRescuer() {
    // Setup
    TestSituationGenerator situationGenerator(_syncPal);
    situationGenerator.generateInitialSituation(R"({"a":1})");

    const auto lnodeA = situationGenerator.getNode(ReplicaSide::Local, "a");
    const auto fileName = lnodeA->name();
    const auto filePath = _localTempDir.path() / fileName;
    testhelpers::generateOrEditTestFile(filePath);

    const auto rnodeA = situationGenerator.getNode(ReplicaSide::Remote, "a");
    const Conflict conflict(lnodeA, rnodeA, ConflictType::EditDelete);
    _syncPal->conflictQueue()->push(conflict);

    // Rescue file "A"
    const auto syncOp = std::make_shared<SyncOperation>();
    syncOp->setType(OperationType::Move);
    syncOp->setAffectedNode(lnodeA);
    syncOp->setCorrespondingNode(lnodeA);
    syncOp->setTargetSide(ReplicaSide::Local);
    syncOp->setRelativeOriginPath(lnodeA->getPath());
    syncOp->setConflict(conflict);
    syncOp->setIsRescueOperation(true);
    lnodeA->setStatus(NodeStatus::ConflictOpGenerated);

    FileRescuer fileRescuer(_syncPal);
    CPPUNIT_ASSERT(fileRescuer.executeRescueMoveJob(syncOp));
    // Check that rescue folder has been created
    CPPUNIT_ASSERT_EQUAL(true, std::filesystem::exists(_localTempDir.path() / FileRescuer::rescueFolderName()));
    // Check that file "A" has been moved into rescue folder
    CPPUNIT_ASSERT_EQUAL(true, std::filesystem::exists(_localTempDir.path() / FileRescuer::rescueFolderName() / fileName));
    CPPUNIT_ASSERT_EQUAL(true, !std::filesystem::exists(filePath));

    // Generate another file "A" and rescue it again
    testhelpers::generateOrEditTestFile(filePath);
    CPPUNIT_ASSERT(fileRescuer.executeRescueMoveJob(syncOp));
    // Check that both files are now in rescue folder
    CPPUNIT_ASSERT_EQUAL(true, std::filesystem::exists(_localTempDir.path() / FileRescuer::rescueFolderName() / fileName));
    CPPUNIT_ASSERT_EQUAL(
            true, std::filesystem::exists(_localTempDir.path() / FileRescuer::rescueFolderName() / (fileName + Str(" (1)"))));
}

void TestFileRescuer::testGetDestinationPath() {
    FileRescuer fileRescuer(_syncPal);

    // Test with a simple file name
    SyncName fileName = WStr2SyncName(L"test.txt");
    SyncPath res = fileRescuer.getDestinationPath(fileName);
    CPPUNIT_ASSERT(res.filename().native() == Str2SyncName("test.txt"));
    res = fileRescuer.getDestinationPath(fileName, 1);
    CPPUNIT_ASSERT(res.filename().native() == Str2SyncName("test (1).txt"));

#ifdef _WIN32
    // Test with a file name with special characters
    fileName = WStr2SyncName(L"�.txt");
    res = fileRescuer.getDestinationPath(fileName);
    CPPUNIT_ASSERT(res.filename().native() == L"�.txt");
    res = fileRescuer.getDestinationPath(fileName, 1);
    CPPUNIT_ASSERT(res.filename().native() == L"� (1).txt");
#endif // _WIN32
}
} // namespace KDC
