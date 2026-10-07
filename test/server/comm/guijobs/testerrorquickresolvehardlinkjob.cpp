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

#include "testerrorquickresolvehardlinkjob.h"

#include "libcommon/utility/types.h"
#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/vfs/vfs.h"
#include "libparms/db/parmsdb.h"
#include "libsyncengine/db/dbnode.h"
#include "libsyncengine/propagation/executor/filerescuer.h"
#include "libsyncengine/syncpal/syncpal.h"
#include "mocks/libcommonserver/db/mockdb.h"
#include "server/comm/guijobs/errorquickresolvehardlinkjob.h"

#include <version.h>
#include <filesystem>
#include <fstream>

namespace KDC {

void TestErrorQuickResolveHardlinkJob::setUp() {
    TestBase::start();

    _logger = Log::instance()->getLogger();

    // Make sure to start with a fresh parameters database, as other tests may have created one.
    ParmsDb::reset();
    (void) ParmsDb::instance(_localParmsDbTempDir.path() / MockDb::makeDbMockFileName(), KDRIVE_VERSION_STRING, true, true);

    /// Insert user, account, drive & sync
    User user(1, 1, "123");
    (void) ParmsDb::instance()->insertUser(user);

    Account account(1, 1, user.dbId(), "account1");
    (void) ParmsDb::instance()->insertAccount(account);

    Drive drive(1, 1, account.dbId(), std::string(), 0, std::string());
    (void) ParmsDb::instance()->insertDrive(drive);

    Sync sync(1, drive.dbId(), _localTempDir.path().string(), "", "/remote");
    const auto syncDbPath = MockDb::makeDbName(user.userId(), account.accountId(), drive.driveId(), sync.dbId());
    sync.setDbPath(syncDbPath);
    (void) ParmsDb::instance()->insertSync(sync);

    _syncPal = std::make_shared<SyncPal>(std::make_shared<VfsOff>(VfsSetupParams(Log::instance()->getLogger())), 1,
                                         KDRIVE_VERSION_STRING);
}

void TestErrorQuickResolveHardlinkJob::tearDown() {
    if (_syncPal && _syncPal->syncDb()) {
        _syncPal->syncDb()->close();
    }
    if (ParmsDb::instance()) {
        ParmsDb::instance()->close();
    }
    ParmsDb::reset();
    _syncPal.reset();

    TestBase::stop();
}

NodeId TestErrorQuickResolveHardlinkJob::createFileAndDbNode(const SyncName &name, const std::string &content,
                                                             const SyncName &linkName, int64_t dbNodeSizeOffset) {
    const SyncPath filePath = _localTempDir.path() / name;
    {
        std::ofstream file(filePath, std::ios::binary);
        file.write(content.data(), static_cast<std::streamsize>(content.size()));
        CPPUNIT_ASSERT_MESSAGE("Failed to create the file", file.good());
    }

    if (!linkName.empty()) {
        const SyncPath linkPath = _localTempDir.path() / linkName;
        std::error_code ec;
        std::filesystem::create_hard_link(filePath, linkPath, ec);
        CPPUNIT_ASSERT_MESSAGE("Failed to create the hardlink: " + ec.message(), !ec);
    }

    FileStat fileStat;
    IoError ioError = IoError::Success;
    CPPUNIT_ASSERT_MESSAGE("Failed to get file stat",
                           IoHelper::getFileStat(filePath, &fileStat, ioError, IoHelper::PathCheckOption::Insensitive));

    // Get the root node DB id. The root node is inserted by the SyncDb initialization and has empty names, so the path of the
    // inserted file node is reconstructed as "name".
    bool found = false;
    DbNodeId rootDbNodeId = 0;
    CPPUNIT_ASSERT(_syncPal->syncDb()->dbId(ReplicaSide::Local, NodeId("1"), rootDbNodeId, found));
    CPPUNIT_ASSERT_MESSAGE("Root node not found in the sync database", found);

    DbNode fileDbNode(0, rootDbNodeId, name, name, std::to_string(fileStat.inode), std::string("r_") + SyncName2Str(name),
                      std::nullopt, fileStat.modificationTime, fileStat.modificationTime, NodeType::File,
                      fileStat.size + dbNodeSizeOffset);
    bool constraintError = false;
    DbNodeId fileDbNodeId = 0;
    CPPUNIT_ASSERT(_syncPal->syncDb()->insertNode(fileDbNode, fileDbNodeId, constraintError));
    CPPUNIT_ASSERT_MESSAGE("Failed to insert the file node into the sync database", !constraintError);

    return std::to_string(fileStat.inode);
}

ExitInfo TestErrorQuickResolveHardlinkJob::runQuickResolveExpect(const NodeId &nodeId, const SyncPath &relativePath) {
    ErrorQuickResolveHardlinkJob job(nullptr, 1, Poco::DynamicStruct(), nullptr);
    job._syncDbId = _syncPal->syncDbId();
    job._errorDbId = 42;
    job._nodeId = nodeId;
    job._relativeLocalPath = relativePath;

    return job.quickResolve(_syncPal);
}

void TestErrorQuickResolveHardlinkJob::runQuickResolve(const NodeId &nodeId, const SyncPath &relativePath) {
    const ExitInfo exitInfo = runQuickResolveExpect(nodeId, relativePath);
    CPPUNIT_ASSERT_MESSAGE("quickResolve failed: " + std::string(exitInfo), exitInfo.code() == ExitCode::Ok);
}

bool TestErrorQuickResolveHardlinkJob::nodeExistsInDb(const NodeId &nodeId) const {
    DbNode dbNode;
    bool found = false;
    (void) _syncPal->syncDb()->node(ReplicaSide::Local, nodeId, dbNode, found);
    return found;
}

bool TestErrorQuickResolveHardlinkJob::pathExists(const SyncPath &path) {
    bool exists = false;
    IoError ioError = IoError::Success;
    (void) IoHelper::checkIfPathExists(path, exists, ioError, IoHelper::PathCheckOption::Insensitive);
    return exists;
}

void TestErrorQuickResolveHardlinkJob::testInSyncFile() {
    const SyncName fileName = Str("file1.txt");
    const SyncName linkName = Str("link1.txt");
    const std::string content("Hello, World!");
    const NodeId nodeId = createFileAndDbNode(fileName, content, linkName);

    CPPUNIT_ASSERT(nodeExistsInDb(nodeId));
    CPPUNIT_ASSERT(pathExists(_localTempDir.path() / fileName));
    CPPUNIT_ASSERT(pathExists(_localTempDir.path() / linkName));

    runQuickResolve(nodeId, SyncPath(fileName));

    CPPUNIT_ASSERT_MESSAGE("The node has not been deleted from the database", !nodeExistsInDb(nodeId));
    CPPUNIT_ASSERT_MESSAGE("The file has not been deleted", !pathExists(_localTempDir.path() / fileName));
    CPPUNIT_ASSERT_MESSAGE("The hardlink has not been deleted", !pathExists(_localTempDir.path() / linkName));
    CPPUNIT_ASSERT_MESSAGE("No rescue copy should have been made",
                           !pathExists(_localTempDir.path() / FileRescuer::rescueFolderName()));
}

void TestErrorQuickResolveHardlinkJob::testModifiedFile() {
    const SyncName fileName = Str("file1.txt");
    const SyncName linkName = Str("link1.txt");
    const std::string content("Hello, World!");
    // Make the size stored in the database differ from the actual file size: the file is not in sync with the database.
    const NodeId nodeId = createFileAndDbNode(fileName, content, linkName, 1);

    runQuickResolve(nodeId, SyncPath(fileName));

    CPPUNIT_ASSERT_MESSAGE("The node has not been deleted from the database", !nodeExistsInDb(nodeId));
    CPPUNIT_ASSERT_MESSAGE("The file has not been deleted", !pathExists(_localTempDir.path() / fileName));
    CPPUNIT_ASSERT_MESSAGE("The hardlink has not been deleted", !pathExists(_localTempDir.path() / linkName));

    // The file content may differ from the remote version: a copy must have been saved into the rescue folder.
    const SyncPath rescueFilePath =
            _localTempDir.path() / FileRescuer::rescueFolderName() / fileName;
    CPPUNIT_ASSERT_MESSAGE("The file has not been rescued", pathExists(rescueFilePath));

    std::ifstream rescueFile(rescueFilePath, std::ios::binary);
    const std::string rescuedContent((std::istreambuf_iterator<char>(rescueFile)), std::istreambuf_iterator<char>());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("The rescued file content does not match", content, rescuedContent);
}

void TestErrorQuickResolveHardlinkJob::testMissingFile() {
    const SyncName fileName = Str("file1.txt");
    // Insert a node referencing an inode that does not exist in the file system.
    bool found = false;
    DbNodeId rootDbNodeId = 0;
    CPPUNIT_ASSERT(_syncPal->syncDb()->dbId(ReplicaSide::Local, NodeId("1"), rootDbNodeId, found));
    CPPUNIT_ASSERT(found);

    const NodeId nodeId("999999");
    DbNode fileDbNode(0, rootDbNodeId, fileName, fileName, nodeId, "r_file1", std::nullopt, 123, 123, NodeType::File, 10);
    bool constraintError = false;
    DbNodeId fileDbNodeId = 0;
    CPPUNIT_ASSERT(_syncPal->syncDb()->insertNode(fileDbNode, fileDbNodeId, constraintError));
    CPPUNIT_ASSERT(!constraintError);

    CPPUNIT_ASSERT(nodeExistsInDb(nodeId));

    runQuickResolve(nodeId, SyncPath(fileName));

    CPPUNIT_ASSERT_MESSAGE("The node has not been deleted from the database", !nodeExistsInDb(nodeId));
    CPPUNIT_ASSERT_MESSAGE("No rescue copy should have been made",
                           !pathExists(_localTempDir.path() / FileRescuer::rescueFolderName()));
}

void TestErrorQuickResolveHardlinkJob::testLinkOutsideSyncRoot() {
    const SyncName fileName = Str("file1.txt");
    const SyncName linkName = Str("link1.txt");
    const SyncName outsideLinkName = Str("link_outside.txt");
    const std::string content("Hello, World!");
    const NodeId nodeId = createFileAndDbNode(fileName, content, linkName);

    // Create a hardlink located outside of the sync root. It must be kept.
    const SyncPath outsideLinkPath = _localOtherDir.path() / outsideLinkName;
    std::error_code ec;
    std::filesystem::create_hard_link(_localTempDir.path() / fileName, outsideLinkPath, ec);
    CPPUNIT_ASSERT_MESSAGE("Failed to create the hardlink outside of the sync root: " + ec.message(), !ec);

    runQuickResolve(nodeId, SyncPath(fileName));

    CPPUNIT_ASSERT_MESSAGE("The node has not been deleted from the database", !nodeExistsInDb(nodeId));
    CPPUNIT_ASSERT_MESSAGE("The file has not been deleted", !pathExists(_localTempDir.path() / fileName));
    CPPUNIT_ASSERT_MESSAGE("The hardlink under the sync root has not been deleted",
                           !pathExists(_localTempDir.path() / linkName));
    CPPUNIT_ASSERT_MESSAGE("The hardlink outside of the sync root has been deleted", pathExists(outsideLinkPath));
    CPPUNIT_ASSERT_MESSAGE("No rescue copy should have been made",
                           !pathExists(_localTempDir.path() / FileRescuer::rescueFolderName()));
}

void TestErrorQuickResolveHardlinkJob::testRescueFilenameCollision() {
    const SyncName fileName = Str("file1.txt");
    const SyncName linkName = Str("link1.txt");
    const std::string content("Hello, World!");
    // Make the size stored in the database differ from the actual file size: the file is not in sync with the database.
    const NodeId nodeId = createFileAndDbNode(fileName, content, linkName, 1);

    // Pre-create an item named as the rescue copy would be: it must be preserved.
    const SyncPath rescueFolderPath = _localTempDir.path() / FileRescuer::rescueFolderName();
    std::error_code ec;
    std::filesystem::create_directory(rescueFolderPath, ec);
    CPPUNIT_ASSERT_MESSAGE("Failed to create the rescue folder: " + ec.message(), !ec);
    const SyncPath existingRescueFilePath = rescueFolderPath / fileName;
    const std::string existingContent("Already here");
    {
        std::ofstream file(existingRescueFilePath, std::ios::binary);
        file << existingContent;
        CPPUNIT_ASSERT_MESSAGE("Failed to create the existing rescue copy", file.good());
    }

    runQuickResolve(nodeId, SyncPath(fileName));

    CPPUNIT_ASSERT_MESSAGE("The existing rescue copy has been overwritten", pathExists(existingRescueFilePath));
    std::ifstream existingRescueFile(existingRescueFilePath, std::ios::binary);
    const std::string existingRescuedContent((std::istreambuf_iterator<char>(existingRescueFile)),
                                             std::istreambuf_iterator<char>());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("The existing rescue copy content does not match", existingContent, existingRescuedContent);

    // The file content may differ from the remote version: a copy must have been saved into the rescue folder with a
    // suffixed name.
    const SyncPath suffixedRescueFilePath = rescueFolderPath / (Str("file1 (1)") + Str2SyncName(".txt"));
    CPPUNIT_ASSERT_MESSAGE("The file has not been rescued with a suffixed name", pathExists(suffixedRescueFilePath));
    std::ifstream suffixedRescueFile(suffixedRescueFilePath, std::ios::binary);
    const std::string suffixedRescuedContent((std::istreambuf_iterator<char>(suffixedRescueFile)),
                                             std::istreambuf_iterator<char>());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("The suffixed rescue copy content does not match", content, suffixedRescuedContent);

    CPPUNIT_ASSERT_MESSAGE("The node has not been deleted from the database", !nodeExistsInDb(nodeId));
    CPPUNIT_ASSERT_MESSAGE("The file has not been deleted", !pathExists(_localTempDir.path() / fileName));
    CPPUNIT_ASSERT_MESSAGE("The hardlink has not been deleted", !pathExists(_localTempDir.path() / linkName));
}

void TestErrorQuickResolveHardlinkJob::testUnknownNode() {
    const NodeId nodeId("999999");
    CPPUNIT_ASSERT_MESSAGE("The node should not be in the database", !nodeExistsInDb(nodeId));

    const ExitInfo exitInfo = runQuickResolveExpect(nodeId, SyncPath(Str("file1.txt")));
    CPPUNIT_ASSERT_MESSAGE("The job must reject a node that is not present in the sync database",
                           exitInfo.code() == ExitCode::InvalidOperation);
}

void TestErrorQuickResolveHardlinkJob::testInvalidPath() {
    const SyncName fileName = Str("file1.txt");
    const SyncName linkName = Str("link1.txt");
    const NodeId nodeId = createFileAndDbNode(fileName, "Hello, World!", linkName);

    // An absolute path must be rejected.
    ExitInfo exitInfo = runQuickResolveExpect(nodeId, _localTempDir.path() / fileName);
    CPPUNIT_ASSERT_MESSAGE("The job must reject an absolute path", exitInfo.code() == ExitCode::InvalidOperation);

    // A path escaping the sync root must be rejected.
    exitInfo = runQuickResolveExpect(nodeId, SyncPath(Str("..")) / Str("escaped.txt"));
    CPPUNIT_ASSERT_MESSAGE("The job must reject a path escaping the sync root", exitInfo.code() == ExitCode::InvalidOperation);

    CPPUNIT_ASSERT_MESSAGE("The node has been deleted from the database", nodeExistsInDb(nodeId));
    CPPUNIT_ASSERT_MESSAGE("The file has been deleted", pathExists(_localTempDir.path() / fileName));
    CPPUNIT_ASSERT_MESSAGE("The hardlink has been deleted", pathExists(_localTempDir.path() / linkName));
    CPPUNIT_ASSERT_MESSAGE("No rescue copy should have been made",
                           !pathExists(_localTempDir.path() / FileRescuer::rescueFolderName()));
}

void TestErrorQuickResolveHardlinkJob::testNodeIdMismatch() {
    const SyncName fileName = Str("file1.txt");
    const SyncName linkName = Str("link1.txt");
    const std::string content("Hello, World!");
    const SyncPath filePath = _localTempDir.path() / fileName;
    {
        std::ofstream file(filePath, std::ios::binary);
        file << content;
        CPPUNIT_ASSERT_MESSAGE("Failed to create the file", file.good());
    }
    std::error_code ec;
    std::filesystem::create_hard_link(filePath, _localTempDir.path() / linkName, ec);
    CPPUNIT_ASSERT_MESSAGE("Failed to create the hardlink: " + ec.message(), !ec);

    // Insert a node referencing an inode that is not the one of the file located at the reported path.
    bool found = false;
    DbNodeId rootDbNodeId = 0;
    CPPUNIT_ASSERT(_syncPal->syncDb()->dbId(ReplicaSide::Local, NodeId("1"), rootDbNodeId, found));
    CPPUNIT_ASSERT(found);
    const NodeId nodeId("999999");
    DbNode fileDbNode(0, rootDbNodeId, fileName, fileName, nodeId, "r_file1", std::nullopt, 123, 123, NodeType::File, 10);
    bool constraintError = false;
    DbNodeId fileDbNodeId = 0;
    CPPUNIT_ASSERT(_syncPal->syncDb()->insertNode(fileDbNode, fileDbNodeId, constraintError));
    CPPUNIT_ASSERT(!constraintError);

    const ExitInfo exitInfo = runQuickResolveExpect(nodeId, SyncPath(fileName));
    CPPUNIT_ASSERT_MESSAGE("The job must reject a seed path that does not refer to the reported node",
                           exitInfo.code() == ExitCode::InvalidOperation);

    CPPUNIT_ASSERT_MESSAGE("The node has been deleted from the database", nodeExistsInDb(nodeId));
    CPPUNIT_ASSERT_MESSAGE("The file has been deleted", pathExists(filePath));
    CPPUNIT_ASSERT_MESSAGE("The hardlink has been deleted", pathExists(_localTempDir.path() / linkName));
    CPPUNIT_ASSERT_MESSAGE("No rescue copy should have been made",
                           !pathExists(_localTempDir.path() / FileRescuer::rescueFolderName()));
}

} // namespace KDC
