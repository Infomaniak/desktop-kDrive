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

#include "testintegration.h"

#include <memory>

#include "db/db.h"
#include "jobs/local/localcopyjob.h"
#include "jobs/local/localdeletejob.h"
#include "jobs/local/localmovejob.h"
#include "jobs/jobmanager.h"
#include "jobs/network/API_v2/upload/uploadjob.h"
#include "requests/syncnodecache.h"
#include "requests/exclusiontemplatecache.h"
#include "libcommon/utility/utility.h"
#include "libcommon/keychainmanager/keychainmanager.h"
#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/network/proxy.h"
#include "libsyncengine/jobs/network/API_v2/copytodirectoryjob.h"
#include "libsyncengine/jobs/network/API_v2/createdirjob.h"
#include "libsyncengine/jobs/network/API_v2/deletejob.h"
#include "libsyncengine/jobs/network/API_v2/duplicatejob.h"
#include "libsyncengine/jobs/network/API_v2/getfileinfojob.h"
#include "libsyncengine/jobs/network/API_v2/getfilelistjob.h"
#include "libsyncengine/jobs/network/API_v2/movejob.h"
#include "libsyncengine/jobs/network/API_v2/renamejob.h"
#include "libsyncengine/update_detection/file_system_observer/filesystemobserverworker.h"
#include "requests/syncnodecache.h"
#include "requests/exclusiontemplatecache.h"
#include "mocks/libcommonserver/db/mockdb.h"
#include "test_utility/testhelpers.h"
#include "requests/parameterscache.h"
#include "test_utility/remotetemporarydirectory.h"

using namespace CppUnit;
using namespace std::chrono;

namespace KDC {

// Temporary test in drive "kDrive Desktop Team"
const NodeId commonDocumentsNodeId = "3";
const NodeId testCiFolderId = "56850";

void TestIntegration::setUp() {
    TestBase::start();
    _logger = Log::instance()->getLogger();

    LOGW_DEBUG(_logger, L"$$$$$ Set Up");

    const testhelpers::TestVariables testVariables;

    // Insert api token into keystore
    ApiToken apiToken;
    apiToken.setAccessToken(testVariables.apiToken);

    const std::string keychainKey("123");
    (void) KeyChainManager::instance(true);
    (void) KeyChainManager::instance()->writeToken(keychainKey, apiToken.reconstructJsonString());

    // Create parmsDb
    bool alreadyExists = false;
    const std::filesystem::path parmsDbPath = MockDb::makeDbName(alreadyExists);
    (void) ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);

    // Insert user, account, drive & sync
    const User user(1, 12321, keychainKey);
    (void) ParmsDb::instance()->insertUser(user);

    const Account account(1, atoi(testVariables.accountId.c_str()), user.dbId());
    (void) ParmsDb::instance()->insertAccount(account);

    _driveDbId = 1;
    const Drive drive(_driveDbId, atoi(testVariables.driveId.c_str()), account.dbId(), std::string(), 0, std::string());
    (void) ParmsDb::instance()->insertDrive(drive);

    // Create remote sync folder (so that synchronization only takes place in a subdirectory, not taking into account the whole
    // drive items)
    _remoteSyncDir.generate(_driveDbId, testCiFolderId);
    // Add initial test file
    _tmpFilePath = std::filesystem::temp_directory_path() / ("tmpFile_" + CommonUtility::generateRandomStringAlphaNum(10));
    testhelpers::generateOrEditTestFile(_tmpFilePath);
    UploadJob job(nullptr, _driveDbId, _tmpFilePath, _tmpFilePath.filename(), _remoteSyncDir.id(), testhelpers::defaultTime,
                  testhelpers::defaultTime);
    (void) job.runSynchronously();
    _testFileRemoteId = job.nodeId();

    const Sync sync(1, drive.dbId(), _localSyncDir.path(), "/", _remoteSyncDir.id());
    (void) ParmsDb::instance()->insertSync(sync);

    // Setup proxy
    Parameters parameters;
    if (bool found = false; ParmsDb::instance()->selectParameters(parameters, found) && found) {
        (void) Proxy::instance(parameters.proxyConfig());
    }

    _syncPal = std::make_shared<SyncPal>(std::make_shared<VfsOff>(VfsSetupParams(Log::instance()->getLogger())), sync.dbId(),
                                         KDRIVE_VERSION_STRING);
    _syncPal->createSharedObjects();
    _syncPal->syncDb()->setAutoDelete(true);
    ParametersCache::instance()->parameters().setExtendedLog(true); // Enable extended log to see more details in the logs
}

void TestIntegration::tearDown() {
    _syncPal->stop(false, true, false);
    _remoteSyncDir.deleteNow();

    ParmsDb::instance()->close();
    ParmsDb::reset();
    JobManager::stop();
    JobManager::clear();
    JobManager::reset();
    TestBase::stop();
}

void TestIntegration::testAll() {
    if (!testhelpers::isExtendedTest()) return;

    // Start sync
    _syncPal->start();
    // Wait for the end of 1st sync
    waitForSyncToFinish(1);
    logStep("initialization");

    // Run test cases
    basicTests();
    inconsistencyTests();
    conflictTests();

    // &TestIntegration::testEditEditPseudoConflict,
    // &TestIntegration::testEditEditConflict,
    // &TestIntegration::testMoveCreateConflict,
    // &TestIntegration::testEditDeleteConflict1,
    // &TestIntegration::testEditDeleteConflict2,
    // &TestIntegration::testMoveDeleteConflict1,
    // &TestIntegration::testMoveDeleteConflict2,
    // &TestIntegration::testMoveDeleteConflict3,
    // &TestIntegration::testMoveDeleteConflict4,
    // &TestIntegration::testMoveDeleteConflict5,
    // &TestIntegration::testMoveParentDeleteConflict,
    // &TestIntegration::testCreateParentDeleteConflict,
    // &TestIntegration::testMoveMoveSourcePseudoConflict,
    // &TestIntegration::testMoveMoveSourceConflict,
    // &TestIntegration::testMoveMoveDestConflict,
    // &TestIntegration::testMoveMoveCycleConflict,
}

void TestIntegration::basicTests() {
    testLocalChanges();
    testRemoteChanges();
    testSimultaneousChanges();
}

void TestIntegration::testLocalChanges() {
    // Generate create operations.
    auto syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));

    const SyncPath subDirPath = _syncPal->localPath() / "testSubDirLocal";
    (void) std::filesystem::create_directories(subDirPath);

    SyncPath filePath = _syncPal->localPath() / "testLocal.txt";
    testhelpers::generateOrEditTestFile(filePath);

    waitForSyncToFinish(syncCount);

    auto remoteTestFileInfo = getRemoteFileInfo(_driveDbId, _remoteSyncDir.id(), filePath.filename());
    CPPUNIT_ASSERT(remoteTestFileInfo.isValid());

    const auto remoteTestDirInfo = getRemoteFileInfo(_driveDbId, _remoteSyncDir.id(), subDirPath.filename());
    CPPUNIT_ASSERT(remoteTestDirInfo.isValid());

    logStep("test create local file");

    // Generate an edit operation.
    syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    testhelpers::generateOrEditTestFile(filePath);

    FileStat fileStat;
    bool exists = false;
    IoHelper::getFileStat(filePath, &fileStat, exists);

    waitForSyncToFinish(syncCount);

    const auto prevRemoteTestFileInfo = remoteTestFileInfo;
    remoteTestFileInfo = getRemoteFileInfo(_driveDbId, _remoteSyncDir.id(), filePath.filename());
    CPPUNIT_ASSERT_EQUAL(fileStat.modificationTime, remoteTestFileInfo.modificationTime);
    CPPUNIT_ASSERT_LESS(remoteTestFileInfo.modificationTime, prevRemoteTestFileInfo.modificationTime);
    CPPUNIT_ASSERT_EQUAL(fileStat.size, remoteTestFileInfo.size);
    CPPUNIT_ASSERT_LESS(remoteTestFileInfo.size, prevRemoteTestFileInfo.size);

    logStep("test edit local file");

    // Generate a move operation.
    syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    const auto newName = "testLocal_renamed.txt";
    const std::filesystem::path destinationPath = subDirPath / newName;
    {
        LocalMoveJob job(filePath, destinationPath);
        (void) job.runSynchronously();
    }

    waitForSyncToFinish(syncCount);

    remoteTestFileInfo = getRemoteFileInfo(_driveDbId, remoteTestDirInfo.id, newName);

    CPPUNIT_ASSERT(remoteTestFileInfo.isValid());
    CPPUNIT_ASSERT_EQUAL(remoteTestDirInfo.id, remoteTestFileInfo.parentId);

    filePath = destinationPath;

    logStep("test move local file");

    // Generate a delete operation.
    syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    {
        LocalDeleteJob deleteJob(subDirPath);
        (void) deleteJob.runSynchronously();
    }

    waitForSyncToFinish(syncCount);

    remoteTestFileInfo = getRemoteFileInfo(_driveDbId, remoteTestDirInfo.id, filePath.filename());
    CPPUNIT_ASSERT(!remoteTestFileInfo.isValid());

    logStep("test delete local file");
}

void TestIntegration::testRemoteChanges() {
    // Generate create operations.
    auto syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    const SyncPath subDirPath = _syncPal->localPath() / "testSubDirRemote";
    NodeId subDirId;
    SyncPath filePath = _syncPal->localPath() / "testRemote.txt";
    NodeId fileId;
    {
        CreateDirJob createDirJob(nullptr, _driveDbId, subDirPath, _remoteSyncDir.id(), subDirPath.filename());
        (void) createDirJob.runSynchronously();
        subDirId = createDirJob.nodeId();

        fileId = duplicateRemoteFile(_testFileRemoteId, filePath.filename());
    }

    waitForSyncToFinish(syncCount);

    CPPUNIT_ASSERT(std::filesystem::exists(subDirPath));
    CPPUNIT_ASSERT(std::filesystem::exists(filePath));

    logStep("test create remote file");

    // Generate an edit operation.
    syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    int64_t modificationTime = 0;
    int64_t size = 0;
    testhelpers::generateOrEditTestFile(_tmpFilePath);
    {
        UploadJob job(nullptr, _driveDbId, _tmpFilePath, fileId, testhelpers::defaultTime);
        (void) job.runSynchronously();
        modificationTime = job.modificationTime();
        size = job.size();
    }

    waitForSyncToFinish(syncCount);

    FileStat filestat;
    IoError ioError = IoError::Unknown;
    (void) IoHelper::getFileStat(filePath, &filestat, ioError);
    CPPUNIT_ASSERT_EQUAL(modificationTime, filestat.modificationTime);
    CPPUNIT_ASSERT_EQUAL(size, filestat.size);
    logStep("test edit remote file");

    // Generate a move operation.
    syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    filePath = subDirPath / "testRemote_renamed.txt";
    {
        MoveJob job(nullptr, _driveDbId, filePath, fileId, subDirId, filePath.filename());
        job.setBypassCheck(true);
        (void) job.runSynchronously();
    }

    waitForSyncToFinish(syncCount);

    CPPUNIT_ASSERT(std::filesystem::exists(filePath));

    logStep("test move remote file");

    // Generate a delete operation.
    syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    {
        DeleteJob job(_driveDbId, subDirId);
        job.setBypassCheck(true);
        (void) job.runSynchronously();
    }

    waitForSyncToFinish(syncCount);

    CPPUNIT_ASSERT(!std::filesystem::exists(subDirPath));
    CPPUNIT_ASSERT(!std::filesystem::exists(filePath));

    logStep("test delete remote file");
}

void TestIntegration::testSimultaneousChanges() {
    const auto syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));

    // Create a file on local replica.
    const SyncPath localFilePath = _syncPal->localPath() / "testSimultaneousChanges.txt";
    testhelpers::generateOrEditTestFile(localFilePath);

    // Rename a file on remote replica.
    const SyncPath remoteFilePath = _syncPal->localPath() / "renamed.txt";
    (void) RenameJob(nullptr, _driveDbId, _testFileRemoteId, remoteFilePath).runSynchronously();

    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForSyncToFinish(syncCount);

    CPPUNIT_ASSERT(std::filesystem::exists(remoteFilePath));
    const auto remoteTestFileInfo = getRemoteFileInfo(_driveDbId, _remoteSyncDir.id(), localFilePath.filename());
    CPPUNIT_ASSERT(remoteTestFileInfo.isValid());

    logStep("testSimultaneousChanges");
}

void TestIntegration::inconsistencyTests() {
    // Duplicate remote files to set up the tests.
    auto syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    const auto testForbiddenCharsRemoteId = duplicateRemoteFile(_testFileRemoteId, "testForbiddenChar");
    const auto testNameClashRemoteId1 = duplicateRemoteFile(_testFileRemoteId, "testNameClash");
    const auto testNameClashRemoteId2 = duplicateRemoteFile(_testFileRemoteId, "testnameclash1");

    waitForSyncToFinish(syncCount);

    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / "testForbiddenChar"));
    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / "testNameClash"));
    const SyncPath nameClashLocalPath = _syncPal->localPath() / "testnameclash1";
    CPPUNIT_ASSERT(std::filesystem::exists(nameClashLocalPath));

    // Rename files on remote side.
    syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    (void) RenameJob(nullptr, _driveDbId, testForbiddenCharsRemoteId, "test/:*ForbiddenChar").runSynchronously();
    (void) RenameJob(nullptr, _driveDbId, testNameClashRemoteId2, "testnameclash").runSynchronously();

    waitForSyncToFinish(syncCount);

    CPPUNIT_ASSERT(!std::filesystem::exists(_syncPal->localPath() / "testForbiddenChar"));
    CPPUNIT_ASSERT(!std::filesystem::exists(_syncPal->localPath() / "test/:*ForbiddenChar"));
    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / "testNameClash"));
    CPPUNIT_ASSERT(std::filesystem::exists(nameClashLocalPath));

    // Edit local name clash file.
    syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    testhelpers::generateOrEditTestFile(nameClashLocalPath);
    FileStat filestat;
    IoError ioError = IoError::Unknown;
    (void) IoHelper::getFileStat(nameClashLocalPath, &filestat, ioError);

    waitForSyncToFinish(syncCount);

    auto remoteFileInfo = getRemoteFileInfo(_driveDbId, _remoteSyncDir.id(), "testnameclash");
    CPPUNIT_ASSERT(remoteFileInfo.isValid());
    CPPUNIT_ASSERT_LESS(filestat.size, remoteFileInfo.size); // The local edit is not propagated.

    // Rename again the remote file to avoid the name clash.
    syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    (void) RenameJob(nullptr, _driveDbId, testNameClashRemoteId2, "testnameclash2").runSynchronously();

    waitForSyncToFinish(syncCount);

    /************************************/
    // Temporary, needed because of a bug if an item has remote move and local edit operations:
    // https://infomaniak.atlassian.net/browse/KDESKTOP-1752
    syncCount++;
    waitForSyncToFinish(syncCount);
    /************************************/

    (void) IoHelper::getFileStat(_syncPal->localPath() / "testnameclash2", &filestat, ioError);
    remoteFileInfo = getRemoteFileInfo(_driveDbId, _remoteSyncDir.id(), "testnameclash2");

    CPPUNIT_ASSERT(remoteFileInfo.isValid());
    CPPUNIT_ASSERT_EQUAL(filestat.size, remoteFileInfo.size); // The local edit has been propagated.
    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / "testnameclash2"));

    logStep("testInconsistency");
}

void TestIntegration::conflictTests() {
    testCreateCreatePseudoConflict();
    testCreateCreateConflict();
    // testEditEditPseudoConflict();
    // testEditEditConflict();
    testMoveCreateConflict();
    testEditDeleteConflict();
}

void TestIntegration::testCreateCreatePseudoConflict() {
    const auto syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));

    // Remove the test file from DB to simulate the pseudo conflict.
    DbNodeId dbNodeId = 0;
    bool found = false;
    CPPUNIT_ASSERT(_syncPal->syncDb()->dbId(ReplicaSide::Remote, _testFileRemoteId, dbNodeId, found) && found);
    NodeId localId;
    CPPUNIT_ASSERT(_syncPal->syncDb()->id(ReplicaSide::Local, dbNodeId, localId, found) && found);
    CPPUNIT_ASSERT(_syncPal->syncDb()->deleteNode(dbNodeId, found) && found);

    // Remove the test file from the update trees.
    (void) _syncPal->_localUpdateTree->deleteNode(localId);
    (void) _syncPal->_remoteUpdateTree->deleteNode(_testFileRemoteId);

    _syncPal->forceInvalidateSnapshots();
    waitForSyncToFinish(syncCount);

    // Check that both remote and local ID has been re-inserted in DB.
    SyncName name;
    CPPUNIT_ASSERT(_syncPal->syncDb()->name(ReplicaSide::Remote, _testFileRemoteId, name, found) && found);
    CPPUNIT_ASSERT(_syncPal->syncDb()->name(ReplicaSide::Local, localId, name, found) && found);
}

void TestIntegration::testCreateCreateConflict() {
    auto syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));

    // Create a file on remote replica.
    const auto remoteId = duplicateRemoteFile(_testFileRemoteId, "testCreateCreatePseudoConflict");

    // Create a file on local replica.
    const SyncPath localFilePath = _syncPal->localPath() / "testCreateCreatePseudoConflict";
    testhelpers::generateOrEditTestFile(localFilePath);

    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForSyncToFinish(syncCount);

    const auto conflictedFilePath = findLocalFileByNamePrefix(_localSyncDir.path(), "testCreateCreatePseudoConflict_conflict_");
    CPPUNIT_ASSERT(std::filesystem::exists(conflictedFilePath));
    CPPUNIT_ASSERT(!std::filesystem::exists(localFilePath));

    syncCount++; // Previous sync only fixed the conflict.
    waitForSyncToFinish(syncCount);

    CPPUNIT_ASSERT(std::filesystem::exists(conflictedFilePath));
    CPPUNIT_ASSERT(std::filesystem::exists(localFilePath));

    logStep("testCreateCreateConflict");
}

void TestIntegration::testEditEditPseudoConflict() {
    const auto syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));

    // Change the last modification date in DB.
    bool found = false;
    DbNode dbNode;
    CPPUNIT_ASSERT(_syncPal->syncDb()->node(ReplicaSide::Remote, _testFileRemoteId, dbNode, found) && found);
    const auto newLastModified = *dbNode.lastModifiedLocal() - 1000;
    dbNode.setLastModifiedLocal(newLastModified);
    dbNode.setLastModifiedRemote(newLastModified);
    CPPUNIT_ASSERT(_syncPal->syncDb()->updateNode(dbNode, found) && found);

    _syncPal->forceInvalidateSnapshots();
    waitForSyncToFinish(syncCount);

    // Check that the modification time has been updated in DB.
    FileStat fileStat;
    bool exists = false;
    IoHelper::getFileStat(_syncPal->localPath() / dbNode.name(ReplicaSide::Local), &fileStat, exists);
    std::optional<SyncTime> lastModified = std::nullopt;
    CPPUNIT_ASSERT_EQUAL(true,
                         _syncPal->syncDb()->lastModified(ReplicaSide::Remote, _testFileRemoteId, lastModified, found) && found);
    CPPUNIT_ASSERT_EQUAL(fileStat.modificationTime, *lastModified);

    // Check that the local ID has not changed.
    SyncName name;
    CPPUNIT_ASSERT(_syncPal->syncDb()->name(ReplicaSide::Local, *dbNode.nodeIdLocal(), name, found) && found);
}

void TestIntegration::testEditEditConflict() {
    auto syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));

    // Edit local file.
    testhelpers::generateOrEditTestFile(_tmpFilePath);

    // Edit remote file.
    int64_t creationTime = 0;
    int64_t modificationTime = std::chrono::system_clock::now().time_since_epoch().count();
    {
        UploadJob job(nullptr, _driveDbId, _tmpFilePath, _testFileRemoteId, modificationTime);
        (void) job.runSynchronously();
        creationTime = job.creationTime();
        modificationTime = job.modificationTime();
    }

    const SyncPath localFilePath = _syncPal->localPath() / _tmpFilePath.filename();
    (void) IoHelper::setFileDates(localFilePath, creationTime, modificationTime, false);

    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForSyncToFinish(syncCount);

    const auto conflictedFilePath =
            findLocalFileByNamePrefix(_localSyncDir.path(), _tmpFilePath.filename().string() + "_conflict_");
    CPPUNIT_ASSERT(std::filesystem::exists(conflictedFilePath));
    CPPUNIT_ASSERT(!std::filesystem::exists(localFilePath));

    syncCount++; // Previous sync only fixed the conflict.
    waitForSyncToFinish(syncCount);

    CPPUNIT_ASSERT(std::filesystem::exists(conflictedFilePath));
    CPPUNIT_ASSERT(std::filesystem::exists(localFilePath));
}

void TestIntegration::testMoveCreateConflict() {
    const SyncPath filename = "testMoveCreateConflict1";
    const SyncPath localFilePath = _syncPal->localPath() / filename;

    // 1 - if remote file has been moved, rename local file
    {
        auto syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));

        // Create a file a local replica.
        testhelpers::generateOrEditTestFile(localFilePath);

        // Rename remote file.
        (void) RenameJob(nullptr, _driveDbId, _testFileRemoteId, filename).runSynchronously();

        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForSyncToFinish(syncCount);

        // The local file has been excluded from sync.
        const auto conflictedFilePath = findLocalFileByNamePrefix(_localSyncDir.path(), filename.native() + Str("_conflict_"));
        CPPUNIT_ASSERT(std::filesystem::exists(conflictedFilePath));
        CPPUNIT_ASSERT(!std::filesystem::exists(localFilePath));

        syncCount++; // Previous sync only fixed the conflict.
        waitForSyncToFinish(syncCount);

        // The remote rename operation has been propagated.
        CPPUNIT_ASSERT(std::filesystem::exists(conflictedFilePath));
        CPPUNIT_ASSERT(std::filesystem::exists(localFilePath));
    }

    // 2 - if local file has been moved, undo move operation.
    {
        auto syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));

        // Rename a file on local replica.
        const SyncName newFilename = "testMoveCreateConflict2";
        const SyncPath newLocalFilePath = _syncPal->localPath() / newFilename;
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::renameItem(localFilePath, newLocalFilePath, ioError) && ioError == IoError::Success);

        // Create a file on remote replica.
        (void) duplicateRemoteFile(_testFileRemoteId, newFilename);

        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForSyncToFinish(syncCount);

        // The local file has been put back in its origin location.
        CPPUNIT_ASSERT(std::filesystem::exists(localFilePath));
        CPPUNIT_ASSERT(!std::filesystem::exists(newLocalFilePath));

        syncCount++; // Previous sync only fixed the conflict.
        waitForSyncToFinish(syncCount);

        // The remote creation has been propagated.
        CPPUNIT_ASSERT(std::filesystem::exists(localFilePath));
        CPPUNIT_ASSERT(std::filesystem::exists(newLocalFilePath));
    }
}

void TestIntegration::testEditDeleteConflict() {}

// void TestIntegration::testEditDeleteConflict1() {
//     LOGW_DEBUG(_logger, L"$$$$$ test Edit-Delete conflict 1");
//     std::cout << "test Edit-Delete conflict 1 : ";
//
//     // Init: simulate a remote file creation by duplicating an existing file
//     std::filesystem::path newTestFileName =
//             Str("test_editDeleteConflict1_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) + Str(".txt");
//
//     DuplicateJob initJob(_syncPal->vfs(), _driveDbId, testExecutorFileRemoteId, newTestFileName.native());
//     initJob.runSynchronously();
//     NodeId initRemoteId = initJob.nodeId();
//     waitForSyncToFinish(SourceLocation::currentLoc());
//
//     _syncPal->pause();
//
//     // Delete file on remote replica
//     DeleteJob deleteJob(_driveDbId, initRemoteId, "", "",
//                         NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
//     deleteJob.runSynchronously();
//
//     // Edit file on local replica
//     std::filesystem::path localExecutorFolderPath = _localPath / testExecutorFolderRelativePath;
//     std::filesystem::path sourceFile = localExecutorFolderPath / newTestFileName;
//
//     SyncName testCallStr = Str(R"(echo "This is an edit test )") +
//     Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) +
//                            Str(R"(" >> ")") + sourceFile.make_preferred().native() + Str(R"(")");
//     std::system(SyncName2Str(testCallStr).c_str());
//
//     FileStat fileStat;
//     bool exists = false;
//     IoHelper::getFileStat(sourceFile.make_preferred(), &fileStat, exists);
//     NodeId localId = std::to_string(fileStat.inode);
//
//     Utility::msleep(10000); // Wait more to make sure the remote snapshot has been updated (TODO : not needed once longpoll
//                             // request is implemented)
//
//     _syncPal->unpause();
//     waitForSyncToFinish(SourceLocation::currentLoc());
//
//     // Edit operation must win, so local file should not have changed
//     CPPUNIT_ASSERT(_syncPal->_localSnapshot->exists(localId));
//
//     // Remote file should have been re-uploaded
//     // File with same name should exist
//     GetFileListJob job(_driveDbId, testExecutorFolderRemoteId);
//     job.runSynchronously();
//
//     Poco::JSON::Object::Ptr resObj = job.jsonRes();
//     CPPUNIT_ASSERT(resObj);
//
//     bool newFileFound = false;
//     Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
//     CPPUNIT_ASSERT(dataArray);
//
//     NodeId remoteId;
//     for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
//         Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
//         if (newTestFileName.filename().string() == obj->get(nameKey).toString()) {
//             newFileFound = true;
//             remoteId = obj->get(idKey).toString();
//             break;
//         }
//     }
//     CPPUNIT_ASSERT(newFileFound);
//     // But initial remote ID should have changed
//     GetFileInfoJob fileInfoJob(_driveDbId, initRemoteId);
//     fileInfoJob.runSynchronously();
//     CPPUNIT_ASSERT(fileInfoJob.hasHttpError());
//
//     // Remove the test files
//     DeleteJob deleteJob1(_driveDbId, remoteId, "", "",
//                          NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
//     deleteJob1.runSynchronously();
//
//     waitForSyncToFinish(SourceLocation::currentLoc());
//
//     std::cout << "OK" << std::endl;
// }
//
// void TestIntegration::testEditDeleteConflict2() {
//     LOGW_DEBUG(_logger, L"$$$$$ test Edit-Delete conflict 2");
//     std::cout << "test Edit-Delete conflict 2 : ";
//
//     // Init: create a folder and duplicate an existing file inside it
//     SyncName dirName = Str("test_dir_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10));
//     CreateDirJob initCreateDirJob(nullptr, _driveDbId, dirName, testExecutorFolderRemoteId, dirName);
//     initCreateDirJob.runSynchronously();
//
//     // Extract dir ID
//     NodeId dirRemoteId = initCreateDirJob.nodeId();
//     CPPUNIT_ASSERT(!dirRemoteId.empty());
//
//     // Simulate a create into this directory
//     std::filesystem::path newTestFileName =
//             Str("test_editDeleteConflict2_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) + Str(".txt");
//
//     CopyToDirectoryJob initJob(_driveDbId, testExecutorFileRemoteId, dirRemoteId, newTestFileName.native());
//     initJob.runSynchronously();
//     NodeId initFileRemoteId = initJob.nodeId();
//     waitForSyncToFinish(SourceLocation::currentLoc());
//
//     _syncPal->pause();
//
//     // Delete dir on remote replica
//     DeleteJob deleteJob(_driveDbId, dirRemoteId, "", "",
//                         NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
//     deleteJob.runSynchronously();
//
//     // Edit file on local replica
//     std::filesystem::path localExecutorFolderPath = _localPath / testExecutorFolderRelativePath / dirName;
//     std::filesystem::path sourceFile = localExecutorFolderPath / newTestFileName;
//
//     SyncName testCallStr = Str(R"(echo "This is an edit test )") +
//     Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) +
//                            Str(R"(" >> ")") + sourceFile.make_preferred().native() + Str(R"(")");
//     std::system(SyncName2Str(testCallStr).c_str());
//
//     FileStat fileStat;
//     bool exists = false;
//     IoHelper::getFileStat(sourceFile.make_preferred(), &fileStat, exists);
//     NodeId localFileId = std::to_string(fileStat.inode);
//
//     Utility::msleep(10000); // Wait more to make sure the remote snapshot has been updated (TODO : not needed once longpoll
//                             // request is implemented)
//
//     _syncPal->unpause();
//     waitForSyncToFinish(SourceLocation::currentLoc());
//
//     // Look for local file in root directory that have been renamed (and excluded from sync)
//     bool found = false;
//     std::filesystem::path localExcludedPath;
//     for (auto const &dir_entry: std::filesystem::directory_iterator{_localPath}) {
//         if (Utility::startsWith(dir_entry.path().filename().string(), newTestFileName.stem().string() + "_conflict_")) {
//             found = true;
//             localExcludedPath = dir_entry.path();
//             break;
//         }
//     }
//     CPPUNIT_ASSERT(found);
//
//     // Remove the test files
//     std::filesystem::remove(localExcludedPath);
//
//     std::cout << "OK" << std::endl;
// }
//
// const std::string testConflictFolderName = "test_conflict";
// const std::string testConflictFolderId = "451588";
// const std::string aRefFolderId = "451591";
//
// void testMoveDeleteConflict_initPhase(int driveDbId, NodeId &aRemoteId, NodeId &rRemoteId, NodeId &sRemoteId, NodeId
// &qRemoteId) {
//     // Duplicate the test folder
//     DuplicateJob initDuplicateJob(nullptr, driveDbId, aRefFolderId, Str("A"));
//     initDuplicateJob.runSynchronously();
//     aRemoteId = initDuplicateJob.nodeId();
//     CPPUNIT_ASSERT(!aRemoteId.empty());
//
//     // Find the other folder IDs
//     {
//         GetFileListJob initJob(driveDbId, aRemoteId);
//         initJob.runSynchronously();
//         Poco::JSON::Object::Ptr resObj = initJob.jsonRes();
//         CPPUNIT_ASSERT(resObj);
//         Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
//         CPPUNIT_ASSERT(dataArray);
//         for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
//             Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
//             if (obj->get(nameKey).toString() == "R") {
//                 rRemoteId = obj->get(idKey).toString();
//             } else if (obj->get(nameKey).toString() == "S") {
//                 sRemoteId = obj->get(idKey).toString();
//             }
//         }
//     }
//     CPPUNIT_ASSERT(!rRemoteId.empty());
//     CPPUNIT_ASSERT(!sRemoteId.empty());
//
//     {
//         GetFileListJob initJob(driveDbId, rRemoteId);
//         initJob.runSynchronously();
//         Poco::JSON::Object::Ptr resObj = initJob.jsonRes();
//         CPPUNIT_ASSERT(resObj);
//         Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
//         CPPUNIT_ASSERT(dataArray);
//         for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
//             Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
//             if (obj->get(nameKey).toString() == "Q") {
//                 qRemoteId = obj->get(idKey).toString();
//             }
//         }
//     }
//     CPPUNIT_ASSERT(!qRemoteId.empty());
// }
//
// // See p.85, case 1
// void TestIntegration::testMoveDeleteConflict1() {
//     LOGW_DEBUG(_logger, L"$$$$$ test Move-Delete conflict 1");
//     std::cout << "test Move-Delete conflict 1: ";
//
//     // Init phase
//     LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 1 : Init phase");
//
//     NodeId aRemoteId;
//     NodeId rRemoteId;
//     NodeId sRemoteId;
//     NodeId qRemoteId;
//     testMoveDeleteConflict_initPhase(_driveDbId, aRemoteId, rRemoteId, sRemoteId, qRemoteId);
//
//     waitForSyncToFinish(SourceLocation::currentLoc());
//     _syncPal->pause();
//
//     // Setup phase
//     LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 1 : Setup phase");
//
//     // On local replica
//     // Delete S
//     {
//         const SyncPalInfo syncPalInfo(_driveDbId, _localPath);
//         LocalDeleteJob localDeleteJob(syncPalInfo, testExecutorFolderRelativePath / testConflictFolderName / "A/S", false,
//                                       sRemoteId);
//         localDeleteJob.setBypassCheck(true);
//         localDeleteJob.runSynchronously();
//     }
//
//     // Rename A into B
//     {
//         LocalMoveJob localMoveJob(_localPath / testExecutorFolderRelativePath / testConflictFolderName / "A",
//                                   _localPath / testExecutorFolderRelativePath / testConflictFolderName / "B");
//         localMoveJob.runSynchronously();
//     }
//
//     // On remote replica
//     // Delete A
//     {
//         DeleteJob deleteJob(_driveDbId, aRemoteId, "", "",
//                             NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
//         deleteJob.runSynchronously();
//     }
//
//     Utility::msleep(10000); // Wait more to make sure the remote snapshot has been updated (TODO : not needed once longpoll
//                             // request is implemented)
//
//     LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 1 : Resolution phase");
//     _syncPal->unpause();
//     waitForSyncToFinish(SourceLocation::currentLoc());
//
//     // Test phase
//     LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 1 : Test phase");
//
//     // On remote
//     bool sFound = false;
//     {
//         bool found = false;
//         GetFileListJob testJob(_driveDbId, testConflictFolderId);
//         testJob.runSynchronously();
//         Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
//         CPPUNIT_ASSERT(resObj);
//         Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
//         CPPUNIT_ASSERT(dataArray);
//         for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
//             Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
//             if (obj->get(nameKey).toString() == "B") {
//                 aRemoteId = obj->get(idKey).toString();
//                 found = true;
//             } else if (obj->get(nameKey).toString() == "S") {
//                 sFound = true;
//             }
//         }
//         CPPUNIT_ASSERT(found);
//     }
//
//     {
//         bool found = false;
//         GetFileListJob testJob(_driveDbId, aRemoteId);
//         testJob.runSynchronously();
//         Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
//         CPPUNIT_ASSERT(resObj);
//         Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
//         CPPUNIT_ASSERT(dataArray);
//         for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
//             Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
//             if (obj->get(nameKey).toString() == "R") {
//                 rRemoteId = obj->get(idKey).toString();
//                 found = true;
//             } else if (obj->get(nameKey).toString() == "S") {
//                 sFound = true;
//             }
//         }
//         CPPUNIT_ASSERT(found);
//     }
//
//     {
//         bool found = false;
//         GetFileListJob testJob(_driveDbId, rRemoteId);
//         testJob.runSynchronously();
//         Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
//         CPPUNIT_ASSERT(resObj);
//         Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
//         CPPUNIT_ASSERT(dataArray);
//         for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
//             Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
//             if (obj->get(nameKey).toString() == "Q") {
//                 rRemoteId = obj->get(idKey).toString();
//                 found = true;
//             } else if (obj->get(nameKey).toString() == "S") {
//                 sFound = true;
//             }
//         }
//         CPPUNIT_ASSERT(found);
//     }
//
//     CPPUNIT_ASSERT(!sFound);
//
//     // On local
//     // Look for local file that have been renamed (and excluded from sync)
//     CPPUNIT_ASSERT(std::filesystem::exists(_localPath / testExecutorFolderRelativePath / testConflictFolderName /
//     "B/R/Q")); CPPUNIT_ASSERT(!std::filesystem::exists(_localPath / testExecutorFolderRelativePath / testConflictFolderName
//     / "B/S"));
//
//     // Clean phase
//     LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 1 : Clean phase");
//
//     // Remove the test files
//     DeleteJob finalDeleteJob(_driveDbId, aRemoteId, "", "",
//                              NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
//     finalDeleteJob.runSynchronously();
//
//     waitForSyncToFinish(SourceLocation::currentLoc());
//
//     std::cout << "OK" << std::endl;
// }
//
// // See p.85, case 2
// void TestIntegration::testMoveDeleteConflict2() {
//     LOGW_DEBUG(_logger, L"$$$$$ test Move-Delete conflict 2");
//     std::cout << "test Move-Delete conflict 2: ";
//
//     // Init phase
//     LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 2 : Init phase");
//
//     NodeId aRemoteId;
//     NodeId rRemoteId;
//     NodeId sRemoteId;
//     NodeId qRemoteId;
//     testMoveDeleteConflict_initPhase(_driveDbId, aRemoteId, rRemoteId, sRemoteId, qRemoteId);
//
//     waitForSyncToFinish(SourceLocation::currentLoc());
//     _syncPal->pause();
//
//     // Setup phase
//     LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 2 : Setup phase");
//
//     // On local replica
//     // Delete A
//     LocalDeleteJob localDeleteJob(SyncPalInfo{_driveDbId, _localPath},
//                                   testExecutorFolderRelativePath / testConflictFolderName / "A", false, aRemoteId);
//     localDeleteJob.setBypassCheck(true);
//     localDeleteJob.runSynchronously();
//
//     // On remote replica
//     // Rename A
//     RenameJob setupRenameJob1(_syncPal->vfs(), _driveDbId, aRemoteId, Str("B"));
//     setupRenameJob1.runSynchronously();
//
//     // Edit Q
//     std::filesystem::path tmpFile = std::filesystem::temp_directory_path() / L"tmp_test_file.txt";
//     SyncName testCallStr = Str(R"(echo "This is an edit test )") +
//     Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) +
//                            Str(R"(" >> ")") + tmpFile.make_preferred().native() + Str(R"(")");
//     std::system(SyncName2Str(testCallStr).c_str());
//
//     UploadJob setupUploadJob(_syncPal->vfs(), _driveDbId, tmpFile, Str("Q"), rRemoteId, 0);
//     setupUploadJob.runSynchronously();
//
//     // Create A/S/X
//     NodeId xRemoteId;
//     CreateDirJob setupCreateDirJob(_syncPal->vfs(), _driveDbId, Str("X"), sRemoteId, Str("X"));
//     setupCreateDirJob.runSynchronously();
//     xRemoteId = setupCreateDirJob.nodeId();
//
//     Utility::msleep(10000); // Wait more to make sure the remote snapshot has been updated (TODO : not needed once longpoll
//                             // request is implemented)
//
//     LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 2 : Resolution phase");
//     _syncPal->unpause();
//     waitForSyncToFinish(SourceLocation::currentLoc());
//
//     // Test phase
//     LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 2 : Test phase");
//
//     // On remote
//     {
//         bool found = false;
//         GetFileListJob testJob(_driveDbId, testConflictFolderId);
//         testJob.runSynchronously();
//         Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
//         CPPUNIT_ASSERT(resObj);
//         Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
//         CPPUNIT_ASSERT(dataArray);
//         for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
//             Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
//             if (obj->get(nameKey).toString() == "B") {
//                 aRemoteId = obj->get(idKey).toString();
//                 found = true;
//             }
//         }
//         CPPUNIT_ASSERT(found);
//     }
//
//     {
//         bool rFound = false;
//         bool sFound = false;
//         GetFileListJob testJob(_driveDbId, aRemoteId);
//         testJob.runSynchronously();
//         Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
//         CPPUNIT_ASSERT(resObj);
//         Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
//         CPPUNIT_ASSERT(dataArray);
//         for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
//             Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
//             if (obj->get(nameKey).toString() == "R") {
//                 rRemoteId = obj->get(idKey).toString();
//                 rFound = true;
//             } else if (obj->get(nameKey).toString() == "S") {
//                 sRemoteId = obj->get(idKey).toString();
//                 sFound = true;
//             }
//         }
//         CPPUNIT_ASSERT(rFound);
//         CPPUNIT_ASSERT(sFound);
//     }
//
//     {
//         bool qFound = false;
//         GetFileListJob testJob(_driveDbId, rRemoteId);
//         testJob.runSynchronously();
//         Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
//         CPPUNIT_ASSERT(resObj);
//         Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
//         CPPUNIT_ASSERT(dataArray);
//         for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
//             Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
//             if (obj->get(nameKey).toString() == "Q") {
//                 qRemoteId = obj->get(idKey).toString();
//                 qFound = true;
//             }
//         }
//         CPPUNIT_ASSERT(qFound);
//     }
//
//     {
//         bool xFound = false;
//         GetFileListJob testJob(_driveDbId, sRemoteId);
//         testJob.runSynchronously();
//         Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
//         CPPUNIT_ASSERT(resObj);
//         Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
//         CPPUNIT_ASSERT(dataArray);
//         for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
//             Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
//             if (obj->get(nameKey).toString() == "X") {
//                 xRemoteId = obj->get(idKey).toString();
//                 xFound = true;
//             }
//         }
//         CPPUNIT_ASSERT(xFound);
//     }
//
//     // On local
//     // Look for local file that have been renamed (and excluded from sync)
//     CPPUNIT_ASSERT(std::filesystem::exists(_localPath / testExecutorFolderRelativePath / testConflictFolderName /
//     "B/R/Q")); CPPUNIT_ASSERT(std::filesystem::exists(_localPath / testExecutorFolderRelativePath / testConflictFolderName
//     / "B/S/X"));
//
//     // Clean phase
//     LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 2 : Clean phase");
//
//     // Remove the test files
//     DeleteJob finalDeleteJob(_driveDbId, aRemoteId, "", "",
//                              NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
//     finalDeleteJob.runSynchronously();
//
//     waitForSyncToFinish(SourceLocation::currentLoc());
//
//     std::cout << "OK" << std::endl;
// }
//
// // See p.86, case 3
// void TestIntegration::testMoveDeleteConflict3() {
//     LOGW_DEBUG(_logger, L"$$$$$ test Move-Delete conflict 3");
//     std::cout << "test Move-Delete conflict 3: ";
//
//     // Init phase
//     LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 3 : Init phase");
//
//     NodeId aRemoteId;
//     NodeId rRemoteId;
//     NodeId sRemoteId;
//     NodeId qRemoteId;
//     testMoveDeleteConflict_initPhase(_driveDbId, aRemoteId, rRemoteId, sRemoteId, qRemoteId);
//
//     waitForSyncToFinish(SourceLocation::currentLoc());
//     _syncPal->pause();
//
//     // Setup phase
//     LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 3 : Setup phase");
//
//     // On local replica
//     // Rename A into B
//     LocalMoveJob setupMoveJob1(_localPath / testExecutorFolderRelativePath / testConflictFolderName / "A",
//                                _localPath / testExecutorFolderRelativePath / testConflictFolderName / "B");
//     setupMoveJob1.runSynchronously();
//
//     // Move S under root
//     LocalMoveJob setupMoveJob2(_localPath / testExecutorFolderRelativePath / testConflictFolderName / "B/S",
//                                _localPath / testExecutorFolderRelativePath / testConflictFolderName / "S");
//     setupMoveJob2.runSynchronously();
//
//     // On remote replica
//     // Delete A
//     DeleteJob setupDeleteJob(_driveDbId, aRemoteId, "", "",
//                              NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
//     setupDeleteJob.runSynchronously();
//
//     Utility::msleep(10000); // Wait more to make sure the remote snapshot has been updated (TODO : not needed once longpoll
//                             // request is implemented)
//
//     LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 3 : Resolution phase");
//     _syncPal->unpause();
//     waitForSyncToFinish(SourceLocation::currentLoc());
//
//     // Test phase
//     LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 3 : Test phase");
//
//     // On remote
//     {
//         bool foundB = false;
//         bool foundS = false;
//         GetFileListJob testJob(_driveDbId, testConflictFolderId);
//         testJob.runSynchronously();
//         Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
//         CPPUNIT_ASSERT(resObj);
//         Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
//         CPPUNIT_ASSERT(dataArray);
//         for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
//             Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
//             if (obj->get(nameKey).toString() == "B") {
//                 aRemoteId = obj->get(idKey).toString();
//                 foundB = true;
//             } else if (obj->get(nameKey).toString() == "S") {
//                 sRemoteId = obj->get(idKey).toString();
//                 foundS = true;
//             }
//         }
//         CPPUNIT_ASSERT(foundB);
//         CPPUNIT_ASSERT(foundS);
//     }
//
//     {
//         bool found = false;
//         GetFileListJob testJob(_driveDbId, aRemoteId);
//         testJob.runSynchronously();
//         Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
//         CPPUNIT_ASSERT(resObj);
//         Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
//         CPPUNIT_ASSERT(dataArray);
//         for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
//             Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
//             if (obj->get(nameKey).toString() == "R") {
//                 rRemoteId = obj->get(idKey).toString();
//                 found = true;
//             }
//         }
//         CPPUNIT_ASSERT(found);
//     }
//
//     {
//         bool found = false;
//         GetFileListJob testJob(_driveDbId, rRemoteId);
//         testJob.runSynchronously();
//         Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
//         CPPUNIT_ASSERT(resObj);
//         Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
//         CPPUNIT_ASSERT(dataArray);
//         for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
//             Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
//             if (obj->get(nameKey).toString() == "Q") {
//                 qRemoteId = obj->get(idKey).toString();
//                 found = true;
//             }
//         }
//         CPPUNIT_ASSERT(found);
//     }
//
//     // On local
//     // Look for local file that have been renamed (and excluded from sync)
//     CPPUNIT_ASSERT(std::filesystem::exists(_localPath / testExecutorFolderRelativePath / testConflictFolderName /
//     "B/R/Q")); CPPUNIT_ASSERT(std::filesystem::exists(_localPath / testExecutorFolderRelativePath / testConflictFolderName
//     / "S"));
//
//     // Clean phase
//     LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 3 : Clean phase");
//
//     // Remove the test files
//     DeleteJob finalDeleteJob1(_driveDbId, aRemoteId, "", "",
//                               NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
//     finalDeleteJob1.runSynchronously();
//
//     DeleteJob finalDeleteJob2(_driveDbId, sRemoteId, "", "",
//                               NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
//     finalDeleteJob2.runSynchronously();
//
//     waitForSyncToFinish(SourceLocation::currentLoc());
//     std::cout << "OK" << std::endl;
// }
//
// // See p.86, case 4
// void TestIntegration::testMoveDeleteConflict4() {
//     LOGW_DEBUG(_logger, L"$$$$$ test Move-Delete conflict 4");
//     std::cout << "test Move-Delete conflict 4: ";
//
//     // Init phase
//     LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 4 : Init phase");
//
//     NodeId aRemoteId;
//     NodeId rRemoteId;
//     NodeId sRemoteId;
//     NodeId qRemoteId;
//     testMoveDeleteConflict_initPhase(_driveDbId, aRemoteId, rRemoteId, sRemoteId, qRemoteId);
//
//     waitForSyncToFinish(SourceLocation::currentLoc());
//     _syncPal->pause();
//
//     // Setup phase
//     LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 4 : Setup phase");
//
//     // On local replica
//     // Move S under root
//     LocalMoveJob setupMoveJob(_localPath / testExecutorFolderRelativePath / testConflictFolderName / "A/S",
//                               _localPath / testExecutorFolderRelativePath / testConflictFolderName / "S");
//     setupMoveJob.runSynchronously();
//
//     LocalDeleteJob setupDeleteJob(SyncPalInfo{_driveDbId, _localPath},
//                                   testExecutorFolderRelativePath / testConflictFolderName / "A", false, aRemoteId);
//     setupDeleteJob.setBypassCheck(true);
//     setupDeleteJob.runSynchronously();
//
//     // On remote replica
//     // Rename A into B
//     RenameJob setupRenameJob(_syncPal->vfs(), _driveDbId, aRemoteId, Str("B"));
//     setupRenameJob.runSynchronously();
//
//     Utility::msleep(10000); // Wait more to make sure the remote snapshot has been updated (TODO : not needed once longpoll
//                             // request is implemented)
//
//     LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 4 : Resolution phase");
//     _syncPal->unpause();
//     waitForSyncToFinish(SourceLocation::currentLoc());
//
//     // Test phase
//     LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 4 : Test phase");
//
//     // On remote
//     {
//         bool foundB = false;
//         bool foundS = false;
//         GetFileListJob testJob(_driveDbId, testConflictFolderId);
//         testJob.runSynchronously();
//         Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
//         CPPUNIT_ASSERT(resObj);
//         Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
//         CPPUNIT_ASSERT(dataArray);
//         for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
//             Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
//             if (obj->get(nameKey).toString() == "B") {
//                 aRemoteId = obj->get(idKey).toString();
//                 foundB = true;
//             } else if (obj->get(nameKey).toString() == "S") {
//                 sRemoteId = obj->get(idKey).toString();
//                 foundS = true;
//             }
//         }
//         CPPUNIT_ASSERT(foundB);
//         CPPUNIT_ASSERT(foundS);
//     }
//
//     {
//         bool found = false;
//         GetFileListJob testJob(_driveDbId, aRemoteId);
//         testJob.runSynchronously();
//         Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
//         CPPUNIT_ASSERT(resObj);
//         Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
//         CPPUNIT_ASSERT(dataArray);
//         for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
//             Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
//             if (obj->get(nameKey).toString() == "R") {
//                 rRemoteId = obj->get(idKey).toString();
//                 found = true;
//             }
//         }
//         CPPUNIT_ASSERT(found);
//     }
//
//     {
//         bool found = false;
//         GetFileListJob testJob(_driveDbId, rRemoteId);
//         testJob.runSynchronously();
//         Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
//         CPPUNIT_ASSERT(resObj);
//         Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
//         CPPUNIT_ASSERT(dataArray);
//         for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
//             Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
//             if (obj->get(nameKey).toString() == "Q") {
//                 qRemoteId = obj->get(idKey).toString();
//                 found = true;
//             }
//         }
//         CPPUNIT_ASSERT(found);
//     }
//
//     // On local
//     // Look for local file that have been renamed (and excluded from sync)
//     CPPUNIT_ASSERT(std::filesystem::exists(_localPath / testExecutorFolderRelativePath / testConflictFolderName /
//     "B/R/Q")); CPPUNIT_ASSERT(std::filesystem::exists(_localPath / testExecutorFolderRelativePath / testConflictFolderName
//     / "S"));
//
//     // Clean phase
//     LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 4 : Clean phase");
//
//     // Remove the test files
//     DeleteJob finalDeleteJob1(_driveDbId, aRemoteId, "", "",
//                               NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
//     finalDeleteJob1.runSynchronously();
//
//     DeleteJob finalDeleteJob2(_driveDbId, sRemoteId, "", "",
//                               NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
//     finalDeleteJob2.runSynchronously();
//
//     waitForSyncToFinish(SourceLocation::currentLoc());
//
//     std::cout << "OK" << std::endl;
// }
//
// // See p.87, case 5
// void TestIntegration::testMoveDeleteConflict5() {
//     // Note: This case is in fact solve as a Move-ParentDelete conflict
//
//     LOGW_DEBUG(_logger, L"$$$$$ test Move-Delete conflict 5");
//     std::cout << "test Move-Delete conflict 5: ";
//
//     // Init phase
//     LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 5 : Init phase");
//
//     NodeId aRemoteId;
//     NodeId rRemoteId;
//     NodeId sRemoteId;
//     NodeId qRemoteId;
//     testMoveDeleteConflict_initPhase(_driveDbId, aRemoteId, rRemoteId, sRemoteId, qRemoteId);
//
//     waitForSyncToFinish(SourceLocation::currentLoc());
//     _syncPal->pause();
//
//     // Setup phase
//     LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 5 : Setup phase");
//
//     // On local replica
//     // Move S under root
//     LocalMoveJob setupMoveJob(_localPath / testExecutorFolderRelativePath / testConflictFolderName / "A/R",
//                               _localPath / testExecutorFolderRelativePath / testConflictFolderName / "A/X");
//     setupMoveJob.runSynchronously();
//
//     // On remote replica
//     // Rename A into B
//     DeleteJob setupDeleteJob(_driveDbId, aRemoteId, "", "",
//                              NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
//     setupDeleteJob.runSynchronously();
//
//     Utility::msleep(10000); // Wait more to make sure the remote snapshot has been updated (TODO : not needed once longpoll
//                             // request is implemented)
//
//     LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 5 : Resolution phase");
//     _syncPal->unpause();
//     waitForSyncToFinish(SourceLocation::currentLoc());
//
//     // Test phase
//     LOGW_DEBUG(_logger, L"----- test Move-Delete conflict 5 : Test phase");
//
//     // On remote
//     {
//         bool foundA = false;
//         GetFileListJob testJob(_driveDbId, testConflictFolderId);
//         testJob.runSynchronously();
//         Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
//         CPPUNIT_ASSERT(resObj);
//         Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
//         CPPUNIT_ASSERT(dataArray);
//         for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
//             Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
//             if (obj->get(nameKey).toString() == "A") {
//                 aRemoteId = obj->get(idKey).toString();
//                 foundA = true;
//             }
//         }
//         CPPUNIT_ASSERT(!foundA);
//     }
//
//     // On local
//     // Look for local file that have been renamed (and excluded from sync)
//     CPPUNIT_ASSERT(!std::filesystem::exists(_localPath / testExecutorFolderRelativePath / testConflictFolderName / "A"));
//
//     std::cout << "OK" << std::endl;
// }
//
// void TestIntegration::testMoveParentDeleteConflict() {
//     LOGW_DEBUG(_logger, L"$$$$$ test MoveParent-Delete conflict");
//     std::cout << "test MoveParent-Delete conflict : ";
//
//     // Init phase
//     LOGW_DEBUG(_logger, L"----- test MoveParent-Delete conflict : Init phase");
//
//     NodeId aRemoteId;
//     NodeId rRemoteId;
//     NodeId sRemoteId;
//     NodeId qRemoteId;
//     testMoveDeleteConflict_initPhase(_driveDbId, aRemoteId, rRemoteId, sRemoteId, qRemoteId);
//
//     waitForSyncToFinish(SourceLocation::currentLoc());
//     _syncPal->pause();
//
//     // Setup phase
//     LOGW_DEBUG(_logger, L"----- test MoveParent-Delete conflict : Setup phase");
//
//     // On local replica
//     // Delete R
//     LocalDeleteJob setupDeleteJob(SyncPalInfo{_driveDbId, _localPath},
//                                   testExecutorFolderRelativePath / testConflictFolderName / "A/R", false, aRemoteId);
//     setupDeleteJob.setBypassCheck(true);
//     setupDeleteJob.runSynchronously();
//
//     // On remote replica
//     // Move S into R
//     MoveJob setupMoveJob(_syncPal->vfs(), _driveDbId, "", sRemoteId, rRemoteId);
//     setupMoveJob.runSynchronously();
//
//     Utility::msleep(10000); // Wait more to make sure the remote snapshot has been updated (TODO : not needed once longpoll
//                             // request is implemented)
//
//     LOGW_DEBUG(_logger, L"----- test MoveParent-Delete conflict : Resolution phase");
//     _syncPal->unpause();
//     waitForSyncToFinish(SourceLocation::currentLoc());
//
//     // Test phase
//     LOGW_DEBUG(_logger, L"----- test MoveParent-Delete conflict : Test phase");
//
//     // On remote
//     {
//         bool foundR = false;
//         bool foundS = false;
//         GetFileListJob testJob(_driveDbId, aRemoteId);
//         testJob.runSynchronously();
//         Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
//         CPPUNIT_ASSERT(resObj);
//         Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
//         CPPUNIT_ASSERT(dataArray);
//         for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
//             Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
//             if (obj->get(nameKey).toString() == "R") {
//                 rRemoteId = obj->get(idKey).toString();
//                 foundR = true;
//             } else if (obj->get(nameKey).toString() == "S") {
//                 sRemoteId = obj->get(idKey).toString();
//                 foundS = true;
//             }
//         }
//         CPPUNIT_ASSERT(!foundR);
//         CPPUNIT_ASSERT(foundS);
//     }
//
//     // On local
//     // Look for local file that have been renamed (and excluded from sync)
//     CPPUNIT_ASSERT(!std::filesystem::exists(_localPath / testExecutorFolderRelativePath / testConflictFolderName /
//     "A/R/Q")); CPPUNIT_ASSERT(std::filesystem::exists(_localPath / testExecutorFolderRelativePath / testConflictFolderName
//     / "A/S"));
//
//     // Clean phase
//     LOGW_DEBUG(_logger, L"----- test MoveParent-Delete conflict : Clean phase");
//
//     // Remove the test files
//     DeleteJob finalDeleteJob(_driveDbId, aRemoteId, "", "",
//                              NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
//     finalDeleteJob.runSynchronously();
//
//     waitForSyncToFinish(SourceLocation::currentLoc());
//
//     std::cout << "OK" << std::endl;
// }
//
// void TestIntegration::testCreateParentDeleteConflict() {
//     LOGW_DEBUG(_logger, L"$$$$$ test Create-ParentDelete conflict");
//     std::cout << "test Create-ParentDelete conflict : ";
//
//     // Init phase
//     LOGW_DEBUG(_logger, L"----- test Create-ParentDelete conflict : Init phase");
//
//     NodeId aRemoteId;
//     NodeId rRemoteId;
//     NodeId sRemoteId;
//     NodeId qRemoteId;
//     testMoveDeleteConflict_initPhase(_driveDbId, aRemoteId, rRemoteId, sRemoteId, qRemoteId);
//
//     waitForSyncToFinish(SourceLocation::currentLoc());
//     _syncPal->pause();
//
//     // Setup phase
//     LOGW_DEBUG(_logger, L"----- test Create-ParentDelete conflict : Setup phase");
//
//     // On local replica
//     // Create file in R
//     SyncName tmpFileName =
//             Str("testCreateParentDelete_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) + Str(".txt");
//     SyncPath tmpFile = _localPath / testExecutorFolderRelativePath / testConflictFolderName / Str("A/R") / tmpFileName;
//     SyncName testCallStr = Str(R"(echo "This is an edit test )") +
//     Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) +
//                            Str(R"(" >> ")") + tmpFile.make_preferred().native() + Str(R"(")");
//     std::system(SyncName2Str(testCallStr).c_str());
//
//     // On remote replica
//     // Delete A
//     DeleteJob setupDeleteJob(_driveDbId, aRemoteId, "", "",
//                              NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
//     setupDeleteJob.runSynchronously();
//
//     Utility::msleep(10000); // Wait more to make sure the remote snapshot has been updated (TODO : not needed once longpoll
//                             // request is implemented)
//
//     LOGW_DEBUG(_logger, L"----- test Create-ParentDelete : Resolution phase");
//     _syncPal->unpause();
//     waitForSyncToFinish(SourceLocation::currentLoc());
//
//     // Test phase
//     LOGW_DEBUG(_logger, L"----- test Create-ParentDelete conflict : Test phase");
//
//     // On remote
//     NodeId testFileRemoteId;
//     {
//         // Files with "_conflict_" suffix are excluded from sync
//         bool found = false;
//         GetFileListJob testJob(_driveDbId, "1");
//         testJob.runSynchronously();
//         Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
//         CPPUNIT_ASSERT(resObj);
//         Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
//         CPPUNIT_ASSERT(dataArray);
//         for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
//             Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
//             if (Utility::startsWith(obj->get(nameKey).toString(), tmpFile.stem().string() + "_conflict_")) {
//                 testFileRemoteId = obj->get(idKey).toString();
//                 found = true;
//             }
//         }
//         CPPUNIT_ASSERT(!found);
//     }
//
//     {
//         // Directory A should have been deleted
//         bool found = false;
//         GetFileListJob testJob(_driveDbId, testConflictFolderId);
//         testJob.runSynchronously();
//         Poco::JSON::Object::Ptr resObj = testJob.jsonRes();
//         CPPUNIT_ASSERT(resObj);
//         Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
//         CPPUNIT_ASSERT(dataArray);
//         for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
//             Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
//             if (obj->get(nameKey).toString() == "A") {
//                 found = true;
//             }
//         }
//         CPPUNIT_ASSERT(!found);
//     }
//
//     // On local
//     // Files with "_conflict_" suffix should be present in root directory
//     bool found = false;
//     std::filesystem::path localExcludedPath;
//     for (auto const &dir_entry: std::filesystem::directory_iterator{_localPath}) {
//         if (Utility::startsWith(dir_entry.path().filename().native(), tmpFile.stem().native() + Str("_conflict_"))) {
//             found = true;
//             localExcludedPath = dir_entry.path();
//             break;
//         }
//     }
//     CPPUNIT_ASSERT(found);
//
//     CPPUNIT_ASSERT(!std::filesystem::exists(_localPath / testExecutorFolderRelativePath / testConflictFolderName / "A"));
//
//     // Clean phase
//     LOGW_DEBUG(_logger, L"----- test Create-ParentDelete conflict : Clean phase");
//
//     // Remove the test files
//     std::filesystem::remove(localExcludedPath);
//
//     waitForSyncToFinish(SourceLocation::currentLoc());
//
//     std::cout << "OK" << std::endl;
// }
//
// void TestIntegration::testMoveMoveSourcePseudoConflict() {
//     LOGW_DEBUG(_logger, L"$$$$$ test Move-MoveSource pseudo conflict");
//     std::cout << "test Move-MoveSource pseudo conflict : ";
//
//     // Init phase
//     LOGW_DEBUG(_logger, L"----- test Move-MoveSource pseudo conflict : Init phase");
//
//     // Duplicate the test file
//     SyncName testFileName = Str("testMoveMoveSourcePseudoConflict_") +
//                             Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) + Str(".txt");
//     DuplicateJob initJob(_syncPal->vfs(), _driveDbId, testExecutorFileRemoteId, testFileName);
//     initJob.runSynchronously();
//     NodeId testFileRemoteId = initJob.nodeId();
//
//     waitForSyncToFinish(SourceLocation::currentLoc());
//     _syncPal->pause();
//
//     // Setup phase
//     LOGW_DEBUG(_logger, L"----- test Move-MoveSource pseudo conflict : Setup phase");
//
//     // On local replica
//     // Move test file into executor sub folder
//     SyncPath sourcePath = _localPath / testExecutorFolderRelativePath / testFileName;
//     FileStat fileStat;
//     bool exists = false;
//     IoHelper::getFileStat(sourcePath.make_preferred(), &fileStat, exists);
//     NodeId prevLocalId = std::to_string(fileStat.inode);
//
//     SyncPath destPath = _localPath / testExecutorFolderRelativePath / testExecutorSubFolderName / testFileName;
//     LocalMoveJob setupLocalMoveJob(sourcePath, destPath);
//     setupLocalMoveJob.runSynchronously();
//
//     // On remote replica
//     // Move test file into executor sub folder
//     MoveJob setupRemoteMoveJob(_syncPal->vfs(), _driveDbId, "", testFileRemoteId, testExecutorSubFolderRemoteId);
//     setupRemoteMoveJob.runSynchronously();
//
//     Utility::msleep(10000); // Wait more to make sure the remote snapshot has been updated (TODO : not needed once longpoll
//                             // request is implemented)
//
//     LOGW_DEBUG(_logger, L"----- test Move-MoveSource pseudo conflict : Resolution phase");
//     _syncPal->unpause();
//     waitForSyncToFinish(SourceLocation::currentLoc());
//
//     // Test phase
//     LOGW_DEBUG(_logger, L"----- test Move-MoveSource pseudo conflict : Test phase");
//
//     // Local file ID should be the same
//     IoHelper::getFileStat(destPath.make_preferred(), &fileStat, exists);
//     NodeId newLocalId = std::to_string(fileStat.inode);
//     CPPUNIT_ASSERT(newLocalId == prevLocalId);
//
//     // Clean phase
//     LOGW_DEBUG(_logger, L"----- test Move-MoveSource pseudo conflict : Clean phase");
//
//     // Remove the test files
//     DeleteJob finalDeleteJob(_driveDbId, testFileRemoteId, "", "",
//                              NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
//     finalDeleteJob.runSynchronously();
//
//     waitForSyncToFinish(SourceLocation::currentLoc());
//
//     std::cout << "OK" << std::endl;
// }
//
// void TestIntegration::testMoveMoveSourceConflict() {
//     LOGW_DEBUG(_logger, L"$$$$$ test Move-MoveSource conflict");
//     std::cout << "test Move-MoveSource conflict : ";
//
//     // Init phase
//     LOGW_DEBUG(_logger, L"----- test Move-MoveSource conflict : Init phase");
//
//     // Duplicate the test file
//     SyncName testFileName = Str("testMoveMoveSourcePseudoConflict_") +
//                             Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) + Str(".txt");
//     DuplicateJob initJob(_syncPal->vfs(), _driveDbId, testExecutorFileRemoteId, testFileName);
//     initJob.runSynchronously();
//     NodeId testFileRemoteId = initJob.nodeId();
//
//     waitForSyncToFinish(SourceLocation::currentLoc());
//     _syncPal->pause();
//
//     // Setup phase
//     LOGW_DEBUG(_logger, L"----- test Move-MoveSource conflict : Setup phase");
//
//     // On local replica
//     // Move test file into executor sub folder
//     SyncPath sourcePath = _localPath / testExecutorFolderRelativePath / testFileName;
//     FileStat fileStat;
//     bool exists = false;
//     IoHelper::getFileStat(sourcePath.make_preferred(), &fileStat, exists);
//     NodeId prevLocalId = std::to_string(fileStat.inode);
//
//     SyncPath destPath = _localPath / testExecutorFolderRelativePath / testConflictFolderName / testFileName;
//     LocalMoveJob setupLocalMoveJob(sourcePath, destPath);
//     setupLocalMoveJob.runSynchronously();
//
//     // On remote replica
//     // Move test file into executor sub folder
//     MoveJob setupRemoteMoveJob(_syncPal->vfs(), _driveDbId, "", testFileRemoteId, testExecutorSubFolderRemoteId);
//     setupRemoteMoveJob.runSynchronously();
//
//     Utility::msleep(10000); // Wait more to make sure the remote snapshot has been updated (TODO : not needed once longpoll
//                             // request is implemented)
//
//     LOGW_DEBUG(_logger, L"----- test Move-MoveSource conflict : Resolution phase");
//     _syncPal->unpause();
//     waitForSyncToFinish(SourceLocation::currentLoc());
//
//     // Test phase
//     LOGW_DEBUG(_logger, L"----- test Move-MoveSource conflict : Test phase");
//
//     // Remote should win
//     CPPUNIT_ASSERT(!std::filesystem::exists(destPath));
//
//     GetFileInfoJob testJob(_driveDbId, testFileRemoteId);
//     testJob.runSynchronously();
//     CPPUNIT_ASSERT(testJob.parentNodeId() == testExecutorSubFolderRemoteId);
//
//     // Clean phase
//     LOGW_DEBUG(_logger, L"----- test Move-MoveSource conflict : Clean phase");
//
//     // Remove the test files
//     DeleteJob finalDeleteJob(_driveDbId, testFileRemoteId, "", "",
//                              NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
//     finalDeleteJob.runSynchronously();
//
//     waitForSyncToFinish(SourceLocation::currentLoc());
//
//     std::cout << "OK" << std::endl;
// }
//
// void TestIntegration::testMoveMoveDestConflict() {
//     LOGW_DEBUG(_logger, L"$$$$$ test Move-MoveDest conflict");
//     std::cout << "test Move-MoveDest conflict : ";
//
//     // Init phase
//     LOGW_DEBUG(_logger, L"----- test Move-MoveDest conflict : Init phase");
//     SyncName testFileName =
//             Str("testMoveMoveDestConflict_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10)) + Str(".txt");
//
//     // Duplicate the remote test file
//     DuplicateJob initDuplicateJob(_syncPal->vfs(), _driveDbId, testExecutorFileRemoteId, testFileName);
//     initDuplicateJob.runSynchronously();
//     NodeId testFileRemoteId = initDuplicateJob.nodeId();
//
//     // Duplicate the local test file
//     SyncPath sourcePath = _localPath / testExecutorFolderRelativePath / "test_executor_copy.txt";
//     SyncPath destPath = _localPath / testExecutorFolderRelativePath / "test_executor_copy_tmp.txt";
//     LocalCopyJob initLocalCopyJob(sourcePath, destPath);
//     initLocalCopyJob.runSynchronously();
//
//     waitForSyncToFinish(SourceLocation::currentLoc());
//     _syncPal->pause();
//
//     // Setup phase
//     LOGW_DEBUG(_logger, L"----- test Move-MoveDest conflict : Setup phase");
//
//     // On remote replica
//     // Move the remote test file under folder "test_executor_sub"
//     MoveJob setupMoveJob(_syncPal->vfs(), _driveDbId, "", testFileRemoteId, testExecutorSubFolderRemoteId);
//     setupMoveJob.runSynchronously();
//
//     // On local replica
//     // Move the local test file under folder "test_executor_sub"
//     sourcePath = destPath;
//     destPath = _localPath / testExecutorFolderRelativePath / testExecutorSubFolderName / testFileName;
//     LocalMoveJob setupLocalMoveJob(sourcePath, destPath);
//     setupLocalMoveJob.runSynchronously();
//
//     waitForSyncToFinish(SourceLocation::currentLoc());
//
//     Utility::msleep(10000); // Wait more to make sure the remote snapshot has been updated (TODO : not needed once longpoll
//                             // request is implemented)
//
//     LOGW_DEBUG(_logger, L"----- test Move-MoveDest conflict : Resolution phase");
//     _syncPal->unpause();
//     waitForSyncToFinish(SourceLocation::currentLoc());
//
//     // Test phase
//     LOGW_DEBUG(_logger, L"----- test Move-MoveDest conflict : Test phase");
//
//     // Remote should win
//     // On local
//     bool found = false;
//     std::filesystem::path localExcludedPath;
//     for (auto const &dir_entry:
//          std::filesystem::directory_iterator{_localPath / testExecutorFolderRelativePath / testExecutorSubFolderName}) {
//         if (Utility::startsWith(dir_entry.path().filename().native(), destPath.stem().native() + Str("_conflict_"))) {
//             found = true;
//             localExcludedPath = dir_entry.path();
//             break;
//         }
//     }
//     CPPUNIT_ASSERT(found);
//
//     // On remote
//     GetFileInfoJob testJob(_driveDbId, testFileRemoteId);
//     testJob.runSynchronously();
//     CPPUNIT_ASSERT(testJob.parentNodeId() == testExecutorSubFolderRemoteId);
//
//     // Clean phase
//     LOGW_DEBUG(_logger, L"----- test Move-MoveDest conflict : Clean phase");
//
//     // Remove the test files
//     DeleteJob finalDeleteJob(_driveDbId, testFileRemoteId, "", "",
//                              NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
//     finalDeleteJob.runSynchronously();
//
//     std::filesystem::remove(localExcludedPath);
//
//     waitForSyncToFinish(SourceLocation::currentLoc());
//
//     std::cout << "OK" << std::endl;
// }
//
// void TestIntegration::testMoveMoveCycleConflict() {
//     LOGW_DEBUG(_logger, L"$$$$$ test Move-MoveCycle conflict");
//     std::cout << "test Move-MoveCycle conflict : ";
//
//     // Init phase
//     LOGW_DEBUG(_logger, L"----- test Move-MoveCycle conflict : Init phase");
//     SyncName testDirA = Str("testMoveMoveDestConflictA_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10));
//     SyncName testDirB = Str("testMoveMoveDestConflictB_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10));
//
//     // Create folders
//     CreateDirJob initCreateDirJobA(nullptr, _driveDbId, testDirA, testExecutorFolderRemoteId, testDirA);
//     initCreateDirJobA.runSynchronously();
//     NodeId testDirRemoteIdA = initCreateDirJobA.nodeId();
//
//     CreateDirJob initCreateDirJobB(nullptr, _driveDbId, testDirB, testExecutorFolderRemoteId, testDirB);
//     initCreateDirJobB.runSynchronously();
//     NodeId testDirRemoteIdB = initCreateDirJobB.nodeId();
//
//     waitForSyncToFinish(SourceLocation::currentLoc());
//     _syncPal->pause();
//
//     // Setup phase
//     LOGW_DEBUG(_logger, L"----- test Move-MoveCycle conflict : Setup phase");
//
//     // On remote replica
//     // Move A into B
//     MoveJob setupMoveJob(nullptr, _driveDbId, "", testDirRemoteIdA, testDirRemoteIdB);
//     setupMoveJob.runSynchronously();
//
//     // On local replica
//     // Move B into A
//     SyncPath sourcePath = _localPath / testExecutorFolderRelativePath / testDirB;
//     SyncPath destPath = _localPath / testExecutorFolderRelativePath / testDirA / testDirB;
//     LocalMoveJob setupLocalMoveJob(sourcePath, destPath);
//     setupLocalMoveJob.runSynchronously();
//
//     waitForSyncToFinish(SourceLocation::currentLoc());
//
//     Utility::msleep(10000); // Wait more to make sure the remote snapshot has been updated (TODO : not needed once longpoll
//                             // request is implemented)
//
//     LOGW_DEBUG(_logger, L"----- test Move-MoveCycle conflict : Resolution phase");
//     _syncPal->unpause();
//     waitForSyncToFinish(SourceLocation::currentLoc());
//
//     // Test phase
//     LOGW_DEBUG(_logger, L"----- test Move-MoveCycle conflict : Test phase");
//
//     // Remote should win
//     // On remote
//     GetFileInfoJob testJob(_driveDbId, testDirRemoteIdA);
//     testJob.runSynchronously();
//     CPPUNIT_ASSERT(testJob.parentNodeId() == testDirRemoteIdB);
//
//     // Clean phase
//     LOGW_DEBUG(_logger, L"----- test Move-MoveCycle conflict : Clean phase");
//
//     // Remove the test files
//     DeleteJob finalDeleteJob(_driveDbId, testDirRemoteIdB, "", "",
//                              NodeType::File); // TODO : this test needs to be fixed, local ID and path are now mandatory
//     finalDeleteJob.runSynchronously();
//
//     waitForSyncToFinish(SourceLocation::currentLoc());
//
//     std::cout << "OK" << std::endl;
// }

#ifdef __unix__
void TestIntegration::testNodeIdReuseFile2DirAndDir2File() {
    if (!testhelpers::isExtendedTest()) return;

    LOGW_DEBUG(_logger, L"$$$$$ testNodeIdReuseFile2DirAndDir2File");

    SyncNodeCache::instance()->update(_driveDbId, SyncNodeType::BlackList,
                                      {commonDocumentsNodeId}); // Exclude common documents folder
    const RemoteTemporaryDirectory remoteTempDir(_driveDbId, "1", "testNodeIdReuseFile2DirAndDir2File");
    const SyncPath relativeWorkingDirPath = remoteTempDir.name();
    const SyncPath absoluteLocalWorkingDir = _syncPal->localPath() / relativeWorkingDirPath;
    _syncPal->start();
    waitForSyncToFinish(SourceLocation::currentLoc());

    const auto localSnapshot = _syncPal->snapshot(ReplicaSide::Local);
    const auto remoteSnapshot = _syncPal->snapshot(ReplicaSide::Remote);
    CPPUNIT_ASSERT(!localSnapshot->itemId(relativeWorkingDirPath).empty());
    CPPUNIT_ASSERT(!remoteSnapshot->itemId(relativeWorkingDirPath).empty());

    MockIoHelperFileStat mockIoHelper;
    // Create a file with a custom inode on the local side
    mockIoHelper.setPathWithFakeInode(absoluteLocalWorkingDir / "testNodeIdReuseFile", 2);
    { const std::ofstream file((absoluteLocalWorkingDir / "testNodeIdReuseFile").string()); }
    waitForSyncToFinish(SourceLocation::currentLoc());
    CPPUNIT_ASSERT_EQUAL(NodeId("2"), localSnapshot->itemId(relativeWorkingDirPath / "testNodeIdReuseFile"));
    const NodeId remoteFileId = remoteSnapshot->itemId(relativeWorkingDirPath / "testNodeIdReuseFile");
    CPPUNIT_ASSERT(!remoteFileId.empty());
    CPPUNIT_ASSERT_EQUAL(NodeType::File, remoteSnapshot->type(remoteFileId));

    // Replace the file with a directory on the local side (with the same id)
    _syncPal->pause();
    while (!_syncPal->isPaused()) {
        Utility::msleep(100);
    }
    IoError ioError = IoError::Success;
    IoHelper::deleteItem(absoluteLocalWorkingDir / "testNodeIdReuseFile", ioError);
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    mockIoHelper.setPathWithFakeInode(absoluteLocalWorkingDir / "testNodeIdReuseDir", 2);
    IoHelper::createDirectory(absoluteLocalWorkingDir / "testNodeIdReuseDir", ioError);
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    _syncPal->unpause();
    waitForSyncToFinish(SourceLocation::currentLoc());

    // Check that the file has been replaced by a directory on the remote with a different ID
    const NodeId newRemoteDirId = remoteSnapshot->itemId(relativeWorkingDirPath / "testNodeIdReuseDir");
    CPPUNIT_ASSERT(!newRemoteDirId.empty());
    CPPUNIT_ASSERT(newRemoteDirId != remoteFileId);
    CPPUNIT_ASSERT_EQUAL(NodeType::Directory, remoteSnapshot->type(newRemoteDirId));
    CPPUNIT_ASSERT(!remoteSnapshot->exists(remoteFileId));

    // Replace the directory with a file on the local side with the same id
    _syncPal->pause();
    while (!_syncPal->isPaused()) {
        Utility::msleep(100);
    }
    IoHelper::deleteItem(absoluteLocalWorkingDir / "testNodeIdReuseDir", ioError);
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    { const std::ofstream file((absoluteLocalWorkingDir / "testNodeIdReuseFile").string()); }
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    _syncPal->unpause();
    waitForSyncToFinish(SourceLocation::currentLoc());

    // Check that the directory has been replaced by a file on the remote with a different ID
    const NodeId newRemoteFileId = remoteSnapshot->itemId(relativeWorkingDirPath / "testNodeIdReuseFile");
    CPPUNIT_ASSERT(newRemoteFileId != "");
    CPPUNIT_ASSERT(newRemoteFileId != newRemoteDirId);
    CPPUNIT_ASSERT(newRemoteFileId != remoteFileId);
    CPPUNIT_ASSERT(!remoteSnapshot->exists(remoteFileId));
    CPPUNIT_ASSERT(!remoteSnapshot->exists(newRemoteDirId));
    CPPUNIT_ASSERT_EQUAL(NodeType::File, remoteSnapshot->type(newRemoteFileId));
}

void TestIntegration::testNodeIdReuseFile2File() {
    if (!testhelpers::isExtendedTest()) return;

    LOGW_DEBUG(_logger, L"$$$$$ testNodeIdReuseFile2File");

    SyncNodeCache::instance()->update(_driveDbId, SyncNodeType::BlackList,
                                      {commonDocumentsNodeId}); // Exclude common documents folder
    const RemoteTemporaryDirectory remoteTempDir(_driveDbId, "1", "testNodeIdReuseFile2File");
    const SyncPath relativeWorkingDirPath = remoteTempDir.name();
    const SyncPath absoluteLocalWorkingDir = _syncPal->localPath() / relativeWorkingDirPath;
    _syncPal->start();
    waitForSyncToFinish(SourceLocation::currentLoc());

    const auto localSnapshot = _syncPal->snapshot(ReplicaSide::Local);
    const auto remoteSnapshot = _syncPal->snapshot(ReplicaSide::Remote);
    CPPUNIT_ASSERT(!localSnapshot->itemId(relativeWorkingDirPath).empty());
    CPPUNIT_ASSERT(!remoteSnapshot->itemId(relativeWorkingDirPath).empty());

    MockIoHelperFileStat mockIoHelper;
    mockIoHelper.setPathWithFakeInode(absoluteLocalWorkingDir / "testNodeIdReuseFile", 2);
    { const std::ofstream file((absoluteLocalWorkingDir / "testNodeIdReuseFile").string()); }
    waitForSyncToFinish(SourceLocation::currentLoc());
    CPPUNIT_ASSERT_EQUAL(NodeId("2"), localSnapshot->itemId(relativeWorkingDirPath / "testNodeIdReuseFile"));
    const NodeId remoteFileId = remoteSnapshot->itemId(relativeWorkingDirPath / "testNodeIdReuseFile");
    CPPUNIT_ASSERT(!remoteFileId.empty());
    CPPUNIT_ASSERT_EQUAL(NodeType::File, remoteSnapshot->type(remoteFileId));

    // Expected behavior:
    // - changed: ctime, mtime/size/content, path (chmod or + edit + move)
    //   unchanged: inode
    //   - new file on remote side
    //
    // - changed: mtime/size/content, path (edit + move)
    //   unchanged: inode, ctime
    //   - Edit + Move on remote side


    // Delete a file and create another file with the same inode at a different path with different content and ctime
    // Expected behavior: new file on remote side
    _syncPal->pause();
    while (!_syncPal->isPaused()) {
        Utility::msleep(100);
    }

    IoError ioError = IoError::Success;
    IoHelper::deleteItem(absoluteLocalWorkingDir / "testNodeIdReuseFile", ioError);
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    mockIoHelper.setPathWithFakeInode(absoluteLocalWorkingDir / "testNodeIdReuseFile2", 2);
    { std::ofstream((absoluteLocalWorkingDir / "testNodeIdReuseFile2").string()) << "New content"; }
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    _syncPal->unpause();
    waitForSyncToFinish(SourceLocation::currentLoc());
    _syncPal->pause();
    const NodeId newRemoteFileId = remoteSnapshot->itemId(relativeWorkingDirPath / "testNodeIdReuseFile2");
    CPPUNIT_ASSERT(!newRemoteFileId.empty());
    CPPUNIT_ASSERT(remoteFileId != newRemoteFileId);
    CPPUNIT_ASSERT(!remoteSnapshot->exists(remoteFileId));

    // Edit a file and move it to a different path.
    // Expected behavior: Edit + Move on remote side
    {
        std::ofstream file(absoluteLocalWorkingDir / "testNodeIdReuseFile2");
        file << "New content2"; // Content change -> edit
        file.close();
    }
    mockIoHelper.setPathWithFakeInode(absoluteLocalWorkingDir / "testNodeIdReuseFile3", 2);
    IoHelper::moveItem(absoluteLocalWorkingDir / "testNodeIdReuseFile2", absoluteLocalWorkingDir / "testNodeIdReuseFile3",
                       ioError);
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    _syncPal->unpause();
    waitForSyncToFinish(SourceLocation::currentLoc());
    CPPUNIT_ASSERT(remoteSnapshot->itemId(relativeWorkingDirPath / "testNodeIdReuseFile2").empty());
    const NodeId newRemoteFileId2 = remoteSnapshot->itemId(relativeWorkingDirPath / "testNodeIdReuseFile3");
    CPPUNIT_ASSERT(!newRemoteFileId2.empty());
    CPPUNIT_ASSERT_EQUAL(newRemoteFileId, newRemoteFileId2);
    CPPUNIT_ASSERT_EQUAL(remoteSnapshot->size(newRemoteFileId2), localSnapshot->size("2"));
}
#endif

uint64_t TestIntegration::waitForSyncToBeIdle(
        const SourceLocation &srcLoc, const std::chrono::milliseconds minWaitTime /*= std::chrono::milliseconds(3000)*/) const {
    const auto timeOutDuration = minutes(2);
    const TimerUtility timeoutTimer;

    // Wait for end of sync (A sync is considered ended when it stay in Idle for more than 3s
    bool ended = false;
    while (!ended) {
        CPPUNIT_ASSERT_MESSAGE(srcLoc.toString(), timeoutTimer.elapsed<minutes>() < timeOutDuration);

        if (_syncPal->isIdle() && !_syncPal->_localFSObserverWorker->updating() &&
            !_syncPal->_remoteFSObserverWorker->updating()) {
            const TimerUtility idleTimer;
            while (_syncPal->isIdle() && idleTimer.elapsed<milliseconds>() < minWaitTime) {
                CPPUNIT_ASSERT_MESSAGE(srcLoc.toString(), timeoutTimer.elapsed<minutes>() < timeOutDuration);
                Utility::msleep(5);
            }
            ended = idleTimer.elapsed<milliseconds>() >= minWaitTime;
        }
        Utility::msleep(100);
    }
    return _syncPal->syncCount();
}

void TestIntegration::waitForSyncToFinish(const uint64_t syncCount) {
    const TimerUtility timer;
    LOG_DEBUG(_logger, "Start waiting for sync to finish: " << syncCount);

    const auto timeOutDuration = seconds(30);
    const TimerUtility timeoutTimer;

    while (syncCount + 1 > _syncPal->syncCount()) {
        if (timeoutTimer.elapsed<seconds>() > timeOutDuration) {
            LOG_WARN(_logger, "waitForSyncToFinish timed out");
            break;
        }
        Utility::msleep(10);
    }

    std::stringstream ss;
    ss << "Stop waiting: " << timer.elapsed<std::chrono::milliseconds>();
    LOG_DEBUG(_logger, ss.str());
}

void TestIntegration::logStep(const std::string &str) {
    std::stringstream ss;
    ss << "$$$$$ Step `" << str << "` done in " << _timer.lap<std::chrono::milliseconds>() << " $$$$$";
    LOG_DEBUG(_logger, ss.str());
}

NodeId TestIntegration::duplicateRemoteFile(const NodeId &id, const SyncName &newName) const {
    DuplicateJob job(nullptr, _driveDbId, id, newName);
    (void) job.runSynchronously();
    return job.nodeId();
}

SyncPath TestIntegration::findLocalFileByNamePrefix(const SyncPath &parentAbsolutePath, const SyncName &namePrefix) const {
    IoError ioError(IoError::Unknown);
    IoHelper::DirectoryIterator dirIt(parentAbsolutePath, false, ioError);
    bool endOfDir = false;
    DirectoryEntry entry;
    while (dirIt.next(entry, endOfDir, ioError) && !endOfDir && ioError == IoError::Success) {
        if (Utility::startsWith(entry.path().filename(), namePrefix)) return entry.path();
    }
    return SyncPath();
}

TestIntegration::RemoteFileInfo TestIntegration::getRemoteFileInfo(const int driveDbId, const NodeId &parentId,
                                                                   const SyncName &name) const {
    RemoteFileInfo fileInfo;

    GetFileListJob job(driveDbId, parentId);
    (void) job.runSynchronously();

    const auto resObj = job.jsonRes();
    if (!resObj) return fileInfo;

    const auto dataArray = resObj->getArray(dataKey);
    if (!dataArray) return fileInfo;

    for (auto it = dataArray->begin(); it != dataArray->end(); ++it) {
        const auto obj = it->extract<Poco::JSON::Object::Ptr>();
        if (name == obj->get(nameKey).toString()) {
            fileInfo.id = obj->get(idKey).toString();
            fileInfo.parentId = obj->get(parentIdKey).toString();
            fileInfo.modificationTime = toInt(obj->get(lastModifiedAtKey));
            fileInfo.creationTime = toInt(obj->get(addedAtKey));
            fileInfo.type = obj->get(typeKey).toString() == "file" ? NodeType::File : NodeType::Directory;
            if (fileInfo.type == NodeType::File) {
                fileInfo.size = toInt(obj->get(sizeKey));
            }
        }
    }

    return fileInfo;
}

} // namespace KDC
