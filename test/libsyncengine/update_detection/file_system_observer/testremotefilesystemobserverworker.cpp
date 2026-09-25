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

#include "testremotefilesystemobserverworker.h"
#include "update_detection/file_system_observer/remotefilesystemobserverworker.h"
#include "jobs/network/kDrive_API/listing/snapshotitemhandler.h"
#include "requests/syncnodecache.h"

#include "libcommon/utility/utility.h"

#include "libcommonserver/utility/utility.h"
#include "libcommonserver/keychainmanager/keychainmanager.h"
#include "mocks/mockkeychainstorage.h"

#include "libsyncengine/jobs/syncjobmanager.h"
#include "libsyncengine/jobs/network/kDrive_API/deletejob.h"
#include "libsyncengine/jobs/network/kDrive_API/movejob.h"
#include "libsyncengine/jobs/network/kDrive_API/renamejob.h"
#include "libsyncengine/jobs/network/kDrive_API/upload/uploadjob.h"
#include "libsyncengine/jobs/network/networkjobsparams.h"

#include "mocks/libcommonserver/db/mockdb.h"

#include "test_utility/localtemporarydirectory.h"
#include "test_utility/remotetemporarydirectory.h"
#include "test_utility/testhelpers_requests.h"
#include "test_utility/testhelpers.h"

#include <fstream>
#include <memory>

using namespace CppUnit;
using namespace std::literals;

namespace KDC {

// Test in drive "kDrive Desktop Team"
static const uint64_t nbFileInTestDir = 5; // "Common documents/Test kDrive/test_ci/test_remote_FSO/" contains 5 files
static const std::string endOfFileDelimiter("#EOF");
const NodeId testRemoteFsoDirId = "59541"; // Common documents/Test kDrive/test_ci/test_remote_FSO/
const NodeId testBlackListedDirId = "56851"; // Common documents/Test kDrive/test_ci/test_pictures/
const NodeId testBlackListedFileId = "97373"; // Common documents/Test kDrive/test_ci/test_pictures/picture-1.jpg

void TestRemoteFileSystemObserverWorker::setUp() {
    TestBase::start();
    _logger = Log::instance()->getLogger();

    LOG_DEBUG(_logger, "$$$$$ Set Up $$$$$");

    const testhelpers::TestVariables testVariables;

    _testFolderId = testVariables.remoteDirId;

    // Insert api token into keystore
    ApiToken apiToken;
    apiToken.setAccessToken(testVariables.apiToken);

    std::string keychainKey("123");
    (void) KeyChainManager::instance(std::make_shared<MockKeyChainStorage>());
    (void) KeyChainManager::instance()->writeData(keychainKey, apiToken.reconstructJsonString());

    // Create parmsDb
    (void) ParmsDb::instance(_localTempDir.path() / MockDb::makeDbMockFileName(), KDRIVE_VERSION_STRING, true, true);


    // Insert user, account, drive & sync
    const int userId(atoi(testVariables.userId.c_str()));
    User user(1, userId, keychainKey);
    (void) ParmsDb::instance()->insertUser(user);

    const int accountId(atoi(testVariables.accountId.c_str()));
    Account account(1, accountId, user.dbId(), "account1");
    (void) ParmsDb::instance()->insertAccount(account);

    _driveDbId = 1;
    const int driveId(atoi(testVariables.driveId.c_str()));
    Drive drive(_driveDbId, driveId, account.dbId(), std::string(), 0, std::string());
    (void) ParmsDb::instance()->insertDrive(drive);

    Sync sync(1, drive.dbId(), testhelpers::localTestDirPath(), "", "/");
    const auto syncDbPath = MockDb::makeDbName(userId, accountId, driveId, 1);
    sync.setDbPath(syncDbPath);
    (void) ParmsDb::instance()->insertSync(sync);

    _syncPal = std::make_shared<SyncPalTest>(sync.dbId(), KDRIVE_VERSION_STRING);
    _syncPal->syncDb()->setAutoDelete(true);
    _syncPal->createSharedObjects();
    _syncPal->createWorkers();
    /// Insert node in blacklist
    SyncNodeCache::instance()->update(_syncPal->syncDbId(), SyncNodeType::BlackList, {testBlackListedDirId});

    _syncPal->_remoteFSObserverWorker =
            std::make_shared<RemoteFileSystemObserverWorker>(_syncPal, "Remote File System Observer", "RFSO");
}

void TestRemoteFileSystemObserverWorker::tearDown() {
    LOG_DEBUG(_logger, "$$$$$ Tear Down $$$$$");

    // Delete file
    if (!_testFileId.empty()) {
        DeleteJob job(_driveDbId, _testFileId, "", "", NodeType::Directory);
        job.setBypassCheck(true);
        job.runSynchronously();
    }

    ParmsDb::instance()->close();
    ParmsDb::reset();
    SyncJobManagerSingleton::instance()->stop();
    SyncJobManagerSingleton::clear();
    if (_syncPal && _syncPal->syncDb()) {
        _syncPal->syncDb()->close();
    }
    TestBase::stop();
}

void TestRemoteFileSystemObserverWorker::testGenerateRemoteInitialSnapshot() {
    // Generating the initial snapshot requires access to the remote drive: it is only done by the tests that need it.
    const ExitInfo exitInfo = _syncPal->_remoteFSObserverWorker->generateInitialSnapshot();
    CPPUNIT_ASSERT_MESSAGE("Failed to generate the initial remote snapshot", exitInfo);

    NodeSet ids;
    _syncPal->liveSnapshot(ReplicaSide::Remote).ids(ids);

    NodeSet childrenIds;
    CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Remote).getChildrenIds(testRemoteFsoDirId, childrenIds));
    CPPUNIT_ASSERT_EQUAL(size_t(nbFileInTestDir), childrenIds.size());

    // Blacklisted folder should not appear in liveSnapshot.
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Remote).exists(testBlackListedDirId));
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Remote).exists(testBlackListedFileId));
}

void TestRemoteFileSystemObserverWorker::testUpdateSnapshot() {
    const ExitInfo exitInfo = _syncPal->_remoteFSObserverWorker->generateInitialSnapshot();
    CPPUNIT_ASSERT_MESSAGE("Failed to generate the initial remote snapshot", exitInfo);

    // Create test file locally
    const LocalTemporaryDirectory temporaryDirectory("testRFSO");
    const SyncName testFileName = Str("test_file_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum()) + Str(".txt");
    SyncPath testFilePath = temporaryDirectory.path() / testFileName;
    {
        std::ofstream testFile(testFilePath);
        testFile << "File creation\n";
        CPPUNIT_ASSERT(testFile.good());
    }
    RemoteTemporaryDirectory remoteTmpDir(_driveDbId, _testFolderId, "test_remote_FSO");
    const NodeId nestedRemoteTmpDirId =
            testhelpers::createRemoteDir(_driveDbId, remoteTmpDir.id(), Str("test_remote_FSO_nested"));
    const NodeId nodeIdA = testhelpers::createRemoteDir(_driveDbId, remoteTmpDir.id(), Str("A"));
    const NodeId nodeIdAA = testhelpers::createRemoteDir(_driveDbId, nodeIdA, Str("AA"));
    const NodeId nodeIdB = testhelpers::createRemoteDir(_driveDbId, remoteTmpDir.id(), Str("B"));

    {
        LOG_DEBUG(_logger, "***** test create file *****");

        // Upload in Common documents subdirectory
        {
            using namespace std::chrono;
            const auto time = system_clock::to_time_t(system_clock::now());
            UploadJob job(nullptr, _driveDbId, testFilePath, testFileName, remoteTmpDir.id(), time, time);
            (void) job.runSynchronously();

            // Extract file ID
            Poco::JSON::Object::Ptr resObj = job.jsonRes();
            Poco::JSON::Object::Ptr dataObj = resObj->getObject(dataKey);
            if (dataObj) {
                _testFileId = dataObj->get(idKey).toString();
            }
        }

        // Get activity from the server
        (void) _syncPal->_remoteFSObserverWorker->processEvents();

        CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Remote).exists(_testFileId));
        CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Remote).canWrite(_testFileId));
    }

    {
        LOG_DEBUG(_logger, "***** test create directory *****");

        const NodeId nodeIdC = testhelpers::createRemoteDir(_driveDbId, remoteTmpDir.id(), Str("C"));
        const NodeId nodeIdCC = testhelpers::createRemoteDir(_driveDbId, nodeIdC, Str("CC"));

        // Get activity from the server
        (void) _syncPal->_remoteFSObserverWorker->processEvents();

        CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Remote).exists(nodeIdC));
        CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Remote).exists(nodeIdCC));
    }

    {
        LOG_DEBUG(_logger, "***** test edit file *****");

        {
            std::ofstream testFile(testFilePath, std::ios::app);
            testFile << "This is an edit test\n";
            CPPUNIT_ASSERT(testFile.good());
        }

        SyncTime prevCreationTime = _syncPal->liveSnapshot(ReplicaSide::Remote).createdAt(_testFileId);
        SyncTime prevModificationTime = _syncPal->liveSnapshot(ReplicaSide::Remote).lastModified(_testFileId);

        Utility::msleep(1000);

        const std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        UploadJob job(nullptr, _driveDbId, testFilePath, _testFileId, time);
        (void) job.runSynchronously();

        // Get activity from the server
        (void) _syncPal->_remoteFSObserverWorker->processEvents();

        CPPUNIT_ASSERT_EQUAL(prevCreationTime, _syncPal->liveSnapshot(ReplicaSide::Remote).createdAt(_testFileId));
        CPPUNIT_ASSERT_GREATER(prevModificationTime, _syncPal->liveSnapshot(ReplicaSide::Remote).lastModified(_testFileId));
    }

    {
        LOG_DEBUG(_logger, "***** test move file *****");

        MoveJob job(nullptr, _driveDbId, testhelpers::localTestDirPath(), _testFileId, nestedRemoteTmpDirId);
        (void) job.runSynchronously();

        // Get activity from the server
        (void) _syncPal->_remoteFSObserverWorker->processEvents();

        CPPUNIT_ASSERT_EQUAL(nestedRemoteTmpDirId, _syncPal->liveSnapshot(ReplicaSide::Remote).parentId(_testFileId));

        CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Remote).exists(nodeIdA));
        CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Remote).exists(nodeIdAA));
    }

    {
        LOG_DEBUG(_logger, "***** test move directory *****");

        // Move /A to /B/A
        testhelpers::moveRemoteItem(_driveDbId, nodeIdA, nodeIdB);

        // Get activity from the server
        (void) _syncPal->_remoteFSObserverWorker->processEvents();

        CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Remote).exists(nodeIdA));
        CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Remote).exists(nodeIdAA));
        CPPUNIT_ASSERT_EQUAL(nodeIdA, _syncPal->liveSnapshot(ReplicaSide::Remote).parentId(nodeIdAA));
    }

    {
        LOG_DEBUG(_logger, "***** test rename file *****");

        const SyncName newFileName =
                Str("test_file_renamed_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum()) + Str(".txt");

        RenameJob job(nullptr, _driveDbId, _testFileId, newFileName);
        (void) job.runSynchronously();

        // Get activity from the server
        (void) _syncPal->_remoteFSObserverWorker->processEvents();

        CPPUNIT_ASSERT_EQUAL(SyncName2Str(newFileName),
                             SyncName2Str(_syncPal->liveSnapshot(ReplicaSide::Remote).name(_testFileId)));
    }

    {
        LOG_DEBUG(_logger, "***** test delete file *****");

        DeleteJob job(_driveDbId, _testFileId, "", "", NodeType::File);
        job.setBypassCheck(true);
        (void) job.runSynchronously();

        // Get activity from the server
        (void) _syncPal->_remoteFSObserverWorker->processEvents();

        CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Remote).exists(_testFileId));
    }
}

void TestRemoteFileSystemObserverWorker::testCheckSnapshotIntegrity() {
    // This test does not require any remote drive access.
    const auto remoteFSObserverWorker =
            std::dynamic_pointer_cast<RemoteFileSystemObserverWorker>(_syncPal->_remoteFSObserverWorker);
    CPPUNIT_ASSERT(remoteFSObserverWorker);

    LiveSnapshot &liveSnapshot = remoteFSObserverWorker->_liveSnapshot;
    const NodeId rootId = liveSnapshot.rootFolderId();
    CPPUNIT_ASSERT(!rootId.empty());

    int nbErrors = 0;
    _syncPal->setAddErrorCallback([&nbErrors](const Error &) { ++nbErrors; });

    // Insert a consistent directory with a file inside.
    const SnapshotItem dirItem("dir", rootId, Str("dir"), testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory,
                               testhelpers::defaultFileSize, false, true, true);
    CPPUNIT_ASSERT(liveSnapshot.updateItem(dirItem));

    const SnapshotItem fileItem("file", "dir", Str("file.txt"), testhelpers::defaultTime, testhelpers::defaultTime,
                                NodeType::File, testhelpers::defaultFileSize, false, true, true);
    CPPUNIT_ASSERT(liveSnapshot.updateItem(fileItem));

    // Insert an item whose parent is a file. Such items are skipped by getItemsInDir before being inserted into the
    // snapshot, the integrity check must leave them untouched.
    const SnapshotItem childOfFileItem("child", "file", Str("child.txt"), testhelpers::defaultTime, testhelpers::defaultTime,
                                       NodeType::File, testhelpers::defaultFileSize, false, true, true);
    CPPUNIT_ASSERT(liveSnapshot.updateItem(childOfFileItem));
    CPPUNIT_ASSERT(liveSnapshot.exists("child"));

    // Insert an orphan item. The integrity check must remove it from the snapshot.
    // Note: `exists` returns false for orphan items, so `type` is used to check the presence of the item in the snapshot.
    const SnapshotItem orphanItem("orphan", "missingParentId", Str("orphan.txt"), testhelpers::defaultTime,
                                  testhelpers::defaultTime, NodeType::File, testhelpers::defaultFileSize, false, true, true);
    CPPUNIT_ASSERT(liveSnapshot.updateItem(orphanItem));
    CPPUNIT_ASSERT_EQUAL(NodeType::File, liveSnapshot.type("orphan"));

    const ExitInfo exitInfo = remoteFSObserverWorker->checkSnapshotIntegrity();
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), exitInfo);

    // Items whose parent is a file are left untouched.
    CPPUNIT_ASSERT(liveSnapshot.exists("child"));

    // Consistent items are left untouched.
    CPPUNIT_ASSERT(liveSnapshot.exists("dir"));
    CPPUNIT_ASSERT(liveSnapshot.exists("file"));

    // Orphan items are removed from the snapshot.
    CPPUNIT_ASSERT_EQUAL(NodeType::Unknown, liveSnapshot.type("orphan"));

    // No error is reported by the integrity check.
    CPPUNIT_ASSERT_EQUAL(0, nbErrors);
}

void TestRemoteFileSystemObserverWorker::testInsertItemsFromCorruptedCsvReply() {
    // This test does not require any remote drive access.
    const auto remoteFSObserverWorker =
            std::dynamic_pointer_cast<RemoteFileSystemObserverWorker>(_syncPal->_remoteFSObserverWorker);
    CPPUNIT_ASSERT(remoteFSObserverWorker);

    LiveSnapshot &liveSnapshot = remoteFSObserverWorker->_liveSnapshot;
    const NodeId rootId = liveSnapshot.rootFolderId();
    CPPUNIT_ASSERT(!rootId.empty());

    // The items of the CSV replies have "1" as parent id (the kDrive root folder). If the snapshot root folder id
    // differs from "1", insert the corresponding directory item so that these items are not considered as orphans.
    if (rootId != NodeId("1")) {
        const SnapshotItem driveRootItem("1", rootId, Str("kDrive"), testhelpers::defaultTime, testhelpers::defaultTime,
                                         NodeType::Directory, testhelpers::defaultFileSize, false, true, true);
        CPPUNIT_ASSERT(liveSnapshot.updateItem(driveRootItem));
    }

    // Real-world CSV replies containing a corrupted item (id 2891437) whose name contains an escaped double quote.
    // Same replies as in TestSnapshotItemHandler::testGetItemWithCorruptedItem.
    const std::string commonDocumentsLine = R"(3,1,"Common documents",dir,,1627909284,1779373659,,)";
    const std::string symlinkLine = "2891434,1,symlink_to_folder_outside_sync_dir,file,17,1789713856,1789713856,1,1";
    const std::string myVirusLine = "2891435,1,myVirus.txt,file,14,1789716406,1789716417,1,";
    const std::string corruptedItemLines = R"csv(2891437,1,"A\"
2891435,2891434,myVirus.txt,file,,1786459004,1788263590,1,
Z",file,4,1789735691,1789735698,1,)csv";

    const std::vector csvBodies = {
            commonDocumentsLine + "\n" + symlinkLine + "\n" + myVirusLine + "\n" + corruptedItemLines, // Case 1
            commonDocumentsLine + "\n" + symlinkLine + "\n" + corruptedItemLines + "\n" + myVirusLine, // Case 2
            commonDocumentsLine + "\n" + corruptedItemLines + "\n" + symlinkLine + "\n" + myVirusLine, // Case 3
            commonDocumentsLine + "\n" + corruptedItemLines + "\n" + myVirusLine + "\n" + symlinkLine, // Case 4
    };

    for (const auto &csvBody: csvBodies) {
        // Parse the reply and insert the valid items into the snapshot, as done by getItemsInDir.
        std::stringstream ss;
        ss << "id,parent_id,name,type,size,created_at,last_modified_at,can_write,is_link\n"
           << csvBody << "\n"
           << endOfFileDelimiter;

        SnapshotItemHandler handler(_logger);
        SnapshotItem item;
        bool error = false;
        bool ignore = false;
        bool eof = false;
        SyncNameSet existingFiles;
        while (handler.getItem(item, ss, error, ignore, eof)) {
            if (ignore) continue; // Items parsed from a malformed line are blacklisted by getItemsInDir.
            if (eof) break;

            CPPUNIT_ASSERT(remoteFSObserverWorker->insertItemInSnapshot(item, existingFiles));
        }
        CPPUNIT_ASSERT(!error);
        CPPUNIT_ASSERT(eof);

        // Whatever the position of the corrupted item in the reply, the valid items must be inserted into the snapshot
        // and the corrupted item (id 2891437) must be absent.
        CPPUNIT_ASSERT(liveSnapshot.exists("3"));
        CPPUNIT_ASSERT(liveSnapshot.exists("2891434"));
        CPPUNIT_ASSERT(liveSnapshot.exists("2891435"));
        CPPUNIT_ASSERT_EQUAL(NodeId("1"), liveSnapshot.parentId("2891435"));
        CPPUNIT_ASSERT_EQUAL(NodeType::Unknown, liveSnapshot.type("2891437"));

        // Reset the snapshot for the next case.
        CPPUNIT_ASSERT(liveSnapshot.removeItem("3"));
        CPPUNIT_ASSERT(liveSnapshot.removeItem("2891434"));
        CPPUNIT_ASSERT(liveSnapshot.removeItem("2891435"));
    }
}

} // namespace KDC
