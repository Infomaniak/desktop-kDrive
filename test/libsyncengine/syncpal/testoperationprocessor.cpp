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

#include "testoperationprocessor.h"
#include "testsyncpal.h"
#include "syncpal/tmpblacklistmanager.h"
#include "libcommon/keychainmanager/keychainmanager.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/network/proxy.h"
#include "libsyncengine/jobs/network/kDrive_API/movejob.h"
#include "mocks/libcommonserver/db/mockdb.h"

#include "test_utility/testhelpers.h"
#include "test_classes/testsituationgenerator.h"
#include "update_detection/file_system_observer/filesystemobserverworker.h"

#include <cstdlib>

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
    (void) KeyChainManager::instance(true);
    (void) KeyChainManager::instance()->writeToken(keychainKey, apiToken.reconstructJsonString());

    // Create parmsDb
    (void) ParmsDb::instance(_localTempDir.path() / MockDb::makeDbMockFileName(), KDRIVE_VERSION_STRING, true, true);

    // Insert user, account, drive & sync
    int userId = atoi(testVariables.userId.c_str());
    User user(1, userId, keychainKey);
    (void) ParmsDb::instance()->insertUser(user);

    int accountId(atoi(testVariables.accountId.c_str()));
    Account account(1, accountId, user.dbId());
    (void) ParmsDb::instance()->insertAccount(account);

    _driveDbId = 1;
    int driveId = atoi(testVariables.driveId.c_str());
    Drive drive(_driveDbId, driveId, account.dbId(), std::string(), 0, std::string());
    (void) ParmsDb::instance()->insertDrive(drive);
    Sync sync(1, drive.dbId(), localPathStr, "", testVariables.remotePath);
    (void) ParmsDb::instance()->insertSync(sync);

    // Setup proxy
    Parameters parameters;
    bool found = false;
    if (ParmsDb::instance()->selectParameters(parameters, found) && found) {
        Proxy::instance(parameters.proxyConfig());
    }

    _syncPal = std::make_shared<SyncPal>(std::make_shared<VfsOff>(VfsSetupParams(Log::instance()->getLogger())), sync.dbId(),
                                         KDRIVE_VERSION_STRING);
    _syncPal->syncDb()->setAutoDelete(true);
    _syncPal->createSharedObjects();
    _syncPal->createWorkers();
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
            bool isPseudoConflictTest(const std::shared_ptr<Node> node, const std::shared_ptr<Node> correspondingNode) {
                return isPseudoConflict(node, correspondingNode);
            }
    };

    MockOperationProcessor opProcessor(_syncPal, "Operation Processor", "OPP");

    // Initial situation
    // .
    // └── A
    //     └── AA
    TestSituationGenerator situationGenerator(_syncPal);
    situationGenerator.generateInitialSituation(R"({"a":{"aa":1}})");

    const auto lNodeA = situationGenerator.getNode(ReplicaSide::Local, "a");
    const auto rNodeA = situationGenerator.getNode(ReplicaSide::Remote, "a");
    const auto lNodeAA = situationGenerator.getNode(ReplicaSide::Local, "aa");
    const auto rNodeAA = situationGenerator.getNode(ReplicaSide::Remote, "aa");
    LiveSnapshot &localLiveSnapshot = _syncPal->_localFSObserverWorker->_liveSnapshot;
    LiveSnapshot &remotelLiveSnapshot = _syncPal->_remoteFSObserverWorker->_liveSnapshot;

    (void) localLiveSnapshot.updateItem(SnapshotItem("ldid", localLiveSnapshot.rootFolderId(), Str("testLocalDir"),
                                                     testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory,
                                                     testhelpers::defaultDirSize, false, true, true));
    const auto newLocalDirNode = std::make_shared<Node>( // Not synced directory (no dbId)
            ReplicaSide::Local, Str("testLocalDir"), NodeType::Directory, OperationType::None, "ldid", testhelpers::defaultTime,
            testhelpers::defaultTime, testhelpers::defaultDirSize, _syncPal->updateTree(ReplicaSide::Local)->rootNode());

    (void) remotelLiveSnapshot.updateItem(SnapshotItem("rdid", remotelLiveSnapshot.rootFolderId(), Str("testRemoteDir"),
                                                       testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory,
                                                       testhelpers::defaultDirSize, false, true, true));
    const auto newRemoteDirNode = std::make_shared<Node>( // Not synced directory (no dbId)
            ReplicaSide::Remote, Str("testRemoteDir"), NodeType::Directory, OperationType::None, "rdid", testhelpers::defaultTime,
            testhelpers::defaultTime, testhelpers::defaultDirSize, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());

    (void) localLiveSnapshot.updateItem(SnapshotItem("lfid", localLiveSnapshot.rootFolderId(), Str("testLocalFile"),
                                                     testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File,
                                                     testhelpers::defaultFileSize, true, true, true));
    const auto newLocalFileNode = std::make_shared<Node>( // Not synced file (no dbId)
            ReplicaSide::Local, Str("testLocalFile"), NodeType::File, OperationType::None, "lfid", testhelpers::defaultTime,
            testhelpers::defaultTime, testhelpers::defaultFileSize, _syncPal->updateTree(ReplicaSide::Local)->rootNode());

    (void) remotelLiveSnapshot.updateItem(SnapshotItem("rfid", remotelLiveSnapshot.rootFolderId(), Str("testRemoteFile"),
                                                       testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File,
                                                       testhelpers::defaultFileSize, true, true, true));
    const auto newRemoteFileNode = std::make_shared<Node>( // Not synced file (no dbId)
            ReplicaSide::Remote, Str("testRemoteFile"), NodeType::File, OperationType::None, "rfid", testhelpers::defaultTime,
            testhelpers::defaultTime, testhelpers::defaultFileSize, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());

    _syncPal->copySnapshots();

    // Two nodes without change events
    CPPUNIT_ASSERT(!opProcessor.isPseudoConflictTest(lNodeAA, rNodeAA));

    // # Create-Create conflict
    // ## On a directory
    lNodeA->setChangeEvents(OperationType::Create);
    rNodeA->setChangeEvents(OperationType::Create);
    CPPUNIT_ASSERT(opProcessor.isPseudoConflictTest(lNodeA, rNodeA));

    // ## On a file
    // ### Same size and same modtime
    lNodeAA->setChangeEvents(OperationType::Create);
    rNodeAA->setChangeEvents(OperationType::Create);
    CPPUNIT_ASSERT(opProcessor.isPseudoConflictTest(lNodeAA, rNodeAA));

    // ### Same size and same modtime, but items are links
    newLocalFileNode->setChangeEvents(OperationType::Create);
    newRemoteFileNode->setChangeEvents(OperationType::Create);
    CPPUNIT_ASSERT(opProcessor.isPseudoConflictTest(newLocalFileNode, newRemoteFileNode));

    // ### Different size and same modtime
    lNodeAA->setSize(testhelpers::defaultFileSize + 1);
    CPPUNIT_ASSERT(!opProcessor.isPseudoConflictTest(lNodeAA, rNodeAA));
    lNodeAA->setSize(testhelpers::defaultFileSize); // Reset size

    rNodeAA->setSize(testhelpers::defaultFileSize + 1);
    CPPUNIT_ASSERT(!opProcessor.isPseudoConflictTest(lNodeAA, rNodeAA));
    rNodeAA->setSize(testhelpers::defaultFileSize); // Reset size

    // ### Different size and same modtime, but items are links
    newLocalFileNode->setSize(testhelpers::defaultFileSize + 1);
    CPPUNIT_ASSERT(opProcessor.isPseudoConflictTest(newLocalFileNode, newRemoteFileNode));
    newLocalFileNode->setSize(testhelpers::defaultFileSize); // Reset size

    newRemoteFileNode->setSize(testhelpers::defaultFileSize + 1);
    CPPUNIT_ASSERT(opProcessor.isPseudoConflictTest(newLocalFileNode, newRemoteFileNode));
    newRemoteFileNode->setSize(testhelpers::defaultFileSize); // Reset size

    // ### Same size and different modtime
    lNodeAA->setModificationTime(testhelpers::defaultTime + 1);
    CPPUNIT_ASSERT(!opProcessor.isPseudoConflictTest(lNodeAA, rNodeAA));
    lNodeAA->setModificationTime(testhelpers::defaultTime); // Reset time

    rNodeAA->setModificationTime(testhelpers::defaultTime + 1);
    CPPUNIT_ASSERT(!opProcessor.isPseudoConflictTest(lNodeAA, rNodeAA));
    rNodeAA->setModificationTime(testhelpers::defaultTime); // Reset time

    // ### Same size and different modtime, but items are links
    newLocalFileNode->setModificationTime(testhelpers::defaultTime + 1);
    CPPUNIT_ASSERT(!opProcessor.isPseudoConflictTest(newLocalFileNode, newRemoteFileNode));
    newLocalFileNode->setModificationTime(testhelpers::defaultTime); // Reset time

    newRemoteFileNode->setModificationTime(testhelpers::defaultTime + 1);
    CPPUNIT_ASSERT(!opProcessor.isPseudoConflictTest(newLocalFileNode, newRemoteFileNode));
    newRemoteFileNode->setModificationTime(testhelpers::defaultTime); // Reset time

    // ### Different size and different modtime
    lNodeAA->setModificationTime(testhelpers::defaultTime + 1);
    lNodeAA->setSize(testhelpers::defaultFileSize + 1);
    CPPUNIT_ASSERT(!opProcessor.isPseudoConflictTest(lNodeAA, rNodeAA));
    lNodeAA->setModificationTime(testhelpers::defaultTime); // Reset time
    lNodeAA->setSize(testhelpers::defaultFileSize); // Reset size

    rNodeAA->setModificationTime(testhelpers::defaultTime + 1);
    rNodeAA->setSize(testhelpers::defaultFileSize + 1);
    CPPUNIT_ASSERT(!opProcessor.isPseudoConflictTest(lNodeAA, rNodeAA));
    rNodeAA->setModificationTime(testhelpers::defaultTime); // Reset time
    rNodeAA->setSize(testhelpers::defaultFileSize); // Reset size

    // ### Different size and different modtime, but items are links
    newLocalFileNode->setModificationTime(testhelpers::defaultTime + 1);
    newLocalFileNode->setSize(testhelpers::defaultFileSize + 1);
    CPPUNIT_ASSERT(!opProcessor.isPseudoConflictTest(newLocalFileNode, newRemoteFileNode));
    newLocalFileNode->setModificationTime(testhelpers::defaultTime); // Reset time
    newLocalFileNode->setSize(testhelpers::defaultFileSize); // Reset size

    newRemoteFileNode->setModificationTime(testhelpers::defaultTime + 1);
    newRemoteFileNode->setSize(testhelpers::defaultFileSize + 1);
    CPPUNIT_ASSERT(!opProcessor.isPseudoConflictTest(newLocalFileNode, newRemoteFileNode));
    newRemoteFileNode->setModificationTime(testhelpers::defaultTime); // Reset time
    newRemoteFileNode->setSize(testhelpers::defaultFileSize); // Reset size

    // # Move-Move (Source) conflict
    lNodeAA->setMoveOriginInfos({lNodeAA->getPath(), lNodeA->id().value()});
    rNodeAA->setMoveOriginInfos({rNodeAA->getPath(), rNodeA->id().value()});
    lNodeAA->setChangeEvents(OperationType::Move);
    rNodeAA->setChangeEvents(OperationType::Move);

    // ## Both destination nodes are the same already synchronized directory and the file keeps the same name
    CPPUNIT_ASSERT(lNodeAA->setParentNode(lNodeA)); // Moved to a synced directory
    CPPUNIT_ASSERT(rNodeAA->setParentNode(rNodeA)); // Moved to a synced directory
    CPPUNIT_ASSERT(opProcessor.isPseudoConflictTest(lNodeAA, rNodeAA));

    // ## Both destination nodes are the same already synchronized directory and the file name changes
    lNodeAA->setName(Str("ThisNameIsDifferentFromTheCorrespondingNodeOne"));
    CPPUNIT_ASSERT(!opProcessor.isPseudoConflictTest(lNodeAA, rNodeAA));
    lNodeAA->setName(rNodeAA->name()); // Reset name

    // ## One destination node is a not synchronized directory (Created in this sync) and the other is an already
    // synchronized directory
    CPPUNIT_ASSERT(lNodeAA->setParentNode(newLocalDirNode)); // Moved to a not synced directory
    CPPUNIT_ASSERT(rNodeAA->setParentNode(rNodeA)); // Moved to a synced directory
    CPPUNIT_ASSERT(!opProcessor.isPseudoConflictTest(lNodeAA, rNodeAA));

    // ## Both destination nodes are two different not synchronized directory (Created in this sync)
    CPPUNIT_ASSERT(lNodeAA->setParentNode(newLocalDirNode)); // Moved to a not synced directory
    CPPUNIT_ASSERT(rNodeAA->setParentNode(newRemoteDirNode)); // Moved to a not synced directory
    CPPUNIT_ASSERT(!opProcessor.isPseudoConflictTest(lNodeAA, rNodeAA));


    // Ensure that all the other combinations of operations do not generate pseudo conflicts
    const std::array<OperationType, 4> opTypes = {
            OperationType::Edit, OperationType::Delete, OperationType::Move /*, OperationType::Create CreateCreate is handled and
                                                                               other CreateXXXXX combinations are not possible*/
    };
    for (const auto &localOp: opTypes) {
        for (const auto &remoteOp: opTypes) {
            if (localOp == OperationType::Move && remoteOp == OperationType::Move) continue; // Already tested
            if (localOp == OperationType::Edit && remoteOp == OperationType::Edit) continue; // Already tested

            if (localOp == OperationType::Move) lNodeAA->setMoveOriginInfos({lNodeAA->getPath(), lNodeA->id().value()});
            if (remoteOp == OperationType::Move) rNodeAA->setMoveOriginInfos({rNodeAA->getPath(), rNodeA->id().value()});

            lNodeAA->setChangeEvents(localOp);
            rNodeAA->setChangeEvents(remoteOp);
            CPPUNIT_ASSERT_MESSAGE(toString(lNodeAA->changeEvents()) + std::string(" and ") + toString(rNodeAA->changeEvents()),
                                   !opProcessor.isPseudoConflictTest(lNodeAA, rNodeAA));
        }
    }
}
} // namespace KDC
