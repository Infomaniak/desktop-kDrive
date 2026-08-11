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

#include "testintegration.h"
#include "jobs/local/synclocaldeletejob.h"
#include "jobs/local/localmovejob.h"
#include "jobs/network/kDrive_API/createdirjob.h"
#include "jobs/network/kDrive_API/deletejob.h"
#include "jobs/network/kDrive_API/getfileinfojob.h"
#include "jobs/network/kDrive_API/renamejob.h"
#include "jobs/network/kDrive_API/upload/uploadjob.h"
#include "propagation/executor/filerescuer.h"
#include "test_utility/testhelpers_requests.h"
#include "test_utility/testhelpers.h"
#include "syncpal_test_helper/syncpaltesthelper.h"
#include "update_detection/file_system_observer/filesystemobserverworker.h"

namespace KDC {
void TestIntegration::basicTests() {
    testLocalChanges();
    testRemoteChanges();
    testSimultaneousChanges();
    testUploadBigFile();
}

void TestIntegration::testLocalChanges() {
    waitForSyncToBeIdle(std::source_location::current());

    // Generate create operations.
    const SyncPath subDirPath = _syncPal->localPath() / "testSubDirLocal";
    (void) std::filesystem::create_directories(subDirPath);

    SyncPath filePath = _syncPal->localPath() / "testFileLocal";
    testhelpers::generateOrEditTestFile(filePath);

    FileStat fileStat;
    bool exists = false;
    IoHelper::getFileStat(filePath, &fileStat, exists, IoHelper::PathCheckOption::Insensitive);
    waitForSyncToBeIdle(std::source_location::current());

    auto remoteTestFileInfo = getRemoteFileInfoByName(_driveDbId, _remoteSyncDir.id(), filePath.filename());
    CPPUNIT_ASSERT(remoteTestFileInfo.isValid());

    const auto remoteTestDirInfo = getRemoteFileInfoByName(_driveDbId, _remoteSyncDir.id(), subDirPath.filename());
    CPPUNIT_ASSERT(remoteTestDirInfo.isValid());
    CPPUNIT_ASSERT_EQUAL(fileStat.size, remoteTestFileInfo.size);
    CPPUNIT_ASSERT_EQUAL(fileStat.modificationTime, remoteTestFileInfo.modificationTime);
    logStep("test create local file");

    // Generate an edit operation.
    testhelpers::generateOrEditTestFile(filePath);
    IoHelper::getFileStat(filePath, &fileStat, exists, IoHelper::PathCheckOption::Insensitive);
    waitForSyncToBeIdle(std::source_location::current());

    const auto prevRemoteTestFileInfo = remoteTestFileInfo;
    remoteTestFileInfo = getRemoteFileInfoByName(_driveDbId, _remoteSyncDir.id(), filePath.filename());
    CPPUNIT_ASSERT_EQUAL(fileStat.modificationTime, remoteTestFileInfo.modificationTime);
    CPPUNIT_ASSERT_LESS(remoteTestFileInfo.modificationTime, prevRemoteTestFileInfo.modificationTime);
    CPPUNIT_ASSERT_EQUAL(fileStat.size, remoteTestFileInfo.size);
    CPPUNIT_ASSERT_LESS(remoteTestFileInfo.size, prevRemoteTestFileInfo.size);
    logStep("test edit local file");

    // Generate a move operation.
    const SyncName newName = Str("testFileLocal_renamed");
    const std::filesystem::path destinationPath = subDirPath / newName;
    {
        LocalMoveJob job(filePath, destinationPath);
        (void) job.runSynchronously();
    }
    waitForSyncToBeIdle(std::source_location::current());

    remoteTestFileInfo = getRemoteFileInfoByName(_driveDbId, remoteTestDirInfo.id, newName);
    CPPUNIT_ASSERT(remoteTestFileInfo.isValid());
    CPPUNIT_ASSERT_EQUAL(remoteTestDirInfo.id, remoteTestFileInfo.parentId);

    filePath = destinationPath;
    logStep("test move local file");

    // Generate a delete operation.
    {
        GenericLocalDeleteJob deleteJob(subDirPath);
        (void) deleteJob.runSynchronously();
    }
    waitForSyncToBeIdle(std::source_location::current());

    remoteTestFileInfo = getRemoteFileInfoByName(_driveDbId, remoteTestDirInfo.id, filePath.filename());
    CPPUNIT_ASSERT(!remoteTestFileInfo.isValid());

#if defined(KD_LINUX)
    CPPUNIT_ASSERT(testhelpers::hasTrashInfo());
    CPPUNIT_ASSERT(testhelpers::isInTrash(subDirPath));
#else
    CPPUNIT_ASSERT(testhelpers::isInTrash(subDirPath.filename()));
#endif

#if defined(KD_MACOS) || defined(KD_LINUX)
    testhelpers::eraseFromTrash(subDirPath.filename());
#endif

    logStep("test delete local file");
}

void TestIntegration::testRemoteChanges() {
    waitForSyncToBeIdle(std::source_location::current());

    // Generate create operations.
    const SyncPath subDirPath = _syncPal->localPath() / "testSubDirRemote";
    NodeId subDirId;
    SyncPath filePath = _syncPal->localPath() / "testFileRemote";
    NodeId fileId;
    {
        CreateDirJob createDirJob(nullptr, _driveDbId, subDirPath, _remoteSyncDir.id(), subDirPath.filename());
        (void) createDirJob.runSynchronously();
        subDirId = createDirJob.nodeId();

        fileId = testhelpers::duplicateRemoteItem(_driveDbId, _testFileRemoteId, filePath.filename());
    }
    GetFileInfoJob fileInfoJob(_driveDbId, fileId);
    (void) fileInfoJob.runSynchronously();
    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForSyncToBeIdle(std::source_location::current());

    CPPUNIT_ASSERT(std::filesystem::exists(subDirPath));
    CPPUNIT_ASSERT(std::filesystem::exists(filePath));

    FileStat fileStat;
    bool exists = false;
    IoHelper::getFileStat(filePath, &fileStat, exists, IoHelper::PathCheckOption::Insensitive);
    CPPUNIT_ASSERT_EQUAL(fileInfoJob.size(), fileStat.size);
    CPPUNIT_ASSERT_EQUAL(fileInfoJob.modificationTime(), fileStat.modificationTime);

    logStep("test create remote file");

    // Generate an edit operation.
    SyncTime modificationTime = 0;
    int64_t size = 0;
    testhelpers::editRemoteFile(_driveDbId, fileId, nullptr, &modificationTime, &size);
    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForSyncToBeIdle(std::source_location::current());

    FileStat filestat;
    IoError ioError = IoError::Unknown;
    (void) IoHelper::getFileStat(filePath, &filestat, ioError, IoHelper::PathCheckOption::Insensitive);
    CPPUNIT_ASSERT_EQUAL(modificationTime, filestat.modificationTime);
    CPPUNIT_ASSERT_EQUAL(size, filestat.size);
    logStep("test edit remote file");

    // Generate a move operation.
    filePath = subDirPath / "testFileRemote_renamed";
    testhelpers::moveRemoteItem(_driveDbId, fileId, subDirId, filePath.filename());
    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForSyncToBeIdle(std::source_location::current());

    CPPUNIT_ASSERT(std::filesystem::exists(filePath));
    logStep("test move remote file");

    // Generate a delete operation.
    {
        DeleteJob job(_driveDbId, subDirId);
        job.setBypassCheck(true);
        (void) job.runSynchronously();
    }
    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForSyncToBeIdle(std::source_location::current());

    CPPUNIT_ASSERT(!std::filesystem::exists(subDirPath));
    CPPUNIT_ASSERT(!std::filesystem::exists(filePath));
#if defined(KD_LINUX)
    CPPUNIT_ASSERT(testhelpers::hasTrashInfo());
    CPPUNIT_ASSERT(testhelpers::isInTrash(subDirPath));
#else
    CPPUNIT_ASSERT(testhelpers::isInTrash(subDirPath.filename()));
#endif

#if defined(KD_MACOS) || defined(KD_LINUX)
    testhelpers::eraseFromTrash(subDirPath.filename());
#endif

    logStep("test delete remote file");
}

void TestIntegration::testSimultaneousChanges() {
    waitForSyncToBeIdle(std::source_location::current());

    // Rename a file on remote replica.
    const SyncPath remoteFilePath = _syncPal->localPath() / "testSimultaneousChanges_remote";
    (void) RenameJob(nullptr, _driveDbId, _testFileRemoteId, remoteFilePath).runSynchronously();

    // Create a file on local replica.
    const SyncPath localFilePath = _syncPal->localPath() / "testSimultaneousChanges_local";
    testhelpers::generateOrEditTestFile(localFilePath);

    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForSyncToBeIdle(std::source_location::current());

    CPPUNIT_ASSERT(std::filesystem::exists(remoteFilePath));
    const auto remoteTestFileInfo = getRemoteFileInfoByName(_driveDbId, _remoteSyncDir.id(), localFilePath.filename());
    CPPUNIT_ASSERT(remoteTestFileInfo.isValid());
    logStep("testSimultaneousChanges");
}

void TestIntegration::testUploadBigFile() {
    const LocalTemporaryDirectory temporaryDir("testUploadBigFile", _syncPal->localPath());

    waitForSyncToBeIdle(std::source_location::current());

    const auto localFilePath = testhelpers::generateBigFile(temporaryDir.path(), 110); // Generate a 110MB local file.

    bool found = false;
    FileStat fileStat;
    IoHelper::getFileStat(localFilePath, &fileStat, found, IoHelper::PathCheckOption::Insensitive);

    waitForSyncToBeIdle(std::source_location::current());

    DbNode dbNode;
    CPPUNIT_ASSERT(_syncPal->syncDb()->node(ReplicaSide::Local, std::to_string(fileStat.inode), dbNode, found) && found);

    GetFileInfoJob fileInfoJob(_driveDbId, *dbNode.nodeIdRemote());
    (void) fileInfoJob.runSynchronously();

    CPPUNIT_ASSERT_EQUAL(fileStat.creationTime, fileInfoJob.creationTime());
    CPPUNIT_ASSERT_EQUAL(fileStat.modificationTime, fileInfoJob.modificationTime());
    CPPUNIT_ASSERT_EQUAL(fileStat.size, fileInfoJob.size());
    logStep("testUploadBigFile");
}

void TestIntegration::testSimpleComparison() {
    SyncpalTestHelper testHelper(_syncPal);
    testHelper.setUp();

    const Situation situation{Str2SyncName(R"({
        "content" : [
            {
                "type" : "Directory",
                "name" : "A",
                "content" : [ {"type" : "Directory", "name" : "AA", "content" : [ {"type" : "File", "name" : "AAA"} ]} ]
            },
            {"type" : "Directory", "name" : "B"}, {"type" : "File", "name" : "C", "size" : 1234}
        ]
    })")};

    CPPUNIT_ASSERT(testHelper.setInitialSituation(situation, situation));

    CPPUNIT_ASSERT(testHelper.executeSyncUntilEnd());

    CPPUNIT_ASSERT(testHelper.getSituation(situation, situation));

    testHelper.tearDown();
    logStep("testSimpleComparison");
}

void TestIntegration::testSimpleUpload() {
    SyncpalTestHelper testHelper(_syncPal);
    testHelper.setUp();

    const Situation startsituation{Str2SyncName(R"({
        "content" : [
            {"type" : "Directory", "name" : "A"}
        ]
    })")};

    const Operations localoperations{Str2SyncName(R"({
        "operations": [
            { "type": "Create", "itemType": "File", "path": "A", "name": "B", "size" : 1234 }
        ]
    })")};

    const Situation endsituation{Str2SyncName(R"({
        "content" : [
            {
                "type" : "Directory",
                "name" : "A",
                "content" : [ {"type" : "File", "name" : "B", "size" : 1234} ]
            }
        ]
    })")};

    CPPUNIT_ASSERT(testHelper.setInitialSituation(startsituation, startsituation));

    CPPUNIT_ASSERT(testHelper.executeSyncUntilEnd());

    CPPUNIT_ASSERT(testHelper.execute(ReplicaSide::Local, localoperations));

    CPPUNIT_ASSERT(testHelper.executeSyncUntilEnd());

    CPPUNIT_ASSERT(testHelper.getSituation(endsituation, endsituation));

    testHelper.tearDown();
    logStep("testSimpleUpload");
}

void TestIntegration::testGlobalFramework() {
    SyncpalTestHelper testHelper(_syncPal);
    testHelper.setUp();

    CPPUNIT_ASSERT(testHelper.executeSyncUntilEnd());

    // Note: SyncTime is a real Unix epoch (seconds since 1970), not a "YYYYMMDDHHMMSS"-formatted number.
    const SyncTime dirTime = testhelpers::defaultTime - 3600; // 1 hour ago
    const Situation situation{Str2SyncName(R"({
        "content" : [
            {
                "type" : "Directory",
                "name" : "A",
                "content" : [ {"type" : "Directory", "name" : "AA", "content" : [ {"type" : "File", "name" : "AAA"} ]} ]
            },
            {"type" : "Directory", "name" : "B"}, {"type" : "File", "name" : "C", "size" : 1234}
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.setInitialSituation(situation, situation));

    CPPUNIT_ASSERT(testHelper.executeSyncUntilEnd());
    // Sanity-check the initial situation landed on the remote before touching anything.
    CPPUNIT_ASSERT(testHelper.getSituation(situation, situation));

    const Operations localoperations{Str2SyncName(R"({
        "operations": [
            { "type": "Delete", "path":"A/AA/AAA" },
            { "type": "Create", "itemType": "File", "path": "A/AA", "name": "BBB" }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.execute(ReplicaSide::Local, localoperations));

    // The local Create operation above wrote the file directly to disk, simulating a user action, so we
    // need to wait for the SyncPal to detect it and upload it to the remote replica before checking below.
    CPPUNIT_ASSERT(testHelper.executeSyncUntilEnd());

    // AAA -> BBB under A/AA, same as `situation` otherwise.
    const Situation situationAfterLocalOps{Str2SyncName(R"({
        "content" : [
            {
                "type" : "Directory",
                "name" : "A",
                "content" : [ {"type" : "Directory", "name" : "AA", "content" : [ {"type" : "File", "name" : "BBB"} ]} ]
            },
            {"type" : "Directory", "name" : "B"}, {"type" : "File", "name" : "C", "size" : 1234}
        ]
    })")};
    // Replaces the two manual getRemoteFileInfoByPath lookups below: confirms BBB exists remotely and AAA is gone.
    CPPUNIT_ASSERT(testHelper.getSituation(situationAfterLocalOps, situationAfterLocalOps));

    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / "C"));
    CPPUNIT_ASSERT(!std::filesystem::exists(_syncPal->localPath() / "CC"));

    // Now apply an operation on the remote replica (move C -> CC, i.e. rename since they share the same parent)
    // and verify it gets propagated back to the local replica.
    const Operations remoteoperations{Str2SyncName(R"({
        "operations": [
            { "type": "Move", "fromPath":"C", "toPath":"CC" }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.execute(ReplicaSide::Remote, remoteoperations));
    CPPUNIT_ASSERT(testHelper.executeSyncUntilEnd());

    CPPUNIT_ASSERT(!std::filesystem::exists(_syncPal->localPath() / "C"));
    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / "CC"));

    // C -> CC, same as `situationAfterLocalOps` otherwise. New check: confirms the rename landed remotely too,
    // not just that the local replica picked something up.
    const Situation finalSituation{Str2SyncName(R"({
        "content" : [
            {
                "type" : "Directory",
                "name" : "A",
                "content" : [ {"type" : "Directory", "name" : "AA", "content" : [ {"type" : "File", "name" : "BBB"} ]} ]
            },
            {"type" : "Directory", "name" : "B"}, {"type" : "File", "name" : "CC", "size" : 1234}
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.getSituation(finalSituation, finalSituation));

    testHelper.tearDown();
    logStep("testGlobalFramework");
}

void TestIntegration::testNestedRemoteOperations() {
    SyncpalTestHelper testHelper(_syncPal);
    testHelper.setUp();

    CPPUNIT_ASSERT(testHelper.executeSyncUntilEnd());

    // Start from an empty situation.
    const Situation situation{Str2SyncName(R"({"content": []})")};
    CPPUNIT_ASSERT(testHelper.setInitialSituation(situation, situation));

    // Imbricated remote operations applied in a single batch: create a directory, then create a file inside
    // that same directory, right away. Resolving "A/AAA"'s parent must not rely on a stale SyncDb lookup,
    // since "A" was only just created earlier in this very batch.
    const Operations remoteOperations{Str2SyncName(R"({
        "operations": [
            { "type": "Create", "itemType": "Directory", "name": "A" },
            { "type": "Create", "itemType": "File", "path": "A", "name": "AAA" }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.execute(ReplicaSide::Remote, remoteOperations));
    CPPUNIT_ASSERT(testHelper.executeSyncUntilEnd());

    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / "A" / "AAA"));

    // Replaces the manual getRemoteFileInfoByPath lookup: confirms A/AAA exists remotely too.
    const Situation finalSituation{Str2SyncName(R"({
        "content": [
            { "type": "Directory", "name": "A", "content": [ {"type": "File", "name": "AAA"} ] }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.getSituation(finalSituation, finalSituation));

    testHelper.tearDown();
    logStep("testNestedRemoteOperations");
}

void TestIntegration::testRemoteMoveDirectoryDescendantRekey() {
    SyncpalTestHelper testHelper(_syncPal);
    testHelper.setUp();

    CPPUNIT_ASSERT(testHelper.executeSyncUntilEnd());

    // Start from an empty situation.
    const Situation situation{Str2SyncName(R"({"content": []})")};
    CPPUNIT_ASSERT(testHelper.setInitialSituation(situation, situation));

    // Single batch: create A, create A/f, move A -> B, then edit B/f. Resolving "B/f" for the Edit must rely
    // on _batchRemoteIds rekeying A/f -> B/f done by the Move, since SyncDb hasn't been refreshed yet.
    const Operations remoteOperations{Str2SyncName(R"({
        "operations": [
            { "type": "Create", "itemType": "Directory", "name": "A" },
            { "type": "Create", "itemType": "File", "path": "A", "name": "f", "size": 111 },
            { "type": "Move", "fromPath": "A", "toPath": "B" },
            { "type": "Edit", "path": "B/f", "newSize": 222 }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.execute(ReplicaSide::Remote, remoteOperations));
    CPPUNIT_ASSERT(testHelper.executeSyncUntilEnd());

    const Situation finalSituation{Str2SyncName(R"({
        "content": [
            { "type": "Directory", "name": "B", "content": [ {"type": "File", "name": "f", "size": 222} ] }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.getSituation(finalSituation, finalSituation));

    testHelper.tearDown();
    logStep("testRemoteMoveDirectoryDescendantRekey");
}

void TestIntegration::testExecuteSyncUpToStep() {
    SyncpalTestHelper testHelper(_syncPal);
    testHelper.setUp();

    // Start from an empty situation.
    const Situation situation{Str2SyncName(R"({"content": []})")};
    CPPUNIT_ASSERT(testHelper.setInitialSituation(situation, situation));
    CPPUNIT_ASSERT(testHelper.executeSyncUntilEnd());

    const std::vector<SyncStep> stepsToTest = {
            SyncStep::UpdateDetection1, SyncStep::UpdateDetection2, SyncStep::Reconciliation1,
            SyncStep::Reconciliation2,  SyncStep::Reconciliation4,  SyncStep::Propagation1,
            SyncStep::Propagation2,     SyncStep::Done,
    };

    for (const auto step: stepsToTest) {
        // Generate a real local change so the sync actually has work to progress through.
        const SyncPath filePath =
                _syncPal->localPath() / ("testExecuteSyncUpToStep_" + std::to_string(static_cast<int>(step)));
        testhelpers::generateOrEditTestFile(filePath);

        CPPUNIT_ASSERT(testHelper.executeSyncUpToStep(static_cast<int64_t>(step), 10000));

        CPPUNIT_ASSERT_EQUAL(step, _syncPal->step());

        // Wait a bit and make sure the sync stayed frozen at the requested step.
        Utility::msleep(1000);
        CPPUNIT_ASSERT_EQUAL(step, _syncPal->step());

        CPPUNIT_ASSERT(testHelper.executeSyncUntilEnd());
    }

    testHelper.tearDown();
    logStep("testExecuteSyncUpToStep");
}

} // namespace KDC
