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

#include "testsyncpal.h"
#include "libsyncengine/jobs/network/API_v2/movejob.h"
#include "libcommon/keychainmanager/keychainmanager.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/network/proxy.h"
#include "syncpal/tmpblacklistmanager.h"
#include "test_utility/testhelpers.h"

#include <cstdlib>
#include "testoperationprocessor.h"

using namespace CppUnit;

namespace KDC {

void TestOperationProcessor::setUp() {
    TestBase::start();
    const testhelpers::TestVariables testVariables;

    const std::string localPathStr = _localTempDir.path().string();
    // Insert api token into keystore
    ApiToken apiToken;
    apiToken.setAccessToken(testVariables.apiToken);

    std::string keychainKey("123");
    KeyChainManager::instance(true);
    KeyChainManager::instance()->writeToken(keychainKey, apiToken.reconstructJsonString());

    // Create parmsDb
    bool alreadyExists = false;
    std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExists, true);
    ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);

    // Insert user, account, drive & sync
    int userId = atoi(testVariables.userId.c_str());
    User user(1, userId, keychainKey);
    ParmsDb::instance()->insertUser(user);

    int accountId(atoi(testVariables.accountId.c_str()));
    Account account(1, accountId, user.dbId());
    ParmsDb::instance()->insertAccount(account);

    _driveDbId = 1;
    int driveId = atoi(testVariables.driveId.c_str());
    Drive drive(_driveDbId, driveId, account.dbId(), std::string(), 0, std::string());
    ParmsDb::instance()->insertDrive(drive);
    Sync sync(1, drive.dbId(), localPathStr, testVariables.remotePath);
    ParmsDb::instance()->insertSync(sync);

    // Setup proxy
    Parameters parameters;
    bool found;
    if (ParmsDb::instance()->selectParameters(parameters, found) && found) {
        Proxy::instance(parameters.proxyConfig());
    }

    _syncPal = std::make_shared<SyncPal>(std::make_shared<VfsOff>(VfsSetupParams(Log::instance()->getLogger())), sync.dbId(),
                                         KDRIVE_VERSION_STRING);
    _syncPal->createSharedObjects();
    _syncPal->_tmpBlacklistManager = std::make_shared<TmpBlacklistManager>(_syncPal);
}

void TestOperationProcessor::tearDown() {
    // Stop SyncPal and delete sync DB
    ParmsDb::instance()->close();
    if (_syncPal) {
        _syncPal->stop(false, true, true);
    }
    ParmsDb::reset();
    TestBase::stop();
}

void TestOperationProcessor::testIsPseudoConflict() {
    class MockOperationProcessor : public OperationProcessor {
        public:
            using OperationProcessor::OperationProcessor;
            void execute() override {}
            bool isPseudoConflictTest(std::shared_ptr<Node> node, std::shared_ptr<Node> correspondingNode) {
                return OperationProcessor::isPseudoConflict(node, correspondingNode);
            }
    };

    MockOperationProcessor opProcessor(_syncPal, "Operation Processor", "OPP");

    auto localNodeSyncedDir =
            std::make_shared<Node>(3, ReplicaSide::Local, Str("testSyncedDir"), NodeType::Directory, OperationType::None, "ldid1",
                                   testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize,
                                   _syncPal->updateTree(ReplicaSide::Local)->rootNode());
    auto remoteNodeSyncedDir =
            std::make_shared<Node>(3, ReplicaSide::Remote, Str("testSyncedDir"), NodeType::Directory, OperationType::None,
                                   "rdid2", testhelpers::defaultTime + 2, testhelpers::defaultTime, testhelpers::defaultFileSize,
                                   _syncPal->updateTree(ReplicaSide::Remote)->rootNode());

    const auto localNodeFile = std::make_shared<Node>(2, ReplicaSide::Local, Str("testFile"), NodeType::File, OperationType::None,
                                                      "lfid", testhelpers::defaultTime, testhelpers::defaultTime,
                                                      testhelpers::defaultFileSize, localNodeSyncedDir);
    const auto remoteNodeFile = std::make_shared<Node>(
            2, ReplicaSide::Remote, Str("testFile"), NodeType::File, OperationType::None, "rfid", testhelpers::defaultTime + 2,
            testhelpers::defaultTime, testhelpers::defaultFileSize, remoteNodeSyncedDir);


    const auto newLocalNodeNotSyncedDir = std::make_shared<Node>( // Not synced directory (no dbId)
            ReplicaSide::Local, Str("testNotSyncedDirLocal"), NodeType::Directory, OperationType::None, "ldid3",
            testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize,
            _syncPal->updateTree(ReplicaSide::Local)->rootNode());
    const auto newRemoteNodeNotSyncedDir = std::make_shared<Node>( // Not synced directory (no dbId)
            ReplicaSide::Remote, Str("testNotSyncedDirRemote"), NodeType::Directory, OperationType::None, "rdid4",
            testhelpers::defaultTime + 2, testhelpers::defaultTime, testhelpers::defaultFileSize,
            _syncPal->updateTree(ReplicaSide::Remote)->rootNode());

    // Two nodes without change events
    CPPUNIT_ASSERT(!opProcessor.isPseudoConflictTest(localNodeFile, remoteNodeFile));

    // # Create-Created conflict
    // ## On a directory
    localNodeSyncedDir->setChangeEvents(OperationType::Create);
    remoteNodeSyncedDir->setChangeEvents(OperationType::Create);
    CPPUNIT_ASSERT(opProcessor.isPseudoConflictTest(localNodeSyncedDir, remoteNodeSyncedDir));

    // ## On a file
    // ### Same size and same modtime
    localNodeFile->setChangeEvents(OperationType::Create);
    remoteNodeFile->setChangeEvents(OperationType::Create);
    CPPUNIT_ASSERT(opProcessor.isPseudoConflictTest(localNodeFile, remoteNodeFile));

    // ### Different size and same modtime
    localNodeFile->setSize(testhelpers::defaultFileSize + 1);
    CPPUNIT_ASSERT(!opProcessor.isPseudoConflictTest(localNodeFile, remoteNodeFile));
    localNodeFile->setSize(testhelpers::defaultFileSize);
    remoteNodeFile->setSize(testhelpers::defaultFileSize + 1);
    CPPUNIT_ASSERT(!opProcessor.isPseudoConflictTest(localNodeFile, remoteNodeFile));
    remoteNodeFile->setSize(testhelpers::defaultFileSize);

    // ### Same size and different modtime
    localNodeFile->setLastModified(testhelpers::defaultTime + 1);
    CPPUNIT_ASSERT(!opProcessor.isPseudoConflictTest(localNodeFile, remoteNodeFile));
    localNodeFile->setLastModified(testhelpers::defaultTime);
    remoteNodeFile->setLastModified(testhelpers::defaultTime + 1);
    CPPUNIT_ASSERT(!opProcessor.isPseudoConflictTest(localNodeFile, remoteNodeFile));
    remoteNodeFile->setLastModified(testhelpers::defaultTime);

    // ### Different size and different modtime
    localNodeFile->setSize(testhelpers::defaultFileSize + 1);
    CPPUNIT_ASSERT(!opProcessor.isPseudoConflictTest(localNodeFile, remoteNodeFile));
    localNodeFile->setSize(testhelpers::defaultFileSize);
    remoteNodeFile->setSize(testhelpers::defaultFileSize + 1);
    CPPUNIT_ASSERT(!opProcessor.isPseudoConflictTest(localNodeFile, remoteNodeFile));
    remoteNodeFile->setSize(testhelpers::defaultFileSize);

    // # Move-Move (Source) conflict
    localNodeFile->setChangeEvents(OperationType::Move);
    remoteNodeFile->setChangeEvents(OperationType::Move);
    localNodeFile->setMoveOriginInfos({localNodeFile->getPath(), localNodeSyncedDir->id().value()});
    remoteNodeFile->setMoveOriginInfos({remoteNodeFile->getPath(), remoteNodeSyncedDir->id().value()});

    // ## Both destination nodes are the same already synchronized directory and the file keeps the same name
    CPPUNIT_ASSERT(localNodeFile->setParentNode(localNodeSyncedDir)); // Moved to a synced directory
    CPPUNIT_ASSERT(remoteNodeFile->setParentNode(remoteNodeSyncedDir)); // Moved to a synced directory
    CPPUNIT_ASSERT(opProcessor.isPseudoConflictTest(localNodeFile, remoteNodeFile));

    // ## Both destination nodes are the same already synchronized directory and the file name changes
    localNodeFile->setName(Str("ThisNameIsDifferentFromTheCorrespondingNodeOne"));
    CPPUNIT_ASSERT(!opProcessor.isPseudoConflictTest(localNodeFile, remoteNodeFile));
    localNodeFile->setName(remoteNodeFile->name()); // Reset name

    // ## One destination node is a not synchronized directory (Created in this sync) and the other is an already
    // synchronized directory
    CPPUNIT_ASSERT(localNodeFile->setParentNode(newLocalNodeNotSyncedDir)); // Moved to a not synced directory
    CPPUNIT_ASSERT(remoteNodeFile->setParentNode(remoteNodeSyncedDir)); // Moved to a synced directory
    CPPUNIT_ASSERT(!opProcessor.isPseudoConflictTest(localNodeFile, remoteNodeFile));

    // ## Both destination nodes are two different not synchronized directory (Created in this sync)
    CPPUNIT_ASSERT(localNodeFile->setParentNode(newLocalNodeNotSyncedDir)); // Moved to a not synced directory
    CPPUNIT_ASSERT(remoteNodeFile->setParentNode(newRemoteNodeNotSyncedDir)); // Moved to a not synced directory
    CPPUNIT_ASSERT(!opProcessor.isPseudoConflictTest(localNodeFile, remoteNodeFile));


    // Ensure that all the other combinations of operations do not generate pseudo conflicts
    const std::array<OperationType, 4> allOp = {
            OperationType::Edit, OperationType::Delete, OperationType::Move /*, OperationType::Create CreateCreate is handled and
                                                                               other CreateXXXXX combinations are not possible*/
    };
    for (const auto &op: allOp) {
        for (const auto &op2: allOp) {
            if (op == OperationType::Move && op2 == OperationType::Move) continue;
            if (op == OperationType::Edit && op2 == OperationType::Edit) continue;

            if (op == OperationType::Move)
                localNodeFile->setMoveOriginInfos({localNodeFile->getPath(), localNodeSyncedDir->id().value()});
            if (op2 == OperationType::Move)
                remoteNodeFile->setMoveOriginInfos({remoteNodeFile->getPath(), remoteNodeSyncedDir->id().value()});

            localNodeFile->setChangeEvents(op);
            remoteNodeFile->setChangeEvents(op2);
            CPPUNIT_ASSERT_MESSAGE(
                    toString(localNodeFile->changeEvents()) + std::string(" and ") + toString(remoteNodeFile->changeEvents()),
                    !opProcessor.isPseudoConflictTest(localNodeFile, remoteNodeFile));
        }
    }
}
} // namespace KDC
