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

// The behavior tests rely on IoHelper::getHardlinkPaths to locate the links of an item, which is only available on Windows.
#if defined(KD_WINDOWS)

#include "testutilityunlinkhardlinksjob.h"

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
#include "server/comm/guijobs/utilityunlinkhardlinksjob.h"

#include <version.h>
#include <filesystem>
#include <fstream>
#include <vector>

namespace KDC {

// Exposes SyncPal::createWorkers so that the tests can create the sync workers, in particular the temporary blacklist manager
// used by the sync engine, without starting a full synchronization.
class TestSyncPal : public SyncPal {
    public:
        using SyncPal::createWorkers;
        using SyncPal::SyncPal;
};

void TestUtilityUnlinkHardlinksJob::setUp() {
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

    _syncPal = std::make_shared<TestSyncPal>(std::make_shared<VfsOff>(VfsSetupParams(Log::instance()->getLogger())), 1,
                                             KDRIVE_VERSION_STRING);
}

void TestUtilityUnlinkHardlinksJob::tearDown() {
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

NodeId TestUtilityUnlinkHardlinksJob::createFileAndDbNode(const SyncName &name, const std::string &content,
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

void TestUtilityUnlinkHardlinksJob::insertDbNode(const SyncName &name, const NodeId &nodeId, const NodeType type) {
    bool found = false;
    DbNodeId rootDbNodeId = 0;
    CPPUNIT_ASSERT(_syncPal->syncDb()->dbId(ReplicaSide::Local, NodeId("1"), rootDbNodeId, found));
    CPPUNIT_ASSERT_MESSAGE("Root node not found in the sync database", found);

    DbNode dbNode(0, rootDbNodeId, name, name, nodeId, std::string("r_") + SyncName2Str(name), std::nullopt, 123, 123, type,
                  type == NodeType::File ? 10 : 0);
    bool constraintError = false;
    DbNodeId dbNodeId = 0;
    CPPUNIT_ASSERT(_syncPal->syncDb()->insertNode(dbNode, dbNodeId, constraintError));
    CPPUNIT_ASSERT_MESSAGE("Failed to insert the node into the sync database", !constraintError);
}

Error TestUtilityUnlinkHardlinksJob::makeHardlinkError(const NodeId &nodeId, const SyncPath &relativePath) const {
    return Error(_syncPal->syncDbId(), nodeId, std::string("r_") + nodeId, NodeType::File, relativePath, ConflictType::None,
                 InconsistencyType::None, CancelType::None, SyncPath(), ExitCode::SystemError, ExitCause::HardlinkNotSupported);
}

ErrorDbId TestUtilityUnlinkHardlinksJob::insertParmsDbError(Error &error) {
    CPPUNIT_ASSERT_MESSAGE("Failed to insert the error into the parameters database", ParmsDb::instance()->insertError(error));
    return error.dbId();
}

ErrorDbId TestUtilityUnlinkHardlinksJob::insertHardlinkError(const NodeId &nodeId, const SyncPath &relativePath) {
    Error error = makeHardlinkError(nodeId, relativePath);
    return insertParmsDbError(error);
}

ExitInfo TestUtilityUnlinkHardlinksJob::runUnlinkExpect(const NodeId &nodeId, const SyncPath &relativePath,
                                                        const std::optional<ErrorDbId> &errorDbId) {
    UtilityUnlinkHardlinksJob job(nullptr, 1, Poco::DynamicStruct(), nullptr);
    job._syncDbId = _syncPal->syncDbId();
    job._errorDbId = errorDbId ? *errorDbId : insertHardlinkError(nodeId, relativePath);
    job._nodeId = nodeId;
    job._relativeLocalPath = relativePath;

    return job.unlinkHardlinks(_syncPal);
}

void TestUtilityUnlinkHardlinksJob::runUnlink(const NodeId &nodeId, const SyncPath &relativePath,
                                              const std::optional<ErrorDbId> &errorDbId) {
    const ExitInfo exitInfo = runUnlinkExpect(nodeId, relativePath, errorDbId);
    CPPUNIT_ASSERT_MESSAGE("unlinkHardlinks failed: " + std::string(exitInfo), exitInfo.code() == ExitCode::Ok);
}

bool TestUtilityUnlinkHardlinksJob::nodeExistsInDb(const NodeId &nodeId) const {
    DbNode dbNode;
    bool found = false;
    (void) _syncPal->syncDb()->node(ReplicaSide::Local, nodeId, dbNode, found);
    return found;
}

bool TestUtilityUnlinkHardlinksJob::errorExistsInDb(const ErrorDbId errorDbId) {
    Error error;
    bool found = false;
    CPPUNIT_ASSERT_MESSAGE("Failed to select the error in the parameters database",
                           ParmsDb::instance()->selectError(errorDbId, error, found));
    return found;
}

NodeId TestUtilityUnlinkHardlinksJob::localNodeId(const SyncPath &path) {
    FileStat fileStat;
    IoError ioError = IoError::Success;
    const bool success = IoHelper::getFileStat(path, &fileStat, ioError, IoHelper::PathCheckOption::Insensitive);
    CPPUNIT_ASSERT_MESSAGE("Failed to get file stat: " + toString(ioError), success && ioError == IoError::Success);
    return std::to_string(fileStat.inode);
}

bool TestUtilityUnlinkHardlinksJob::pathExists(const SyncPath &path) {
    bool exists = false;
    IoError ioError = IoError::Success;
    (void) IoHelper::checkIfPathExists(path, exists, ioError, IoHelper::PathCheckOption::Insensitive);
    return exists;
}

bool TestUtilityUnlinkHardlinksJob::isSymlink(const SyncPath &path) {
    std::error_code ec;
    const auto status = std::filesystem::symlink_status(path, ec);
    return !ec && std::filesystem::is_symlink(status);
}

void TestUtilityUnlinkHardlinksJob::testInSyncFile() {
    const SyncName fileName = Str("file1.txt");
    const SyncName linkName = Str("link1.txt");
    const std::string content("Hello, World!");
    const NodeId nodeId = createFileAndDbNode(fileName, content, linkName);

    CPPUNIT_ASSERT(nodeExistsInDb(nodeId));
    CPPUNIT_ASSERT(pathExists(_localTempDir.path() / fileName));
    CPPUNIT_ASSERT(pathExists(_localTempDir.path() / linkName));

    runUnlink(nodeId, SyncPath(fileName));

    CPPUNIT_ASSERT_MESSAGE("The node has not been deleted from the database", !nodeExistsInDb(nodeId));
    CPPUNIT_ASSERT_MESSAGE("The file has not been deleted", !pathExists(_localTempDir.path() / fileName));
    CPPUNIT_ASSERT_MESSAGE("The hardlink has not been deleted", !pathExists(_localTempDir.path() / linkName));
    CPPUNIT_ASSERT_MESSAGE("No rescue copy should have been made",
                           !pathExists(_localTempDir.path() / FileRescuer::rescueFolderName()));
}

void TestUtilityUnlinkHardlinksJob::testModifiedFile() {
    const SyncName fileName = Str("file1.txt");
    const SyncName linkName = Str("link1.txt");
    const std::string content("Hello, World!");
    // Make the size stored in the database differ from the actual file size: the file is not in sync with the database.
    const NodeId nodeId = createFileAndDbNode(fileName, content, linkName, 1);

    runUnlink(nodeId, SyncPath(fileName));

    CPPUNIT_ASSERT_MESSAGE("The node has not been deleted from the database", !nodeExistsInDb(nodeId));
    CPPUNIT_ASSERT_MESSAGE("The file has not been deleted", !pathExists(_localTempDir.path() / fileName));
    CPPUNIT_ASSERT_MESSAGE("The hardlink has not been deleted", !pathExists(_localTempDir.path() / linkName));

    // The file content may differ from the remote version: a copy must have been saved into the rescue folder.
    const SyncPath rescueFilePath = _localTempDir.path() / FileRescuer::rescueFolderName() / fileName;
    CPPUNIT_ASSERT_MESSAGE("The file has not been rescued", pathExists(rescueFilePath));

    std::ifstream rescueFile(rescueFilePath, std::ios::binary);
    const std::string rescuedContent((std::istreambuf_iterator<char>(rescueFile)), std::istreambuf_iterator<char>());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("The rescued file content does not match", content, rescuedContent);
}

void TestUtilityUnlinkHardlinksJob::testMissingFile() {
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

    runUnlink(nodeId, SyncPath(fileName));

    CPPUNIT_ASSERT_MESSAGE("The node has not been deleted from the database", !nodeExistsInDb(nodeId));
    CPPUNIT_ASSERT_MESSAGE("No rescue copy should have been made",
                           !pathExists(_localTempDir.path() / FileRescuer::rescueFolderName()));
}

void TestUtilityUnlinkHardlinksJob::testRemovedReportedLink() {
    const SyncName fileName = Str("file1.txt");
    const SyncName linkName = Str("link1.txt");
    const std::string content("Hello, World!");
    const NodeId nodeId = createFileAndDbNode(fileName, content);

    // The links under the sync root share the same name, so that the name of the rescue copy does not depend on the order in
    // which they are found.
    const SyncPath filePath = _localTempDir.path() / fileName;
    const SyncPath linkPath1 = _localTempDir.path() / Str("dir1") / linkName;
    const SyncPath linkPath2 = _localTempDir.path() / Str("dir2") / linkName;
    const SyncPath outsideLinkPath = _localOtherDir.path() / Str("link_outside.txt");
    for (const auto &linkPath: {linkPath1, linkPath2, outsideLinkPath}) {
        std::error_code ec;
        (void) std::filesystem::create_directories(linkPath.parent_path(), ec);
        CPPUNIT_ASSERT_MESSAGE("Failed to create the parent directory: " + ec.message(), !ec);
        std::filesystem::create_hard_link(filePath, linkPath, ec);
        CPPUNIT_ASSERT_MESSAGE("Failed to create the hardlink: " + ec.message(), !ec);
    }

    // Remove the link reported in the error.
    std::error_code ec;
    const bool removed = std::filesystem::remove(filePath, ec);
    CPPUNIT_ASSERT_MESSAGE("Failed to remove the reported link: " + ec.message(), removed && !ec);

    const ErrorDbId errorDbId = insertHardlinkError(nodeId, SyncPath(fileName));
    runUnlink(nodeId, SyncPath(fileName), errorDbId);

    CPPUNIT_ASSERT_MESSAGE("The node has not been deleted from the database", !nodeExistsInDb(nodeId));
    CPPUNIT_ASSERT_MESSAGE("The error has not been deleted from the parameters database", !errorExistsInDb(errorDbId));
    CPPUNIT_ASSERT_MESSAGE("A hardlink under the sync root has not been deleted", !pathExists(linkPath1));
    CPPUNIT_ASSERT_MESSAGE("A hardlink under the sync root has not been deleted", !pathExists(linkPath2));
    CPPUNIT_ASSERT_MESSAGE("The hardlink outside of the sync root has been deleted", pathExists(outsideLinkPath));

    // The remaining links are not located at the path stored in the database: a single copy of the file must have been saved
    // into the rescue folder.
    const SyncPath rescueFolderPath = _localTempDir.path() / FileRescuer::rescueFolderName();
    const SyncPath rescueFilePath = rescueFolderPath / linkName;
    CPPUNIT_ASSERT_MESSAGE("The file has not been rescued", pathExists(rescueFilePath));
    CPPUNIT_ASSERT_MESSAGE("The file has been rescued more than once", !pathExists(rescueFolderPath / Str("link1 (1).txt")));

    std::ifstream rescueFile(rescueFilePath, std::ios::binary);
    const std::string rescuedContent((std::istreambuf_iterator<char>(rescueFile)), std::istreambuf_iterator<char>());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("The rescued file content does not match", content, rescuedContent);
}

void TestUtilityUnlinkHardlinksJob::testMovedFile() {
    const SyncName fileName = Str("file1.txt");
    const SyncName movedFileName = Str("moved1.txt");
    const std::string content("Hello, World!");
    const NodeId nodeId = createFileAndDbNode(fileName, content);

    // Move the file into a subdirectory of the sync root after the error was reported.
    const SyncPath movedFilePath = _localTempDir.path() / Str("dir1") / movedFileName;
    std::error_code ec;
    (void) std::filesystem::create_directory(movedFilePath.parent_path(), ec);
    CPPUNIT_ASSERT_MESSAGE("Failed to create the directory: " + ec.message(), !ec);
    std::filesystem::rename(_localTempDir.path() / fileName, movedFilePath, ec);
    CPPUNIT_ASSERT_MESSAGE("Failed to move the file: " + ec.message(), !ec);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("The moved file must keep its node id", nodeId, localNodeId(movedFilePath));

    const ErrorDbId errorDbId = insertHardlinkError(nodeId, SyncPath(fileName));
    runUnlink(nodeId, SyncPath(fileName), errorDbId);

    CPPUNIT_ASSERT_MESSAGE("The node has not been deleted from the database", !nodeExistsInDb(nodeId));
    CPPUNIT_ASSERT_MESSAGE("The error has not been deleted from the parameters database", !errorExistsInDb(errorDbId));
    CPPUNIT_ASSERT_MESSAGE("The moved file has not been deleted", !pathExists(movedFilePath));

    // The file is not located at the path stored in the database anymore: a copy must have been saved into the rescue folder.
    const SyncPath rescueFilePath = _localTempDir.path() / FileRescuer::rescueFolderName() / movedFileName;
    CPPUNIT_ASSERT_MESSAGE("The file has not been rescued", pathExists(rescueFilePath));

    std::ifstream rescueFile(rescueFilePath, std::ios::binary);
    const std::string rescuedContent((std::istreambuf_iterator<char>(rescueFile)), std::istreambuf_iterator<char>());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("The rescued file content does not match", content, rescuedContent);
}

void TestUtilityUnlinkHardlinksJob::testLinkSearchFailure() {
    const SyncName fileName = Str("file1.txt");
    const NodeId nodeId = createFileAndDbNode(fileName, "Hello, World!");

    // Remove the sync root, as if it was not accessible anymore: the reported path is missing and the sync root cannot be
    // searched.
    std::error_code ec;
    (void) std::filesystem::remove_all(_localTempDir.path(), ec);
    CPPUNIT_ASSERT_MESSAGE("Failed to remove the sync root: " + ec.message(), !ec);

    const ErrorDbId errorDbId = insertHardlinkError(nodeId, SyncPath(fileName));
    const ExitInfo exitInfo = runUnlinkExpect(nodeId, SyncPath(fileName), errorDbId);
    CPPUNIT_ASSERT_MESSAGE("The job must fail if the links of the file cannot be searched",
                           exitInfo.code() == ExitCode::SystemError);
    CPPUNIT_ASSERT_MESSAGE("The node has been deleted from the database", nodeExistsInDb(nodeId));
    CPPUNIT_ASSERT_MESSAGE("The error has been deleted from the parameters database", errorExistsInDb(errorDbId));
}

void TestUtilityUnlinkHardlinksJob::testLinkOutsideSyncRoot() {
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

    runUnlink(nodeId, SyncPath(fileName));

    CPPUNIT_ASSERT_MESSAGE("The node has not been deleted from the database", !nodeExistsInDb(nodeId));
    CPPUNIT_ASSERT_MESSAGE("The file has not been deleted", !pathExists(_localTempDir.path() / fileName));
    CPPUNIT_ASSERT_MESSAGE("The hardlink under the sync root has not been deleted", !pathExists(_localTempDir.path() / linkName));
    CPPUNIT_ASSERT_MESSAGE("The hardlink outside of the sync root has been deleted", pathExists(outsideLinkPath));
    CPPUNIT_ASSERT_MESSAGE("No rescue copy should have been made",
                           !pathExists(_localTempDir.path() / FileRescuer::rescueFolderName()));
}

void TestUtilityUnlinkHardlinksJob::testModifiedThroughOtherLink() {
    const SyncName fileName = Str("file1.txt");
    const std::string content("Hello, World!");
    const NodeId nodeId = createFileAndDbNode(fileName, content);

    // Modify the file through a hardlink located outside of the sync root. On Windows, NTFS does not update the size and the
    // dates cached in the directory entry of the link located under the sync root.
    const SyncPath outsideLinkPath = _localOtherDir.path() / Str("link_outside.txt");
    std::error_code ec;
    std::filesystem::create_hard_link(_localTempDir.path() / fileName, outsideLinkPath, ec);
    CPPUNIT_ASSERT_MESSAGE("Failed to create the hardlink outside of the sync root: " + ec.message(), !ec);

    const std::string modifiedContent("Hello, World! Modified through the hardlink located outside of the sync root.");
    {
        std::ofstream file(outsideLinkPath, std::ios::binary | std::ios::trunc);
        file.write(modifiedContent.data(), static_cast<std::streamsize>(modifiedContent.size()));
        CPPUNIT_ASSERT_MESSAGE("Failed to modify the file through the hardlink", file.good());
    }

    runUnlink(nodeId, SyncPath(fileName));

    CPPUNIT_ASSERT_MESSAGE("The node has not been deleted from the database", !nodeExistsInDb(nodeId));
    CPPUNIT_ASSERT_MESSAGE("The file has not been deleted", !pathExists(_localTempDir.path() / fileName));
    CPPUNIT_ASSERT_MESSAGE("The hardlink outside of the sync root has been deleted", pathExists(outsideLinkPath));

    // The file is not in sync with the database anymore: a copy of the modified file must have been saved into the rescue
    // folder.
    const SyncPath rescueFilePath = _localTempDir.path() / FileRescuer::rescueFolderName() / fileName;
    CPPUNIT_ASSERT_MESSAGE("The file has not been rescued", pathExists(rescueFilePath));

    std::ifstream rescueFile(rescueFilePath, std::ios::binary);
    const std::string rescuedContent((std::istreambuf_iterator<char>(rescueFile)), std::istreambuf_iterator<char>());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("The rescued file content does not match", modifiedContent, rescuedContent);
}

void TestUtilityUnlinkHardlinksJob::testRescueFilenameCollision() {
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

    runUnlink(nodeId, SyncPath(fileName));

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

void TestUtilityUnlinkHardlinksJob::testUnknownNode() {
    const NodeId nodeId("999999");
    CPPUNIT_ASSERT_MESSAGE("The node should not be in the database", !nodeExistsInDb(nodeId));

    const ErrorDbId errorDbId = insertHardlinkError(nodeId, SyncPath(Str("file1.txt")));

    // A node that is no longer in the sync database cannot be resolved by the action, but the reported error is removed so
    // that the card does not remain displayed forever. This is the path followed when retrying a resolution whose final
    // cleanup step failed.
    runUnlink(nodeId, SyncPath(Str("file1.txt")), errorDbId);
    CPPUNIT_ASSERT_MESSAGE("The error has not been deleted from the parameters database", !errorExistsInDb(errorDbId));
}

void TestUtilityUnlinkHardlinksJob::testInvalidPath() {
    const SyncName fileName = Str("file1.txt");
    const SyncName linkName = Str("link1.txt");
    const NodeId nodeId = createFileAndDbNode(fileName, "Hello, World!", linkName);

    // An absolute path must be rejected.
    ExitInfo exitInfo = runUnlinkExpect(nodeId, _localTempDir.path() / fileName);
    CPPUNIT_ASSERT_MESSAGE("The job must reject an absolute path", exitInfo.code() == ExitCode::InvalidOperation);

    // A path escaping the sync root must be rejected.
    exitInfo = runUnlinkExpect(nodeId, SyncPath(Str("..")) / Str("escaped.txt"));
    CPPUNIT_ASSERT_MESSAGE("The job must reject a path escaping the sync root", exitInfo.code() == ExitCode::InvalidOperation);

    CPPUNIT_ASSERT_MESSAGE("The node has been deleted from the database", nodeExistsInDb(nodeId));
    CPPUNIT_ASSERT_MESSAGE("The file has been deleted", pathExists(_localTempDir.path() / fileName));
    CPPUNIT_ASSERT_MESSAGE("The hardlink has been deleted", pathExists(_localTempDir.path() / linkName));
    CPPUNIT_ASSERT_MESSAGE("No rescue copy should have been made",
                           !pathExists(_localTempDir.path() / FileRescuer::rescueFolderName()));
}

void TestUtilityUnlinkHardlinksJob::testNodeIdMismatch() {
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

    const ExitInfo exitInfo = runUnlinkExpect(nodeId, SyncPath(fileName));
    CPPUNIT_ASSERT_MESSAGE("The job must reject a seed path that does not refer to the reported node",
                           exitInfo.code() == ExitCode::InvalidOperation);

    CPPUNIT_ASSERT_MESSAGE("The node has been deleted from the database", nodeExistsInDb(nodeId));
    CPPUNIT_ASSERT_MESSAGE("The file has been deleted", pathExists(filePath));
    CPPUNIT_ASSERT_MESSAGE("The hardlink has been deleted", pathExists(_localTempDir.path() / linkName));
    CPPUNIT_ASSERT_MESSAGE("No rescue copy should have been made",
                           !pathExists(_localTempDir.path() / FileRescuer::rescueFolderName()));
}

void TestUtilityUnlinkHardlinksJob::testSeedSelection() {
    const SyncName fileName = Str("file1.txt");
    const SyncName linkName = Str("link1.txt");
    const SyncName otherLinkName = Str("link2.txt");
    const NodeId nodeId = createFileAndDbNode(fileName, "Hello, World!", linkName);
    std::error_code ec;
    std::filesystem::create_hard_link(_localTempDir.path() / fileName, _localTempDir.path() / otherLinkName, ec);
    CPPUNIT_ASSERT_MESSAGE("Failed to create the hardlink: " + ec.message(), !ec);

    UtilityUnlinkHardlinksJob job(nullptr, 1, Poco::DynamicStruct(), nullptr);
    job._nodeId = nodeId;

    const SyncPath filePath = _localTempDir.path() / fileName;
    const SyncPath linkPath = _localTempDir.path() / linkName;
    const SyncPath otherLinkPath = _localTempDir.path() / otherLinkName;

    bool seedFound = false;
    SyncPath seedPath = filePath;
    CPPUNIT_ASSERT_MESSAGE("selectSeedPath failed", job.selectSeedPath({filePath, linkPath, otherLinkPath}, seedPath, seedFound));
    CPPUNIT_ASSERT_MESSAGE("The current seed path should have been kept", seedFound && seedPath == filePath);

    // The item located at the seed path is replaced by another file: another link of the reported node is selected, so that the
    // consistency check and the rescue copy are done on the file reported by the error.
    std::filesystem::remove(filePath, ec);
    CPPUNIT_ASSERT_MESSAGE("Failed to remove the file: " + ec.message(), !ec);
    {
        std::ofstream replacedFile(filePath, std::ios::binary);
        replacedFile << "Replaced";
        CPPUNIT_ASSERT_MESSAGE("Failed to create the replacement file", replacedFile.good());
    }

    seedPath = filePath;
    CPPUNIT_ASSERT_MESSAGE("selectSeedPath failed", job.selectSeedPath({filePath, linkPath, otherLinkPath}, seedPath, seedFound));
    CPPUNIT_ASSERT_MESSAGE("A link of the reported node should have been selected", seedFound && seedPath == linkPath);

    // When no link refers to the reported node anymore, nothing can be selected: the caller removes the node from the database
    // so that the file is downloaded again.
    std::filesystem::remove(linkPath, ec);
    std::filesystem::remove(otherLinkPath, ec);
    CPPUNIT_ASSERT_MESSAGE("Failed to remove the links: " + ec.message(), !ec);

    CPPUNIT_ASSERT_MESSAGE("selectSeedPath failed", job.selectSeedPath({filePath}, seedPath, seedFound));
    CPPUNIT_ASSERT_MESSAGE("No seed path should have been found", !seedFound);
}

void TestUtilityUnlinkHardlinksJob::testErrorRemoval() {
    const SyncName fileName = Str("file1.txt");
    const SyncName linkName = Str("link1.txt");
    const NodeId nodeId = createFileAndDbNode(fileName, "Hello, World!", linkName);

    const ErrorDbId errorDbId = insertHardlinkError(nodeId, SyncPath(fileName));
    const ErrorDbId otherErrorDbId = insertHardlinkError(NodeId("999999"), SyncPath(Str("other.txt")));
    CPPUNIT_ASSERT(errorExistsInDb(errorDbId));
    CPPUNIT_ASSERT(errorExistsInDb(otherErrorDbId));

    // A request rejected after the validation of the reported error keeps the error.
    const SyncPath absolutePath = _localTempDir.path() / fileName;
    const ErrorDbId absolutePathErrorDbId = insertHardlinkError(nodeId, absolutePath);
    const ExitInfo exitInfo = runUnlinkExpect(nodeId, absolutePath, absolutePathErrorDbId);
    CPPUNIT_ASSERT_MESSAGE("The job must reject an absolute path", exitInfo.code() == ExitCode::InvalidOperation);
    CPPUNIT_ASSERT_MESSAGE("The error has been deleted from the parameters database", errorExistsInDb(absolutePathErrorDbId));
    CPPUNIT_ASSERT_MESSAGE("The node has been deleted from the database", nodeExistsInDb(nodeId));

    // A successful request removes the reported error only.
    runUnlink(nodeId, SyncPath(fileName), errorDbId);
    CPPUNIT_ASSERT_MESSAGE("The node has not been deleted from the database", !nodeExistsInDb(nodeId));
    CPPUNIT_ASSERT_MESSAGE("The error has not been deleted from the parameters database", !errorExistsInDb(errorDbId));
    CPPUNIT_ASSERT_MESSAGE("Another error has been deleted from the parameters database", errorExistsInDb(otherErrorDbId));
}

void TestUtilityUnlinkHardlinksJob::testErrorMismatch() {
    const SyncName fileName = Str("file1.txt");
    const SyncName linkName = Str("link1.txt");
    const SyncPath relativePath(fileName);
    const NodeId nodeId = createFileAndDbNode(fileName, "Hello, World!", linkName);

    const auto checkRejected = [&](const ErrorDbId errorDbId, const std::string &message) {
        const ExitInfo exitInfo = runUnlinkExpect(nodeId, relativePath, errorDbId);
        CPPUNIT_ASSERT_MESSAGE(message, exitInfo.code() == ExitCode::InvalidOperation);
    };

    // An error that is not present in the parameters database.
    CPPUNIT_ASSERT(!errorExistsInDb(unknownErrorDbId));
    checkRejected(unknownErrorDbId, "The job must reject an error that does not exist");

    std::vector<ErrorDbId> errorDbIds;

    // An error reported for another node.
    errorDbIds.push_back(insertHardlinkError(NodeId("999999"), relativePath));
    checkRejected(errorDbIds.back(), "The job must reject an error reported for another node");

    // An error reported for another path.
    errorDbIds.push_back(insertHardlinkError(nodeId, SyncPath(Str("other.txt"))));
    checkRejected(errorDbIds.back(), "The job must reject an error reported for another path");

    // An error reported for another sync.
    Sync otherSync(2, 1, _localOtherDir.path(), "", "/remote2");
    CPPUNIT_ASSERT(ParmsDb::instance()->insertSync(otherSync));
    Error otherSyncError = makeHardlinkError(nodeId, relativePath);
    otherSyncError.setSyncDbId(otherSync.dbId());
    errorDbIds.push_back(insertParmsDbError(otherSyncError));
    checkRejected(errorDbIds.back(), "The job must reject an error reported for another sync");

    // An error that is not reported for a node.
    Error syncPalError = makeHardlinkError(nodeId, relativePath);
    syncPalError.setLevel(ErrorLevel::SyncPal);
    errorDbIds.push_back(insertParmsDbError(syncPalError));
    checkRejected(errorDbIds.back(), "The job must reject an error that is not reported for a node");

    // An error with another exit code.
    Error otherExitCodeError = makeHardlinkError(nodeId, relativePath);
    otherExitCodeError.setExitCode(ExitCode::DataError);
    errorDbIds.push_back(insertParmsDbError(otherExitCodeError));
    checkRejected(errorDbIds.back(), "The job must reject an error with another exit code");

    // An error with another exit cause.
    Error otherExitCauseError = makeHardlinkError(nodeId, relativePath);
    otherExitCauseError.setExitCause(ExitCause::FileAccessError);
    errorDbIds.push_back(insertParmsDbError(otherExitCauseError));
    checkRejected(errorDbIds.back(), "The job must reject an error with another exit cause");

    CPPUNIT_ASSERT_MESSAGE("The node has been deleted from the database", nodeExistsInDb(nodeId));
    CPPUNIT_ASSERT_MESSAGE("The file has been deleted", pathExists(_localTempDir.path() / fileName));
    CPPUNIT_ASSERT_MESSAGE("The hardlink has been deleted", pathExists(_localTempDir.path() / linkName));
    CPPUNIT_ASSERT_MESSAGE("No rescue copy should have been made",
                           !pathExists(_localTempDir.path() / FileRescuer::rescueFolderName()));
    for (const ErrorDbId errorDbId: errorDbIds) {
        CPPUNIT_ASSERT_MESSAGE("A mismatching error has been deleted from the parameters database", errorExistsInDb(errorDbId));
    }

    // The same request is accepted with the matching error.
    runUnlink(nodeId, relativePath);
    CPPUNIT_ASSERT_MESSAGE("The node has not been deleted from the database", !nodeExistsInDb(nodeId));
    CPPUNIT_ASSERT_MESSAGE("The file has not been deleted", !pathExists(_localTempDir.path() / fileName));
    CPPUNIT_ASSERT_MESSAGE("The hardlink has not been deleted", !pathExists(_localTempDir.path() / linkName));
}

void TestUtilityUnlinkHardlinksJob::testTmpBlacklistRemoval() {
    const SyncName fileName = Str("file1.txt");
    const SyncName linkName = Str("link1.txt");
    const SyncPath relativePath(fileName);
    const NodeId nodeId = createFileAndDbNode(fileName, "Hello, World!", linkName);
    const ErrorDbId errorDbId = insertHardlinkError(nodeId, relativePath);

    // Create the sync workers so that the temporary blacklist manager used by the sync engine is available, then blacklist the
    // item on both sides, as the sync engine does when an operation on the item has failed.
    static_cast<TestSyncPal *>(_syncPal.get())->createWorkers();
    _syncPal->blacklistTemporarily(nodeId, relativePath, ReplicaSide::Local);
    _syncPal->blacklistTemporarily(std::string("r_") + nodeId, relativePath, ReplicaSide::Remote);
    CPPUNIT_ASSERT_MESSAGE("The item is not blacklisted on the local side",
                           _syncPal->isTmpBlacklisted(relativePath, ReplicaSide::Local));
    CPPUNIT_ASSERT_MESSAGE("The item is not blacklisted on the remote side",
                           _syncPal->isTmpBlacklisted(relativePath, ReplicaSide::Remote));

    runUnlink(nodeId, relativePath, errorDbId);

    // The items have been removed from the temporary blacklist only once the node has been removed from the sync database, so
    // that the deletion of the links cannot propagate to the remote replica instead of triggering a new download.
    CPPUNIT_ASSERT_MESSAGE("The item is still blacklisted on the local side",
                           !_syncPal->isTmpBlacklisted(relativePath, ReplicaSide::Local));
    CPPUNIT_ASSERT_MESSAGE("The item is still blacklisted on the remote side",
                           !_syncPal->isTmpBlacklisted(relativePath, ReplicaSide::Remote));
    CPPUNIT_ASSERT_MESSAGE("The node has not been deleted from the database", !nodeExistsInDb(nodeId));
    CPPUNIT_ASSERT_MESSAGE("The file has not been deleted", !pathExists(_localTempDir.path() / fileName));
    CPPUNIT_ASSERT_MESSAGE("The hardlink has not been deleted", !pathExists(_localTempDir.path() / linkName));
}

void TestUtilityUnlinkHardlinksJob::testDirectoryNode() {
    // A directory node.
    const SyncName dirName = Str("dir1");
    const SyncPath dirPath = _localTempDir.path() / dirName;
    std::error_code ec;
    (void) std::filesystem::create_directory(dirPath, ec);
    CPPUNIT_ASSERT_MESSAGE("Failed to create the directory: " + ec.message(), !ec);
    const SyncPath childFilePath = dirPath / Str("child.txt");
    {
        std::ofstream file(childFilePath, std::ios::binary);
        file << "Hello, World!";
        CPPUNIT_ASSERT_MESSAGE("Failed to create the child file", file.good());
    }
    const NodeId dirNodeId = localNodeId(dirPath);
    insertDbNode(dirName, dirNodeId, NodeType::Directory);

    ExitInfo exitInfo = runUnlinkExpect(dirNodeId, SyncPath(dirName));
    CPPUNIT_ASSERT_MESSAGE("The job must reject a directory node", exitInfo.code() == ExitCode::InvalidOperation);

    // A file node whose reported item is a directory.
    const SyncName otherDirName = Str("dir2");
    const SyncPath otherDirPath = _localTempDir.path() / otherDirName;
    (void) std::filesystem::create_directory(otherDirPath, ec);
    CPPUNIT_ASSERT_MESSAGE("Failed to create the directory: " + ec.message(), !ec);
    const NodeId otherDirNodeId = localNodeId(otherDirPath);
    insertDbNode(otherDirName, otherDirNodeId, NodeType::File);

    exitInfo = runUnlinkExpect(otherDirNodeId, SyncPath(otherDirName));
    CPPUNIT_ASSERT_MESSAGE("The job must reject a reported item that is a directory",
                           exitInfo.code() == ExitCode::InvalidOperation);

    CPPUNIT_ASSERT_MESSAGE("The directory node has been deleted from the database", nodeExistsInDb(dirNodeId));
    CPPUNIT_ASSERT_MESSAGE("The file node has been deleted from the database", nodeExistsInDb(otherDirNodeId));
    CPPUNIT_ASSERT_MESSAGE("The directory has been deleted", pathExists(dirPath));
    CPPUNIT_ASSERT_MESSAGE("The directory content has been deleted", pathExists(childFilePath));
    CPPUNIT_ASSERT_MESSAGE("The other directory has been deleted", pathExists(otherDirPath));
}

void TestUtilityUnlinkHardlinksJob::testSymlinkKept() {
    const SyncName fileName = Str("file1.txt");
    const SyncName linkName = Str("link1.txt");
    const NodeId nodeId = createFileAndDbNode(fileName, "Hello, World!", linkName);

    // A symbolic link targeting the file is not a link of the file: it must be kept.
    const SyncPath symlinkPath = _localTempDir.path() / Str("symlink1.txt");
    IoError ioError = IoError::Success;
    const bool created = IoHelper::createSymlink(_localTempDir.path() / fileName, symlinkPath, false, ioError);
    CPPUNIT_ASSERT_MESSAGE("Failed to create the symbolic link: " + toString(ioError), created);

    runUnlink(nodeId, SyncPath(fileName));

    CPPUNIT_ASSERT_MESSAGE("The node has not been deleted from the database", !nodeExistsInDb(nodeId));
    CPPUNIT_ASSERT_MESSAGE("The file has not been deleted", !pathExists(_localTempDir.path() / fileName));
    CPPUNIT_ASSERT_MESSAGE("The hardlink has not been deleted", !pathExists(_localTempDir.path() / linkName));
    CPPUNIT_ASSERT_MESSAGE("The symbolic link has been deleted", isSymlink(symlinkPath));
    CPPUNIT_ASSERT_MESSAGE("No rescue copy should have been made",
                           !pathExists(_localTempDir.path() / FileRescuer::rescueFolderName()));
}

void TestUtilityUnlinkHardlinksJob::testSymlinkSeed() {
    const SyncName fileName = Str("file1.txt");
    const SyncName linkName = Str("link1.txt");
    const SyncName symlinkName = Str("symlink1.txt");
    const NodeId fileNodeId = createFileAndDbNode(fileName, "Hello, World!", linkName);

    // Report the symbolic link itself, with its own node id: its target and the links of the target must not be removed.
    const SyncPath symlinkPath = _localTempDir.path() / symlinkName;
    IoError ioError = IoError::Success;
    const bool created = IoHelper::createSymlink(_localTempDir.path() / fileName, symlinkPath, false, ioError);
    CPPUNIT_ASSERT_MESSAGE("Failed to create the symbolic link: " + toString(ioError), created);
    const NodeId symlinkNodeId = localNodeId(symlinkPath);
    CPPUNIT_ASSERT_MESSAGE("The symbolic link must have its own node id", symlinkNodeId != fileNodeId);
    insertDbNode(symlinkName, symlinkNodeId, NodeType::File);

    const ExitInfo exitInfo = runUnlinkExpect(symlinkNodeId, SyncPath(symlinkName));
    CPPUNIT_ASSERT_MESSAGE("The job must reject a symbolic link", exitInfo.code() == ExitCode::InvalidOperation);

    CPPUNIT_ASSERT_MESSAGE("The symbolic link node has been deleted from the database", nodeExistsInDb(symlinkNodeId));
    CPPUNIT_ASSERT_MESSAGE("The file node has been deleted from the database", nodeExistsInDb(fileNodeId));
    CPPUNIT_ASSERT_MESSAGE("The symbolic link has been deleted", isSymlink(symlinkPath));
    CPPUNIT_ASSERT_MESSAGE("The file has been deleted", pathExists(_localTempDir.path() / fileName));
    CPPUNIT_ASSERT_MESSAGE("The hardlink has been deleted", pathExists(_localTempDir.path() / linkName));
    CPPUNIT_ASSERT_MESSAGE("No rescue copy should have been made",
                           !pathExists(_localTempDir.path() / FileRescuer::rescueFolderName()));
}

} // namespace KDC

#endif
