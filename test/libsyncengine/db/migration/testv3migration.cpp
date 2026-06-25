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

#include "testv3migration.h"
#include "test_utility/testhelpers.h"
#include "test_utility/localtemporarydirectory.h"

#include "libcommon/utility/logiffail.h"
#include "libcommonserver/io/iohelper.h"
#include "libparms/db/parmsdb.h"
#include "libsyncengine/propagation/executor/filerescuer.h"

#include "mocks/libcommonserver/db/mockdb.h"
#include "jobs/network/kDrive_API/apitranslator.h"

#include <algorithm>
#include <time.h>

using namespace CppUnit;

namespace KDC {

namespace {

std::time_t now() {
    return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

} // namespace

void TestV3Migration::setUp() {
    TestBase::start();

    // Create SyncDb
    const auto syncDbPath = MockDb::makeDbName(1, 1, 1, 1);
    auto syncDbPtr = std::make_shared<SyncDb>(syncDbPath.string());
    syncDbPtr->init(KDRIVE_VERSION_STRING);
    syncDbPtr->setAutoDelete(true);

    _testObj = std::make_unique<V3MigrationMock>(syncDbPtr);
}

void TestV3Migration::tearDown() {
    _testObj->syncDb()->close();

    TestBase::stop();
    LogIfFailSettings::assertEnabled = true;
}

void TestV3Migration::createParmsDb(const SyncPath &syncDbPath, const SyncPath &localPath, SyncType syncType) const {
    bool alreadyExists = false;
    const std::filesystem::path parmsDbPath = MockDb::makeDbName(alreadyExists);
    ParmsDb::instance(parmsDbPath, "3.6.1", true, true);
    ParmsDb::instance()->setAutoDelete(true);

    const User user(1, 5555555, "123");
    (void) ParmsDb::instance()->insertUser(user);
    const Account acc(1, 12345678, user.dbId(), "account1");
    (void) ParmsDb::instance()->insertAccount(acc);
    Drive drive(_driveDbId, 99999991, acc.dbId(), "Drive 1", 2000000000, "#000000");
    (void) ParmsDb::instance()->insertDrive(drive);

    Sync sync;
    sync.setDbId(1);
    sync.setDriveDbId(drive.dbId());
    sync.setLocalPath(localPath);
    sync.setDbPath(syncDbPath);
    if (syncType == SyncType::Advanced) sync.setTargetPath(Str("/target"));
    (void) ParmsDb::instance()->insertSync(sync);
}

TestV3Migration::MigrationFileSetup TestV3Migration::setupSyncMigrationToLocalPrivateDir(const SyncPath &localPath) {
    /**
     * FS tree:
     *      *      Root
     *      |-- Common documents
     *      |-- Shared
     *      |-- .kdrive-cache
     *      |-- kDrive Rescue Folder
     *      |-- Private
     *      |-- a
     *      |   `-- b
     *      |-- c.txt
     */

    // File system setup.
    const SyncPath commonDocumentsPath =
            localPath / ApiTranslator::v3SpecialFolderNames.at(ApiTranslator::SpecialFolder::CommonDocuments);
    const SyncPath sharedPath = localPath / ApiTranslator::v3SpecialFolderNames.at(ApiTranslator::SpecialFolder::Shared);
    const SyncPath privatePath = localPath / ApiTranslator::v3SpecialFolderNames.at(ApiTranslator::SpecialFolder::Private);
    const SyncPath cachePath = localPath / CacheDirectory::name();
    const SyncPath rescueFolderPath = localPath / FileRescuer::rescueFolderName().filename();
    const SyncPath pathA = localPath / Str("a");
    const SyncPath pathB = pathA / Str("b");
    const SyncPath pathC = localPath / Str("c.txt");

    for (const auto &path: {commonDocumentsPath, sharedPath, privatePath, cachePath, rescueFolderPath, pathA})
        (void) std::filesystem::create_directories(path);

    for (const auto &path: {pathB, pathC}) std::ofstream file{path};

    // SyncDb setup.
    const time_t tLoc = now();
    const time_t tDrive = now();
    const auto rootId = _testObj->syncDb()->rootNode().nodeId();

    std::vector<DbNode> folderNodes;
    Count itemCount = 2; // Starts with 2 to avoid ID conflict with the root node IDs.
    for (const auto &path: {commonDocumentsPath, sharedPath, privatePath, cachePath, rescueFolderPath}) {
        const auto nodeId = std::to_string(itemCount);
        DbNode node(rootId, path.filename(), path.filename(), nodeId, nodeId, tLoc, tLoc, tDrive, NodeType::Directory, 0);
        (void) _testObj->syncDb()->insertNode(node);
        ++itemCount;
    }

    auto nodeId = std::to_string(itemCount);
    DbNode nodeA(rootId, pathA.filename(), pathA.filename(), nodeId, nodeId, tLoc, tLoc, tDrive, NodeType::Directory, 0);
    DbNodeId dbNodeId{0};
    bool constraintsError = false;
    (void) _testObj->syncDb()->insertNode(nodeA, dbNodeId, constraintsError);
    ++itemCount;

    nodeId = std::to_string(itemCount);
    DbNode nodeB(dbNodeId, pathB.filename(), pathB.filename(), nodeId, nodeId, tLoc, tLoc, tDrive, NodeType::Directory, 0);
    (void) _testObj->syncDb()->insertNode(nodeB);
    ++itemCount;

    nodeId = std::to_string(itemCount);
    DbNode nodeC(rootId, Str("c.txt"), Str("c.txt"), nodeId, nodeId, tLoc, tLoc, tDrive, NodeType::File, 0);
    (void) _testObj->syncDb()->insertNode(nodeC);

    MigrationFileSetup fileSetup;
    fileSetup.movedItems = {pathA, pathB, pathC};
    fileSetup.remainingItems = {commonDocumentsPath, sharedPath, privatePath, cachePath, rescueFolderPath};

    return fileSetup;
}

namespace {
SyncNameSet getDirContent(const SyncPath &dirPath) {
    using namespace std::filesystem;
    std::error_code ec;
    const auto dirIt = recursive_directory_iterator(dirPath, directory_options::skip_permission_denied, ec);

    SyncNameSet paths;
    for (const auto &dirEntry: dirIt) (void) paths.emplace(dirEntry.path().native().c_str());

    return paths;
}
} // namespace

void TestV3Migration::testMigrateLocalItemsToPrivateDir() {
    LocalTemporaryDirectory localTmpDir("testMigrateLocalItemsToPrivateDir");
    createParmsDb(_testObj->syncDb()->dbPath(), localTmpDir.path());

    const TestV3Migration::MigrationFileSetup fileSetup = setupSyncMigrationToLocalPrivateDir(localTmpDir.path());

    CPPUNIT_ASSERT(_testObj->migrateLocalItemsToPrivateDir());

    /**
     * Expected FS tree:
     *      *      Root
     *      |-- Common documents
     *      |-- Shared
     *      |-- .kdrive-cache
     *      |-- kDrive Rescue Folder
     *      |-- Private
     *             |-- Private
     *             |-- a
     *             |   `-- b
     *             | -- c.txt
     */

    for (const auto &path: fileSetup.movedItems) CPPUNIT_ASSERT(!std::filesystem::exists(path));
    for (const auto &path: fileSetup.remainingItems) CPPUNIT_ASSERT(std::filesystem::exists(path));

    const SyncPath privatePath =
            localTmpDir.path() / ApiTranslator::v3SpecialFolderNames.at(ApiTranslator::SpecialFolder::Private);
    const SyncPath privatePrivatePath = privatePath / privatePath.filename();
    const SyncPath pathA = privatePath / Str("a");
    const SyncPath pathB = pathA / Str("b");
    const SyncPath pathC = privatePath / Str("c.txt");

    CPPUNIT_ASSERT(std::filesystem::exists(privatePath));
    CPPUNIT_ASSERT(std::filesystem::exists(privatePrivatePath));
    CPPUNIT_ASSERT(std::filesystem::exists(pathA));
    CPPUNIT_ASSERT(std::filesystem::exists(pathB));
    CPPUNIT_ASSERT(std::filesystem::exists(pathC));

    const SyncNameSet expectedPrivateDirContent = {privatePrivatePath, pathA, pathB, pathC};
    CPPUNIT_ASSERT(getDirContent(privatePath) == expectedPrivateDirContent);

    SyncNameSet expectedSyncDirContent = {fileSetup.remainingItems.begin(), fileSetup.remainingItems.end()};
    (void) expectedSyncDirContent.insert(expectedPrivateDirContent.cbegin(), expectedPrivateDirContent.cend());
    CPPUNIT_ASSERT(getDirContent(localTmpDir.path()) == expectedSyncDirContent);

    // Check that the Private folder has been inserted in Sync DB properly.
    std::optional<NodeId> nodeIdLocalPrivate;
    bool found = false;
    CPPUNIT_ASSERT(_testObj->syncDb()->id(ReplicaSide::Local, privatePath.filename(), nodeIdLocalPrivate, found));
    CPPUNIT_ASSERT(found);
    DbNode privateDbNode;
    CPPUNIT_ASSERT(_testObj->syncDb()->node(ReplicaSide::Local, *nodeIdLocalPrivate, privateDbNode, found));
    CPPUNIT_ASSERT(found);
    CPPUNIT_ASSERT(privatePath.filename() == privateDbNode.nameLocal());
    CPPUNIT_ASSERT(privatePath.filename() == privateDbNode.nameRemote());
    CPPUNIT_ASSERT_EQUAL(_testObj->syncDb()->rootNode().nodeId(), privateDbNode.parentNodeId().value());
    NodeId privateLocalNodeId;
    CPPUNIT_ASSERT(IoHelper::getNodeId(privatePath, privateLocalNodeId));
    CPPUNIT_ASSERT_EQUAL(privateLocalNodeId, privateDbNode.nodeIdLocal().value());
    RemoteNodeId privateRemoteNodeId;
    (void) _testObj->getPrivateDirRemoteNodeId(_driveDbId, privateRemoteNodeId);
    CPPUNIT_ASSERT_EQUAL(privateRemoteNodeId, privateDbNode.nodeIdRemote().value());

    // Check that the parent node ID of the moved items is the private node ID.
    const DbNodeId privateDbNodeId = privateDbNode.nodeId();
    for (const auto &path: {privatePrivatePath, pathA, pathC}) {
        bool nodeFound = false;
        std::optional<NodeId> nodeIdLocal;
        CPPUNIT_ASSERT(_testObj->syncDb()->id(ReplicaSide::Local, std::filesystem::relative(path, localTmpDir.path()),
                                              nodeIdLocal, nodeFound));
        CPPUNIT_ASSERT(nodeFound);
        DbNode dbNode;
        CPPUNIT_ASSERT(_testObj->syncDb()->node(ReplicaSide::Local, *nodeIdLocal, dbNode, nodeFound));
        CPPUNIT_ASSERT(nodeFound);
        CPPUNIT_ASSERT_EQUAL(privateDbNodeId, dbNode.parentNodeId().value());
    }

    // Check that the parent node ID of the items that were not moved is still the root node ID.
    for (const auto &path: fileSetup.remainingItems) {
        bool nodeFound = false;
        std::optional<NodeId> nodeIdLocal;
        CPPUNIT_ASSERT(_testObj->syncDb()->id(ReplicaSide::Local, std::filesystem::relative(path, localTmpDir.path()),
                                              nodeIdLocal, nodeFound));
        DbNode dbNode;
        if (!nodeFound) continue;

        CPPUNIT_ASSERT(_testObj->syncDb()->node(ReplicaSide::Local, *nodeIdLocal, dbNode, nodeFound));
        CPPUNIT_ASSERT(nodeFound);
        CPPUNIT_ASSERT_EQUAL(_testObj->syncDb()->rootNode().nodeId(), dbNode.parentNodeId().value());
    }

    ParmsDb::instance()->close();
    ParmsDb::reset();
}

void TestV3Migration::testMigrationOfNonRootAdvancedSync() {
    LocalTemporaryDirectory localTmpDir("testMigrateLocalItemsToPrivateDir");
    createParmsDb(_testObj->syncDb()->dbPath(), localTmpDir.path(), SyncType::Advanced);

    const time_t tLoc = now();
    const time_t tDrive = now();
    const DbNode rootDbNode(_testObj->syncDb()->rootNode().nodeId(), std::nullopt, Str("Root"), Str("Root"), "local root id",
                            "remote target node id", tLoc, tLoc, tDrive, NodeType::Directory, 0, std::nullopt);

    bool found = false;
    _testObj->syncDb()->updateNode(rootDbNode, found);

    CPPUNIT_ASSERT(_testObj->migrateLocalItemsToPrivateDir());

    ParmsDb::instance()->close();
    ParmsDb::reset();
}

void TestV3Migration::testNoOpMigration() {
    CPPUNIT_ASSERT(_testObj->migrate("5.0.0"));
}
} // namespace KDC
