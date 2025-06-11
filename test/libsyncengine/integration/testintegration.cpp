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
#include "propagation/executor/filerescuer.h"
#include "test_utility/testhelpers.h"
#include "requests/parameterscache.h"
#include "test_utility/remotetemporarydirectory.h"

using namespace CppUnit;
using namespace std::chrono;

namespace KDC {

// Temporary test in drive "kDrive Desktop Team"
const NodeId commonDocumentsNodeId = "3";
const NodeId testCiFolderId = "56850";

namespace {
void createRemoteFile(const int driveDbId, const SyncName &name, const NodeId &remoteParentFileId,
                      SyncTime *creationTime = nullptr, SyncTime *modificationTime = nullptr, int64_t *size = nullptr) {
    const LocalTemporaryDirectory temporaryDir;
    const auto tmpFilePath = temporaryDir.path() / ("tmpFile_" + CommonUtility::generateRandomStringAlphaNum(10));
    testhelpers::generateOrEditTestFile(tmpFilePath);

    const auto timestamp = duration_cast<seconds>(time_point_cast<seconds>(system_clock::now()).time_since_epoch());
    UploadJob job(nullptr, driveDbId, tmpFilePath, name, remoteParentFileId, timestamp.count(), timestamp.count());
    (void) job.runSynchronously();
    if (creationTime) {
        *creationTime = job.creationTime();
    }
    if (modificationTime) {
        *modificationTime = job.modificationTime();
    }
    if (size) {
        *size = job.size();
    }
}

void editRemoteFile(const int driveDbId, const NodeId &remoteFileId, SyncTime *creationTime = nullptr,
                    SyncTime *modificationTime = nullptr, int64_t *size = nullptr) {
    const LocalTemporaryDirectory temporaryDir;
    const auto tmpFilePath = temporaryDir.path() / ("tmpFile_" + CommonUtility::generateRandomStringAlphaNum(10));
    testhelpers::generateOrEditTestFile(tmpFilePath);

    const auto timestamp = duration_cast<seconds>(time_point_cast<seconds>(system_clock::now()).time_since_epoch());
    UploadJob job(nullptr, driveDbId, tmpFilePath, remoteFileId, timestamp.count());
    (void) job.runSynchronously();
    if (creationTime) {
        *creationTime = job.creationTime();
    }
    if (modificationTime) {
        *modificationTime = job.modificationTime();
    }
    if (size) {
        *size = job.size();
    }
}

NodeId duplicateRemoteFile(const int driveDbId, const NodeId &id, const SyncName &newName) {
    DuplicateJob job(nullptr, driveDbId, id, newName);
    (void) job.runSynchronously();
    return job.nodeId();
}

void deleteRemoteFile(const int driveDbId, const NodeId &id) {
    DeleteJob job(driveDbId, id);
    job.setBypassCheck(true);
    (void) job.runSynchronously();
}

SyncPath findLocalFileByNamePrefix(const SyncPath &parentAbsolutePath, const SyncName &namePrefix) {
    IoError ioError(IoError::Unknown);
    IoHelper::DirectoryIterator dirIt(parentAbsolutePath, false, ioError);
    bool endOfDir = false;
    DirectoryEntry entry;
    while (dirIt.next(entry, endOfDir, ioError) && !endOfDir && ioError == IoError::Success) {
        if (Utility::startsWith(entry.path().filename(), namePrefix)) return entry.path();
    }
    return SyncPath();
}
} // namespace

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
    {
        // Generate a temporary file.
        const LocalTemporaryDirectory temporaryDir;
        const auto tmpFilePath = temporaryDir.path() / ("tmpFile_" + CommonUtility::generateRandomStringAlphaNum(10));
        testhelpers::generateOrEditTestFile(tmpFilePath);
        // Upload the temporary file to create on remote replica the file that will be used in tests.
        UploadJob job(nullptr, _driveDbId, tmpFilePath, tmpFilePath.filename(), _remoteSyncDir.id(), testhelpers::defaultTime,
                      testhelpers::defaultTime);
        (void) job.runSynchronously();
        _testFileRemoteId = job.nodeId();
    }

    FileStat fileStat;
    IoError ioError = IoError::Unknown;
    (void) IoHelper::getFileStat(_localSyncDir.path(), &fileStat, ioError);

    const Sync sync(1, drive.dbId(), _localSyncDir.path(), std::to_string(fileStat.inode), "/", _remoteSyncDir.id());
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
    JobManager::instance()->stop();
    JobManager::instance()->clear();
    TestBase::stop();
}

void TestIntegration::testAll() {
    if (!testhelpers::isExtendedTest()) return;

    // Start sync
    _syncPal->start();
    // Wait for the end of 1st sync
    waitForCurrentSyncToFinish(1);
    logStep("initialization");

    // Run test cases
    basicTests();
    inconsistencyTests();
    conflictTests();

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

    waitForCurrentSyncToFinish(syncCount);

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

    waitForCurrentSyncToFinish(syncCount);

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

    waitForCurrentSyncToFinish(syncCount);

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

    waitForCurrentSyncToFinish(syncCount);

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

        fileId = duplicateRemoteFile(_driveDbId, _testFileRemoteId, filePath.filename());
    }

    waitForCurrentSyncToFinish(syncCount);

    CPPUNIT_ASSERT(std::filesystem::exists(subDirPath));
    CPPUNIT_ASSERT(std::filesystem::exists(filePath));

    logStep("test create remote file");

    // Generate an edit operation.
    syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    SyncTime modificationTime = 0;
    int64_t size = 0;
    editRemoteFile(_driveDbId, _testFileRemoteId, nullptr, &modificationTime, &size);

    waitForCurrentSyncToFinish(syncCount);

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

    waitForCurrentSyncToFinish(syncCount);

    CPPUNIT_ASSERT(std::filesystem::exists(filePath));

    logStep("test move remote file");

    // Generate a delete operation.
    syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    {
        DeleteJob job(_driveDbId, subDirId);
        job.setBypassCheck(true);
        (void) job.runSynchronously();
    }

    waitForCurrentSyncToFinish(syncCount);

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
    waitForCurrentSyncToFinish(syncCount);

    CPPUNIT_ASSERT(std::filesystem::exists(remoteFilePath));
    const auto remoteTestFileInfo = getRemoteFileInfo(_driveDbId, _remoteSyncDir.id(), localFilePath.filename());
    CPPUNIT_ASSERT(remoteTestFileInfo.isValid());

    logStep("testSimultaneousChanges");
}

void TestIntegration::inconsistencyTests() {
    // Duplicate remote files to set up the tests.
    auto syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    const auto testForbiddenCharsRemoteId = duplicateRemoteFile(_driveDbId, _testFileRemoteId, "testForbiddenChar");
    const auto testNameClashRemoteId1 = duplicateRemoteFile(_driveDbId, _testFileRemoteId, "testNameClash");
    const auto testNameClashRemoteId2 = duplicateRemoteFile(_driveDbId, _testFileRemoteId, "testnameclash1");

    waitForCurrentSyncToFinish(syncCount);

    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / "testForbiddenChar"));
    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / "testNameClash"));
    const SyncPath nameClashLocalPath = _syncPal->localPath() / "testnameclash1";
    CPPUNIT_ASSERT(std::filesystem::exists(nameClashLocalPath));

    // Rename files on remote side.
    syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    (void) RenameJob(nullptr, _driveDbId, testForbiddenCharsRemoteId, "test/:*ForbiddenChar").runSynchronously();
    (void) RenameJob(nullptr, _driveDbId, testNameClashRemoteId2, "testnameclash").runSynchronously();

    waitForCurrentSyncToFinish(syncCount);

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

    waitForCurrentSyncToFinish(syncCount);

    auto remoteFileInfo = getRemoteFileInfo(_driveDbId, _remoteSyncDir.id(), "testnameclash");
    CPPUNIT_ASSERT(remoteFileInfo.isValid());
    CPPUNIT_ASSERT_LESS(filestat.size, remoteFileInfo.size); // The local edit is not propagated.

    // Rename again the remote file to avoid the name clash.
    syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    (void) RenameJob(nullptr, _driveDbId, testNameClashRemoteId2, "testnameclash2").runSynchronously();

    waitForCurrentSyncToFinish(syncCount);

    /************************************/
    // Temporary, needed because of a bug if an item has remote move and local edit operations:
    // https://infomaniak.atlassian.net/browse/KDESKTOP-1752
    syncCount++;
    waitForCurrentSyncToFinish(syncCount);
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
    testEditEditPseudoConflict();
    testEditEditConflict();
    testMoveCreateConflict();
    testEditDeleteConflict();
    testMoveDeleteConflict();
    testMoveParentDeleteConflict();
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
    waitForCurrentSyncToFinish(syncCount);

    // Check that both remote and local ID has been re-inserted in DB.
    SyncName name;
    CPPUNIT_ASSERT(_syncPal->syncDb()->name(ReplicaSide::Remote, _testFileRemoteId, name, found) && found);
    CPPUNIT_ASSERT(_syncPal->syncDb()->name(ReplicaSide::Local, localId, name, found) && found);
}

void TestIntegration::testCreateCreateConflict() {
    auto syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));

    // Create a file on remote replica.
    const auto remoteId = duplicateRemoteFile(_driveDbId, _testFileRemoteId, "testCreateCreatePseudoConflict");

    // Create a file on local replica.
    const SyncPath localFilePath = _syncPal->localPath() / "testCreateCreatePseudoConflict";
    testhelpers::generateOrEditTestFile(localFilePath);

    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForCurrentSyncToFinish(syncCount);

    const auto conflictedFilePath = findLocalFileByNamePrefix(_localSyncDir.path(), "testCreateCreatePseudoConflict_conflict_");
    CPPUNIT_ASSERT(std::filesystem::exists(conflictedFilePath));
    CPPUNIT_ASSERT(!std::filesystem::exists(localFilePath));

    syncCount++; // Previous sync only fixed the conflict.
    waitForCurrentSyncToFinish(syncCount);

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
    waitForCurrentSyncToFinish(syncCount);

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
    SyncPath relativePath;
    bool ignore = false;
    (void) _syncPal->_remoteFSObserverWorker->liveSnapshot().path(_testFileRemoteId, relativePath, ignore);
    const SyncPath absoluteLocalPath = _syncPal->localPath() / relativePath;
    testhelpers::generateOrEditTestFile(absoluteLocalPath);

    // Edit remote file.
    int64_t creationTime = 0;
    int64_t modificationTime = 0;
    editRemoteFile(_driveDbId, _testFileRemoteId, &creationTime, &modificationTime);

    (void) IoHelper::setFileDates(absoluteLocalPath, creationTime, modificationTime, false);

    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForCurrentSyncToFinish(syncCount);

    const auto conflictedFilePath =
            findLocalFileByNamePrefix(_localSyncDir.path(), absoluteLocalPath.filename().native() + Str("_conflict_"));
    CPPUNIT_ASSERT(std::filesystem::exists(conflictedFilePath));
    CPPUNIT_ASSERT(!std::filesystem::exists(absoluteLocalPath));

    syncCount++; // Previous sync only fixed the conflict.
    waitForCurrentSyncToFinish(syncCount);

    CPPUNIT_ASSERT(std::filesystem::exists(conflictedFilePath));
    CPPUNIT_ASSERT(std::filesystem::exists(absoluteLocalPath));
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
        waitForCurrentSyncToFinish(syncCount);

        // The local file has been excluded from sync.
        const auto conflictedFilePath = findLocalFileByNamePrefix(_localSyncDir.path(), filename.native() + Str("_conflict_"));
        CPPUNIT_ASSERT(std::filesystem::exists(conflictedFilePath));
        CPPUNIT_ASSERT(!std::filesystem::exists(localFilePath));

        syncCount++; // Previous sync only fixed the conflict.
        waitForCurrentSyncToFinish(syncCount);

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
        (void) duplicateRemoteFile(_driveDbId, _testFileRemoteId, newFilename);

        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForCurrentSyncToFinish(syncCount);

        // The local file has been put back in its origin location.
        CPPUNIT_ASSERT(std::filesystem::exists(localFilePath));
        CPPUNIT_ASSERT(!std::filesystem::exists(newLocalFilePath));

        syncCount++; // Previous sync only fixed the conflict.
        waitForCurrentSyncToFinish(syncCount);

        // The remote creation has been propagated.
        CPPUNIT_ASSERT(std::filesystem::exists(localFilePath));
        CPPUNIT_ASSERT(std::filesystem::exists(newLocalFilePath));
    }
}

void TestIntegration::testEditDeleteConflict() {
    // Delete happen on the edited file.
    {
        auto syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));

        // Generate the test file
        const SyncName filename = "testEditDeleteConflict1";
        const auto tmpNodeId = duplicateRemoteFile(_driveDbId, _testFileRemoteId, filename);
        waitForCurrentSyncToFinish(syncCount);

        // Edit the file on remote replica.
        SyncTime creationTime = 0;
        SyncTime modificationTime = 0;
        int64_t size = 0;
        editRemoteFile(_driveDbId, tmpNodeId, &creationTime, &modificationTime, &size);

        // Delete the file on local replica.
        const SyncPath filepath = _syncPal->localPath() / filename;
        IoError ioError = IoError::Unknown;
        (void) IoHelper::deleteItem(filepath, ioError);
        syncCount = _syncPal->syncCount();
        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForCurrentSyncToFinish(syncCount);

        syncCount++; // Previous sync only fixed the conflict.
        waitForCurrentSyncToFinish(syncCount);

        // Edit operation has won.
        CPPUNIT_ASSERT(std::filesystem::exists(filepath));
        FileStat fileStat;
        (void) IoHelper::getFileStat(filepath, &fileStat, ioError);
        // CPPUNIT_ASSERT_EQUAL(creationTime, fileStat.creationTime);
        CPPUNIT_ASSERT_EQUAL(modificationTime, fileStat.modificationTime);
        CPPUNIT_ASSERT_EQUAL(size, fileStat.size);
    }

    // Delete happen on a parent of the edited files.
    {
        auto syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));

        // Generate the test file
        const SyncPath dirpath = _syncPal->localPath() / "testEditDeleteConflict2_dir";
        IoError ioError = IoError::Unknown;
        (void) IoHelper::createDirectory(dirpath, ioError);
        const SyncPath filepath = dirpath / "testEditDeleteConflict2_file";
        testhelpers::generateOrEditTestFile(filepath);
        waitForCurrentSyncToFinish(syncCount);

        std::optional<NodeId> remoteDirNodeId = std::nullopt;
        bool found = false;
        CPPUNIT_ASSERT(_syncPal->syncDb()->id(ReplicaSide::Remote, dirpath.filename(), remoteDirNodeId, found) && found);

        // Edit the file on local replica.
        testhelpers::generateOrEditTestFile(filepath);

        // Delete the file on remote.
        DeleteJob deleteJob(_driveDbId, *remoteDirNodeId);
        deleteJob.setBypassCheck(true);
        (void) deleteJob.runSynchronously();

        syncCount = _syncPal->syncCount();
        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForCurrentSyncToFinish(syncCount);

        syncCount++; // Previous sync only fixed the conflict.
        waitForCurrentSyncToFinish(syncCount);

        // Delete operation has won...
        CPPUNIT_ASSERT(!std::filesystem::exists(filepath));
        // ... but edited file has been rescued.
        CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / FileRescuer::rescueFolderName() / filepath.filename()));
    }
}

namespace {
struct TestMoveDeleteInfo {
        NodeId remoteNodeIdA;
        NodeId remoteNodeIdR;
        NodeId remoteNodeIdS;
        NodeId remoteNodeIdQ;
};

void generateInitialMoveDeleteSituation(const int driveDbId, const NodeId &parentId, TestMoveDeleteInfo &info) {
    // .
    // `-- A
    //     |-- R
    //     |   `-- Q
    //     `-- S
    {
        CreateDirJob jobA(nullptr, driveDbId, parentId, "A");
        (void) jobA.runSynchronously();
        info.remoteNodeIdA = jobA.nodeId();
    }
    {
        CreateDirJob jobR(nullptr, driveDbId, info.remoteNodeIdA, "R");
        (void) jobR.runSynchronously();
        info.remoteNodeIdR = jobR.nodeId();
    }
    {
        CreateDirJob jobS(nullptr, driveDbId, info.remoteNodeIdA, "S");
        (void) jobS.runSynchronously();
        info.remoteNodeIdS = jobS.nodeId();
    }
    createRemoteFile(driveDbId, "Q", info.remoteNodeIdR);
}
} // namespace

void TestIntegration::testMoveDeleteConflict() {
    {
        auto syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
        const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
        TestMoveDeleteInfo info;
        generateInitialMoveDeleteSituation(_driveDbId, tmpRemoteDir.id(), info);
        waitForCurrentSyncToFinish(syncCount);

        // Rename A to B on local replica
        const SyncPath localPathA = _syncPal->localPath() / tmpRemoteDir.name() / "A";
        const SyncPath localPathB = _syncPal->localPath() / tmpRemoteDir.name() / "B";
        IoError ioError = IoError::Unknown;
        (void) IoHelper::renameItem(localPathA, localPathB, ioError);

        // Delete A/S on local replica
        const SyncPath localPathS = localPathB / "S";
        (void) IoHelper::deleteItem(localPathS, ioError);

        // Delete A on remote replica
        deleteRemoteFile(_driveDbId, info.remoteNodeIdA);

        syncCount = _syncPal->syncCount();
        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForCurrentSyncToFinish(syncCount);

        syncCount++; // Previous sync only fixed the conflict.
        waitForCurrentSyncToFinish(syncCount);

        // Delete operation wins
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathA));
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathB));
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathS));
    }
    {
        auto syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
        const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
        TestMoveDeleteInfo info;
        generateInitialMoveDeleteSituation(_driveDbId, tmpRemoteDir.id(), info);
        waitForCurrentSyncToFinish(syncCount);

        // Rename A to B on local replica
        const SyncPath localPathA = _syncPal->localPath() / tmpRemoteDir.name() / "A";
        const SyncPath localPathB = _syncPal->localPath() / tmpRemoteDir.name() / "B";
        IoError ioError = IoError::Unknown;
        (void) IoHelper::renameItem(localPathA, localPathB, ioError);

        // Edit A/R/Q on local replica
        const SyncPath localPathQ = localPathB / "R" / "Q";
        testhelpers::generateOrEditTestFile(localPathQ);

        // Create A/S/X on local replica
        const SyncPath localPathX = localPathB / "S" / "X";
        testhelpers::generateOrEditTestFile(localPathX);

        // Delete A on remote replica
        deleteRemoteFile(_driveDbId, info.remoteNodeIdA);

        syncCount = _syncPal->syncCount();
        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForCurrentSyncToFinish(syncCount);

        syncCount++; // Previous sync only fixed the conflict.
        waitForCurrentSyncToFinish(syncCount);

        // Delete operation wins
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathA));
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathB));
        // ... but created and edited files has been rescued.
        CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / FileRescuer::rescueFolderName() / "Q"));
        CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / FileRescuer::rescueFolderName() / "X"));
    }
    {
        auto syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
        const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
        TestMoveDeleteInfo info;
        generateInitialMoveDeleteSituation(_driveDbId, tmpRemoteDir.id(), info);
        waitForCurrentSyncToFinish(syncCount);

        // Rename A to B on local replica
        const SyncPath localPathA = _syncPal->localPath() / tmpRemoteDir.name() / "A";
        const SyncPath localPathB = _syncPal->localPath() / tmpRemoteDir.name() / "B";
        IoError ioError = IoError::Unknown;
        (void) IoHelper::renameItem(localPathA, localPathB, ioError);

        // Move A/S to S on local replica
        const SyncPath originLocalPathS = localPathB / "S";
        const SyncPath destLocalPathS = _syncPal->localPath() / tmpRemoteDir.name() / "S";
        (void) IoHelper::moveItem(originLocalPathS, destLocalPathS, ioError);

        // Delete A on remote replica
        deleteRemoteFile(_driveDbId, info.remoteNodeIdA);

        syncCount = _syncPal->syncCount();
        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForCurrentSyncToFinish(syncCount);

        syncCount++; // Previous sync only fixed the conflict.
        waitForCurrentSyncToFinish(syncCount);

        // Delete operation wins
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathA));
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathB));
        CPPUNIT_ASSERT(!std::filesystem::exists(originLocalPathS));
        // In this test, 2 MoveDelete conflicts are generated :
        // - between local node B and remote node A
        // - between local node S nad remote node S
        // For conflicts with same priority level, it is always the conflict on the node closer to the root node that is processed
        // first. Therefore, solving the first conflict (propagating operation delete on local node B) will also resolve the
        // conflict on node S. However, S will be seen as new on local replica and re-created on remote replica.
        CPPUNIT_ASSERT(std::filesystem::exists(destLocalPathS));
    }
    {
        auto syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
        const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
        TestMoveDeleteInfo info;
        generateInitialMoveDeleteSituation(_driveDbId, tmpRemoteDir.id(), info);
        waitForCurrentSyncToFinish(syncCount);

        // Rename A to B on local replica
        const SyncPath localPathA = _syncPal->localPath() / tmpRemoteDir.name() / "A";
        const SyncPath localPathB = _syncPal->localPath() / tmpRemoteDir.name() / "B";
        IoError ioError = IoError::Unknown;
        (void) IoHelper::renameItem(localPathA, localPathB, ioError);

        // Move A/S to S on remote replica
        {
            MoveJob moveJob(nullptr, _driveDbId, "", info.remoteNodeIdS, tmpRemoteDir.id());
            moveJob.setBypassCheck(true);
            (void) moveJob.runSynchronously();
        }

        // Delete A on remote replica
        deleteRemoteFile(_driveDbId, info.remoteNodeIdA);

        syncCount = _syncPal->syncCount();
        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForCurrentSyncToFinish(syncCount);

        syncCount++; // Previous sync only fixed the conflict.
        waitForCurrentSyncToFinish(syncCount);

        // Delete operation wins.
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathA));
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathB));
        // But S has been moved outside deleted branch.
        CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / tmpRemoteDir.name() / ""));
    }
    {
        auto syncCount = waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
        const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
        TestMoveDeleteInfo info;
        generateInitialMoveDeleteSituation(_driveDbId, tmpRemoteDir.id(), info);
        waitForCurrentSyncToFinish(syncCount);

        // Rename A/R to A/X on local replica
        const SyncPath localPathR = _syncPal->localPath() / tmpRemoteDir.name() / "R";
        const SyncPath localPathX = _syncPal->localPath() / tmpRemoteDir.name() / "X";
        IoError ioError = IoError::Unknown;
        (void) IoHelper::renameItem(localPathR, localPathX, ioError);

        // Delete A on remote replica
        deleteRemoteFile(_driveDbId, info.remoteNodeIdA);

        syncCount = _syncPal->syncCount();
        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForCurrentSyncToFinish(syncCount);

        syncCount++; // Previous sync only fixed the conflict.
        waitForCurrentSyncToFinish(syncCount);

        // This is treated as a Move-ParentDelete conflict, the Move-Delete conflict is ignored.
        // Delete operation wins.
        CPPUNIT_ASSERT(!std::filesystem::exists(_syncPal->localPath() / tmpRemoteDir.name() / "A"));
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathR));
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathX));
    }
}

void TestIntegration::testMoveParentDeleteConflict() {}

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

void TestIntegration::waitForCurrentSyncToFinish(const uint64_t syncCount) const {
    const TimerUtility timer;
    LOG_DEBUG(_logger, "Start waiting for sync to finish: " << syncCount);

    static const auto timeOutDuration = seconds(30);
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
