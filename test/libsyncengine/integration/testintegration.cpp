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
#include "jobs/local/localcreatedirjob.h"
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
#include "syncpal/excludelistpropagator.h"
#include "test_utility/remotetemporarydirectory.h"
#include "update_detection/blacklist_changes_propagator/blacklistpropagator.h"

using namespace CppUnit;
using namespace std::chrono;

namespace KDC {

// Temporary test in drive "kDrive Desktop Team"
const NodeId testCiFolderId = "56850";

namespace {
NodeId createRemoteFile(const int driveDbId, const SyncName &name, const NodeId &remoteParentFileId,
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
    return job.nodeId();
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

void moveRemoteFile(const int driveDbId, const NodeId &remoteFileId, const NodeId &destinationRemoteParentId,
                    const SyncName name = {}) {
    MoveJob job(nullptr, driveDbId, {}, remoteFileId, destinationRemoteParentId, name);
    job.setBypassCheck(true);
    (void) job.runSynchronously();
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
    _remoteSyncDir.createDirectory(_driveDbId, testCiFolderId);
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
    _remoteSyncDir.deleteDirectory();

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
    waitForCurrentSyncToFinish();
    logStep("initialization");

    // Run test cases
    // basicTests();
    // inconsistencyTests();
    // conflictTests();
    // testBreakCycle();
    // testBlacklist();
    testExclusionTemplates();
}

void TestIntegration::basicTests() {
    testLocalChanges();
    testRemoteChanges();
    testSimultaneousChanges();
}

void TestIntegration::testLocalChanges() {
    // Generate create operations.
    waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));

    const SyncPath subDirPath = _syncPal->localPath() / "testSubDirLocal";
    (void) std::filesystem::create_directories(subDirPath);

    SyncPath filePath = _syncPal->localPath() / "testFileLocal";
    testhelpers::generateOrEditTestFile(filePath);

    FileStat fileStat;
    bool exists = false;
    IoHelper::getFileStat(filePath, &fileStat, exists);

    waitForCurrentSyncToFinish();

    auto remoteTestFileInfo = getRemoteFileInfoByName(_driveDbId, _remoteSyncDir.id(), filePath.filename());
    CPPUNIT_ASSERT(remoteTestFileInfo.isValid());

    const auto remoteTestDirInfo = getRemoteFileInfoByName(_driveDbId, _remoteSyncDir.id(), subDirPath.filename());
    CPPUNIT_ASSERT(remoteTestDirInfo.isValid());
    CPPUNIT_ASSERT_EQUAL(fileStat.size, remoteTestFileInfo.size);
    CPPUNIT_ASSERT_EQUAL(fileStat.modificationTime, remoteTestFileInfo.modificationTime);
    logStep("test create local file");

    // Generate an edit operation.
    waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    testhelpers::generateOrEditTestFile(filePath);

    IoHelper::getFileStat(filePath, &fileStat, exists);

    waitForCurrentSyncToFinish();

    const auto prevRemoteTestFileInfo = remoteTestFileInfo;
    remoteTestFileInfo = getRemoteFileInfoByName(_driveDbId, _remoteSyncDir.id(), filePath.filename());
    CPPUNIT_ASSERT_EQUAL(fileStat.modificationTime, remoteTestFileInfo.modificationTime);
    CPPUNIT_ASSERT_LESS(remoteTestFileInfo.modificationTime, prevRemoteTestFileInfo.modificationTime);
    CPPUNIT_ASSERT_EQUAL(fileStat.size, remoteTestFileInfo.size);
    CPPUNIT_ASSERT_LESS(remoteTestFileInfo.size, prevRemoteTestFileInfo.size);
    logStep("test edit local file");

    // Generate a move operation.
    waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    const SyncName newName = Str("testFileLocal_renamed");
    const std::filesystem::path destinationPath = subDirPath / newName;
    {
        LocalMoveJob job(filePath, destinationPath);
        (void) job.runSynchronously();
    }

    waitForCurrentSyncToFinish();

    remoteTestFileInfo = getRemoteFileInfoByName(_driveDbId, remoteTestDirInfo.id, newName);

    CPPUNIT_ASSERT(remoteTestFileInfo.isValid());
    CPPUNIT_ASSERT_EQUAL(remoteTestDirInfo.id, remoteTestFileInfo.parentId);

    filePath = destinationPath;
    logStep("test move local file");

    // Generate a delete operation.
    waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    {
        LocalDeleteJob deleteJob(subDirPath);
        (void) deleteJob.runSynchronously();
    }

    waitForCurrentSyncToFinish();

    remoteTestFileInfo = getRemoteFileInfoByName(_driveDbId, remoteTestDirInfo.id, filePath.filename());
    CPPUNIT_ASSERT(!remoteTestFileInfo.isValid());
    logStep("test delete local file");
}

void TestIntegration::testRemoteChanges() {
    // Generate create operations.
    waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    const SyncPath subDirPath = _syncPal->localPath() / "testSubDirRemote";
    NodeId subDirId;
    SyncPath filePath = _syncPal->localPath() / "testFileRemote";
    NodeId fileId;
    {
        CreateDirJob createDirJob(nullptr, _driveDbId, subDirPath, _remoteSyncDir.id(), subDirPath.filename());
        (void) createDirJob.runSynchronously();
        subDirId = createDirJob.nodeId();

        fileId = duplicateRemoteFile(_driveDbId, _testFileRemoteId, filePath.filename());
    }

    GetFileInfoJob fileInfoJob(_driveDbId, fileId);
    (void) fileInfoJob.runSynchronously();

    waitForCurrentSyncToFinish();

    CPPUNIT_ASSERT(std::filesystem::exists(subDirPath));
    CPPUNIT_ASSERT(std::filesystem::exists(filePath));

    FileStat fileStat;
    bool exists = false;
    IoHelper::getFileStat(filePath, &fileStat, exists);
    CPPUNIT_ASSERT_EQUAL(fileInfoJob.size(), fileStat.size);
    CPPUNIT_ASSERT_EQUAL(fileInfoJob.modificationTime(), fileStat.modificationTime);

    logStep("test create remote file");

    // Generate an edit operation.
    waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    SyncTime modificationTime = 0;
    int64_t size = 0;
    editRemoteFile(_driveDbId, fileId, nullptr, &modificationTime, &size);

    waitForCurrentSyncToFinish();

    FileStat filestat;
    IoError ioError = IoError::Unknown;
    (void) IoHelper::getFileStat(filePath, &filestat, ioError);
    CPPUNIT_ASSERT_EQUAL(modificationTime, filestat.modificationTime);
    CPPUNIT_ASSERT_EQUAL(size, filestat.size);
    logStep("test edit remote file");

    // Generate a move operation.
    waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    filePath = subDirPath / "testFileRemote_renamed";
    moveRemoteFile(_driveDbId, fileId, subDirId, filePath.filename());

    waitForCurrentSyncToFinish();

    CPPUNIT_ASSERT(std::filesystem::exists(filePath));
    logStep("test move remote file");

    // Generate a delete operation.
    waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    {
        DeleteJob job(_driveDbId, subDirId);
        job.setBypassCheck(true);
        (void) job.runSynchronously();
    }

    waitForCurrentSyncToFinish();

    CPPUNIT_ASSERT(!std::filesystem::exists(subDirPath));
    CPPUNIT_ASSERT(!std::filesystem::exists(filePath));
    logStep("test delete remote file");
}

void TestIntegration::testSimultaneousChanges() {
    waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));

    // Create a file on local replica.
    const SyncPath localFilePath = _syncPal->localPath() / "testSimultaneousChanges_local";
    testhelpers::generateOrEditTestFile(localFilePath);

    // Rename a file on remote replica.
    const SyncPath remoteFilePath = _syncPal->localPath() / "testSimultaneousChanges_remote";
    (void) RenameJob(nullptr, _driveDbId, _testFileRemoteId, remoteFilePath).runSynchronously();

    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForCurrentSyncToFinish();

    CPPUNIT_ASSERT(std::filesystem::exists(remoteFilePath));
    const auto remoteTestFileInfo = getRemoteFileInfoByName(_driveDbId, _remoteSyncDir.id(), localFilePath.filename());
    CPPUNIT_ASSERT(remoteTestFileInfo.isValid());
    logStep("testSimultaneousChanges");
}

void TestIntegration::inconsistencyTests() {
    // Duplicate remote files to set up the tests.
    waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    const auto testForbiddenCharsRemoteId = duplicateRemoteFile(_driveDbId, _testFileRemoteId, Str("testForbiddenChar"));
    const auto testNameClashRemoteId1 = duplicateRemoteFile(_driveDbId, _testFileRemoteId, Str("testNameClash"));
    const auto testNameClashRemoteId2 = duplicateRemoteFile(_driveDbId, _testFileRemoteId, Str("testnameclash1"));

    waitForCurrentSyncToFinish();

    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / "testForbiddenChar"));
    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / "testNameClash"));
    const SyncPath nameClashLocalPath = _syncPal->localPath() / "testnameclash1";
    CPPUNIT_ASSERT(std::filesystem::exists(nameClashLocalPath));

    // Rename files on remote side.
    waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    (void) RenameJob(nullptr, _driveDbId, testForbiddenCharsRemoteId, "test/:*ForbiddenChar").runSynchronously();
    (void) RenameJob(nullptr, _driveDbId, testNameClashRemoteId2, "testnameclash").runSynchronously();

    waitForCurrentSyncToFinish();

    CPPUNIT_ASSERT(!std::filesystem::exists(_syncPal->localPath() / "testForbiddenChar"));
    CPPUNIT_ASSERT(!std::filesystem::exists(_syncPal->localPath() / "test/:*ForbiddenChar"));
    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / "testNameClash"));
    CPPUNIT_ASSERT(std::filesystem::exists(nameClashLocalPath));

    // Edit local name clash file.
    waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    testhelpers::generateOrEditTestFile(nameClashLocalPath);
    FileStat filestat;
    IoError ioError = IoError::Unknown;
    (void) IoHelper::getFileStat(nameClashLocalPath, &filestat, ioError);

    waitForCurrentSyncToFinish();

    auto remoteFileInfo = getRemoteFileInfoByName(_driveDbId, _remoteSyncDir.id(), Str("testnameclash"));
    CPPUNIT_ASSERT(remoteFileInfo.isValid());
    CPPUNIT_ASSERT_LESS(filestat.size, remoteFileInfo.size); // The local edit is not propagated.

    // Rename again the remote file to avoid the name clash.
    waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    (void) RenameJob(nullptr, _driveDbId, testNameClashRemoteId2, "testnameclash2").runSynchronously();

    waitForCurrentSyncToFinish();

    // Needed because when an item has remote move and local edit operations at the same time, the move operation is processed
    // first. Then, the edit operation could not be propagated since the item path has changed. The sync is therefore restarted
    // and a new edit operation, with the new path, is generated.
    waitForCurrentSyncToFinish();

    (void) IoHelper::getFileStat(_syncPal->localPath() / "testnameclash2", &filestat, ioError);
    remoteFileInfo = getRemoteFileInfoByName(_driveDbId, _remoteSyncDir.id(), Str("testnameclash2"));

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
    testCreateParentDeleteConflict();
    testMoveMoveSourcePseudoConflict();
    testMoveMoveSourceConflict();
    testMoveMoveDestConflict();
    testMoveMoveCycleConflict();
}

void TestIntegration::testCreateCreatePseudoConflict() {
    waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));

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
    waitForCurrentSyncToFinish();

    // Check that both remote and local ID has been re-inserted in DB.
    SyncName name;
    CPPUNIT_ASSERT(_syncPal->syncDb()->name(ReplicaSide::Remote, _testFileRemoteId, name, found) && found);
    CPPUNIT_ASSERT(_syncPal->syncDb()->name(ReplicaSide::Local, localId, name, found) && found);
    logStep("testCreateCreatePseudoConflict");
}

void TestIntegration::testCreateCreateConflict() {
    waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));

    // Create a file on remote replica.
    const auto remoteId = duplicateRemoteFile(_driveDbId, _testFileRemoteId, Str("testCreateCreatePseudoConflict"));

    // Create a file on local replica.
    const SyncPath localFilePath = _syncPal->localPath() / "testCreateCreatePseudoConflict";
    testhelpers::generateOrEditTestFile(localFilePath);

    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForCurrentSyncToFinish();

    const auto conflictedFilePath =
            findLocalFileByNamePrefix(_localSyncDir.path(), Str("testCreateCreatePseudoConflict_conflict_"));
    CPPUNIT_ASSERT(std::filesystem::exists(conflictedFilePath));
    CPPUNIT_ASSERT(!std::filesystem::exists(localFilePath));

    waitForCurrentSyncToFinish(); // Previous sync only fixed the conflict.

    CPPUNIT_ASSERT(std::filesystem::exists(conflictedFilePath));
    CPPUNIT_ASSERT(std::filesystem::exists(localFilePath));
    logStep("testCreateCreateConflict");
}

void TestIntegration::testEditEditPseudoConflict() {
    waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));

    // Change the last modification date in DB.
    bool found = false;
    DbNode dbNode;
    CPPUNIT_ASSERT(_syncPal->syncDb()->node(ReplicaSide::Remote, _testFileRemoteId, dbNode, found) && found);
    const auto newLastModified = *dbNode.lastModifiedLocal() - 1000;
    dbNode.setLastModifiedLocal(newLastModified);
    dbNode.setLastModifiedRemote(newLastModified);
    CPPUNIT_ASSERT(_syncPal->syncDb()->updateNode(dbNode, found) && found);

    _syncPal->forceInvalidateSnapshots();
    waitForCurrentSyncToFinish();

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
    logStep("testEditEditPseudoConflict");
}

void TestIntegration::testEditEditConflict() {
    waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));

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
    waitForCurrentSyncToFinish();

    const auto conflictedFilePath =
            findLocalFileByNamePrefix(_localSyncDir.path(), absoluteLocalPath.filename().native() + Str("_conflict_"));
    CPPUNIT_ASSERT(std::filesystem::exists(conflictedFilePath));
    CPPUNIT_ASSERT(!std::filesystem::exists(absoluteLocalPath));

    waitForCurrentSyncToFinish(); // Previous sync only fixed the conflict.

    CPPUNIT_ASSERT(std::filesystem::exists(conflictedFilePath));
    CPPUNIT_ASSERT(std::filesystem::exists(absoluteLocalPath));
    logStep("testEditEditConflict");
}

void TestIntegration::testMoveCreateConflict() {
    const SyncPath filename = "testMoveCreateConflict1";
    const SyncPath localFilePath = _syncPal->localPath() / filename;

    // 1 - if remote file has been moved, rename local file
    {
        waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));

        // Create a file a local replica.
        testhelpers::generateOrEditTestFile(localFilePath);

        // Rename remote file.
        (void) RenameJob(nullptr, _driveDbId, _testFileRemoteId, filename).runSynchronously();

        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForCurrentSyncToFinish();

        // The local file has been excluded from sync.
        const auto conflictedFilePath = findLocalFileByNamePrefix(_localSyncDir.path(), filename.native() + Str("_conflict_"));
        CPPUNIT_ASSERT(std::filesystem::exists(conflictedFilePath));
        CPPUNIT_ASSERT(!std::filesystem::exists(localFilePath));

        waitForCurrentSyncToFinish(); // Previous sync only fixed the conflict.

        // The remote rename operation has been propagated.
        CPPUNIT_ASSERT(std::filesystem::exists(conflictedFilePath));
        CPPUNIT_ASSERT(std::filesystem::exists(localFilePath));
        logStep("testMoveCreateConflict1");
    }

    // 2 - if local file has been moved, undo move operation.
    {
        waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));

        // Rename a file on local replica.
        const SyncName newFilename = Str("testMoveCreateConflict2");
        const SyncPath newLocalFilePath = _syncPal->localPath() / newFilename;
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::renameItem(localFilePath, newLocalFilePath, ioError) && ioError == IoError::Success);

        // Create a file on remote replica.
        (void) duplicateRemoteFile(_driveDbId, _testFileRemoteId, newFilename);

        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForCurrentSyncToFinish();

        // The local file has been put back in its origin location.
        CPPUNIT_ASSERT(std::filesystem::exists(localFilePath));
        CPPUNIT_ASSERT(!std::filesystem::exists(newLocalFilePath));

        waitForCurrentSyncToFinish(); // Previous sync only fixed the conflict.

        // The remote creation has been propagated.
        CPPUNIT_ASSERT(std::filesystem::exists(localFilePath));
        CPPUNIT_ASSERT(std::filesystem::exists(newLocalFilePath));
        logStep("testMoveCreateConflict2");
    }
}

void TestIntegration::testEditDeleteConflict() {
    // Delete happen on the edited file.
    {
        waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));

        // Generate the test file
        const SyncName filename = Str("testEditDeleteConflict1");
        const auto tmpNodeId = duplicateRemoteFile(_driveDbId, _testFileRemoteId, filename);
        waitForCurrentSyncToFinish();

        // Edit the file on remote replica.
        SyncTime modificationTime = 0;
        int64_t size = 0;
        editRemoteFile(_driveDbId, tmpNodeId, nullptr, &modificationTime, &size);

        // Delete the file on local replica.
        const SyncPath filepath = _syncPal->localPath() / filename;
        IoError ioError = IoError::Unknown;
        (void) IoHelper::deleteItem(filepath, ioError);
        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForCurrentSyncToFinish();
        waitForCurrentSyncToFinish(); // Previous sync only fixed the conflict.

        // Edit operation has won.
        CPPUNIT_ASSERT(std::filesystem::exists(filepath));
        FileStat fileStat;
        (void) IoHelper::getFileStat(filepath, &fileStat, ioError);
        CPPUNIT_ASSERT_EQUAL(modificationTime, fileStat.modificationTime);
        CPPUNIT_ASSERT_EQUAL(size, fileStat.size);
        logStep("testEditDeleteConflict1");
    }

    // Delete happen on a parent of the edited files.
    {
        waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));

        // Generate the test file
        const SyncPath dirpath = _syncPal->localPath() / "testEditDeleteConflict2_dir";
        IoError ioError = IoError::Unknown;
        (void) IoHelper::createDirectory(dirpath, false, ioError);
        const SyncPath filepath = dirpath / "testEditDeleteConflict2_file";
        testhelpers::generateOrEditTestFile(filepath);
        waitForCurrentSyncToFinish();

        std::optional<NodeId> remoteDirNodeId = std::nullopt;
        bool found = false;
        CPPUNIT_ASSERT(_syncPal->syncDb()->id(ReplicaSide::Remote, dirpath.filename(), remoteDirNodeId, found) && found);

        // Edit the file on local replica.
        testhelpers::generateOrEditTestFile(filepath);

        // Delete the file on remote.
        DeleteJob deleteJob(_driveDbId, *remoteDirNodeId);
        deleteJob.setBypassCheck(true);
        (void) deleteJob.runSynchronously();

        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForCurrentSyncToFinish();
        waitForCurrentSyncToFinish(); // Previous sync only fixed the conflict.

        // Delete operation has won...
        CPPUNIT_ASSERT(!std::filesystem::exists(filepath));
        // ... but edited file has been rescued.
        CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / FileRescuer::rescueFolderName() / filepath.filename()));
        logStep("testEditDeleteConflict2");
    }
}

namespace {
struct RemoteNodeInfo {
        NodeId remoteNodeIdA;
        NodeId remoteNodeIdAA;
        NodeId remoteNodeIdAB;
};

void generateInitialTestSituation(const int driveDbId, const NodeId &parentId, RemoteNodeInfo &info) {
    // .
    // `-- A
    //     |-- AA
    //     |   `-- AAA
    //     `-- AB
    {
        CreateDirJob jobA(nullptr, driveDbId, parentId, Str("A"));
        (void) jobA.runSynchronously();
        info.remoteNodeIdA = jobA.nodeId();
    }
    {
        CreateDirJob jobAA(nullptr, driveDbId, info.remoteNodeIdA, Str("AA"));
        (void) jobAA.runSynchronously();
        info.remoteNodeIdAA = jobAA.nodeId();
    }
    {
        CreateDirJob jobAB(nullptr, driveDbId, info.remoteNodeIdA, Str("AB"));
        (void) jobAB.runSynchronously();
        info.remoteNodeIdAB = jobAB.nodeId();
    }
    (void) createRemoteFile(driveDbId, Str("AAA"), info.remoteNodeIdAA);
}
} // namespace

void TestIntegration::testMoveDeleteConflict() {
    {
        waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
        const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
        RemoteNodeInfo info;
        generateInitialTestSituation(_driveDbId, tmpRemoteDir.id(), info);
        waitForCurrentSyncToFinish();

        // Rename A to B on local replica
        const SyncPath localPathA = _syncPal->localPath() / tmpRemoteDir.name() / "A";
        const SyncPath localPathB = _syncPal->localPath() / tmpRemoteDir.name() / "B";
        IoError ioError = IoError::Unknown;
        (void) IoHelper::renameItem(localPathA, localPathB, ioError);

        // Delete A/AB on local replica
        const SyncPath localPathAB = localPathB / "AB";
        (void) IoHelper::deleteItem(localPathAB, ioError);

        // Delete A on remote replica
        deleteRemoteFile(_driveDbId, info.remoteNodeIdA);

        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForCurrentSyncToFinish();
        waitForCurrentSyncToFinish(); // Previous sync only fixed the conflict.

        // Delete operation wins
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathA));
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathB));
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathAB));
        logStep("testMoveDeleteConflict1");
    }
    {
        waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
        const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
        RemoteNodeInfo info;
        generateInitialTestSituation(_driveDbId, tmpRemoteDir.id(), info);
        waitForCurrentSyncToFinish();

        // Rename A to B on local replica
        const SyncPath localPathA = _syncPal->localPath() / tmpRemoteDir.name() / "A";
        const SyncPath localPathB = _syncPal->localPath() / tmpRemoteDir.name() / "B";
        IoError ioError = IoError::Unknown;
        (void) IoHelper::renameItem(localPathA, localPathB, ioError);

        // Edit A/AA/AAA on local replica
        const SyncPath localPathAAA = localPathB / "AA" / "AAA";
        testhelpers::generateOrEditTestFile(localPathAAA);

        // Create A/AB/ABA on local replica
        const SyncPath localPathABA = localPathB / "AB" / "ABA";
        testhelpers::generateOrEditTestFile(localPathABA);

        // Delete A on remote replica
        deleteRemoteFile(_driveDbId, info.remoteNodeIdA);

        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForCurrentSyncToFinish();
        waitForCurrentSyncToFinish(); // Previous sync only fixed the conflict.

        // Delete operation wins
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathA));
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathB));
        // ... but created and edited files has been rescued.
        CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / FileRescuer::rescueFolderName() / "AAA"));
        CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / FileRescuer::rescueFolderName() / "ABA"));
        logStep("testMoveDeleteConflict2");
    }
    {
        waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
        const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
        RemoteNodeInfo info;
        generateInitialTestSituation(_driveDbId, tmpRemoteDir.id(), info);
        waitForCurrentSyncToFinish();

        // Rename A to B on local replica
        const SyncPath localPathA = _syncPal->localPath() / tmpRemoteDir.name() / "A";
        const SyncPath localPathB = _syncPal->localPath() / tmpRemoteDir.name() / "B";
        IoError ioError = IoError::Unknown;
        (void) IoHelper::renameItem(localPathA, localPathB, ioError);

        // Move A/AB to AB on local replica
        const SyncPath originLocalPathAB = localPathB / "AB";
        const SyncPath destLocalPathAB = _syncPal->localPath() / tmpRemoteDir.name() / "AB";
        (void) IoHelper::moveItem(originLocalPathAB, destLocalPathAB, ioError);

        // Delete A on remote replica
        deleteRemoteFile(_driveDbId, info.remoteNodeIdA);

        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForCurrentSyncToFinish();
        waitForCurrentSyncToFinish(); // Previous sync only fixed the conflict.

        // Delete operation wins
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathA));
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathB));
        CPPUNIT_ASSERT(!std::filesystem::exists(originLocalPathAB));
        logStep("testMoveDeleteConflict3");
    }
    {
        waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
        const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
        RemoteNodeInfo info;
        generateInitialTestSituation(_driveDbId, tmpRemoteDir.id(), info);
        waitForCurrentSyncToFinish();

        // Rename A to B on local replica
        const SyncPath localPathA = _syncPal->localPath() / tmpRemoteDir.name() / "A";
        const SyncPath localPathB = _syncPal->localPath() / tmpRemoteDir.name() / "B";
        IoError ioError = IoError::Unknown;
        (void) IoHelper::renameItem(localPathA, localPathB, ioError);

        // Move A/AB to AB on remote replica
        moveRemoteFile(_driveDbId, info.remoteNodeIdAB, tmpRemoteDir.id());

        // Delete A on remote replica
        deleteRemoteFile(_driveDbId, info.remoteNodeIdA);

        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForCurrentSyncToFinish();
        waitForCurrentSyncToFinish(); // Previous sync only fixed the conflict.

        // Delete operation wins.
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathA));
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathB));
        // But AB has been moved outside deleted branch.
        CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / tmpRemoteDir.name() / ""));
        logStep("testMoveDeleteConflict4");
    }
    {
        waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
        const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
        RemoteNodeInfo info;
        generateInitialTestSituation(_driveDbId, tmpRemoteDir.id(), info);
        waitForCurrentSyncToFinish();

        // Rename A/AB to A/AB2 on local replica
        const SyncPath localPathAB = _syncPal->localPath() / tmpRemoteDir.name() / "AB";
        const SyncPath localPathAB2 = _syncPal->localPath() / tmpRemoteDir.name() / "AB2";
        IoError ioError = IoError::Unknown;
        (void) IoHelper::renameItem(localPathAB, localPathAB2, ioError);

        // Delete A on remote replica
        deleteRemoteFile(_driveDbId, info.remoteNodeIdA);

        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForCurrentSyncToFinish();
        waitForCurrentSyncToFinish(); // Previous sync only fixed the conflict.

        // This is treated as a Move-ParentDelete conflict, the Move-Delete conflict is ignored.
        // Delete operation wins.
        CPPUNIT_ASSERT(!std::filesystem::exists(_syncPal->localPath() / tmpRemoteDir.name() / "A"));
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathAB));
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathAB2));
        logStep("testMoveDeleteConflict5");
    }
}

void TestIntegration::testMoveParentDeleteConflict() {
    {
        waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
        const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
        RemoteNodeInfo info;
        generateInitialTestSituation(_driveDbId, tmpRemoteDir.id(), info);
        waitForCurrentSyncToFinish();

        // Move A/AA/AAA to A/AB/AAA on local replica
        const SyncPath localPathA = _syncPal->localPath() / tmpRemoteDir.name() / "A";
        const SyncPath originPathAAA = localPathA / "AA" / "AAA";
        const SyncPath destinationPathAAA = localPathA / "AB" / "AAA";
        IoError ioError = IoError::Unknown;
        (void) IoHelper::moveItem(originPathAAA, destinationPathAAA, ioError);

        // Delete A on remote replica
        deleteRemoteFile(_driveDbId, info.remoteNodeIdA);

        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForCurrentSyncToFinish();
        waitForCurrentSyncToFinish(); // Previous sync only fixed the conflict.

        // Delete operation wins
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathA));
        logStep("testMoveParentDeleteConflict1");
    }
    {
        waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
        const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
        RemoteNodeInfo info;
        generateInitialTestSituation(_driveDbId, tmpRemoteDir.id(), info);
        waitForCurrentSyncToFinish();

        // Move A/AA to A/AB/AA on local replica
        const SyncPath localPathA = _syncPal->localPath() / tmpRemoteDir.name() / "A";
        const SyncPath originPathAA = localPathA / "AA";
        const SyncPath destinationPathAA = localPathA / "AB" / "AA";
        IoError ioError = IoError::Unknown;
        (void) IoHelper::moveItem(originPathAA, destinationPathAA, ioError);

        // Rename A to A2 on remote replica
        (void) RenameJob(nullptr, _driveDbId, info.remoteNodeIdA, Str("A2")).runSynchronously();

        // Delete A/AB on remote replica
        deleteRemoteFile(_driveDbId, info.remoteNodeIdAB);

        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForCurrentSyncToFinish();
        waitForCurrentSyncToFinish(); // Previous sync only fixed the conflict.

        // Delete operation wins
        CPPUNIT_ASSERT(!std::filesystem::exists(originPathAA));
        CPPUNIT_ASSERT(!std::filesystem::exists(destinationPathAA));
        CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / tmpRemoteDir.name() / "A2"));
        logStep("testMoveParentDeleteConflict2");
    }
    {
        // Set up a more complex tree
        // .
        // └── A
        //     ├── AA
        //     │   └── AAA
        //     └── AB
        //         ├── ABA
        //         └── ABB
        waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
        const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
        RemoteNodeInfo info;
        generateInitialTestSituation(_driveDbId, tmpRemoteDir.id(), info);
        (void) CreateDirJob(nullptr, _driveDbId, info.remoteNodeIdAB, Str("ABA")).runSynchronously();
        (void) createRemoteFile(_driveDbId, Str("ABB"), info.remoteNodeIdAB);
        waitForCurrentSyncToFinish();

        // Edit A/AB/ABB on local replica
        const SyncPath localPathA = _syncPal->localPath() / tmpRemoteDir.name() / "A";
        const SyncPath localPathABB = localPathA / "AB" / "ABB";
        testhelpers::generateOrEditTestFile(localPathABB);

        // Edit A/AA/AAA on local replica
        const SyncPath localPathAAA = localPathA / "AA" / "AAA";
        testhelpers::generateOrEditTestFile(localPathAAA);

        // Move A/AA/AAA to A/AB/AAA on local replica
        const SyncPath localDestinationPathAAA = localPathA / "AB" / "AAA";
        IoError ioError = IoError::Unknown;
        (void) IoHelper::moveItem(localPathAAA, localDestinationPathAAA, ioError);

        // Delete A/AB on remote replica
        deleteRemoteFile(_driveDbId, info.remoteNodeIdAB);

        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForCurrentSyncToFinish();
        waitForCurrentSyncToFinish(); // Previous sync only fixed the conflict.

        // Delete operation wins...
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathA / "AB"));
        // ...but conflicts EditDelete and MoveDelete should be handled first and edited files should
        // be rescued
        CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / FileRescuer::rescueFolderName() / "AAA"));
        CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / FileRescuer::rescueFolderName() / "ABB"));
        logStep("testMoveParentDeleteConflict3");
    }
}

void TestIntegration::testCreateParentDeleteConflict() {
    waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
    RemoteNodeInfo info;
    generateInitialTestSituation(_driveDbId, tmpRemoteDir.id(), info);
    waitForCurrentSyncToFinish();

    // Create A/AB/ABA on local replica
    const SyncPath localPathA = _syncPal->localPath() / tmpRemoteDir.name() / "A";
    const SyncPath localPathABA = localPathA / "AB" / "ABA";
    testhelpers::generateOrEditTestFile(localPathABA);

    // Delete AB on remote replica
    deleteRemoteFile(_driveDbId, info.remoteNodeIdAB);

    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForCurrentSyncToFinish();
    waitForCurrentSyncToFinish(); // Previous sync only fixed the conflict.

    // Delete operation wins...
    CPPUNIT_ASSERT(!std::filesystem::exists(localPathA / "AB"));
    // ...but created files should be rescued
    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / FileRescuer::rescueFolderName() / "ABA"));
    logStep("testCreateParentDeleteConflict");
}

void TestIntegration::testMoveMoveSourcePseudoConflict() {
    waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
    const SyncPath originLocalPath = _syncPal->localPath() / "testMoveMoveSourcePseudoConflict";
    const auto remoteFileNodeId = createRemoteFile(_driveDbId, originLocalPath.filename().native(), _remoteSyncDir.id());
    waitForCurrentSyncToFinish();

    // Move test file in subdirectory on local replica
    const SyncPath destinationLocalPath = _syncPal->localPath() / tmpRemoteDir.name() / originLocalPath.filename();
    IoError ioError = IoError::Unknown;
    (void) IoHelper::moveItem(originLocalPath, destinationLocalPath, ioError);

    // Move test file in subdirectory on remote replica
    moveRemoteFile(_driveDbId, remoteFileNodeId, tmpRemoteDir.id());

    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForCurrentSyncToFinish();

    // No conflict, change only propagated in DB.
    CPPUNIT_ASSERT(std::filesystem::exists(destinationLocalPath));
    logStep("testMoveMoveSourcePseudoConflict");
}

void TestIntegration::testMoveMoveSourceConflict() {
    waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    const RemoteTemporaryDirectory tmpRemoteDir1(_driveDbId, _remoteSyncDir.id());
    const RemoteTemporaryDirectory tmpRemoteDir2(_driveDbId, _remoteSyncDir.id());
    const SyncPath originLocalPath = _syncPal->localPath() / "testMoveMoveSourceConflict";
    const auto remoteFileNodeId = createRemoteFile(_driveDbId, originLocalPath.filename().native(), _remoteSyncDir.id());
    waitForCurrentSyncToFinish();

    // Move test file in subdirectory1 on local replica
    const SyncPath destinationLocalPath = _syncPal->localPath() / tmpRemoteDir1.name() / originLocalPath.filename();
    IoError ioError = IoError::Unknown;
    (void) IoHelper::moveItem(originLocalPath, destinationLocalPath, ioError);

    // Move test file in subdirectory2 on remote replica
    moveRemoteFile(_driveDbId, remoteFileNodeId, tmpRemoteDir2.id());

    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForCurrentSyncToFinish();
    waitForCurrentSyncToFinish(); // Previous sync only fixed the conflict.

    // Move operation has been undo on local replica then remote operation has been propagated.
    CPPUNIT_ASSERT(!std::filesystem::exists(destinationLocalPath));
    const SyncPath destinationRemotePath = _syncPal->localPath() / tmpRemoteDir2.name() / originLocalPath.filename();
    CPPUNIT_ASSERT(std::filesystem::exists(destinationRemotePath));
    logStep("testMoveMoveSourceConflict");
}

void TestIntegration::testMoveMoveDestConflict() {
    waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    const auto remoteId1 = duplicateRemoteFile(_driveDbId, _testFileRemoteId, Str("testMoveMoveDestConflict1"));
    const auto remoteId2 = duplicateRemoteFile(_driveDbId, _testFileRemoteId, Str("testMoveMoveDestConflict2"));
    waitForCurrentSyncToFinish();

    // Rename test file 1 on local replica
    const SyncPath originLocalPath = _syncPal->localPath() / "testMoveMoveDestConflict1";
    const SyncPath destinationLocalPath = _syncPal->localPath() / "testMoveMoveDestConflict";
    IoError ioError = IoError::Unknown;
    (void) IoHelper::renameItem(originLocalPath, destinationLocalPath, ioError);

    // Rename test file 2 on remote replica
    (void) RenameJob(nullptr, _driveDbId, remoteId2, "testMoveMoveDestConflict").runSynchronously();

    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForCurrentSyncToFinish();
    waitForCurrentSyncToFinish(); // Previous sync only fixed the conflict.

    // Move operation has been undo on local replica then remote operation has been propagated.
    CPPUNIT_ASSERT(std::filesystem::exists(originLocalPath));
    CPPUNIT_ASSERT(std::filesystem::exists(destinationLocalPath));
    logStep("testMoveMoveDestConflict");
}

void TestIntegration::testMoveMoveCycleConflict() {
    waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    const RemoteTemporaryDirectory tmpRemoteDirA(_driveDbId, _remoteSyncDir.id());
    const RemoteTemporaryDirectory tmpRemoteDirB(_driveDbId, _remoteSyncDir.id());
    waitForCurrentSyncToFinish();

    // Move A to B/A on local replica
    const SyncPath originLocalPathA = _syncPal->localPath() / tmpRemoteDirA.name();
    const SyncPath destinationLocalPathA = _syncPal->localPath() / tmpRemoteDirB.name() / tmpRemoteDirA.name();
    IoError ioError = IoError::Unknown;
    (void) IoHelper::moveItem(originLocalPathA, destinationLocalPathA, ioError);

    // Move B to A/B on remote replica
    moveRemoteFile(_driveDbId, tmpRemoteDirB.id(), tmpRemoteDirA.id());

    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForCurrentSyncToFinish();
    waitForCurrentSyncToFinish(); // Previous sync only fixed the conflict.

    // Move operation has been undo on local replica then remote operation has been propagated.
    CPPUNIT_ASSERT(!std::filesystem::exists(destinationLocalPathA));
    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / tmpRemoteDirA.name() / tmpRemoteDirB.name()));
    logStep("testMoveMoveCycleConflict");
}

void TestIntegration::testBreakCycle() {
    waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
    NodeId nodeIdAA;
    NodeId nodeIdAAA;
    {
        CreateDirJob jobA(nullptr, _driveDbId, tmpRemoteDir.id(), Str("A"));
        (void) jobA.runSynchronously();
        CreateDirJob jobAA(nullptr, _driveDbId, jobA.nodeId(), Str("AA"));
        (void) jobAA.runSynchronously();
        nodeIdAA = jobAA.nodeId();
        CreateDirJob jobAAA(nullptr, _driveDbId, nodeIdAA, Str("AAA"));
        (void) jobAAA.runSynchronously();
        nodeIdAAA = jobAAA.nodeId();
    }
    waitForCurrentSyncToFinish();

    const auto pathAA = _syncPal->localPath() / tmpRemoteDir.name() / "A" / "AA";
    // Rename A/AA/AAA to A/AA/AAA* on remote replica.
    (void) RenameJob(nullptr, _driveDbId, nodeIdAAA, Str("AAA*")).runSynchronously();
    // Create A/AA/AAA on remote replica.
    CreateDirJob dirJob(nullptr, _driveDbId, nodeIdAA, Str("AAA"));
    (void) dirJob.runSynchronously();
    // Move A/AA/AAA* to A/AA/AAA/AAA* on remote replica.
    moveRemoteFile(_driveDbId, nodeIdAAA, dirJob.nodeId());

    waitForCurrentSyncToFinish();
    waitForCurrentSyncToFinish(); // Previous sync only fixed the conflict the cycle.

    CPPUNIT_ASSERT(std::filesystem::exists(pathAA / "AAA" / "AAA*"));
    logStep("testBreakCycle");
}

void TestIntegration::testBlacklist() {
    waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
    const auto filename = "testBlacklist";
    const auto fileId = duplicateRemoteFile(_driveDbId, _testFileRemoteId, filename);
    waitForCurrentSyncToFinish();

    const auto dirpath = _syncPal->localPath() / tmpRemoteDir.name();
    CPPUNIT_ASSERT(std::filesystem::exists(dirpath));

    // Add directory to blacklist.
    (void) SyncNodeCache::instance()->update(_syncPal->syncDbId(), SyncNodeType::BlackList, {tmpRemoteDir.id()});
    // Apply new blacklist.
    _syncPal->stop();
    (void) BlacklistPropagator(_syncPal).runSynchronously();
    _syncPal->start();

    waitForCurrentSyncToFinish();

    CPPUNIT_ASSERT(!std::filesystem::exists(dirpath));

    // Move a file inside a blacklisted directory.
    moveRemoteFile(_driveDbId, fileId, tmpRemoteDir.id());

    waitForCurrentSyncToFinish();

    CPPUNIT_ASSERT(!std::filesystem::exists(dirpath / filename));

    // Move a file from inside a blacklisted directory to a synchronized directory.
    moveRemoteFile(_driveDbId, fileId, _remoteSyncDir.id());

    waitForCurrentSyncToFinish();

    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / filename));

    // Remove directory from blacklist.
    (void) SyncNodeCache::instance()->update(_syncPal->syncDbId(), SyncNodeType::BlackList, {});
    // Apply new blacklist.
    _syncPal->stop();
    (void) BlacklistPropagator(_syncPal).runSynchronously();
    _syncPal->start();

    waitForCurrentSyncToFinish();

    CPPUNIT_ASSERT(std::filesystem::exists(dirpath));
    logStep("testBlacklist");
}

void TestIntegration::testExclusionTemplates() {
    waitForSyncToBeIdle(SourceLocation::currentLoc(), milliseconds(500));
    const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
    FileStat filestat;
    bool found = false;
    IoHelper::getFileStat(_syncPal->localPath() / tmpRemoteDir.name(), &filestat, found);
    const auto dirLocalId = std::to_string(filestat.inode);

    const auto filename = "testExclusionTemplates";
    const SyncPath testName = "to_be_excluded";
    const auto fileRemoteId = duplicateRemoteFile(_driveDbId, _testFileRemoteId, testName);
    waitForCurrentSyncToFinish();

    const auto excludedFilePath = _syncPal->localPath() / testName;
    IoHelper::getFileStat(excludedFilePath, &filestat, found);
    auto fileLocalId = std::to_string(filestat.inode);
    CPPUNIT_ASSERT(std::filesystem::exists(excludedFilePath));

    // Add the exclusion template.
    auto templateList = ExclusionTemplateCache::instance()->exclusionTemplates(false);
    (void) templateList.emplace_back("*" + testName.filename().string() + "*");
    (void) ExclusionTemplateCache::instance()->update(false, templateList);
    _syncPal->stop();
    (void) ExcludeListPropagator(_syncPal).runSynchronously();
    _syncPal->start();
    waitForCurrentSyncToFinish();

    CPPUNIT_ASSERT(std::filesystem::exists(excludedFilePath)); // We do not remove the local file.
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Local).exists(fileLocalId));
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Remote).exists(fileRemoteId));

    // Rename remote file so the exclusion templates does not apply anymore.
    (void) RenameJob(nullptr, _driveDbId, fileRemoteId, filename).runSynchronously();
    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForCurrentSyncToFinish();
    // The remote file is synchronized again...
    CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Remote).exists(fileRemoteId));
    // ... but the local file is still excluded. A new file is therefore downloaded.
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Local).exists(fileLocalId));
    IoHelper::getFileStat(_syncPal->localPath() / filename, &filestat, found);
    fileLocalId = std::to_string(filestat.inode);
    CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Local).exists(fileLocalId));

    // Rename remote directory so the exclusion templates applies.
    (void) RenameJob(nullptr, _driveDbId, tmpRemoteDir.id(), testName).runSynchronously();
    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForCurrentSyncToFinish();
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Local).exists(dirLocalId));
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Remote).exists(tmpRemoteDir.id()));
    CPPUNIT_ASSERT(
            !std::filesystem::exists(_syncPal->localPath() / tmpRemoteDir.name())); // The local directory has been deleted.

    // Move a file inside an excluded directory from remote replica.
    moveRemoteFile(_driveDbId, fileRemoteId, tmpRemoteDir.id());
    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForCurrentSyncToFinish();
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Local).exists(fileLocalId));
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Remote).exists(fileRemoteId));

    // Move a file away from an excluded directory from remote replica.
    moveRemoteFile(_driveDbId, fileRemoteId, _remoteSyncDir.id());
    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForCurrentSyncToFinish();
    CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Remote).exists(fileRemoteId));
    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / filename));
    // Local file ID has changed again.
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Local).exists(fileLocalId));
    IoHelper::getFileStat(_syncPal->localPath() / filename, &filestat, found);
    fileLocalId = std::to_string(filestat.inode);
    CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Local).exists(fileLocalId));

    // Rename remote directory so the exclusion templates does not apply anymore.
    (void) RenameJob(nullptr, _driveDbId, tmpRemoteDir.id(), tmpRemoteDir.name()).runSynchronously();
    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForCurrentSyncToFinish();
    CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Remote).exists(tmpRemoteDir.id()));
    // Local dir ID has changed.
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Local).exists(dirLocalId));

    // Rename remote file so the exclusion templates applies.
    (void) RenameJob(nullptr, _driveDbId, fileRemoteId, testName).runSynchronously();
    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForCurrentSyncToFinish();
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Local).exists(fileLocalId));
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Remote).exists(fileRemoteId));
    CPPUNIT_ASSERT(!std::filesystem::exists(_syncPal->localPath() / filename)); // The local file has been deleted.

    // Remove the exclusion template.
    (void) templateList.pop_back();
    (void) ExclusionTemplateCache::instance()->update(false, templateList);
    _syncPal->stop();
    (void) ExcludeListPropagator(_syncPal).runSynchronously();
    _syncPal->start();
    waitForCurrentSyncToFinish();
    CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Remote).exists(fileRemoteId));
    // Local file ID has changed again.
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Local).exists(fileLocalId));
    IoHelper::getFileStat(_syncPal->localPath() / testName, &filestat, found);
    fileLocalId = std::to_string(filestat.inode);
    CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Local).exists(fileLocalId));
    logStep("testExclusionTemplates");
}

#ifdef __unix__
const NodeId commonDocumentsNodeId = "3";

void TestIntegration::testNodeIdReuseFile2DirAndDir2File() {
    if (!testhelpers::isExtendedTest()) return;

    LOGW_DEBUG(_logger, L"$$$$$ testNodeIdReuseFile2DirAndDir2File");

    SyncNodeCache::instance()->update(_driveDbId, SyncNodeType::BlackList,
                                      {commonDocumentsNodeId}); // Exclude common documents folder
    const RemoteTemporaryDirectory remoteTempDir(_driveDbId, "1", "testNodeIdReuseFile2DirAndDir2File");
    const SyncPath relativeWorkingDirPath = remoteTempDir.name();
    const SyncPath absoluteLocalWorkingDir = _syncPal->localPath() / relativeWorkingDirPath;
    _syncPal->start();
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    const auto localSnapshot = _syncPal->snapshot(ReplicaSide::Local);
    const auto remoteSnapshot = _syncPal->snapshot(ReplicaSide::Remote);
    CPPUNIT_ASSERT(!localSnapshot->itemId(relativeWorkingDirPath).empty());
    CPPUNIT_ASSERT(!remoteSnapshot->itemId(relativeWorkingDirPath).empty());

    MockIoHelperFileStat mockIoHelper;
    // Create a file with a custom inode on the local side
    mockIoHelper.setPathWithFakeInode(absoluteLocalWorkingDir / "testNodeIdReuseFile", 2);
    { const std::ofstream file((absoluteLocalWorkingDir / "testNodeIdReuseFile").string()); }
    waitForSyncToBeIdle(SourceLocation::currentLoc());
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
    IoHelper::createDirectory(absoluteLocalWorkingDir / "testNodeIdReuseDir", false, ioError);
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    _syncPal->unpause();
    waitForSyncToBeIdle(SourceLocation::currentLoc());

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
    waitForSyncToBeIdle(SourceLocation::currentLoc());

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
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    const auto localSnapshot = _syncPal->snapshot(ReplicaSide::Local);
    const auto remoteSnapshot = _syncPal->snapshot(ReplicaSide::Remote);
    CPPUNIT_ASSERT(!localSnapshot->itemId(relativeWorkingDirPath).empty());
    CPPUNIT_ASSERT(!remoteSnapshot->itemId(relativeWorkingDirPath).empty());

    MockIoHelperFileStat mockIoHelper;
    mockIoHelper.setPathWithFakeInode(absoluteLocalWorkingDir / "testNodeIdReuseFile", 2);
    { const std::ofstream file((absoluteLocalWorkingDir / "testNodeIdReuseFile").string()); }
    waitForSyncToBeIdle(SourceLocation::currentLoc());
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
    waitForSyncToBeIdle(SourceLocation::currentLoc());
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
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    CPPUNIT_ASSERT(remoteSnapshot->itemId(relativeWorkingDirPath / "testNodeIdReuseFile2").empty());
    const NodeId newRemoteFileId2 = remoteSnapshot->itemId(relativeWorkingDirPath / "testNodeIdReuseFile3");
    CPPUNIT_ASSERT(!newRemoteFileId2.empty());
    CPPUNIT_ASSERT_EQUAL(newRemoteFileId, newRemoteFileId2);
    CPPUNIT_ASSERT_EQUAL(remoteSnapshot->size(newRemoteFileId2), localSnapshot->size("2"));
}
#endif

void TestIntegration::waitForSyncToBeIdle(
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
}

void TestIntegration::waitForCurrentSyncToFinish() const {
    const auto currentSyncCount = _syncPal->syncCount();
    const TimerUtility timer;
    LOG_DEBUG(_logger, "Start waiting for sync to finish: " << currentSyncCount);

    static const auto timeOutDuration = seconds(30);
    const TimerUtility timeoutTimer;

    while (currentSyncCount == _syncPal->syncCount()) {
        if (timeoutTimer.elapsed<seconds>() > timeOutDuration) {
            LOG_WARN(_logger, "waitForSyncToFinish timed out");
            break;
        }
        Utility::msleep(10);
    }

    std::stringstream ss;
#if defined(__APPLE__) || defined(WIN32)
    ss << "Stop waiting: " << timer.elapsed<std::chrono::milliseconds>();
#else
    ss << "Stop waiting: " << timer.elapsed<std::chrono::milliseconds>().count();
#endif
    LOG_DEBUG(_logger, ss.str());
}

void TestIntegration::logStep(const std::string &str) {
    std::stringstream ss;
#if defined(__APPLE__) || defined(WIN32)
    ss << "$$$$$ Step `" << str << "` done in " << _timer.lap<std::chrono::milliseconds>() << " $$$$$";
#else
    ss << "$$$$$ Step `" << str << "` done in " << _timer.lap<std::chrono::milliseconds>().count() << " $$$$$";
#endif
    LOG_DEBUG(_logger, ss.str());
}

TestIntegration::RemoteFileInfo TestIntegration::getRemoteFileInfoByName(const int driveDbId, const NodeId &parentId,
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
        if (SyncName2Str(name) == obj->get(nameKey).toString()) {
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
