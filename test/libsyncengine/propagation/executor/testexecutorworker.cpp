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

#include "testexecutorworker.h"

#include "vfs.h"

#include <memory>
#include "propagation/executor/executorworker.h"
#include "libcommonserver/io/testio.h"
#include "server/vfs/mac/litesyncextconnector.h"
#include "io/filestat.h"
#include "keychainmanager/keychainmanager.h"
#include "network/proxy.h"
#include "server/vfs/mac/vfs_mac.h"
#include "test_utility/testhelpers.h"
#include "utility/utility.h"

namespace KDC {

void TestExecutorWorker::setUp() {
    const testhelpers::TestVariables testVariables;

    const std::string localPathStr = _localTempDir.path().string();

    // Insert api token into keystore
    std::string keychainKey("123");
    KeyChainManager::instance(true);
    KeyChainManager::instance()->writeToken(keychainKey, testVariables.apiToken);

    // Create parmsDb
    bool alreadyExists = false;
    std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExists, true);
    std::filesystem::remove(parmsDbPath);
    ParmsDb::instance(parmsDbPath, "3.4.0", true, true);

    // Insert user, account, drive & sync
    int userId(12321);
    User user(1, userId, keychainKey);
    ParmsDb::instance()->insertUser(user);

    int accountId(atoi(testVariables.accountId.c_str()));
    Account account(1, accountId, user.dbId());
    ParmsDb::instance()->insertAccount(account);

    int driveDbId = 1;
    int driveId = atoi(testVariables.driveId.c_str());
    Drive drive(driveDbId, driveId, account.dbId(), std::string(), 0, std::string());
    ParmsDb::instance()->insertDrive(drive);

    _sync = Sync(1, drive.dbId(), localPathStr, testVariables.remotePath);
    ParmsDb::instance()->insertSync(_sync);

    // Setup proxy
    Parameters parameters;
    if (bool found = false; ParmsDb::instance()->selectParameters(parameters, found) && found) {
        Proxy::instance(parameters.proxyConfig());
    }

    _syncPal = std::make_shared<SyncPal>(_sync.dbId(), "3.4.0");
    _syncPal->createWorkers();
    _syncPal->syncDb()->setAutoDelete(true);
}

void TestExecutorWorker::testCheckLiteSyncInfoForCreate() {
#ifdef __APPLE__
    // Setup dummy values. Test inputs are set in the callbacks defined below.
    const auto opPtr = std::make_shared<SyncOperation>();
    opPtr->setTargetSide(ReplicaSide::Remote);
    const auto node = std::make_shared<Node>(1, ReplicaSide::Local, "test_file.txt", NodeType::File, OperationType::None, "1234",
                                             testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize,
                                             _syncPal->updateTree(ReplicaSide::Local)->rootNode());
    opPtr->setAffectedNode(node);

    // A hydrated placeholder.
    {
        _syncPal->setVfsStatusCallback([]([[maybe_unused]] int syncDbId, [[maybe_unused]] const SyncPath &itemPath,
                                          bool &isPlaceholder, bool &isHydrated, bool &isSyncing, int &progress) -> bool {
            isPlaceholder = true;
            isHydrated = true;
            isSyncing = false;
            progress = 0;
            return true;
        });

        bool isDehydratedPlaceholder = false;
        _syncPal->_executorWorker->checkLiteSyncInfoForCreate(opPtr, "/", isDehydratedPlaceholder);

        CPPUNIT_ASSERT(!isDehydratedPlaceholder);
    }

    // A dehydrated placeholder.
    {
        _syncPal->setVfsStatusCallback([]([[maybe_unused]] int syncDbId, [[maybe_unused]] const SyncPath &itemPath,
                                          bool &isPlaceholder, bool &isHydrated, bool &isSyncing, int &progress) -> bool {
            isPlaceholder = true;
            isHydrated = false;
            isSyncing = false;
            progress = 0;
            return true;
        });

        bool isDehydratedPlaceholder = false;
        _syncPal->_executorWorker->checkLiteSyncInfoForCreate(opPtr, "/", isDehydratedPlaceholder);

        CPPUNIT_ASSERT(isDehydratedPlaceholder);
    }

    // A partially hydrated placeholder (syncing item).
    {
        _syncPal->setVfsStatusCallback([]([[maybe_unused]] int syncDbId, [[maybe_unused]] const SyncPath &itemPath,
                                          bool &isPlaceholder, bool &isHydrated, bool &isSyncing, int &progress) -> bool {
            isPlaceholder = true;
            isHydrated = false;
            isSyncing = true;
            progress = 30;
            return true;
        });

        bool isDehydratedPlaceholder = false;
        _syncPal->_executorWorker->checkLiteSyncInfoForCreate(opPtr, "/", isDehydratedPlaceholder);

        CPPUNIT_ASSERT(!isDehydratedPlaceholder);
    }

    // Not a placeholder.
    {
        _syncPal->setVfsStatusCallback([]([[maybe_unused]] int syncDbId, [[maybe_unused]] const SyncPath &itemPath,
                                          bool &isPlaceholder, bool &isHydrated, bool &isSyncing, int &progress) -> bool {
            isPlaceholder = false;
            isHydrated = false;
            isSyncing = false;
            progress = 0;
            return true;
        });

        bool isDehydratedPlaceholder = false;
        _syncPal->_executorWorker->checkLiteSyncInfoForCreate(opPtr, "/", isDehydratedPlaceholder);

        CPPUNIT_ASSERT(!isDehydratedPlaceholder);
    }
#endif
}

void TestExecutorWorker::testFixModificationDate() {
    // Create temp directory
    const LocalTemporaryDirectory temporaryDirectory;
    // Create file
    const SyncName filename = Str("test_file.txt");
    const SyncPath path = temporaryDirectory.path() / filename;
    {
        std::ofstream ofs(path);
        ofs << "abc";
        ofs.close();
    }

    // Update DB
    DbNode dbNode(0, _syncPal->syncDb()->rootNode().nodeId(), filename, filename, "lid", "rid", testhelpers::defaultTime,
                  testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File, testhelpers::defaultFileSize, "cs");
    DbNodeId dbNodeId;
    bool constraintError = false;
    _syncPal->syncDb()->insertNode(dbNode, dbNodeId, constraintError);

    // Generate sync operation
    std::shared_ptr<Node> node = std::make_shared<Node>(
        dbNodeId, ReplicaSide::Local, filename, NodeType::File, OperationType::None, "lid", testhelpers::defaultTime,
        testhelpers::defaultTime, testhelpers::defaultFileSize, _syncPal->updateTree(ReplicaSide::Local)->rootNode());
    std::shared_ptr<Node> correspondingNode = std::make_shared<Node>(
        dbNodeId, ReplicaSide::Remote, filename, NodeType::File, OperationType::None, "rid", testhelpers::defaultTime,
        testhelpers::defaultTime, testhelpers::defaultFileSize, _syncPal->updateTree(ReplicaSide::Remote)->rootNode());
    SyncOpPtr op = std::make_shared<SyncOperation>();
    op->setAffectedNode(node);
    op->setCorrespondingNode(correspondingNode);

    CPPUNIT_ASSERT(_syncPal->_executorWorker->fixModificationDate(op, path));

    FileStat filestat;
    IoError ioError = IoError::Unknown;
    IoHelper::getFileStat(path, &filestat, ioError);

    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT_EQUAL(testhelpers::defaultTime, filestat.modtime);
}

void TestExecutorWorker::testAffectedUpdateTree() {
    // Normal cases
    auto syncOp = std::make_shared<SyncOperation>();
    syncOp->setTargetSide(ReplicaSide::Local);
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Remote, _syncPal->_executorWorker->affectedUpdateTree(syncOp)->side());

    syncOp->setTargetSide(ReplicaSide::Remote);
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, _syncPal->_executorWorker->affectedUpdateTree(syncOp)->side());

    // ReplicaSide::Unknown case
    syncOp->setTargetSide(ReplicaSide::Unknown);
    CPPUNIT_ASSERT_EQUAL(std::shared_ptr<UpdateTree>(nullptr), _syncPal->_executorWorker->affectedUpdateTree(syncOp));
}

void TestExecutorWorker::testTargetUpdateTree() {
    // Normal cases
    auto syncOp = std::make_shared<SyncOperation>();
    syncOp->setTargetSide(ReplicaSide::Local);
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, _syncPal->_executorWorker->targetUpdateTree(syncOp)->side());

    syncOp->setTargetSide(ReplicaSide::Remote);
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Remote, _syncPal->_executorWorker->targetUpdateTree(syncOp)->side());

    // ReplicaSide::Unknown case
    syncOp->setTargetSide(ReplicaSide::Unknown);
    CPPUNIT_ASSERT_EQUAL(std::shared_ptr<UpdateTree>(nullptr), _syncPal->_executorWorker->targetUpdateTree(syncOp));
}
}  // namespace KDC