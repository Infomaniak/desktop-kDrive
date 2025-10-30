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
#include "test_utility/testhelpers.h"

namespace KDC {
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
    { const std::ofstream file(absoluteLocalWorkingDir / "testNodeIdReuseFile"); }
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

    // Create a child file within "testNodeIdReuseDir".
    { const std::ofstream childFile(absoluteLocalWorkingDir / "testNodeIdReuseDir" / "childFile.txt"); }

    _syncPal->unpause();
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    // Check that the file has been replaced by a directory on the remote replica with a different ID.
    const NodeId newRemoteDirId =
            _syncPal->liveSnapshot(ReplicaSide::Remote).itemId(relativeWorkingDirPath / "testNodeIdReuseDir");
    CPPUNIT_ASSERT(!newRemoteDirId.empty());
    CPPUNIT_ASSERT(newRemoteDirId != remoteFileId);
    CPPUNIT_ASSERT_EQUAL(NodeType::Directory, _syncPal->liveSnapshot(ReplicaSide::Remote).type(newRemoteDirId));
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Remote).exists(remoteFileId));

    // Check that the new directory contains the file "childFile.txt" that was created locally.
    const NodeId newRemoteChildFileId =
            _syncPal->liveSnapshot(ReplicaSide::Remote).itemId(relativeWorkingDirPath / "testNodeIdReuseDir" / "childFile.txt");
    CPPUNIT_ASSERT(!newRemoteChildFileId.empty());
    CPPUNIT_ASSERT_EQUAL(NodeType::File, _syncPal->liveSnapshot(ReplicaSide::Remote).type(newRemoteChildFileId));

    // Replace the directory with a file on the local side with the same ID.
    _syncPal->pause();
    while (!_syncPal->isPaused()) {
        Utility::msleep(100);
    }
    IoHelper::deleteItem(absoluteLocalWorkingDir / "testNodeIdReuseDir", ioError);
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    { const std::ofstream file(absoluteLocalWorkingDir / "testNodeIdReuseFile"); }

    _syncPal->unpause();
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    // Check that the directory has been replaced by a file on the remote with a different ID.
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
    { const std::ofstream file(absoluteLocalWorkingDir / "testNodeIdReuseFile"); }
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

void TestIntegration::nodeIdReuseFalsePositiveInitialSituation(const LocalTemporaryDirectory &localTmpDir) {
    // Initial situation
    // .
    // └── testNodeIdReuseFalsePositive
    //     ├── A
    //     │   └── AA
    //     └── B
    //         └── BA
    std::filesystem::create_directory(localTmpDir.path() / "A");
    testhelpers::generateOrEditTestFile(localTmpDir.path() / "A" / "AA");
    std::filesystem::create_directory(localTmpDir.path() / "B");
    testhelpers::generateOrEditTestFile(localTmpDir.path() / "B" / "BA");

    _syncPal->start();
    waitForSyncToBeIdle(SourceLocation::currentLoc());

    _syncPal->pause();
}

void TestIntegration::testNodeIdReuseFalsePositive() {
    if (!testhelpers::isExtendedTest()) return;

    LOGW_DEBUG(_logger, L"$$$$$ testNodeIdReuseFalsePositive");

    {
        const LocalTemporaryDirectory localTmpDir("testNodeIdReuseFalsePositive", _syncPal->localPath());
        nodeIdReuseFalsePositiveInitialSituation(localTmpDir);

        // Set the creation date of A in DB to 0.
        IoError ioError = IoError::Unknown;
        FileStat filestatA;
        const SyncPath absoluteLocalPathA = localTmpDir.path() / "A";
        IoHelper::getFileStat(absoluteLocalPathA, &filestatA, ioError);
        DbNode dbNode;
        bool found = false;
        CPPUNIT_ASSERT(_syncPal->syncDb()->node(ReplicaSide::Local, std::to_string(filestatA.inode), dbNode, found) && found);
        dbNode.setCreated(0);
        CPPUNIT_ASSERT(_syncPal->syncDb()->updateNode(dbNode, found) && found);

        // Move B/BA to A/BA on local side
        const SyncPath absoluteLocalPathB = localTmpDir.path() / "B";
        IoHelper::moveItem(absoluteLocalPathB / "BA", absoluteLocalPathA / "BA", ioError);

        // Rename A
        const SyncPath newAbsoluteLocalPathA = localTmpDir.path() / "A2";
        IoHelper::renameItem(absoluteLocalPathA, newAbsoluteLocalPathA, ioError);

        _syncPal->unpause();
        waitForSyncToBeIdle(SourceLocation::currentLoc());

        CPPUNIT_ASSERT(std::filesystem::exists(newAbsoluteLocalPathA));
        CPPUNIT_ASSERT(std::filesystem::exists(newAbsoluteLocalPathA / "AA"));
        CPPUNIT_ASSERT(std::filesystem::exists(newAbsoluteLocalPathA / "BA"));
        CPPUNIT_ASSERT(std::filesystem::exists(absoluteLocalPathB));
    }
    {
        const LocalTemporaryDirectory localTmpDir("testNodeIdReuseFalsePositive", _syncPal->localPath());
        nodeIdReuseFalsePositiveInitialSituation(localTmpDir);

        // Set the creation date of A in DB to 0.
        IoError ioError = IoError::Unknown;
        FileStat filestatA;
        const SyncPath absoluteLocalPathA = localTmpDir.path() / "A";
        IoHelper::getFileStat(absoluteLocalPathA, &filestatA, ioError);
        DbNode dbNode;
        bool found = false;
        CPPUNIT_ASSERT(_syncPal->syncDb()->node(ReplicaSide::Local, std::to_string(filestatA.inode), dbNode, found) && found);
        dbNode.setCreated(0);
        CPPUNIT_ASSERT(_syncPal->syncDb()->updateNode(dbNode, found) && found);

        // Move A/AA to B/AA on local side
        const SyncPath absoluteLocalPathB = localTmpDir.path() / "B";
        IoHelper::moveItem(absoluteLocalPathA / "AA", absoluteLocalPathB / "AA", ioError);

        // Rename A
        const SyncPath newAbsoluteLocalPathA = localTmpDir.path() / "A2";
        IoHelper::renameItem(absoluteLocalPathA, newAbsoluteLocalPathA, ioError);

        _syncPal->unpause();
        waitForSyncToBeIdle(SourceLocation::currentLoc());

        CPPUNIT_ASSERT(std::filesystem::exists(newAbsoluteLocalPathA));
        CPPUNIT_ASSERT(std::filesystem::exists(absoluteLocalPathB / "AA"));
        CPPUNIT_ASSERT(std::filesystem::exists(absoluteLocalPathB / "BA"));
        CPPUNIT_ASSERT(std::filesystem::exists(absoluteLocalPathB));
    }
    {
        const LocalTemporaryDirectory localTmpDir("testNodeIdReuseFalsePositive", _syncPal->localPath());
        nodeIdReuseFalsePositiveInitialSituation(localTmpDir);

        // Set the creation date of A in DB to 0.
        IoError ioError = IoError::Unknown;
        FileStat filestatA;
        const SyncPath absoluteLocalPathA = localTmpDir.path() / "A";
        IoHelper::getFileStat(absoluteLocalPathA, &filestatA, ioError);
        DbNode dbNode;
        bool found = false;
        CPPUNIT_ASSERT(_syncPal->syncDb()->node(ReplicaSide::Local, std::to_string(filestatA.inode), dbNode, found) && found);
        dbNode.setCreated(0);
        CPPUNIT_ASSERT(_syncPal->syncDb()->updateNode(dbNode, found) && found);

        // Edit A/AA and B/BA
        testhelpers::generateOrEditTestFile(absoluteLocalPathA / "AA");
        const SyncPath absoluteLocalPathB = localTmpDir.path() / "B";
        testhelpers::generateOrEditTestFile(absoluteLocalPathB / "BA");

        // Rename A
        const SyncPath newAbsoluteLocalPathA = localTmpDir.path() / "A2";
        IoHelper::renameItem(absoluteLocalPathA, newAbsoluteLocalPathA, ioError);

        _syncPal->unpause();
        waitForSyncToBeIdle(SourceLocation::currentLoc());

        CPPUNIT_ASSERT(std::filesystem::exists(newAbsoluteLocalPathA));
        CPPUNIT_ASSERT(std::filesystem::exists(newAbsoluteLocalPathA / "AA"));
        CPPUNIT_ASSERT(std::filesystem::exists(absoluteLocalPathB));
        CPPUNIT_ASSERT(std::filesystem::exists(absoluteLocalPathB / "BA"));
    }
}
#endif
} // namespace KDC
