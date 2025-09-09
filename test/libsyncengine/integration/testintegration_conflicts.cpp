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
#include "jobs/network/kDrive_API/createdirjob.h"
#include "jobs/network/kDrive_API/deletejob.h"
#include "jobs/network/kDrive_API/renamejob.h"
#include "jobs/network/kDrive_API/upload/uploadjob.h"
#include "propagation/executor/filerescuer.h"
#include "test_utility/testhelpers.h"
#include "update_detection/file_system_observer/filesystemobserverworker.h"

namespace KDC {
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
    waitForSyncToBeIdle(SourceLocation::currentLoc());

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
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    // Check that both remote and local ID has been re-inserted in DB.
    SyncName name;
    CPPUNIT_ASSERT(_syncPal->syncDb()->name(ReplicaSide::Remote, _testFileRemoteId, name, found) && found);
    CPPUNIT_ASSERT(_syncPal->syncDb()->name(ReplicaSide::Local, localId, name, found) && found);
    logStep("testCreateCreatePseudoConflict");
}

void TestIntegration::testCreateCreateConflict() {
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    // Create a file on remote replica.
    const auto remoteId = duplicateRemoteFile(_driveDbId, _testFileRemoteId, Str("testCreateCreatePseudoConflict"));

    // Create a file on local replica.
    const SyncPath localFilePath = _syncPal->localPath() / "testCreateCreatePseudoConflict";
    testhelpers::generateOrEditTestFile(localFilePath);

    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    const auto conflictedFilePath =
            findLocalFileByNamePrefix(_localSyncDir.path(), Str("testCreateCreatePseudoConflict_conflict_"));
    CPPUNIT_ASSERT(std::filesystem::exists(conflictedFilePath));
    CPPUNIT_ASSERT(std::filesystem::exists(localFilePath));
    logStep("testCreateCreateConflict");
}

void TestIntegration::testEditEditPseudoConflict() {
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    // Change the last modification date in DB.
    bool found = false;
    DbNode dbNode;
    CPPUNIT_ASSERT(_syncPal->syncDb()->node(ReplicaSide::Remote, _testFileRemoteId, dbNode, found) && found);
    const auto newLastModified = *dbNode.lastModifiedLocal() - 1000;
    dbNode.setLastModifiedLocal(newLastModified);
    dbNode.setLastModifiedRemote(newLastModified);
    CPPUNIT_ASSERT(_syncPal->syncDb()->updateNode(dbNode, found) && found);

    _syncPal->forceInvalidateSnapshots();
    waitForSyncToBeIdle(SourceLocation::currentLoc());

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
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    // Edit remote file.
    int64_t creationTime = 0;
    int64_t modificationTime = 0;
    editRemoteFile(_driveDbId, _testFileRemoteId, &creationTime, &modificationTime);

    // Edit local file.
    SyncPath relativePath;
    bool ignore = false;
    (void) _syncPal->_remoteFSObserverWorker->liveSnapshot().path(_testFileRemoteId, relativePath, ignore);
    const SyncPath absoluteLocalPath = _syncPal->localPath() / relativePath;
    testhelpers::generateOrEditTestFile(absoluteLocalPath);

    (void) IoHelper::setFileDates(absoluteLocalPath, creationTime, modificationTime, false);

    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    const auto conflictedFilePath =
            findLocalFileByNamePrefix(_localSyncDir.path(), absoluteLocalPath.filename().native() + Str("_conflict_"));
    CPPUNIT_ASSERT(std::filesystem::exists(conflictedFilePath));
    CPPUNIT_ASSERT(std::filesystem::exists(absoluteLocalPath));
    logStep("testEditEditConflict");
}

void TestIntegration::testMoveCreateConflict() {
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    const SyncPath filename = "testMoveCreateConflict1";
    const SyncPath localFilePath = _syncPal->localPath() / filename;

    // 1 - If the remote file has been moved, rename local file
    {
        // Rename file on remote replica.
        (void) RenameJob(nullptr, _driveDbId, _testFileRemoteId, filename).runSynchronously();

        // Create a file a local replica.
        testhelpers::generateOrEditTestFile(localFilePath);

        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForSyncToBeIdle(SourceLocation::currentLoc());

        // The local file has been excluded from sync.
        const auto conflictedFilePath = findLocalFileByNamePrefix(_localSyncDir.path(), filename.native() + Str("_conflict_"));
        // The remote rename operation has been propagated.
        CPPUNIT_ASSERT(std::filesystem::exists(conflictedFilePath));
        CPPUNIT_ASSERT(std::filesystem::exists(localFilePath));
        logStep("testMoveCreateConflict1");
    }
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    // 2 - If the local file has been moved, undo move operation.
    {
        // Create a file on remote replica.
        const SyncName newFilename = Str("testMoveCreateConflict2");
        (void) duplicateRemoteFile(_driveDbId, _testFileRemoteId, newFilename);

        // Rename a file on local replica.
        const SyncPath newLocalFilePath = _syncPal->localPath() / newFilename;
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::renameItem(localFilePath, newLocalFilePath, ioError) && ioError == IoError::Success);

        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForSyncToBeIdle(SourceLocation::currentLoc());

        // The local file has been put back in its origin location.
        // The remote creation has been propagated.
        CPPUNIT_ASSERT(std::filesystem::exists(localFilePath));
        CPPUNIT_ASSERT(std::filesystem::exists(newLocalFilePath));
        logStep("testMoveCreateConflict2");
    }
}

void TestIntegration::testEditDeleteConflict() {
    // Delete happen on the edited file.
    {
        waitForSyncToBeIdle(SourceLocation::currentLoc());

        // Generate the test file
        const SyncName filename = Str("testEditDeleteConflict1");
        const auto tmpNodeId = duplicateRemoteFile(_driveDbId, _testFileRemoteId, filename);
        waitForSyncToBeIdle(SourceLocation::currentLoc());

        // Edit the file on remote replica.
        SyncTime modificationTime = 0;
        int64_t size = 0;
        editRemoteFile(_driveDbId, tmpNodeId, nullptr, &modificationTime, &size);

        // Delete the file on local replica.
        const SyncPath filepath = _syncPal->localPath() / filename;
        IoError ioError = IoError::Unknown;
        (void) IoHelper::deleteItem(filepath, ioError);
        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForSyncToBeIdle(SourceLocation::currentLoc());

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
        waitForSyncToBeIdle(SourceLocation::currentLoc());

        // Generate the test file
        const SyncPath dirpath = _syncPal->localPath() / "testEditDeleteConflict2_dir";
        IoError ioError = IoError::Unknown;
        (void) IoHelper::createDirectory(dirpath, false, ioError);
        const SyncPath filepath = dirpath / "testEditDeleteConflict2_file";
        testhelpers::generateOrEditTestFile(filepath);
        waitForSyncToBeIdle(SourceLocation::currentLoc());

        std::optional<NodeId> remoteDirNodeId = std::nullopt;
        bool found = false;
        CPPUNIT_ASSERT(_syncPal->syncDb()->id(ReplicaSide::Remote, dirpath.filename(), remoteDirNodeId, found) && found);

        // Delete the file on remote.
        DeleteJob deleteJob(_driveDbId, *remoteDirNodeId);
        deleteJob.setBypassCheck(true);
        (void) deleteJob.runSynchronously();

        // Edit the file on local replica.
        testhelpers::generateOrEditTestFile(filepath);

        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForSyncToBeIdle(SourceLocation::currentLoc());

        // Delete operation has won...
        CPPUNIT_ASSERT(!std::filesystem::exists(filepath));
        // ... but the edited file has been rescued.
        CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / FileRescuer::rescueFolderName() / filepath.filename()));

        CPPUNIT_ASSERT(Utility::isInTrash(dirpath.filename()));
#if defined(KD_MACOS) || defined(KD_LINUX)
        Utility::eraseFromTrash(dirpath.filename());
#endif

        logStep("testEditDeleteConflict2");
    }
}

namespace {
NodeId createRemoteFile(const int driveDbId, const SyncName &name, const NodeId &remoteParentFileId,
                        SyncTime *creationTime = nullptr, SyncTime *modificationTime = nullptr, int64_t *size = nullptr) {
    const LocalTemporaryDirectory temporaryDir;
    const auto tmpFilePath = temporaryDir.path() / ("tmpFile_" + CommonUtility::generateRandomStringAlphaNum(10));
    testhelpers::generateOrEditTestFile(tmpFilePath);

    const auto timestamp = duration_cast<std::chrono::seconds>(
            time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch());
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
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    {
        const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
        RemoteNodeInfo info;
        generateInitialTestSituation(_driveDbId, tmpRemoteDir.id(), info);
        waitForSyncToBeIdle(SourceLocation::currentLoc());

        // Delete A on remote replica
        deleteRemoteFile(_driveDbId, info.remoteNodeIdA);

        // Rename A to B on local replica
        const SyncPath localPathA = _syncPal->localPath() / tmpRemoteDir.name() / "A";
        const SyncPath localPathB = _syncPal->localPath() / tmpRemoteDir.name() / "B";
        IoError ioError = IoError::Unknown;
        (void) IoHelper::renameItem(localPathA, localPathB, ioError);

        // Delete A/AB on local replica
        const SyncPath localPathAB = localPathB / "AB";
        (void) IoHelper::deleteItem(localPathAB, ioError);

        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForSyncToBeIdle(SourceLocation::currentLoc());

        // Delete operation wins
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathA));
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathB));
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathAB));
        logStep("testMoveDeleteConflict1");
    }
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    {
        const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
        RemoteNodeInfo info;
        generateInitialTestSituation(_driveDbId, tmpRemoteDir.id(), info);
        waitForSyncToBeIdle(SourceLocation::currentLoc());

        // Delete A on remote replica
        deleteRemoteFile(_driveDbId, info.remoteNodeIdA);

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

        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForSyncToBeIdle(SourceLocation::currentLoc());

        // Delete operation wins
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathA));
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathB));
        // ... but created and edited files has been rescued.
        CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / FileRescuer::rescueFolderName() / "AAA"));
        CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / FileRescuer::rescueFolderName() / "ABA"));
        logStep("testMoveDeleteConflict2");
    }
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    {
        const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
        RemoteNodeInfo info;
        generateInitialTestSituation(_driveDbId, tmpRemoteDir.id(), info);
        waitForSyncToBeIdle(SourceLocation::currentLoc());

        // Delete A on remote replica
        deleteRemoteFile(_driveDbId, info.remoteNodeIdA);

        // Rename A to B on local replica
        const SyncPath localPathA = _syncPal->localPath() / tmpRemoteDir.name() / "A";
        const SyncPath localPathB = _syncPal->localPath() / tmpRemoteDir.name() / "B";
        IoError ioError = IoError::Unknown;
        (void) IoHelper::renameItem(localPathA, localPathB, ioError);

        // Move A/AB to AB on local replica
        const SyncPath originLocalPathAB = localPathB / "AB";
        const SyncPath destLocalPathAB = _syncPal->localPath() / tmpRemoteDir.name() / "AB";
        (void) IoHelper::moveItem(originLocalPathAB, destLocalPathAB, ioError);

        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForSyncToBeIdle(SourceLocation::currentLoc());

        // Delete operation wins
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathA));
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathB));
        CPPUNIT_ASSERT(!std::filesystem::exists(originLocalPathAB));
        logStep("testMoveDeleteConflict3");
    }
    {
        waitForSyncToBeIdle(SourceLocation::currentLoc());
        const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
        RemoteNodeInfo info;
        generateInitialTestSituation(_driveDbId, tmpRemoteDir.id(), info);
        waitForSyncToBeIdle(SourceLocation::currentLoc());

        // Delete A on remote replica
        deleteRemoteFile(_driveDbId, info.remoteNodeIdA);

        // Rename A to B on local replica
        const SyncPath localPathA = _syncPal->localPath() / tmpRemoteDir.name() / "A";
        const SyncPath localPathB = _syncPal->localPath() / tmpRemoteDir.name() / "B";
        IoError ioError = IoError::Unknown;
        (void) IoHelper::renameItem(localPathA, localPathB, ioError);

        // Move A/AB to AB on remote replica
        moveRemoteFile(_driveDbId, info.remoteNodeIdAB, tmpRemoteDir.id());

        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForSyncToBeIdle(SourceLocation::currentLoc());

        // Delete operation wins.
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathA));
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathB));
        // But AB has been moved outside deleted branch.
        CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / tmpRemoteDir.name() / ""));
        logStep("testMoveDeleteConflict4");
    }
    {
        waitForSyncToBeIdle(SourceLocation::currentLoc());
        const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
        RemoteNodeInfo info;
        generateInitialTestSituation(_driveDbId, tmpRemoteDir.id(), info);
        waitForSyncToBeIdle(SourceLocation::currentLoc());

        // Delete A on remote replica
        deleteRemoteFile(_driveDbId, info.remoteNodeIdA);

        // Rename A/AB to A/AB2 on local replica
        const SyncPath localPathAB = _syncPal->localPath() / tmpRemoteDir.name() / "AB";
        const SyncPath localPathAB2 = _syncPal->localPath() / tmpRemoteDir.name() / "AB2";
        IoError ioError = IoError::Unknown;
        (void) IoHelper::renameItem(localPathAB, localPathAB2, ioError);

        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForSyncToBeIdle(SourceLocation::currentLoc());

        // This is treated as a Move-ParentDelete conflict, the Move-Delete conflict is ignored.
        // Delete operation wins.
        CPPUNIT_ASSERT(!std::filesystem::exists(_syncPal->localPath() / tmpRemoteDir.name() / "A"));
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathAB));
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathAB2));
        logStep("testMoveDeleteConflict5");
    }
}

void TestIntegration::testMoveParentDeleteConflict() {
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    {
        _syncPal->pause(); // We need to pause the sync because the back might take some time to notify all the events.
        const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
        RemoteNodeInfo info;
        generateInitialTestSituation(_driveDbId, tmpRemoteDir.id(), info);
        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        _syncPal->unpause();
        waitForSyncToBeIdle(SourceLocation::currentLoc());

        // Delete A on remote replica
        deleteRemoteFile(_driveDbId, info.remoteNodeIdA);

        // Move A/AA/AAA to A/AB/AAA on local replica
        const SyncPath localPathA = _syncPal->localPath() / tmpRemoteDir.name() / "A";
        const SyncPath originPathAAA = localPathA / "AA" / "AAA";
        const SyncPath destinationPathAAA = localPathA / "AB" / "AAA";
        IoError ioError = IoError::Unknown;
        (void) IoHelper::moveItem(originPathAAA, destinationPathAAA, ioError);

        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForSyncToBeIdle(SourceLocation::currentLoc());

        // Delete operation wins
        CPPUNIT_ASSERT(!std::filesystem::exists(localPathA));
        logStep("testMoveParentDeleteConflict1");
    }
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    {
        _syncPal->pause(); // We need to pause the sync because the back might take some time to notify all the events.
        const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
        RemoteNodeInfo info;
        generateInitialTestSituation(_driveDbId, tmpRemoteDir.id(), info);
        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        _syncPal->unpause();
        waitForSyncToBeIdle(SourceLocation::currentLoc());

        // Rename A to A2 on remote replica
        (void) RenameJob(nullptr, _driveDbId, info.remoteNodeIdA, Str("A2")).runSynchronously();

        // Delete A/AB on remote replica
        deleteRemoteFile(_driveDbId, info.remoteNodeIdAB);

        // Move A/AA to A/AB/AA on local replica
        const SyncPath localPathA = _syncPal->localPath() / tmpRemoteDir.name() / "A";
        const SyncPath originPathAA = localPathA / "AA";
        const SyncPath destinationPathAA = localPathA / "AB" / "AA";
        IoError ioError = IoError::Unknown;
        (void) IoHelper::moveItem(originPathAA, destinationPathAA, ioError);

        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForSyncToBeIdle(SourceLocation::currentLoc());

        // Delete operation wins
        CPPUNIT_ASSERT(!std::filesystem::exists(originPathAA));
        CPPUNIT_ASSERT(!std::filesystem::exists(destinationPathAA));
        CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / tmpRemoteDir.name() / "A2"));
        logStep("testMoveParentDeleteConflict2");
    }
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    {
        // Set up a more complex tree
        // .
        // └── A
        //     ├── AA
        //     │   └── AAA
        //     └── AB
        //         ├── ABA
        //         └── ABB
        _syncPal->pause(); // We need to pause the sync because the back might take some time to notify all the events.
        const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
        RemoteNodeInfo info;
        generateInitialTestSituation(_driveDbId, tmpRemoteDir.id(), info);
        (void) CreateDirJob(nullptr, _driveDbId, info.remoteNodeIdAB, Str("ABA")).runSynchronously();
        (void) createRemoteFile(_driveDbId, Str("ABB"), info.remoteNodeIdAB);
        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        _syncPal->unpause();
        waitForSyncToBeIdle(SourceLocation::currentLoc());

        // Delete A/AB on remote replica
        deleteRemoteFile(_driveDbId, info.remoteNodeIdAB);

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

        _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
        waitForSyncToBeIdle(SourceLocation::currentLoc());

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
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
    RemoteNodeInfo info;
    generateInitialTestSituation(_driveDbId, tmpRemoteDir.id(), info);
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    // Delete AB on remote replica
    deleteRemoteFile(_driveDbId, info.remoteNodeIdAB);

    // Create A/AB/ABA on local replica
    const SyncPath localPathA = _syncPal->localPath() / tmpRemoteDir.name() / "A";
    const SyncPath localPathABA = localPathA / "AB" / "ABA";
    testhelpers::generateOrEditTestFile(localPathABA);

    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    // Delete operation wins...
    CPPUNIT_ASSERT(!std::filesystem::exists(localPathA / "AB"));
    // ...but created files should be rescued
    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / FileRescuer::rescueFolderName() / "ABA"));
    logStep("testCreateParentDeleteConflict");
}

void TestIntegration::testMoveMoveSourcePseudoConflict() {
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteSyncDir.id());
    const SyncPath originLocalPath = _syncPal->localPath() / "testMoveMoveSourcePseudoConflict";
    const auto remoteFileNodeId = createRemoteFile(_driveDbId, originLocalPath.filename().native(), _remoteSyncDir.id());
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    // Move test file in subdirectory on remote replica
    moveRemoteFile(_driveDbId, remoteFileNodeId, tmpRemoteDir.id());

    // Move test file in subdirectory on local replica
    const SyncPath destinationLocalPath = _syncPal->localPath() / tmpRemoteDir.name() / originLocalPath.filename();
    IoError ioError = IoError::Unknown;
    (void) IoHelper::moveItem(originLocalPath, destinationLocalPath, ioError);

    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    // No conflict, change only propagated in DB.
    CPPUNIT_ASSERT(std::filesystem::exists(destinationLocalPath));
    logStep("testMoveMoveSourcePseudoConflict");
}

void TestIntegration::testMoveMoveSourceConflict() {
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    const RemoteTemporaryDirectory tmpRemoteDir1(_driveDbId, _remoteSyncDir.id());
    const RemoteTemporaryDirectory tmpRemoteDir2(_driveDbId, _remoteSyncDir.id());
    const SyncPath originLocalPath = _syncPal->localPath() / "testMoveMoveSourceConflict";
    const auto remoteFileNodeId = createRemoteFile(_driveDbId, originLocalPath.filename().native(), _remoteSyncDir.id());
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    // Move test file in subdirectory2 on remote replica
    moveRemoteFile(_driveDbId, remoteFileNodeId, tmpRemoteDir2.id());

    // Move test file in subdirectory1 on local replica
    const SyncPath destinationLocalPath = _syncPal->localPath() / tmpRemoteDir1.name() / originLocalPath.filename();
    IoError ioError = IoError::Unknown;
    (void) IoHelper::moveItem(originLocalPath, destinationLocalPath, ioError);

    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    // Move operation has been undo on local replica then remote operation has been propagated.
    CPPUNIT_ASSERT(!std::filesystem::exists(destinationLocalPath));
    const SyncPath destinationRemotePath = _syncPal->localPath() / tmpRemoteDir2.name() / originLocalPath.filename();
    CPPUNIT_ASSERT(std::filesystem::exists(destinationRemotePath));
    logStep("testMoveMoveSourceConflict");
}

void TestIntegration::testMoveMoveDestConflict() {
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    const auto remoteId1 = duplicateRemoteFile(_driveDbId, _testFileRemoteId, Str("testMoveMoveDestConflict1"));
    const auto remoteId2 = duplicateRemoteFile(_driveDbId, _testFileRemoteId, Str("testMoveMoveDestConflict2"));
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    // Rename test file 2 on remote replica
    (void) RenameJob(nullptr, _driveDbId, remoteId2, "testMoveMoveDestConflict").runSynchronously();

    // Rename test file 1 on local replica
    const SyncPath originLocalPath = _syncPal->localPath() / "testMoveMoveDestConflict1";
    const SyncPath destinationLocalPath = _syncPal->localPath() / "testMoveMoveDestConflict";
    IoError ioError = IoError::Unknown;
    (void) IoHelper::renameItem(originLocalPath, destinationLocalPath, ioError);

    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    // Move operation has been undo on local replica then remote operation has been propagated.
    CPPUNIT_ASSERT(std::filesystem::exists(originLocalPath));
    CPPUNIT_ASSERT(std::filesystem::exists(destinationLocalPath));
    logStep("testMoveMoveDestConflict");
}

void TestIntegration::testMoveMoveCycleConflict() {
    waitForSyncToBeIdle(SourceLocation::currentLoc());
    const RemoteTemporaryDirectory tmpRemoteDirA(_driveDbId, _remoteSyncDir.id());
    const RemoteTemporaryDirectory tmpRemoteDirB(_driveDbId, _remoteSyncDir.id());
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    // Move B to A/B on remote replica
    moveRemoteFile(_driveDbId, tmpRemoteDirB.id(), tmpRemoteDirA.id());

    // Move A to B/A on local replica
    const SyncPath originLocalPathA = _syncPal->localPath() / tmpRemoteDirA.name();
    const SyncPath destinationLocalPathA = _syncPal->localPath() / tmpRemoteDirB.name() / tmpRemoteDirA.name();
    IoError ioError = IoError::Unknown;
    (void) IoHelper::moveItem(originLocalPathA, destinationLocalPathA, ioError);

    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    // Move operation has been undo on local replica then remote operation has been propagated.
    CPPUNIT_ASSERT(!std::filesystem::exists(destinationLocalPathA));
    CPPUNIT_ASSERT(std::filesystem::exists(_syncPal->localPath() / tmpRemoteDirA.name() / tmpRemoteDirB.name()));
    logStep("testMoveMoveCycleConflict");
}
} // namespace KDC
