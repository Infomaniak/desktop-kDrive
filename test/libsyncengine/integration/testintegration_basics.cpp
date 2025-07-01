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
#include "jobs/local/localdeletejob.h"
#include "jobs/local/localmovejob.h"
#include "jobs/network/API_v2/createdirjob.h"
#include "jobs/network/API_v2/deletejob.h"
#include "jobs/network/API_v2/getfileinfojob.h"
#include "jobs/network/API_v2/renamejob.h"
#include "jobs/network/API_v2/upload/uploadjob.h"
#include "propagation/executor/filerescuer.h"
#include "test_utility/testhelpers.h"
#include "update_detection/file_system_observer/filesystemobserverworker.h"

namespace KDC {
void TestIntegration::basicTests() {
    testLocalChanges();
    testRemoteChanges();
    testSimultaneousChanges();
}

void TestIntegration::testLocalChanges() {
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    // Generate create operations.
    const SyncPath subDirPath = _syncPal->localPath() / "testSubDirLocal";
    (void) std::filesystem::create_directories(subDirPath);

    SyncPath filePath = _syncPal->localPath() / "testFileLocal";
    testhelpers::generateOrEditTestFile(filePath);

    FileStat fileStat;
    bool exists = false;
    IoHelper::getFileStat(filePath, &fileStat, exists);
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    auto remoteTestFileInfo = getRemoteFileInfoByName(_driveDbId, _remoteSyncDir.id(), filePath.filename());
    CPPUNIT_ASSERT(remoteTestFileInfo.isValid());

    const auto remoteTestDirInfo = getRemoteFileInfoByName(_driveDbId, _remoteSyncDir.id(), subDirPath.filename());
    CPPUNIT_ASSERT(remoteTestDirInfo.isValid());
    CPPUNIT_ASSERT_EQUAL(fileStat.size, remoteTestFileInfo.size);
    CPPUNIT_ASSERT_EQUAL(fileStat.modificationTime, remoteTestFileInfo.modificationTime);
    logStep("test create local file");

    // Generate an edit operation.
    testhelpers::generateOrEditTestFile(filePath);
    IoHelper::getFileStat(filePath, &fileStat, exists);
    waitForSyncToBeIdle(SourceLocation::currentLoc());

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
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    remoteTestFileInfo = getRemoteFileInfoByName(_driveDbId, remoteTestDirInfo.id, newName);
    CPPUNIT_ASSERT(remoteTestFileInfo.isValid());
    CPPUNIT_ASSERT_EQUAL(remoteTestDirInfo.id, remoteTestFileInfo.parentId);

    filePath = destinationPath;
    logStep("test move local file");

    // Generate a delete operation.
    {
        LocalDeleteJob deleteJob(subDirPath);
        (void) deleteJob.runSynchronously();
    }
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    remoteTestFileInfo = getRemoteFileInfoByName(_driveDbId, remoteTestDirInfo.id, filePath.filename());
    CPPUNIT_ASSERT(!remoteTestFileInfo.isValid());
    logStep("test delete local file");
}

void TestIntegration::testRemoteChanges() {
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    // Generate create operations.
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
    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    CPPUNIT_ASSERT(std::filesystem::exists(subDirPath));
    CPPUNIT_ASSERT(std::filesystem::exists(filePath));

    FileStat fileStat;
    bool exists = false;
    IoHelper::getFileStat(filePath, &fileStat, exists);
    CPPUNIT_ASSERT_EQUAL(fileInfoJob.size(), fileStat.size);
    CPPUNIT_ASSERT_EQUAL(fileInfoJob.modificationTime(), fileStat.modificationTime);

    logStep("test create remote file");

    // Generate an edit operation.
    SyncTime modificationTime = 0;
    int64_t size = 0;
    editRemoteFile(_driveDbId, fileId, nullptr, &modificationTime, &size);
    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    FileStat filestat;
    IoError ioError = IoError::Unknown;
    (void) IoHelper::getFileStat(filePath, &filestat, ioError);
    CPPUNIT_ASSERT_EQUAL(modificationTime, filestat.modificationTime);
    CPPUNIT_ASSERT_EQUAL(size, filestat.size);
    logStep("test edit remote file");

    // Generate a move operation.
    filePath = subDirPath / "testFileRemote_renamed";
    moveRemoteFile(_driveDbId, fileId, subDirId, filePath.filename());
    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    CPPUNIT_ASSERT(std::filesystem::exists(filePath));
    logStep("test move remote file");

    // Generate a delete operation.
    {
        DeleteJob job(_driveDbId, subDirId);
        job.setBypassCheck(true);
        (void) job.runSynchronously();
    }
    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    CPPUNIT_ASSERT(!std::filesystem::exists(subDirPath));
    CPPUNIT_ASSERT(!std::filesystem::exists(filePath));
    logStep("test delete remote file");
}

void TestIntegration::testSimultaneousChanges() {
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    // Rename a file on remote replica.
    const SyncPath remoteFilePath = _syncPal->localPath() / "testSimultaneousChanges_remote";
    (void) RenameJob(nullptr, _driveDbId, _testFileRemoteId, remoteFilePath).runSynchronously();

    // Create a file on local replica.
    const SyncPath localFilePath = _syncPal->localPath() / "testSimultaneousChanges_local";
    testhelpers::generateOrEditTestFile(localFilePath);

    _syncPal->_remoteFSObserverWorker->forceUpdate(); // Make sure that the remote change is detected immediately
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    CPPUNIT_ASSERT(std::filesystem::exists(remoteFilePath));
    const auto remoteTestFileInfo = getRemoteFileInfoByName(_driveDbId, _remoteSyncDir.id(), localFilePath.filename());
    CPPUNIT_ASSERT(remoteTestFileInfo.isValid());
    logStep("testSimultaneousChanges");
}
} // namespace KDC
