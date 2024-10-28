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

#include "propagation/executor/executorworker.h"
#include "io/filestat.h"
#include "io/iohelper.h"
#include "keychainmanager/keychainmanager.h"
#include "network/proxy.h"
#include "test_utility/testhelpers.h"

#include <memory>

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
    ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);

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

    _syncPal = std::make_shared<SyncPal>(_sync.dbId(), KDRIVE_VERSION_STRING);
    _syncPal->createWorkers();
    _syncPal->syncDb()->setAutoDelete(true);
}

void TestExecutorWorker::tearDown() {
    ParmsDb::instance()->close();
    ParmsDb::reset();
    if (_syncPal && _syncPal->syncDb()) {
        _syncPal->syncDb()->close();
    }
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

void TestExecutorWorker::testLogCorrespondingNodeErrorMsg() {
    SyncOpPtr op = generateSyncOperation(1, Str("test_file.txt"));
    _syncPal->_executorWorker->logCorrespondingNodeErrorMsg(op);

    op->setCorrespondingNode(nullptr);
    _syncPal->_executorWorker->logCorrespondingNodeErrorMsg(op);
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

    SyncOpPtr op = generateSyncOperation(dbNodeId, filename);
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

void TestExecutorWorker::testHasRight() {
    // Target side = Remote
    auto targetSide = ReplicaSide::Remote;
    DbNodeId dbNodeId = 1;
    /// Normal case with Create operation
    {
        const auto path = SyncPath(_syncPal->localPath() / CommonUtility::generateRandomStringAlphaNum());
        testhelpers::generateOrEditTestFile(path);
        const auto syncOp = generateSyncOperation(++dbNodeId, path.filename(), targetSide, NodeType::File);
        syncOp->setType(OperationType::Create);
        bool exists = false;
        CPPUNIT_ASSERT(_syncPal->_executorWorker->hasRight(syncOp, exists));
        CPPUNIT_ASSERT(exists);
    }

    /// Create operation but local file does not exist anymore
    {
        const auto filename = CommonUtility::generateRandomStringAlphaNum();
        const auto path = SyncPath(_syncPal->localPath() / filename);
        const auto syncOp = generateSyncOperation(++dbNodeId, path.filename(), targetSide, NodeType::File);
        syncOp->setType(OperationType::Create);
        bool exists = false;
        CPPUNIT_ASSERT(!_syncPal->_executorWorker->hasRight(syncOp, exists));
        CPPUNIT_ASSERT(!exists);
    }

    /// Create operation but local file does not have write permissions anymore
    {
        const auto filename = CommonUtility::generateRandomStringAlphaNum();
        const auto path = SyncPath(_syncPal->localPath() / filename);
        testhelpers::generateOrEditTestFile(path);
        permissions(path, std::filesystem::perms::owner_write, std::filesystem::perm_options::remove);
        const auto syncOp = generateSyncOperation(++dbNodeId, path.filename(), targetSide, NodeType::File);
        syncOp->setType(OperationType::Create);
        bool exists = false;
        CPPUNIT_ASSERT(!_syncPal->_executorWorker->hasRight(syncOp, exists));
        CPPUNIT_ASSERT(exists);
    }

    /// Create operation inside the "Common documents" folder
    {
        // Add parent nodes
        const SyncPath parentPath = _syncPal->localPath() / Utility::commonDocumentsFolderName();
        create_directory(parentPath);

        std::shared_ptr<Node> parentNode;
        std::shared_ptr<Node> correspondingParentNode;
        generateNodes(parentNode, correspondingParentNode, ++dbNodeId, Utility::commonDocumentsFolderName(),
                      _syncPal->updateTree(targetSide)->rootNode(), _syncPal->updateTree(otherSide(targetSide))->rootNode(),
                      targetSide, NodeType::Directory);
        _syncPal->updateTree(targetSide)->insertNode(parentNode);
        _syncPal->updateTree(otherSide(targetSide))->insertNode(correspondingParentNode);


        const auto filename = CommonUtility::generateRandomStringAlphaNum();
        const auto path = SyncPath(parentPath / filename);
        testhelpers::generateOrEditTestFile(path);
        const auto syncOp = generateSyncOperation(++dbNodeId, path.filename(), ReplicaSide::Remote, NodeType::Directory,
                                                  parentNode, correspondingParentNode);
        syncOp->setType(OperationType::Create);
        bool exists = false;
        CPPUNIT_ASSERT(!_syncPal->_executorWorker->hasRight(syncOp, exists));
        CPPUNIT_ASSERT(exists);
    }

    /// Create operation inside the "Shared" folder
    {
        // Add parent nodes
        const SyncPath parentPath = _syncPal->localPath() / Utility::sharedFolderName();
        create_directory(parentPath);

        std::shared_ptr<Node> parentNode;
        std::shared_ptr<Node> correspondingParentNode;
        generateNodes(parentNode, correspondingParentNode, ++dbNodeId, Utility::sharedFolderName(),
                      _syncPal->updateTree(targetSide)->rootNode(), _syncPal->updateTree(otherSide(targetSide))->rootNode(),
                      targetSide, NodeType::Directory);
        _syncPal->updateTree(targetSide)->insertNode(parentNode);
        _syncPal->updateTree(otherSide(targetSide))->insertNode(correspondingParentNode);

        const auto filename = CommonUtility::generateRandomStringAlphaNum();
        const auto path = SyncPath(parentPath / filename);
        testhelpers::generateOrEditTestFile(path);
        const auto syncOp = generateSyncOperation(++dbNodeId, path.filename(), ReplicaSide::Remote, NodeType::Directory,
                                                  parentNode, correspondingParentNode);
        syncOp->setType(OperationType::Create);
        bool exists = false;
        CPPUNIT_ASSERT(!_syncPal->_executorWorker->hasRight(syncOp, exists));
        CPPUNIT_ASSERT(exists);
    }

    /// Normal case with Edit operation
    {
        const auto path = SyncPath(_syncPal->localPath() / CommonUtility::generateRandomStringAlphaNum());
        testhelpers::generateOrEditTestFile(path);
        const auto syncOp = generateSyncOperation(++dbNodeId, path.filename(), targetSide, NodeType::File);
        syncOp->setType(OperationType::Edit);
        bool exists = false;
        CPPUNIT_ASSERT(_syncPal->_executorWorker->hasRight(syncOp, exists));
        CPPUNIT_ASSERT(exists);
    }

    /// Normal case with Move operation
    {
        const auto filename = CommonUtility::generateRandomStringAlphaNum();
        const auto syncOp = generateSyncOperation(++dbNodeId, filename, targetSide, NodeType::File);
        syncOp->setType(OperationType::Move);
        bool exists = false;
        CPPUNIT_ASSERT(_syncPal->_executorWorker->hasRight(syncOp, exists));
    }

    /// Normal case with Delete operation
    {
        const auto filename = CommonUtility::generateRandomStringAlphaNum();
        const auto syncOp = generateSyncOperation(++dbNodeId, filename, targetSide, NodeType::File);
        syncOp->setType(OperationType::Delete);
        bool exists = false;
        CPPUNIT_ASSERT(_syncPal->_executorWorker->hasRight(syncOp, exists));
    }

    // Target side = Local
    targetSide = ReplicaSide::Local;

    /// Normal case with Create operation
    {
        const auto path = SyncPath(_syncPal->localPath() / CommonUtility::generateRandomStringAlphaNum());
        const auto syncOp = generateSyncOperation(++dbNodeId, path.filename(), targetSide, NodeType::File);
        syncOp->setType(OperationType::Create);
        bool exists = false;
        CPPUNIT_ASSERT(_syncPal->_executorWorker->hasRight(syncOp, exists));
        CPPUNIT_ASSERT(!exists);
    }

    /// Create operation but local file already exist
    {
        const auto path = SyncPath(_syncPal->localPath() / CommonUtility::generateRandomStringAlphaNum());
        testhelpers::generateOrEditTestFile(path);
        const auto syncOp = generateSyncOperation(++dbNodeId, path.filename(), targetSide, NodeType::File);
        syncOp->setType(OperationType::Create);
        bool exists = false;
        CPPUNIT_ASSERT(_syncPal->_executorWorker->hasRight(syncOp, exists));
        CPPUNIT_ASSERT(exists);
    }

    /// Create operation but local file already exist and does not have write permission
    {
        const auto path = SyncPath(_syncPal->localPath() / CommonUtility::generateRandomStringAlphaNum());
        testhelpers::generateOrEditTestFile(path);
        permissions(path, std::filesystem::perms::owner_write, std::filesystem::perm_options::remove);
        const auto syncOp = generateSyncOperation(++dbNodeId, path.filename(), targetSide, NodeType::File);
        syncOp->setType(OperationType::Create);
        bool exists = false;
        CPPUNIT_ASSERT(!_syncPal->_executorWorker->hasRight(syncOp, exists));
        CPPUNIT_ASSERT(exists);
    }

    /// Create operation but no write permission on parent folder
    {
        const auto path = SyncPath(_syncPal->localPath() / CommonUtility::generateRandomStringAlphaNum());
        permissions(_syncPal->localPath(), std::filesystem::perms::owner_write, std::filesystem::perm_options::remove);
        const auto syncOp = generateSyncOperation(++dbNodeId, path.filename(), targetSide, NodeType::File);
        syncOp->setType(OperationType::Create);
        bool exists = false;
        CPPUNIT_ASSERT(_syncPal->_executorWorker->hasRight(syncOp, exists));
        CPPUNIT_ASSERT(!exists);
        permissions(_syncPal->localPath(), std::filesystem::perms::all, std::filesystem::perm_options::add);
    }

    /// Same behavior for Edit, Move and Delete operations
    const auto opTypes = {OperationType::Edit, OperationType::Move, OperationType::Delete};
    for (const auto opType: opTypes) {
        /// Normal case
        {
            const auto path = SyncPath(_syncPal->localPath() / CommonUtility::generateRandomStringAlphaNum());
            testhelpers::generateOrEditTestFile(path);
            const auto syncOp = generateSyncOperation(++dbNodeId, path.filename(), targetSide, NodeType::File);
            syncOp->setType(opType);
            bool exists = false;
            CPPUNIT_ASSERT(_syncPal->_executorWorker->hasRight(syncOp, exists));
            CPPUNIT_ASSERT(exists);
        }

        /// File does not exist anymore
        {
            const auto path = SyncPath(_syncPal->localPath() / CommonUtility::generateRandomStringAlphaNum());
            const auto syncOp = generateSyncOperation(++dbNodeId, path.filename(), targetSide, NodeType::File);
            syncOp->setType(opType);
            bool exists = false;
            CPPUNIT_ASSERT(!_syncPal->_executorWorker->hasRight(syncOp, exists));
            CPPUNIT_ASSERT(!exists);
        }

        /// File does not have write permission anymore
        {
            const auto path = SyncPath(_syncPal->localPath() / CommonUtility::generateRandomStringAlphaNum());
            testhelpers::generateOrEditTestFile(path);
            permissions(path, std::filesystem::perms::owner_write, std::filesystem::perm_options::remove);
            const auto syncOp = generateSyncOperation(++dbNodeId, path.filename(), targetSide, NodeType::File);
            syncOp->setType(opType);
            bool exists = false;
            CPPUNIT_ASSERT(!_syncPal->_executorWorker->hasRight(syncOp, exists));
            CPPUNIT_ASSERT(exists);
        }
    }
}

void TestExecutorWorker::generateNodes(std::shared_ptr<Node> &node, std::shared_ptr<Node> &correspondingNode, DbNodeId dbNodeId,
                                       const SyncName &filename, const std::shared_ptr<Node> &parentNode,
                                       const std::shared_ptr<Node> &correspondingParentNode, ReplicaSide targetSide,
                                       NodeType nodeType) const {
    const NodeId fileId = CommonUtility::generateRandomStringAlphaNum();
    node = std::make_shared<Node>(dbNodeId, targetSide, filename, nodeType, OperationType::None, "l" + fileId,
                                  testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize, parentNode);
    const auto correspondingSide = otherSide(targetSide);
    correspondingNode = std::make_shared<Node>(dbNodeId, correspondingSide, filename, nodeType, OperationType::None, "r" + fileId,
                                               testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultFileSize,
                                               correspondingParentNode);
}

SyncOpPtr TestExecutorWorker::generateSyncOperation(const DbNodeId dbNodeId, const SyncName &filename,
                                                    const ReplicaSide targetSide /*= ReplicaSide::Local*/,
                                                    const NodeType nodeType /*= NodeType::File*/,
                                                    std::shared_ptr<Node> parentNode /*= nullptr*/,
                                                    std::shared_ptr<Node> correspondingParentNode /*= nullptr*/) const {
    std::shared_ptr<Node> node;
    std::shared_ptr<Node> correspondingNode;
    if (!parentNode) parentNode = _syncPal->updateTree(targetSide)->rootNode();
    if (!correspondingParentNode) parentNode = _syncPal->updateTree(otherSide(targetSide))->rootNode();
    generateNodes(node, correspondingNode, dbNodeId, filename, parentNode, correspondingParentNode, targetSide, nodeType);

    auto op = std::make_shared<SyncOperation>();
    op->setAffectedNode(node);
    op->setCorrespondingNode(correspondingNode);
    op->setTargetSide(targetSide);

    return op;
}

} // namespace KDC
