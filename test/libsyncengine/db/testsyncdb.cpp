/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#include "testsyncdb.h"
#include "test_utility/testhelpers.h"
#include "test_utility/localtemporarydirectory.h"

#include "libcommonserver/io/iohelper.h"
#include "libparms/db/parmsdb.h"

#include <algorithm>
#include <time.h>

using namespace CppUnit;

namespace KDC {

class DbNodeTest : public DbNode {
    public:
        DbNodeTest(std::optional<DbNodeId> parentNodeId, const SyncName &nameLocal, const SyncName &nameRemote,
                   const std::optional<NodeId> &nodeIdLocal, const std::optional<NodeId> &nodeIdRemote,
                   std::optional<SyncTime> created, std::optional<SyncTime> lastModifiedLocal,
                   std::optional<SyncTime> lastModifiedRemote, NodeType type, int64_t size,
                   const std::optional<std::string> &checksum, SyncFileStatus status = SyncFileStatus::Unknown,
                   bool syncing = false) {
            _nodeId = 0;
            _parentNodeId = parentNodeId;
            _nameLocal = nameLocal; // Don't check normalization
            _nameRemote = nameRemote; // Don't check normalization
            _nodeIdLocal = nodeIdLocal;
            _nodeIdRemote = nodeIdRemote;
            _created = created;
            _lastModifiedLocal = lastModifiedLocal;
            _lastModifiedRemote = lastModifiedRemote;
            _type = type;
            _size = size;
            _checksum = checksum;
            _status = status;
            _syncing = syncing;
        }

        inline void setNameLocal(const SyncName &name) override {
            _nameLocal = name; // Don't check normalization
        }
        inline void setNameRemote(const SyncName &name) override {
            _nameRemote = name; // Don't check normalization
        }
};

void TestSyncDb::setUp() {
    bool alreadyExists = false;
    const std::filesystem::path syncDbPath = Db::makeDbName(1, 1, 1, 1, alreadyExists);

    // Delete previous DB
    std::filesystem::remove(syncDbPath);

    // Create DB
    _testObj = new SyncDbMock(syncDbPath.string(), "3.4.0");
    _testObj->setAutoDelete(true);
}

void TestSyncDb::tearDown() {
    // Close and delete DB
    _testObj->close();
    delete _testObj;
}


void createParmsDb(const SyncPath &syncDbPath, const SyncPath &localPath) {
    bool alreadyExists = false;
    const std::filesystem::path parmsDbPath = ParmsDb::makeDbName(alreadyExists, true);
    ParmsDb::instance(parmsDbPath, "3.6.1", true, true);
    ParmsDb::instance()->setAutoDelete(true);

    const User user(1, 5555555, "123");
    ParmsDb::instance()->insertUser(user);
    const Account acc(1, 12345678, user.dbId());
    ParmsDb::instance()->insertAccount(acc);
    Drive drive(1, 99999991, acc.dbId(), "Drive 1", 2000000000, "#000000");
    ParmsDb::instance()->insertDrive(drive);

    Sync sync;
    sync.setDbId(1);
    sync.setDriveDbId(drive.dbId());
    sync.setLocalPath(localPath);
    sync.setDbPath(syncDbPath);
    ParmsDb::instance()->insertSync(sync);
}

// Get file names as actually encoded by the local file system.
std::map<NodeId, SyncName> getActualSystemFileNames(const SyncPath &localPath) {
    using namespace std::filesystem;
    std::error_code ec;
    const auto dirIt = recursive_directory_iterator(localPath, directory_options::skip_permission_denied, ec);

    std::map<NodeId, SyncName> localNames;
    for (const auto &dirEntry: dirIt) {
        NodeId nodeId;
        IoHelper::getNodeId(dirEntry.path(), nodeId);
        localNames.insert({nodeId, dirEntry.path().filename()});
    }

    return localNames;
}

struct SyncFilesInfo {
        std::vector<SyncName> localCreationFileNames;
        std::vector<NodeId> nodeIds;
};


SyncFilesInfo createSyncFiles(const SyncPath &localPath) {
    /**
     * FS tree:
     *      *      Root
     *      |-- a
     *      |   |-- c
     *      |   `-- nfd
     *      `-- b
     *          `-- nfc
     */

    const auto nfc = testhelpers::makeNfcSyncName();
    const auto nfd = testhelpers::makeNfdSyncName();

    const SyncPath path0 = localPath / "a";
    const SyncPath path1 = path0 / "c";
    const SyncPath path2 = path0 / nfc;
    const SyncPath path3 = localPath / "b";
    const SyncPath path4 = path3 / nfd;

    std::filesystem::create_directories(path0);
    std::filesystem::create_directories(path3);
    std::ofstream file1{path1};
    std::ofstream file2{path2};
    std::ofstream file4{path4};

    SyncFilesInfo syncFilesInfo;
    const std::vector<SyncPath> paths = {path0, path1, path2, path3, path4};

    std::transform(paths.cbegin(), paths.cend(), std::back_inserter(syncFilesInfo.nodeIds), [](const SyncPath &path) -> NodeId {
        NodeId nodeId;
        IoHelper::getNodeId(path, nodeId);
        return nodeId;
    });

    std::transform(paths.cbegin(), paths.cend(), std::back_inserter(syncFilesInfo.localCreationFileNames),
                   [](const SyncPath &path) -> SyncName { return path.filename(); });

    return syncFilesInfo;
}

std::vector<DbNode> TestSyncDb::setupSyncDb3_6_5(const std::vector<NodeId> &localNodeIds) {
    const time_t tLoc = std::time(0);
    const time_t tDrive = std::time(0);
    const auto rootId = _testObj->rootNode().nodeId();

    const auto nfc = testhelpers::makeNfcSyncName();
    const auto nfd = testhelpers::makeNfdSyncName();

    DbNode node0(rootId, Str("a"), Str("A"), localNodeIds[0], "id drive 0", tLoc, tLoc, tDrive, NodeType::Directory, 0, "cs 2.2");
    DbNodeTest node1(rootId, Str("c"), nfd, localNodeIds[1], "id drive 1", tLoc, tLoc, tDrive, NodeType::File, 0, "cs 2.2");
    DbNodeTest node2(rootId, nfd, Str("a"), localNodeIds[2], "id drive 2", tLoc, tLoc, tDrive, NodeType::File, 0, "cs 2.2");
    DbNode node3(rootId, Str("b"), Str("B"), localNodeIds[3], "id drive 3", tLoc, tLoc, tDrive, NodeType::Directory, 0, "cs 2.2");
    DbNodeTest node4(rootId, nfc, nfd, localNodeIds[4], "id drive 4", tLoc, tLoc, tDrive, NodeType::File, 0, "cs 2.2");

    {
        /**
         * DB tree:
         *      *      Root
         *      |-- a
         *      |   |-- c
         *      |   `-- nfd
         *      `-- b
         *          `-- nfc
         */
        bool constraintError = false;
        DbNodeId dbNodeId;

        _testObj->setNormalizationEnabled(false); // Will not normalize names in DB automatically.

        _testObj->insertNode(node0, dbNodeId, constraintError);
        node0.setNodeId(dbNodeId);
        node1.setParentNodeId(dbNodeId);
        node2.setParentNodeId(dbNodeId);

        _testObj->insertNode(node1, dbNodeId, constraintError);
        node1.setNodeId(dbNodeId);

        _testObj->insertNode(node2, dbNodeId, constraintError);
        node2.setNodeId(dbNodeId);

        _testObj->insertNode(node3, dbNodeId, constraintError);
        node3.setNodeId(dbNodeId);
        node4.setParentNodeId(dbNodeId);

        _testObj->insertNode(node4, dbNodeId, constraintError);
        node4.setNodeId(dbNodeId);
    }

    return {node0, node1, node2, node3, node4};
}

void TestSyncDb::testUpgradeTo3_6_5CheckNodeMap() {
    setupSyncDb3_6_5();

    SyncDb::NamedNodeMap namedNodeMap;
    _testObj->selectNamesWithDistinctEncodings(namedNodeMap);

    CPPUNIT_ASSERT_EQUAL(size_t(2), namedNodeMap.size());
    CPPUNIT_ASSERT_EQUAL(DbNodeId(4), namedNodeMap.at(4).dbNodeId);
    CPPUNIT_ASSERT_EQUAL(DbNodeId(6), namedNodeMap.at(6).dbNodeId);
}

void TestSyncDb::testUpgradeTo3_6_5() {
    LocalTemporaryDirectory localTmpDir("testUpgradeTo3_6_4");
    createParmsDb(_testObj->dbPath(), localTmpDir.path());
    const auto syncFilesInfo = createSyncFiles(localTmpDir.path());
    const auto initialDbNodes = setupSyncDb3_6_5(syncFilesInfo.nodeIds);

    _testObj->upgrade("3.6.4", "3.6.5");

    CPPUNIT_ASSERT_EQUAL(initialDbNodes.size(), syncFilesInfo.localCreationFileNames.size());

    const auto actualSystemFileNames = getActualSystemFileNames(localTmpDir.path());
    for (int i = 0; i < initialDbNodes.size(); ++i) {
        SyncName localName; // From the sync database.
        bool found = false;
        CPPUNIT_ASSERT(_testObj->name(ReplicaSide::Local, *initialDbNodes[i].nodeIdLocal(), localName, found) && found);

        CPPUNIT_ASSERT(localName == syncFilesInfo.localCreationFileNames[i]); // Name as used with the std API to create the file.
        const auto &actualLocalName = actualSystemFileNames.at(*initialDbNodes[i].nodeIdLocal());
        CPPUNIT_ASSERT(localName == actualLocalName); // Actual name on disk

        SyncName remoteName; // From the sync database.
        CPPUNIT_ASSERT(_testObj->name(ReplicaSide::Remote, *initialDbNodes[i].nodeIdRemote(), remoteName, found) && found);
        CPPUNIT_ASSERT(remoteName == Utility::normalizedSyncName(initialDbNodes[i].nameRemote()));
    }

    ParmsDb::instance()->close();
    ParmsDb::reset();
}

void TestSyncDb::testUpdateLocalName() {
    const auto nfc = testhelpers::makeNfcSyncName();
    const auto nfd = testhelpers::makeNfdSyncName();

    // Insert node
    const time_t tLoc = std::time(0);
    const time_t tDrive = std::time(0);

    DbNodeTest nodeDir1(_testObj->rootNode().nodeId(), nfc, Str("Dir drive 1"), "id loc 1", "id drive 1", tLoc, tLoc, tDrive,
                        NodeType::Directory, 0, std::nullopt);

    DbNodeId dbNodeIdDir1;
    bool constraintError = false;
    _testObj->insertNode(nodeDir1, dbNodeIdDir1, constraintError);

    // Update local name
    bool found = false;
    SyncName localName;
    _testObj->updateNodeLocalName(dbNodeIdDir1, nfd, found);
    CPPUNIT_ASSERT(_testObj->name(ReplicaSide::Local, *nodeDir1.nodeIdLocal(), localName, found) && found);
    CPPUNIT_ASSERT(localName == nfd);
}

void TestSyncDb::testNodes() {
    CPPUNIT_ASSERT(_testObj->exists());
    CPPUNIT_ASSERT(_testObj->clearNodes());

    // Insert node
    time_t tLoc = std::time(0);
    time_t tDrive = std::time(0);

    DbNode nodeDir1(0, _testObj->rootNode().nodeId(), Str("Dir loc 1"), Str("Dir drive 1"), "id loc 1", "id drive 1", tLoc, tLoc,
                    tDrive, NodeType::Directory, 0, std::nullopt);
    DbNode nodeDir2(0, _testObj->rootNode().nodeId(), Str("Dir loc 2"), Str("Dir drive 2"), "id loc 2", "id drive 2", tLoc, tLoc,
                    tDrive, NodeType::Directory, 0, std::nullopt);
    DbNode nodeDir3(0, _testObj->rootNode().nodeId(), Str("家屋香袈睷晦"), Str("家屋香袈睷晦"), "id loc 3", "id drive 3", tLoc,
                    tLoc, tDrive, NodeType::Directory, 0, std::nullopt);
    DbNodeId dbNodeIdDir1;
    DbNodeId dbNodeIdDir2;
    DbNodeId dbNodeIdDir3;
    bool constraintError = false;
    CPPUNIT_ASSERT(_testObj->insertNode(nodeDir1, dbNodeIdDir1, constraintError));
    CPPUNIT_ASSERT(_testObj->insertNode(nodeDir2, dbNodeIdDir2, constraintError));
    CPPUNIT_ASSERT(_testObj->insertNode(nodeDir3, dbNodeIdDir3, constraintError));

    DbNode nodeFile1(dbNodeIdDir1, Str("File loc 1.1"), Str("File drive 1.1"), "id loc 1.1", "id drive 1.1", tLoc, tLoc, tDrive,
                     NodeType::File, 0, "cs 1.1");
    DbNode nodeFile2(dbNodeIdDir1, Str("File loc 1.2"), Str("File drive 1.2"), "id loc 1.2", "id drive 1.2", tLoc, tLoc, tDrive,
                     NodeType::File, 0, "cs 1.2");
    DbNode nodeFile3(dbNodeIdDir1, Str("File loc 1.3"), Str("File drive 1.3"), "id loc 1.3", "id drive 1.3", tLoc, tLoc, tDrive,
                     NodeType::File, 0, "cs 1.3");
    DbNode nodeFile4(dbNodeIdDir1, Str("File loc 1.4"), Str("File drive 1.4"), "id loc 1.4", "id drive 1.4", tLoc, tLoc, tDrive,
                     NodeType::File, 0, "cs 1.4");
    DbNode nodeFile5(dbNodeIdDir1, Str("File loc 1.5"), Str("File drive 1.5"), "id loc 1.5", "id drive 1.5", tLoc, tLoc, tDrive,
                     NodeType::File, 0, "cs 1.5");
    DbNodeId dbNodeIdFile1;
    DbNodeId dbNodeIdFile2;
    DbNodeId dbNodeIdFile3;
    DbNodeId dbNodeIdFile4;
    DbNodeId dbNodeIdFile5;
    CPPUNIT_ASSERT(_testObj->insertNode(nodeFile1, dbNodeIdFile1, constraintError));
    CPPUNIT_ASSERT(_testObj->insertNode(nodeFile2, dbNodeIdFile2, constraintError));
    CPPUNIT_ASSERT(_testObj->insertNode(nodeFile3, dbNodeIdFile3, constraintError));
    CPPUNIT_ASSERT(_testObj->insertNode(nodeFile4, dbNodeIdFile4, constraintError));
    CPPUNIT_ASSERT(_testObj->insertNode(nodeFile5, dbNodeIdFile5, constraintError));

    DbNode nodeFile6(dbNodeIdDir2, Str("File loc 2.1"), Str("File drive 2.1"), "id loc 2.1", "id drive 2.1", tLoc, tLoc, tDrive,
                     NodeType::File, 0, "cs 2.1");
    DbNodeId dbNodeIdFile6;
    CPPUNIT_ASSERT(_testObj->insertNode(nodeFile6, dbNodeIdFile6, constraintError));

    // Insert node with NFD-normalized name
    const SyncName nfdEncodedName = testhelpers::makeNfdSyncName();
    DbNodeTest nodeFile7(dbNodeIdDir1, nfdEncodedName, nfdEncodedName, "id loc 2.2", "id drive 2.2", tLoc, tLoc, tDrive,
                         NodeType::File, 0, "cs 2.2");
    DbNodeId dbNodeIdFile7;
    CPPUNIT_ASSERT(_testObj->insertNode(nodeFile7, dbNodeIdFile7, constraintError));

    SyncName localName;
    SyncName remoteName;
    bool found = false;
    CPPUNIT_ASSERT(_testObj->name(ReplicaSide::Local, nodeFile7.nodeIdLocal().value(), localName, found) && found);
    CPPUNIT_ASSERT(_testObj->name(ReplicaSide::Remote, nodeFile7.nodeIdRemote().value(), remoteName, found) && found);

    const SyncName nfcEncodedName = testhelpers::makeNfcSyncName();
    CPPUNIT_ASSERT(localName == nfdEncodedName); // Local name is not normalized.
    CPPUNIT_ASSERT(remoteName == nfcEncodedName); // Remote name is normalized.

    // Update node
    nodeFile6.setNodeId(dbNodeIdFile6);
    nodeFile6.setNameLocal(nodeFile6.nameLocal() + Str("new"));
    nodeFile6.setNameRemote(nodeFile6.nameRemote() + Str("new"));
    nodeFile6.setLastModifiedLocal(nodeFile6.lastModifiedLocal().value() + 10);
    nodeFile6.setLastModifiedRemote(nodeFile6.lastModifiedRemote().value() + 100);
    nodeFile6.setChecksum(nodeFile6.checksum().value() + "new");
    CPPUNIT_ASSERT(_testObj->updateNode(nodeFile6, found) && found);

    SyncName name;
    std::optional<SyncTime> time;
    std::optional<std::string> cs;
    CPPUNIT_ASSERT(_testObj->name(ReplicaSide::Local, nodeFile6.nodeIdLocal().value(), name, found) && found);
    CPPUNIT_ASSERT(name == nodeFile6.nameLocal());
    CPPUNIT_ASSERT(_testObj->name(ReplicaSide::Remote, nodeFile6.nodeIdRemote().value(), name, found) && found);
    CPPUNIT_ASSERT(name == nodeFile6.nameRemote());
    CPPUNIT_ASSERT(_testObj->lastModified(ReplicaSide::Local, nodeFile6.nodeIdLocal().value(), time, found) && found);
    CPPUNIT_ASSERT_EQUAL(nodeFile6.lastModifiedLocal().value(), time.value());
    CPPUNIT_ASSERT(_testObj->lastModified(ReplicaSide::Remote, nodeFile6.nodeIdRemote().value(), time, found) && found);
    CPPUNIT_ASSERT_EQUAL(nodeFile6.lastModifiedRemote().value(), time.value());
    CPPUNIT_ASSERT(_testObj->checksum(ReplicaSide::Local, nodeFile6.nodeIdLocal().value(), cs, found) && found);
    CPPUNIT_ASSERT_EQUAL(nodeFile6.checksum().value(), cs.value());

    // Update node with NFD-normalized name
    nodeFile7.setNodeId(dbNodeIdFile7);
    nodeFile7.setNameLocal(nfdEncodedName);
    nodeFile7.setNameRemote(nfdEncodedName);
    CPPUNIT_ASSERT(_testObj->updateNode(nodeFile7, found) && found);

    CPPUNIT_ASSERT(_testObj->name(ReplicaSide::Local, nodeFile7.nodeIdLocal().value(), localName, found) && found);
    CPPUNIT_ASSERT(_testObj->name(ReplicaSide::Remote, nodeFile7.nodeIdRemote().value(), remoteName, found) && found);

    CPPUNIT_ASSERT(localName == nfdEncodedName); // Local name is not normalized.
    CPPUNIT_ASSERT(remoteName == nfcEncodedName); // Remote name is normalized.

    // Delete node
    CPPUNIT_ASSERT(_testObj->deleteNode(dbNodeIdFile6, found) && found);

    // id
    std::optional<NodeId> nodeIdRoot;
    CPPUNIT_ASSERT(_testObj->id(ReplicaSide::Local, SyncPath(""), nodeIdRoot, found) && found);
    CPPUNIT_ASSERT_EQUAL(nodeIdRoot.value(), _testObj->rootNode().nodeIdLocal().value());
    CPPUNIT_ASSERT(_testObj->id(ReplicaSide::Remote, SyncPath(""), nodeIdRoot, found) && found);
    CPPUNIT_ASSERT_EQUAL(nodeIdRoot.value(), _testObj->rootNode().nodeIdRemote().value());
    std::optional<NodeId> nodeIdFile3;
    CPPUNIT_ASSERT(_testObj->id(ReplicaSide::Local, SyncPath("Dir loc 1/File loc 1.3"), nodeIdFile3, found) && found);
    CPPUNIT_ASSERT_EQUAL(nodeIdFile3.value(), nodeFile3.nodeIdLocal().value());
    CPPUNIT_ASSERT(_testObj->id(ReplicaSide::Remote, SyncPath("Dir drive 1/File drive 1.3"), nodeIdFile3, found) && found);
    CPPUNIT_ASSERT_EQUAL(nodeIdFile3.value(), nodeFile3.nodeIdRemote().value());
    std::optional<NodeId> nodeIdFile4;
    CPPUNIT_ASSERT(_testObj->id(ReplicaSide::Remote, SyncPath("Dir drive 1/File drive 1.4"), nodeIdFile4, found) && found);
    CPPUNIT_ASSERT(nodeIdFile4);
    std::optional<NodeId> nodeIdFile5;
    CPPUNIT_ASSERT(_testObj->id(ReplicaSide::Local, SyncPath("Dir loc 1/File loc 1.5"), nodeIdFile5, found) && found);
    CPPUNIT_ASSERT(nodeIdFile5);

    // type
    NodeType typeDir1;
    CPPUNIT_ASSERT(_testObj->type(ReplicaSide::Local, nodeDir1.nodeIdLocal().value(), typeDir1, found) && found);
    CPPUNIT_ASSERT_EQUAL(typeDir1, nodeDir1.type());
    CPPUNIT_ASSERT(_testObj->type(ReplicaSide::Remote, nodeDir1.nodeIdRemote().value(), typeDir1, found) && found);
    CPPUNIT_ASSERT_EQUAL(typeDir1, nodeDir1.type());
    NodeType typeFile3;
    CPPUNIT_ASSERT(_testObj->type(ReplicaSide::Local, nodeFile3.nodeIdLocal().value(), typeFile3, found) && found);
    CPPUNIT_ASSERT_EQUAL(typeFile3, nodeFile3.type());
    CPPUNIT_ASSERT(_testObj->type(ReplicaSide::Remote, nodeFile3.nodeIdRemote().value(), typeFile3, found) && found);
    CPPUNIT_ASSERT_EQUAL(typeFile3, nodeFile3.type());

    // lastModified
    std::optional<SyncTime> timeDir1;
    CPPUNIT_ASSERT(_testObj->lastModified(ReplicaSide::Local, nodeDir1.nodeIdLocal().value(), timeDir1, found) && found);
    CPPUNIT_ASSERT_EQUAL(timeDir1.value(), nodeDir1.lastModifiedLocal().value());
    CPPUNIT_ASSERT(_testObj->lastModified(ReplicaSide::Remote, nodeDir1.nodeIdRemote().value(), timeDir1, found) && found);
    CPPUNIT_ASSERT_EQUAL(timeDir1.value(), nodeDir1.lastModifiedRemote().value());
    std::optional<SyncTime> timeFile3;
    CPPUNIT_ASSERT(_testObj->lastModified(ReplicaSide::Local, nodeFile3.nodeIdLocal().value(), timeFile3, found) && found);
    CPPUNIT_ASSERT_EQUAL(timeFile3.value(), nodeFile3.lastModifiedLocal().value());
    CPPUNIT_ASSERT(_testObj->lastModified(ReplicaSide::Remote, nodeFile3.nodeIdRemote().value(), timeFile3, found) && found);
    CPPUNIT_ASSERT_EQUAL(timeFile3.value(), nodeFile3.lastModifiedRemote().value());

    // parent
    NodeId parentNodeidFile3;
    CPPUNIT_ASSERT(_testObj->parent(ReplicaSide::Local, nodeFile3.nodeIdLocal().value(), parentNodeidFile3, found) && found);
    CPPUNIT_ASSERT(nodeDir1.nodeIdLocal() == parentNodeidFile3);
    CPPUNIT_ASSERT(_testObj->parent(ReplicaSide::Remote, nodeFile3.nodeIdRemote().value(), parentNodeidFile3, found) && found);
    CPPUNIT_ASSERT(nodeDir1.nodeIdRemote() == parentNodeidFile3);
    NodeId parentNodeidDir1;
    CPPUNIT_ASSERT(_testObj->parent(ReplicaSide::Local, nodeDir1.nodeIdLocal().value(), parentNodeidDir1, found) && found);
    CPPUNIT_ASSERT(_testObj->rootNode().nodeIdLocal() == parentNodeidDir1);
    CPPUNIT_ASSERT(_testObj->parent(ReplicaSide::Remote, nodeDir1.nodeIdRemote().value(), parentNodeidDir1, found) && found);
    CPPUNIT_ASSERT(_testObj->rootNode().nodeIdRemote() == parentNodeidDir1);

    // path
    SyncPath pathFile3;
    CPPUNIT_ASSERT(_testObj->path(ReplicaSide::Local, nodeFile3.nodeIdLocal().value(), pathFile3, found) && found);
    CPPUNIT_ASSERT_EQUAL(SyncPath(Str("Dir loc 1/File loc 1.3")), pathFile3);
    CPPUNIT_ASSERT(_testObj->path(ReplicaSide::Remote, nodeFile3.nodeIdRemote().value(), pathFile3, found) && found);
    CPPUNIT_ASSERT_EQUAL(SyncPath(Str("Dir drive 1/File drive 1.3")), pathFile3);
    SyncPath pathDir1;
    CPPUNIT_ASSERT(_testObj->path(ReplicaSide::Local, nodeDir1.nodeIdLocal().value(), pathDir1, found) && found);
    CPPUNIT_ASSERT_EQUAL(SyncPath(Str("Dir loc 1")), pathDir1);
    CPPUNIT_ASSERT(_testObj->path(ReplicaSide::Remote, nodeDir1.nodeIdRemote().value(), pathDir1, found) && found);
    CPPUNIT_ASSERT_EQUAL(SyncPath(Str("Dir drive 1")), pathDir1);
    SyncPath pathDir3;
    CPPUNIT_ASSERT(_testObj->path(ReplicaSide::Local, nodeDir3.nodeIdLocal().value(), pathDir3, found) && found);
    CPPUNIT_ASSERT_EQUAL(SyncPath(Str("家屋香袈睷晦")), pathDir3);
    SyncPath pathRoot;
    CPPUNIT_ASSERT(_testObj->path(ReplicaSide::Local, _testObj->rootNode().nodeIdLocal().value(), pathRoot, found) && found);
    CPPUNIT_ASSERT_EQUAL(SyncPath(Str("")), pathRoot);
    CPPUNIT_ASSERT(_testObj->path(ReplicaSide::Remote, _testObj->rootNode().nodeIdRemote().value(), pathRoot, found) && found);
    CPPUNIT_ASSERT_EQUAL(SyncPath(Str("")), pathRoot);

    // name
    CPPUNIT_ASSERT(_testObj->name(ReplicaSide::Local, nodeFile3.nodeIdLocal().value(), name, found) && found);
    CPPUNIT_ASSERT(name == nodeFile3.nameLocal());
    CPPUNIT_ASSERT(_testObj->name(ReplicaSide::Remote, nodeFile3.nodeIdRemote().value(), name, found) && found);
    CPPUNIT_ASSERT(name == nodeFile3.nameRemote());
    CPPUNIT_ASSERT(_testObj->name(ReplicaSide::Local, nodeDir1.nodeIdLocal().value(), name, found) && found);
    CPPUNIT_ASSERT(name == nodeDir1.nameLocal());
    CPPUNIT_ASSERT(_testObj->name(ReplicaSide::Remote, nodeDir1.nodeIdRemote().value(), name, found) && found);
    CPPUNIT_ASSERT(name == nodeDir1.nameRemote());
    CPPUNIT_ASSERT(_testObj->name(ReplicaSide::Local, nodeDir3.nodeIdLocal().value(), name, found) && found);
    CPPUNIT_ASSERT(name == nodeDir3.nameLocal());
    CPPUNIT_ASSERT(_testObj->name(ReplicaSide::Remote, nodeDir3.nodeIdRemote().value(), name, found) && found);
    CPPUNIT_ASSERT(name == nodeDir3.nameRemote());

    // checksum
    CPPUNIT_ASSERT(_testObj->checksum(ReplicaSide::Local, _testObj->rootNode().nodeIdLocal().value(), cs, found) && found);
    CPPUNIT_ASSERT(!cs);
    CPPUNIT_ASSERT(_testObj->checksum(ReplicaSide::Remote, nodeDir1.nodeIdRemote().value(), cs, found) && found);
    CPPUNIT_ASSERT(!cs);
    CPPUNIT_ASSERT(_testObj->checksum(ReplicaSide::Local, nodeFile3.nodeIdLocal().value(), cs, found) && found);
    CPPUNIT_ASSERT_EQUAL(nodeFile3.checksum().value(), cs.value());
    CPPUNIT_ASSERT(_testObj->checksum(ReplicaSide::Remote, nodeFile3.nodeIdRemote().value(), cs, found) && found);
    CPPUNIT_ASSERT_EQUAL(nodeFile3.checksum().value(), cs.value());

    // ids
    std::vector<NodeId> ids;
    CPPUNIT_ASSERT(_testObj->ids(ReplicaSide::Local, ids, found) && found);
    CPPUNIT_ASSERT_EQUAL(size_t(10), ids.size());
    CPPUNIT_ASSERT(ids[0] == _testObj->rootNode().nodeIdLocal());
    CPPUNIT_ASSERT(ids[8] == nodeFile5.nodeIdLocal());
    ids.clear();
    CPPUNIT_ASSERT(_testObj->ids(ReplicaSide::Remote, ids, found) && found);
    CPPUNIT_ASSERT_EQUAL(size_t(10), ids.size());
    CPPUNIT_ASSERT(ids[0] == _testObj->rootNode().nodeIdRemote());
    CPPUNIT_ASSERT(ids[8] == nodeFile5.nodeIdRemote());

    // ancestor
    bool ancestor;
    CPPUNIT_ASSERT(_testObj->ancestor(ReplicaSide::Local, _testObj->rootNode().nodeIdLocal().value(),
                                      _testObj->rootNode().nodeIdLocal().value(), ancestor, found) &&
                   found);
    CPPUNIT_ASSERT(ancestor);
    CPPUNIT_ASSERT(_testObj->ancestor(ReplicaSide::Remote, _testObj->rootNode().nodeIdRemote().value(),
                                      _testObj->rootNode().nodeIdRemote().value(), ancestor, found) &&
                   found);
    CPPUNIT_ASSERT(ancestor);
    CPPUNIT_ASSERT(_testObj->ancestor(ReplicaSide::Local, _testObj->rootNode().nodeIdLocal().value(),
                                      nodeDir1.nodeIdLocal().value(), ancestor, found) &&
                   found);
    CPPUNIT_ASSERT(ancestor);
    CPPUNIT_ASSERT(_testObj->ancestor(ReplicaSide::Remote, _testObj->rootNode().nodeIdRemote().value(),
                                      nodeDir1.nodeIdRemote().value(), ancestor, found) &&
                   found);
    CPPUNIT_ASSERT(ancestor);
    CPPUNIT_ASSERT(_testObj->ancestor(ReplicaSide::Local, _testObj->rootNode().nodeIdLocal().value(),
                                      nodeFile3.nodeIdLocal().value(), ancestor, found) &&
                   found);
    CPPUNIT_ASSERT(ancestor);
    CPPUNIT_ASSERT(_testObj->ancestor(ReplicaSide::Remote, _testObj->rootNode().nodeIdRemote().value(),
                                      nodeFile3.nodeIdRemote().value(), ancestor, found) &&
                   found);
    CPPUNIT_ASSERT(ancestor);
    CPPUNIT_ASSERT(_testObj->ancestor(ReplicaSide::Local, nodeDir1.nodeIdLocal().value(), nodeFile3.nodeIdLocal().value(),
                                      ancestor, found) &&
                   found);
    CPPUNIT_ASSERT(ancestor);
    CPPUNIT_ASSERT(_testObj->ancestor(ReplicaSide::Remote, nodeDir1.nodeIdRemote().value(), nodeFile3.nodeIdRemote().value(),
                                      ancestor, found) &&
                   found);
    CPPUNIT_ASSERT(ancestor);
    CPPUNIT_ASSERT(_testObj->ancestor(ReplicaSide::Local, nodeFile2.nodeIdLocal().value(), nodeDir1.nodeIdLocal().value(),
                                      ancestor, found) &&
                   found);
    CPPUNIT_ASSERT(!ancestor);
    CPPUNIT_ASSERT(_testObj->ancestor(ReplicaSide::Remote, nodeFile2.nodeIdRemote().value(), nodeDir1.nodeIdRemote().value(),
                                      ancestor, found) &&
                   found);
    CPPUNIT_ASSERT(!ancestor);
    CPPUNIT_ASSERT(_testObj->ancestor(ReplicaSide::Local, nodeFile1.nodeIdLocal().value(), nodeFile2.nodeIdLocal().value(),
                                      ancestor, found) &&
                   found);
    CPPUNIT_ASSERT(!ancestor);
    CPPUNIT_ASSERT(_testObj->ancestor(ReplicaSide::Remote, nodeFile1.nodeIdRemote().value(), nodeFile2.nodeIdRemote().value(),
                                      ancestor, found) &&
                   found);
    CPPUNIT_ASSERT(!ancestor);

    // dbId
    DbNodeId dbNodeId;
    CPPUNIT_ASSERT(_testObj->dbId(ReplicaSide::Local, _testObj->rootNode().nodeIdLocal().value(), dbNodeId, found) && found);
    CPPUNIT_ASSERT(dbNodeId == _testObj->rootNode().nodeId());
    CPPUNIT_ASSERT(_testObj->dbId(ReplicaSide::Remote, _testObj->rootNode().nodeIdRemote().value(), dbNodeId, found) && found);
    CPPUNIT_ASSERT(dbNodeId == _testObj->rootNode().nodeId());
    CPPUNIT_ASSERT(_testObj->dbId(ReplicaSide::Local, nodeFile3.nodeIdLocal().value(), dbNodeId, found) && found);
    CPPUNIT_ASSERT_EQUAL(dbNodeIdFile3, dbNodeId);
    CPPUNIT_ASSERT(_testObj->dbId(ReplicaSide::Remote, nodeFile3.nodeIdRemote().value(), dbNodeId, found) && found);
    CPPUNIT_ASSERT_EQUAL(dbNodeIdFile3, dbNodeId);

    // id
    NodeId nodeId;
    CPPUNIT_ASSERT(_testObj->id(ReplicaSide::Local, _testObj->rootNode().nodeId(), nodeId, found) && found);
    CPPUNIT_ASSERT(nodeId == _testObj->rootNode().nodeIdLocal().value());
    CPPUNIT_ASSERT(_testObj->id(ReplicaSide::Remote, _testObj->rootNode().nodeId(), nodeId, found) && found);
    CPPUNIT_ASSERT(nodeId == _testObj->rootNode().nodeIdRemote().value());
    CPPUNIT_ASSERT(_testObj->id(ReplicaSide::Local, dbNodeIdFile3, nodeId, found) && found);
    CPPUNIT_ASSERT(nodeId == nodeFile3.nodeIdLocal().value());
    CPPUNIT_ASSERT(_testObj->id(ReplicaSide::Remote, dbNodeIdFile3, nodeId, found) && found);
    CPPUNIT_ASSERT(nodeId == nodeFile3.nodeIdRemote().value());

    // node from local ID
    {
        DbNode nodeDirLocal;
        CPPUNIT_ASSERT(_testObj->node(ReplicaSide::Local, nodeDir1.nodeIdLocal().value(), nodeDirLocal, found) && found);
        CPPUNIT_ASSERT_EQUAL(dbNodeIdDir1, nodeDirLocal.nodeId());
        CPPUNIT_ASSERT_EQUAL(nodeDir1.parentNodeId().value(), nodeDirLocal.parentNodeId().value());
        CPPUNIT_ASSERT(nodeDirLocal.nameLocal() == nodeDir1.nameLocal());
        CPPUNIT_ASSERT_EQUAL(nodeDir1.created().value(), nodeDirLocal.created().value());
        CPPUNIT_ASSERT_EQUAL(nodeDir1.lastModifiedLocal().value(), nodeDirLocal.lastModifiedLocal().value());
        CPPUNIT_ASSERT_EQUAL(nodeDir1.type(), nodeDirLocal.type());
        CPPUNIT_ASSERT_EQUAL(nodeDir1.size(), nodeDirLocal.size());

        DbNode nodeDirRemote;
        CPPUNIT_ASSERT(_testObj->node(ReplicaSide::Remote, nodeDir1.nodeIdRemote().value(), nodeDirRemote, found) && found);
        CPPUNIT_ASSERT(nodeDirRemote.nameRemote() == nodeDir1.nameRemote());
        CPPUNIT_ASSERT_EQUAL(nodeDir1.lastModifiedRemote().value(), nodeDirRemote.lastModifiedRemote().value());

        DbNode nodeFileLocal;
        CPPUNIT_ASSERT(_testObj->node(ReplicaSide::Local, nodeFile1.nodeIdLocal().value(), nodeFileLocal, found) && found);
        CPPUNIT_ASSERT_EQUAL(dbNodeIdFile1, nodeFileLocal.nodeId());
        CPPUNIT_ASSERT_EQUAL(nodeFile1.parentNodeId().value(), nodeFileLocal.parentNodeId().value());
        CPPUNIT_ASSERT(nodeFileLocal.nameLocal() == nodeFile1.nameLocal());
        CPPUNIT_ASSERT_EQUAL(nodeFile1.created().value(), nodeFileLocal.created().value());
        CPPUNIT_ASSERT_EQUAL(nodeFile1.lastModifiedLocal().value(), nodeFileLocal.lastModifiedLocal().value());
        CPPUNIT_ASSERT_EQUAL(nodeFile1.type(), nodeFileLocal.type());
        CPPUNIT_ASSERT_EQUAL(nodeFile1.size(), nodeFileLocal.size());

        DbNode nodeFileRemote;
        CPPUNIT_ASSERT(_testObj->node(ReplicaSide::Remote, nodeFile1.nodeIdRemote().value(), nodeFileRemote, found) && found);
        CPPUNIT_ASSERT(nodeFileRemote.nameRemote() == nodeFile1.nameRemote());
        CPPUNIT_ASSERT(nodeFileRemote.lastModifiedRemote() == nodeFile1.lastModifiedRemote());
    }

    // dbIds
    {
        std::unordered_set<DbNodeId> dbIds;
        CPPUNIT_ASSERT(_testObj->dbIds(dbIds, found) && found);
        CPPUNIT_ASSERT_EQUAL(size_t(10), dbIds.size());
        CPPUNIT_ASSERT(dbIds.contains(dbNodeIdDir1));
        CPPUNIT_ASSERT(dbIds.contains(dbNodeIdFile3));
    }

    // paths from db ID
    {
        SyncPath localPathFile3;
        SyncPath remotePathFile3;
        CPPUNIT_ASSERT(_testObj->path(dbNodeIdFile3, localPathFile3, remotePathFile3, found) && found);
        CPPUNIT_ASSERT_EQUAL(SyncPath(Str("Dir loc 1/File loc 1.3")), localPathFile3);
        CPPUNIT_ASSERT_EQUAL(SyncPath(Str("Dir drive 1/File drive 1.3")), remotePathFile3);

        SyncPath localPathDir1;
        SyncPath remotePathDir1;
        CPPUNIT_ASSERT(_testObj->path(dbNodeIdDir1, localPathDir1, remotePathDir1, found) && found);
        CPPUNIT_ASSERT_EQUAL(SyncPath(Str("Dir loc 1")), localPathDir1);
        CPPUNIT_ASSERT_EQUAL(SyncPath(Str("Dir drive 1")), remotePathDir1);

        SyncPath localPathDir3;
        SyncPath remotePathDir3;
        CPPUNIT_ASSERT(_testObj->path(dbNodeIdDir3, localPathDir3, remotePathDir3, found) && found);
        CPPUNIT_ASSERT_EQUAL(SyncPath(Str("家屋香袈睷晦")), localPathDir3);
        CPPUNIT_ASSERT_EQUAL(SyncPath(Str("家屋香袈睷晦")), remotePathDir3);

        SyncPath localPathRoot;
        SyncPath remotePathRoot;
        CPPUNIT_ASSERT(_testObj->path(_testObj->rootNode().nodeId(), localPathRoot, remotePathRoot, found) && found);
        CPPUNIT_ASSERT_EQUAL(SyncPath(""), localPathRoot);
        CPPUNIT_ASSERT_EQUAL(SyncPath(""), remotePathRoot);
    }

    // node from db ID
    {
        DbNode nodeDir;
        CPPUNIT_ASSERT(_testObj->node(dbNodeIdDir1, nodeDir, found) && found);
        CPPUNIT_ASSERT_EQUAL(dbNodeIdDir1, nodeDir.nodeId());
        CPPUNIT_ASSERT_EQUAL(nodeDir1.parentNodeId().value(), nodeDir.parentNodeId().value());
        CPPUNIT_ASSERT(nodeDir.nameLocal() == nodeDir1.nameLocal());
        CPPUNIT_ASSERT(nodeDir.nameRemote() == nodeDir1.nameRemote());
        CPPUNIT_ASSERT_EQUAL(nodeDir1.created().value(), nodeDir.created().value());
        CPPUNIT_ASSERT_EQUAL(nodeDir1.lastModifiedLocal().value(), nodeDir.lastModifiedLocal().value());
        CPPUNIT_ASSERT_EQUAL(nodeDir1.lastModifiedRemote().value(), nodeDir.lastModifiedRemote().value());
        CPPUNIT_ASSERT_EQUAL(nodeDir1.type(), nodeDir.type());
        CPPUNIT_ASSERT_EQUAL(nodeDir1.size(), nodeDir.size());

        DbNode nodeFile;
        CPPUNIT_ASSERT(_testObj->node(dbNodeIdFile3, nodeFile, found) && found);
        CPPUNIT_ASSERT(nodeFile.nameRemote() == nodeFile3.nameRemote());
        CPPUNIT_ASSERT_EQUAL(nodeFile3.lastModifiedLocal().value(), nodeFile.lastModifiedLocal().value());
        CPPUNIT_ASSERT_EQUAL(nodeFile3.type(), nodeFile.type());
        CPPUNIT_ASSERT_EQUAL(nodeFile3.size(), nodeFile.size());
        CPPUNIT_ASSERT_EQUAL(nodeFile3.checksum().value(), nodeFile.checksum().value());
    }

    CPPUNIT_ASSERT(_testObj->clearNodes());
}

void TestSyncDb::testSyncNodes() {
    std::unordered_set<NodeId> nodeIdSet;
    nodeIdSet.emplace("1");
    nodeIdSet.emplace("2");
    nodeIdSet.emplace("3");
    nodeIdSet.emplace("4");
    nodeIdSet.emplace("5");

    std::unordered_set<NodeId> nodeIdSet2;
    nodeIdSet2.emplace("11");
    nodeIdSet2.emplace("12");
    nodeIdSet2.emplace("13");

    CPPUNIT_ASSERT(_testObj->updateAllSyncNodes(SyncNodeType::BlackList, nodeIdSet));
    CPPUNIT_ASSERT(_testObj->updateAllSyncNodes(SyncNodeType::UndecidedList, nodeIdSet2));

    std::unordered_set<NodeId> nodeIdSet3;
    CPPUNIT_ASSERT(_testObj->selectAllSyncNodes(SyncNodeType::BlackList, nodeIdSet3));
    CPPUNIT_ASSERT_EQUAL(size_t(5), nodeIdSet3.size());
    CPPUNIT_ASSERT(nodeIdSet3.contains("1"));
    CPPUNIT_ASSERT(nodeIdSet3.contains("2"));
    CPPUNIT_ASSERT(nodeIdSet3.contains("3"));
    CPPUNIT_ASSERT(nodeIdSet3.contains("4"));
    CPPUNIT_ASSERT(nodeIdSet3.contains("5"));
    nodeIdSet3.clear();
    CPPUNIT_ASSERT(_testObj->selectAllSyncNodes(SyncNodeType::UndecidedList, nodeIdSet3));
    CPPUNIT_ASSERT_EQUAL(size_t(3), nodeIdSet3.size());
    CPPUNIT_ASSERT(nodeIdSet3.contains("11"));
    CPPUNIT_ASSERT(nodeIdSet3.contains("12"));
    CPPUNIT_ASSERT(nodeIdSet3.contains("13"));

    CPPUNIT_ASSERT(_testObj->updateAllSyncNodes(SyncNodeType::BlackList, std::unordered_set<NodeId>()));
    nodeIdSet3.clear();
    CPPUNIT_ASSERT(_testObj->selectAllSyncNodes(SyncNodeType::BlackList, nodeIdSet3));
    CPPUNIT_ASSERT_EQUAL(size_t(0), nodeIdSet3.size());
}

void TestSyncDb::testCorrespondingNodeId() {
    time_t tLoc = std::time(0);
    time_t tDrive = std::time(0);
    bool constraintError = false;


    DbNode nodeDir(0, _testObj->rootNode().nodeId(), Str("Dir loc 1"), Str("Dir drive 1"), "id dir loc 1", "id dir drive 1", tLoc,
                   tLoc, tDrive, NodeType::Directory, 0, std::nullopt);
    DbNodeId dbNodeIdDir;
    CPPUNIT_ASSERT(_testObj->insertNode(nodeDir, dbNodeIdDir, constraintError));
    CPPUNIT_ASSERT(!constraintError);

    DbNodeId dbNodeIdFile;
    DbNode nodeFile(0, _testObj->rootNode().nodeId(), Str("File loc 1"), Str("File drive 1"), "id file loc 1", "id file drive 1",
                    tLoc, tLoc, tDrive, NodeType::Directory, 0, std::nullopt);
    CPPUNIT_ASSERT(_testObj->insertNode(nodeFile, dbNodeIdFile, constraintError));
    CPPUNIT_ASSERT(!constraintError);

    // Normal case
    NodeId correspondingNodeId;
    bool found = false;
    CPPUNIT_ASSERT(_testObj->correspondingNodeId(ReplicaSide::Local, "id dir loc 1", correspondingNodeId, found));
    CPPUNIT_ASSERT(found);
    CPPUNIT_ASSERT_EQUAL(std::string("id dir drive 1"), correspondingNodeId);

    CPPUNIT_ASSERT(_testObj->correspondingNodeId(ReplicaSide::Remote, "id dir drive 1", correspondingNodeId, found));
    CPPUNIT_ASSERT(found);
    CPPUNIT_ASSERT_EQUAL(std::string("id dir loc 1"), correspondingNodeId);

    // Wrong Side case
    CPPUNIT_ASSERT(_testObj->correspondingNodeId(ReplicaSide::Remote, "id dir loc 1", correspondingNodeId, found));
    CPPUNIT_ASSERT(!found);

    CPPUNIT_ASSERT(_testObj->correspondingNodeId(ReplicaSide::Local, "id dir drive 1", correspondingNodeId, found));
    CPPUNIT_ASSERT(!found);

    // Wrong id case
    CPPUNIT_ASSERT(_testObj->correspondingNodeId(ReplicaSide::Local, "id dir loc 2", correspondingNodeId, found));
    CPPUNIT_ASSERT(!found);

    CPPUNIT_ASSERT(_testObj->correspondingNodeId(ReplicaSide::Remote, "id dir drive 2", correspondingNodeId, found));
    CPPUNIT_ASSERT(!found);

    // Unknown side case
    CPPUNIT_ASSERT(!_testObj->correspondingNodeId(ReplicaSide::Unknown, "id dir loc 1", correspondingNodeId, found));
    CPPUNIT_ASSERT(!found);
}
} // namespace KDC
