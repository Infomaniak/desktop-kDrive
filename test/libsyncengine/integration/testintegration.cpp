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
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    logStep("initialization");

    // Run test cases
    // basicTests();
    // inconsistencyTests();
    // conflictTests();
    // testBreakCycle();
    // testBlacklist();
    // testExclusionTemplates();
    // testEncoding();
    // testParentRename();
    // testNegativeModificationTime();
    testDeleteAndRecreateBranch();
}

void TestIntegration::inconsistencyTests() {
    // Duplicate remote files to set up the tests.
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    _syncPal->pause();
    const auto testForbiddenCharsRemoteId = duplicateRemoteFile(_driveDbId, _testFileRemoteId, Str("testForbiddenChar"));
    const auto testNameClashRemoteId1 = duplicateRemoteFile(_driveDbId, _testFileRemoteId, Str("testNameClash"));
    const auto testNameClashRemoteId2 = duplicateRemoteFile(_driveDbId, _testFileRemoteId, Str("testnameclash1"));
    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    _syncPal->unpause();
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / "testForbiddenChar"));
    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / "testNameClash"));
    const SyncPath nameClashLocalPath = _syncPal->localPath() / "testnameclash1";
    CPPUNIT_ASSERT(std::filesystem::exists(nameClashLocalPath));

    // Rename files on remote side.
    _syncPal->pause();
    (void) RenameJob(nullptr, _driveDbId, testForbiddenCharsRemoteId, "test:*ForbiddenChar").runSynchronously();
    (void) RenameJob(nullptr, _driveDbId, testNameClashRemoteId2, "testnameclash").runSynchronously();
    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    _syncPal->unpause();
    waitForSyncToBeIdle(SourceLocation::currentLoc());

#if defined(KD_WINDOWS)
    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / "testForbiddenChar"));
    CPPUNIT_ASSERT(!std::filesystem::exists(_syncPal->localPath() / "test:*ForbiddenChar"));
#else
    CPPUNIT_ASSERT(!std::filesystem::exists(_syncPal->localPath() / "testForbiddenChar"));
    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / "test:*ForbiddenChar"));
#endif

    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / "testNameClash"));
#if defined(KD_LINUX)
    // No clash on Linux
    CPPUNIT_ASSERT(!std::filesystem::exists(nameClashLocalPath));
    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / "testNameClash"));
    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / "testnameclash"));
#else
    CPPUNIT_ASSERT(std::filesystem::exists(nameClashLocalPath));

    // Edit local name clash file.
    testhelpers::generateOrEditTestFile(nameClashLocalPath);
    FileStat filestat;
    IoError ioError = IoError::Unknown;
    (void) IoHelper::getFileStat(nameClashLocalPath, &filestat, ioError);
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    auto remoteFileInfo = getRemoteFileInfoByName(_driveDbId, _remoteSyncDir.id(), Str("testnameclash"));
    CPPUNIT_ASSERT(remoteFileInfo.isValid());
    CPPUNIT_ASSERT_LESS(filestat.size, remoteFileInfo.size); // The local edit is not propagated.

    // Rename again the remote file to avoid the name clash.
    (void) RenameJob(nullptr, _driveDbId, testNameClashRemoteId2, "testnameclash2").runSynchronously();
    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    // Needed because when an item has remote move and local edit operations at the same time, the move operation is processed
    // first. Then, the edit operation could not be propagated since the item path has changed. The sync is therefore restarted,
    // and a new edit operation, with the new path, is generated.
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    (void) IoHelper::getFileStat(_syncPal->localPath() / "testnameclash2", &filestat, ioError);
    remoteFileInfo = getRemoteFileInfoByName(_driveDbId, _remoteSyncDir.id(), Str("testnameclash2"));

    CPPUNIT_ASSERT(remoteFileInfo.isValid());
    CPPUNIT_ASSERT_EQUAL(filestat.size, remoteFileInfo.size); // The local edit has been propagated.
    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / "testnameclash2"));
    logStep("testInconsistency");
#endif
}

void TestIntegration::testBreakCycle() {
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    _syncPal->pause(); // We need to pause the sync because the back might take some time to notify all the events.
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
    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    _syncPal->unpause();
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    _syncPal->pause(); // We need to pause the sync because the back might take some time to notify all the events.
    const auto pathAA = _syncPal->localPath() / tmpRemoteDir.name() / "A" / "AA";
    // Rename A/AA/AAA to A/AA/AAA2 on remote replica.
    (void) RenameJob(nullptr, _driveDbId, nodeIdAAA, Str("AAA2")).runSynchronously();
    // Create A/AA/AAA on remote replica.
    CreateDirJob dirJob(nullptr, _driveDbId, nodeIdAA, Str("AAA"));
    (void) dirJob.runSynchronously();
    // Move A/AA/AAA2 to A/AA/AAA/AAA2 on remote replica.
    moveRemoteFile(_driveDbId, nodeIdAAA, dirJob.nodeId());
    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    _syncPal->unpause();
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    waitForSyncToBeIdle(SourceLocation::currentLoc()); // Previous sync only breaks the cycle.

    CPPUNIT_ASSERT(std::filesystem::exists(pathAA / "AAA" / "AAA2"));
    logStep("testBreakCycle");
}

void TestIntegration::testBlacklist() {
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
    const auto filename = Str("testBlacklist");
    const auto fileId = duplicateRemoteFile(_driveDbId, _testFileRemoteId, filename);
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    const auto dirpath = _syncPal->localPath() / tmpRemoteDir.name();
    CPPUNIT_ASSERT(std::filesystem::exists(dirpath));

    // Add directory to blacklist.
    (void) SyncNodeCache::instance()->update(_syncPal->syncDbId(), SyncNodeType::BlackList, {tmpRemoteDir.id()});
    // Apply new blacklist.
    _syncPal->stop();
    (void) BlacklistPropagator(_syncPal).runSynchronously();
    _syncPal->start();
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    CPPUNIT_ASSERT(!std::filesystem::exists(dirpath));

    // Move a file inside a blacklisted directory.
    moveRemoteFile(_driveDbId, fileId, tmpRemoteDir.id());
    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    CPPUNIT_ASSERT(!std::filesystem::exists(dirpath / filename));

    // Move a file from inside a blacklisted directory to a synchronized directory.
    moveRemoteFile(_driveDbId, fileId, _remoteSyncDir.id());
    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / filename));

    // Remove directory from blacklist.
    (void) SyncNodeCache::instance()->update(_syncPal->syncDbId(), SyncNodeType::BlackList, {});
    // Apply new blacklist.
    _syncPal->stop();
    (void) BlacklistPropagator(_syncPal).runSynchronously();
    _syncPal->start();

    waitForSyncToBeIdle(SourceLocation::currentLoc());
    CPPUNIT_ASSERT(std::filesystem::exists(dirpath));
    logStep("testBlacklist");
}

void TestIntegration::testExclusionTemplates() {
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id(), "testExclusionTemplates");
    const RemoteTemporaryDirectory exclusionTemplatesTestDir(_driveDbId, tmpRemoteDir.id(), "testDir");
    FileStat filestat;
    bool found = false;
    IoHelper::getFileStat(_syncPal->localPath() / tmpRemoteDir.name() / exclusionTemplatesTestDir.name(), &filestat, found);
    const auto dirLocalId = std::to_string(filestat.inode);

    const auto filename = "testExclusionTemplates";
    const SyncPath testName = "to_be_excluded";
    const auto fileRemoteId = duplicateRemoteFile(_driveDbId, _testFileRemoteId, testName);
    moveRemoteFile(_driveDbId, fileRemoteId, tmpRemoteDir.id());
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    const auto excludedFilePath = _syncPal->localPath() / tmpRemoteDir.name() / testName;
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
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    CPPUNIT_ASSERT(std::filesystem::exists(excludedFilePath)); // We do not remove the local file.
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Local).exists(fileLocalId));
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Remote).exists(fileRemoteId));

    // Rename the remote file so the exclusion template does not apply anymore.
    (void) RenameJob(nullptr, _driveDbId, fileRemoteId, filename).runSynchronously();
    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    // The remote file is synchronized again...
    CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Remote).exists(fileRemoteId));
    // ... but the local file is still excluded. A new file is therefore downloaded.
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Local).exists(fileLocalId));
    IoHelper::getFileStat(_syncPal->localPath() / tmpRemoteDir.name() / filename, &filestat, found);
    fileLocalId = std::to_string(filestat.inode);
    CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Local).exists(fileLocalId));

    // Rename remote directory so the exclusion template applies.
    (void) RenameJob(nullptr, _driveDbId, exclusionTemplatesTestDir.id(), testName).runSynchronously();
    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Local).exists(dirLocalId));
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Remote).exists(exclusionTemplatesTestDir.id()));
    CPPUNIT_ASSERT(!std::filesystem::exists(_syncPal->localPath() / tmpRemoteDir.name() /
                                            exclusionTemplatesTestDir.name())); // The local directory has been deleted.

    // Move a file inside an excluded directory from remote replica.
    moveRemoteFile(_driveDbId, fileRemoteId, exclusionTemplatesTestDir.id());
    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Local).exists(fileLocalId));
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Remote).exists(fileRemoteId));

    // Move a file away from an excluded directory from remote replica.
    moveRemoteFile(_driveDbId, fileRemoteId, tmpRemoteDir.id());
    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Remote).exists(fileRemoteId));
    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / tmpRemoteDir.name() / filename));
    // Local file ID has changed again.
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Local).exists(fileLocalId));
    IoHelper::getFileStat(_syncPal->localPath() / tmpRemoteDir.name() / filename, &filestat, found);
    fileLocalId = std::to_string(filestat.inode);
    CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Local).exists(fileLocalId));

    // Rename remote directory so the exclusion template does not apply anymore.
    (void) RenameJob(nullptr, _driveDbId, exclusionTemplatesTestDir.id(), exclusionTemplatesTestDir.name()).runSynchronously();
    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Remote).exists(exclusionTemplatesTestDir.id()));
    // Local dir ID has changed.
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Local).exists(dirLocalId));

    // Rename the remote file so the exclusion template applies.
    (void) RenameJob(nullptr, _driveDbId, fileRemoteId, testName).runSynchronously();
    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Local).exists(fileLocalId));
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Remote).exists(fileRemoteId));
    CPPUNIT_ASSERT(
            !std::filesystem::exists(_syncPal->localPath() / tmpRemoteDir.name() / filename)); // The local file has been deleted.

    // Remove the exclusion template.
    (void) templateList.pop_back();
    (void) ExclusionTemplateCache::instance()->update(false, templateList);
    _syncPal->stop();
    (void) ExcludeListPropagator(_syncPal).runSynchronously();
    _syncPal->start();
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Remote).exists(fileRemoteId));
    // Local file ID has changed again.
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Local).exists(fileLocalId));
    IoHelper::getFileStat(_syncPal->localPath() / tmpRemoteDir.name() / testName, &filestat, found);
    fileLocalId = std::to_string(filestat.inode);
    CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Local).exists(fileLocalId));
    logStep("testExclusionTemplates");
}

void TestIntegration::testEncoding() {
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    const auto nfcPath = _syncPal->localPath() / tmpRemoteDir.name() / testhelpers::makeNfcSyncName();
    testhelpers::generateOrEditTestFile(nfcPath);
    const auto nfdPath = _syncPal->localPath() / tmpRemoteDir.name() / testhelpers::makeNfdSyncName();
    testhelpers::generateOrEditTestFile(nfdPath);
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    const auto remoteTestFileInfo = getRemoteFileInfoByName(_driveDbId, tmpRemoteDir.id(), nfcPath.filename().native());
    CPPUNIT_ASSERT(remoteTestFileInfo.isValid());
    CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(1), countItemsInRemoteDir(_driveDbId, tmpRemoteDir.id()));
    logStep("testEncoding");
}

void TestIntegration::testParentRename() {
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    _syncPal->pause(); // We need to pause the sync because the back might take some time to notify all the events.
    const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
    // .
    // └── A
    //     └── AA
    //         └── AAA
    NodeId nodeIdA;
    NodeId nodeIdAA;
    NodeId nodeIdAAA;
    {
        CreateDirJob jobA(nullptr, _driveDbId, tmpRemoteDir.id(), Str("A"));
        (void) jobA.runSynchronously();
        nodeIdA = jobA.nodeId();
        CreateDirJob jobAA(nullptr, _driveDbId, jobA.nodeId(), Str("AA"));
        (void) jobAA.runSynchronously();
        nodeIdAA = jobAA.nodeId();
        CreateDirJob jobAAA(nullptr, _driveDbId, nodeIdAA, Str("AAA"));
        (void) jobAAA.runSynchronously();
        nodeIdAAA = jobAAA.nodeId();
    }
    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    _syncPal->unpause(); // We need to pause the sync because the back might take some time to notify all the events.
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    _syncPal->pause();
    // Rename A to A2
    (void) RenameJob(nullptr, _driveDbId, nodeIdA, "A2").runSynchronously();
    // Delete A/AA/AAA
    DeleteJob deleteJob(_driveDbId, nodeIdAAA);
    deleteJob.setBypassCheck(true);
    (void) deleteJob.runSynchronously();
    // Create A/AA/AAA2
    (void) CreateDirJob(nullptr, _driveDbId, nodeIdAA, Str("AAA2")).runSynchronously();
    _syncPal->unpause();
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / tmpRemoteDir.name() / "A2" / "AA" / "AAA2"));
    logStep("testParentRename");
}

void TestIntegration::testNegativeModificationTime() {
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    const SyncName filename = Str("testNegativeModificationTime");
    const SyncTime timeInput = -10000;
    {
        // Test with only negative modification time.
        const LocalTemporaryDirectory localTmpDir(SyncName2Str(filename), _syncPal->localPath());
        const auto filepath = localTmpDir.path() / filename;
        testhelpers::generateOrEditTestFile(filepath);
        FileStat fileStat;
        bool found = false;
        IoHelper::getFileStat(filepath, &fileStat, found);
        (void) IoHelper::setFileDates(filepath, fileStat.creationTime, timeInput, false);
        waitForSyncToBeIdle(SourceLocation::currentLoc());

        DbNode dbNode;
        CPPUNIT_ASSERT(_syncPal->syncDb()->node(ReplicaSide::Local, std::to_string(fileStat.inode), dbNode, found) && found);

        GetFileInfoJob fileInfoJob(_driveDbId, *dbNode.nodeIdRemote());
        (void) fileInfoJob.runSynchronously();
        CPPUNIT_ASSERT_EQUAL(timeInput, fileInfoJob.modificationTime());
    }
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    {
        // Test with only negative creation time.
        const LocalTemporaryDirectory localTmpDir(SyncName2Str(filename), _syncPal->localPath());
        const auto filepath = localTmpDir.path() / filename;
        testhelpers::generateOrEditTestFile(filepath);
        FileStat fileStat;
        bool found = false;
        IoHelper::getFileStat(filepath, &fileStat, found);
        (void) IoHelper::setFileDates(filepath, timeInput, fileStat.modificationTime, false);
        waitForSyncToBeIdle(SourceLocation::currentLoc());

        DbNode dbNode;
        CPPUNIT_ASSERT(_syncPal->syncDb()->node(ReplicaSide::Local, std::to_string(fileStat.inode), dbNode, found) && found);

        GetFileInfoJob fileInfoJob(_driveDbId, *dbNode.nodeIdRemote());
        (void) fileInfoJob.runSynchronously();
#if defined(KD_WINDOWS)
        // Windows: negative creation dates are accepted.
        CPPUNIT_ASSERT_EQUAL(timeInput, fileInfoJob.creationTime());
#else
        // macOS: Setting a negative creation date when modification date is positive is not accepted.
        // Linux: Creation date cannot be changed.
        CPPUNIT_ASSERT_EQUAL(fileStat.creationTime, fileInfoJob.creationTime());
#endif
    }
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    {
        // Test with both negative creation and modification time.
        const LocalTemporaryDirectory localTmpDir(SyncName2Str(filename), _syncPal->localPath());
        const auto filepath = localTmpDir.path() / filename;
        testhelpers::generateOrEditTestFile(filepath);
        FileStat fileStat;
        bool found = false;
        IoHelper::getFileStat(filepath, &fileStat, found);
        (void) IoHelper::setFileDates(filepath, timeInput, timeInput, false);
        waitForSyncToBeIdle(SourceLocation::currentLoc());

        DbNode dbNode;
        CPPUNIT_ASSERT(_syncPal->syncDb()->node(ReplicaSide::Local, std::to_string(fileStat.inode), dbNode, found) && found);

        GetFileInfoJob fileInfoJob(_driveDbId, *dbNode.nodeIdRemote());
        (void) fileInfoJob.runSynchronously();
#if defined(KD_LINUX)
        // Linux: Creation date cannot be changed.
        CPPUNIT_ASSERT_EQUAL(fileStat.creationTime, fileInfoJob.creationTime());
#else
        // Setting both a negative creation and modification date is accepted.
        CPPUNIT_ASSERT_EQUAL(timeInput, fileInfoJob.creationTime());
#endif
        CPPUNIT_ASSERT_EQUAL(timeInput, fileInfoJob.modificationTime());
    }

    logStep("testNegativeModificationTime");
}

void TestIntegration::testDeleteAndRecreateBranch() {
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    // Setup initial situation
    // .
    // └── A(a)
    //     └── AA(aa)
    //         └── AAA(aaa)
    //             └── AAAA(aaaa)
    _syncPal->pause(); // We need to pause the sync because the back might take some time to notify all the events.
    const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
    NodeId nodeIdA;
    NodeId nodeIdAA;
    NodeId nodeIdAAA;
    NodeId nodeIdAAAA;
    const SyncPath filename = "AAAA";
    const auto filepath = _syncPal->localPath() / tmpRemoteDir.name() / filename;
    {
        CreateDirJob jobA(nullptr, _driveDbId, tmpRemoteDir.id(), Str("A"));
        (void) jobA.runSynchronously();
        nodeIdA = jobA.nodeId();
        CreateDirJob jobAA(nullptr, _driveDbId, jobA.nodeId(), Str("AA"));
        (void) jobAA.runSynchronously();
        nodeIdAA = jobAA.nodeId();
        CreateDirJob jobAAA(nullptr, _driveDbId, nodeIdAA, Str("AAA"));
        (void) jobAAA.runSynchronously();
        nodeIdAAA = jobAAA.nodeId();

        nodeIdAAAA = duplicateRemoteFile(_driveDbId, _testFileRemoteId, filename);
        moveRemoteFile(_driveDbId, nodeIdAAAA, nodeIdAAA);
    }
    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    _syncPal->unpause();
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    _syncPal->pause();

    {
        // Move test file outside deleted directory
        moveRemoteFile(_driveDbId, nodeIdAAAA, tmpRemoteDir.id());

        // Delete A/AA
        deleteRemoteFile(_driveDbId, nodeIdAA);

        // Create A/AA/AAA1
        CreateDirJob jobAA(nullptr, _driveDbId, nodeIdA, Str("AA"));
        (void) jobAA.runSynchronously();
        nodeIdAA = jobAA.nodeId();
        CreateDirJob jobAAA(nullptr, _driveDbId, nodeIdAA, Str("AAA1"));
        (void) jobAAA.runSynchronously();

        // Move back test file
        moveRemoteFile(_driveDbId, nodeIdAAAA, jobAAA.nodeId());
    }

    _syncPal->unpause();
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / tmpRemoteDir.name() / "A" / "AA" / "AAA1"));

    logStep("testDeleteAndRecreateBranch");
}

#if defined(KD_LINUX)
void TestIntegration::testNodeIdReuseFile2DirAndDir2File() {
    if (!testhelpers::isExtendedTest()) return;

    LOGW_DEBUG(_logger, L"$$$$$ testNodeIdReuseFile2DirAndDir2File");

    const RemoteTemporaryDirectory remoteTempDir(_driveDbId, _remoteSyncDir.id(), "testNodeIdReuseFile2DirAndDir2File");
    const SyncPath relativeWorkingDirPath = remoteTempDir.name();
    const SyncPath absoluteLocalWorkingDir = _syncPal->localPath() / relativeWorkingDirPath;
    _syncPal->start();
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Local).itemId(relativeWorkingDirPath).empty());
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Remote).itemId(relativeWorkingDirPath).empty());

    MockIoHelperFileStat mockIoHelper;
    // Create a file with a custom inode on the local side
    mockIoHelper.setPathWithFakeInode(absoluteLocalWorkingDir / "testNodeIdReuseFile", 2);
    { const std::ofstream file((absoluteLocalWorkingDir / "testNodeIdReuseFile").string()); }
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    CPPUNIT_ASSERT_EQUAL(NodeId("2"),
                         _syncPal->liveSnapshot(ReplicaSide::Local).itemId(relativeWorkingDirPath / "testNodeIdReuseFile"));
    const NodeId remoteFileId =
            _syncPal->liveSnapshot(ReplicaSide::Remote).itemId(relativeWorkingDirPath / "testNodeIdReuseFile");
    CPPUNIT_ASSERT(!remoteFileId.empty());
    CPPUNIT_ASSERT_EQUAL(NodeType::File, _syncPal->liveSnapshot(ReplicaSide::Remote).type(remoteFileId));

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
    const NodeId newRemoteDirId =
            _syncPal->liveSnapshot(ReplicaSide::Remote).itemId(relativeWorkingDirPath / "testNodeIdReuseDir");
    CPPUNIT_ASSERT(!newRemoteDirId.empty());
    CPPUNIT_ASSERT(newRemoteDirId != remoteFileId);
    CPPUNIT_ASSERT_EQUAL(NodeType::Directory, _syncPal->liveSnapshot(ReplicaSide::Remote).type(newRemoteDirId));
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Remote).exists(remoteFileId));

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
    const NodeId newRemoteFileId =
            _syncPal->liveSnapshot(ReplicaSide::Remote).itemId(relativeWorkingDirPath / "testNodeIdReuseFile");
    CPPUNIT_ASSERT(newRemoteFileId != "");
    CPPUNIT_ASSERT(newRemoteFileId != newRemoteDirId);
    CPPUNIT_ASSERT(newRemoteFileId != remoteFileId);
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Remote).exists(remoteFileId));
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Remote).exists(newRemoteDirId));
    CPPUNIT_ASSERT_EQUAL(NodeType::File, _syncPal->liveSnapshot(ReplicaSide::Remote).type(newRemoteFileId));
}

void TestIntegration::testNodeIdReuseFile2File() {
    if (!testhelpers::isExtendedTest()) return;

    LOGW_DEBUG(_logger, L"$$$$$ testNodeIdReuseFile2File");

    const RemoteTemporaryDirectory remoteTempDir(_driveDbId, _remoteSyncDir.id(), "testNodeIdReuseFile2File");
    const SyncPath relativeWorkingDirPath = remoteTempDir.name();
    const SyncPath absoluteLocalWorkingDir = _syncPal->localPath() / relativeWorkingDirPath;
    _syncPal->start();
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Local).itemId(relativeWorkingDirPath).empty());
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Remote).itemId(relativeWorkingDirPath).empty());

    MockIoHelperFileStat mockIoHelper;
    mockIoHelper.setPathWithFakeInode(absoluteLocalWorkingDir / "testNodeIdReuseFile", 2);
    { const std::ofstream file((absoluteLocalWorkingDir / "testNodeIdReuseFile").string()); }
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    CPPUNIT_ASSERT_EQUAL(NodeId("2"),
                         _syncPal->liveSnapshot(ReplicaSide::Local).itemId(relativeWorkingDirPath / "testNodeIdReuseFile"));
    const NodeId remoteFileId =
            _syncPal->liveSnapshot(ReplicaSide::Remote).itemId(relativeWorkingDirPath / "testNodeIdReuseFile");
    CPPUNIT_ASSERT(!remoteFileId.empty());
    CPPUNIT_ASSERT_EQUAL(NodeType::File, _syncPal->liveSnapshot(ReplicaSide::Remote).type(remoteFileId));

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
    const NodeId newRemoteFileId =
            _syncPal->liveSnapshot(ReplicaSide::Remote).itemId(relativeWorkingDirPath / "testNodeIdReuseFile2");
    CPPUNIT_ASSERT(!newRemoteFileId.empty());
    CPPUNIT_ASSERT(remoteFileId != newRemoteFileId);
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Remote).exists(remoteFileId));

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
    CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Remote).itemId(relativeWorkingDirPath / "testNodeIdReuseFile2").empty());
    const NodeId newRemoteFileId2 =
            _syncPal->liveSnapshot(ReplicaSide::Remote).itemId(relativeWorkingDirPath / "testNodeIdReuseFile3");
    CPPUNIT_ASSERT(!newRemoteFileId2.empty());
    CPPUNIT_ASSERT_EQUAL(newRemoteFileId, newRemoteFileId2);
    CPPUNIT_ASSERT_EQUAL(_syncPal->liveSnapshot(ReplicaSide::Remote).size(newRemoteFileId2),
                         _syncPal->liveSnapshot(ReplicaSide::Local).size("2"));
}
#endif

void TestIntegration::waitForSyncToBeIdle(
        const SourceLocation &srcLoc, const std::chrono::milliseconds minWaitTime /*= std::chrono::milliseconds(3000)*/) const {
    const auto timeOutDuration = minutes(2);
    const TimerUtility timeoutTimer;

    // Wait for end of sync (A sync is considered ended when it stay in Idle for more than 3s)
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

void TestIntegration::logStep(const std::string &str) {
    std::stringstream ss;
#if defined(KD_MACOS) || defined(KD_WINDOWS)
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
            break;
        }
    }

    return fileInfo;
}

int64_t TestIntegration::countItemsInRemoteDir(int driveDbId, const NodeId &parentId) const {
    GetFileListJob job(driveDbId, parentId);
    (void) job.runSynchronously();

    const auto resObj = job.jsonRes();
    if (!resObj) return -1;

    const auto dataArray = resObj->getArray(dataKey);
    if (!dataArray) return -1;

    return static_cast<int64_t>(dataArray->size());
}

void TestIntegration::editRemoteFile(const int driveDbId, const NodeId &remoteFileId, SyncTime *creationTime /*= nullptr*/,
                                     SyncTime *modificationTime /*= nullptr*/, int64_t *size /*= nullptr*/) const {
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

void TestIntegration::moveRemoteFile(const int driveDbId, const NodeId &remoteFileId, const NodeId &destinationRemoteParentId,
                                     const SyncName &name /*= {}*/) const {
    MoveJob job(nullptr, driveDbId, {}, remoteFileId, destinationRemoteParentId, name);
    job.setBypassCheck(true);
    (void) job.runSynchronously();
}

NodeId TestIntegration::duplicateRemoteFile(const int driveDbId, const NodeId &id, const SyncName &newName) const {
    DuplicateJob job(nullptr, driveDbId, id, newName);
    (void) job.runSynchronously();
    return job.nodeId();
}

void TestIntegration::deleteRemoteFile(const int driveDbId, const NodeId &id) const {
    DeleteJob job(driveDbId, id);
    job.setBypassCheck(true);
    (void) job.runSynchronously();
}

SyncPath TestIntegration::findLocalFileByNamePrefix(const SyncPath &parentAbsolutePath, const SyncName &namePrefix) const {
    IoError ioError(IoError::Unknown);
    IoHelper::DirectoryIterator dirIt(parentAbsolutePath, false, ioError);
    bool endOfDir = false;
    DirectoryEntry entry;
    while (dirIt.next(entry, endOfDir, ioError) && !endOfDir && ioError == IoError::Success) {
        if (CommonUtility::startsWith(entry.path().filename(), namePrefix)) return entry.path();
    }
    return SyncPath();
}

} // namespace KDC
