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

#include "testsyncdbreadonlycache.h"
#include "test_utility/testhelpers.h"
#include "test_utility/localtemporarydirectory.h"

#include "libcommonserver/io/iohelper.h"
#include "libparms/db/parmsdb.h"
#include "mocks/libcommonserver/db/mockdb.h"

#include <algorithm>
#include <time.h>

using namespace CppUnit;

namespace KDC {
void TestSyncDbReadOnlyCache::setUp() {
    TestBase::start();
    bool alreadyExists = false;
    const std::filesystem::path syncDbPath = Db::makeDbName(1, 1, 1, 1, alreadyExists);

    // Delete previous DB
    std::filesystem::remove(syncDbPath);

    // Create DB
    _testObj = new SyncDb(syncDbPath.string(), KDRIVE_VERSION_STRING);
    _testObj->init(KDRIVE_VERSION_STRING);
    _testObj->setAutoDelete(true);
}

void TestSyncDbReadOnlyCache::tearDown() {
    // Close and delete DB
    _testObj->close();
    delete _testObj;
    TestBase::stop();
}

void TestSyncDbReadOnlyCache::testreloadIfNeeded() {
    // Insert node
    time_t tLoc = std::time(nullptr);
    time_t tDrive = std::time(nullptr);

    DbNode nodeDir1(0, _testObj->rootNode().nodeId(), Str("Dir loc 1"), Str("Dir drive 1"), "id loc 1", "id drive 1", tLoc, tLoc,
                    tDrive, NodeType::Directory, 0, std::nullopt);

    DbNodeId dbNodeIdDir1;

    bool constraintError = false;
    CPPUNIT_ASSERT(_testObj->insertNode(nodeDir1, dbNodeIdDir1, constraintError));
    CPPUNIT_ASSERT(!constraintError);

    CPPUNIT_ASSERT_GREATER(_testObj->cache().revision(), _testObj->revision());
    CPPUNIT_ASSERT(_testObj->cache().reloadIfNeeded());
    CPPUNIT_ASSERT_EQUAL(_testObj->revision(), _testObj->cache().revision());

    // Update node
    nodeDir1.setNodeId(dbNodeIdDir1);
    nodeDir1.setNameLocal(nodeDir1.nameLocal() + Str("new"));
    bool found = false;
    CPPUNIT_ASSERT(_testObj->updateNode(nodeDir1, found) && found);

    CPPUNIT_ASSERT_GREATER(_testObj->cache().revision(), _testObj->revision());
    CPPUNIT_ASSERT(_testObj->cache().reloadIfNeeded());
    CPPUNIT_ASSERT_EQUAL(_testObj->revision(), _testObj->cache().revision());

    DbNode cachedNode;
    CPPUNIT_ASSERT(_testObj->cache().node(dbNodeIdDir1, cachedNode, found) && found);
    CPPUNIT_ASSERT_EQUAL(nodeDir1.nodeId(), cachedNode.nodeId());
    CPPUNIT_ASSERT(nodeDir1.nameLocal() == cachedNode.nameLocal());
}

void TestSyncDbReadOnlyCache::testNodes() {
    // Insert node
    DbNode nodeDir1(0, _testObj->rootNode().nodeId(), Str("Dir loc 1"), Str("Dir drive 1"), "id loc 1", "id drive 1",
                    testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0,
                    std::nullopt);
    DbNode nodeDir2(0, _testObj->rootNode().nodeId(), Str("Dir loc 2"), Str("Dir drive 2"), "id loc 2", "id drive 2",
                    testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0,
                    std::nullopt);
    DbNode nodeDir3(0, _testObj->rootNode().nodeId(), Str("家屋香袈睷晦"), Str("家屋香袈睷晦"), "id loc 3", "id drive 3",
                    testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, 0,
                    std::nullopt);
    DbNodeId dbNodeIdDir1;
    DbNodeId dbNodeIdDir2;
    DbNodeId dbNodeIdDir3;
    bool constraintError = false;
    CPPUNIT_ASSERT(_testObj->insertNode(nodeDir1, dbNodeIdDir1, constraintError));
    CPPUNIT_ASSERT(_testObj->insertNode(nodeDir2, dbNodeIdDir2, constraintError));
    CPPUNIT_ASSERT(_testObj->insertNode(nodeDir3, dbNodeIdDir3, constraintError));

    DbNode nodeFile1(dbNodeIdDir1, Str("File loc 1.1"), Str("File drive 1.1"), "id loc 1.1", "id drive 1.1",
                     testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File, 0, "cs 1.1");
    DbNode nodeFile2(dbNodeIdDir1, Str("File loc 1.2"), Str("File drive 1.2"), "id loc 1.2", "id drive 1.2",
                     testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File, 0, "cs 1.2");
    DbNode nodeFile3(dbNodeIdDir1, Str("File loc 1.3"), Str("File drive 1.3"), "id loc 1.3", "id drive 1.3",
                     testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File, 0, "cs 1.3");
    DbNode nodeFile4(dbNodeIdDir1, Str("File loc 1.4"), Str("File drive 1.4"), "id loc 1.4", "id drive 1.4",
                     testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File, 0, "cs 1.4");
    DbNode nodeFile5(dbNodeIdDir1, Str("File loc 1.5"), Str("File drive 1.5"), "id loc 1.5", "id drive 1.5",
                     testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File, 0, "cs 1.5");
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

    DbNode nodeFile6(dbNodeIdDir2, Str("File loc 2.1"), Str("File drive 2.1"), "id loc 2.1", "id drive 2.1",
                     testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime, NodeType::File, 0, "cs 2.1");
    DbNodeId dbNodeIdFile6;
    CPPUNIT_ASSERT(_testObj->insertNode(nodeFile6, dbNodeIdFile6, constraintError));
    bool found = false;

    // Update node
    nodeFile6.setNodeId(dbNodeIdFile6);
    nodeFile6.setNameLocal(nodeFile6.nameLocal() + Str("new"));
    nodeFile6.setNameRemote(nodeFile6.nameRemote() + Str("new"));
    nodeFile6.setLastModifiedLocal(nodeFile6.lastModifiedLocal().value() + 10);
    nodeFile6.setLastModifiedRemote(nodeFile6.lastModifiedRemote().value() + 100);
    nodeFile6.setChecksum(nodeFile6.checksum().value() + "new");
    CPPUNIT_ASSERT(_testObj->updateNode(nodeFile6, found) && found);
    CPPUNIT_ASSERT(_testObj->cache().reloadIfNeeded());

    // id
    std::optional<NodeId> nodeIdRoot;
    CPPUNIT_ASSERT(_testObj->cache().id(ReplicaSide::Local, SyncPath(""), nodeIdRoot, found) && found);
    CPPUNIT_ASSERT_EQUAL(nodeIdRoot.value(), _testObj->rootNode().nodeIdLocal().value());
    CPPUNIT_ASSERT(_testObj->cache().id(ReplicaSide::Remote, SyncPath(""), nodeIdRoot, found) && found);
    CPPUNIT_ASSERT_EQUAL(nodeIdRoot.value(), _testObj->rootNode().nodeIdRemote().value());

    CPPUNIT_ASSERT(_testObj->cache().reloadIfNeeded());
    std::optional<NodeId> nodeIdFile3;
    CPPUNIT_ASSERT(_testObj->cache().id(ReplicaSide::Local, SyncPath("Dir loc 1/File loc 1.3"), nodeIdFile3, found) && found);
    CPPUNIT_ASSERT_EQUAL(nodeIdFile3.value(), nodeFile3.nodeIdLocal().value());
    CPPUNIT_ASSERT(_testObj->cache().id(ReplicaSide::Remote, SyncPath("Dir drive 1/File drive 1.3"), nodeIdFile3, found) &&
                   found);
    CPPUNIT_ASSERT_EQUAL(nodeIdFile3.value(), nodeFile3.nodeIdRemote().value());
    std::optional<NodeId> nodeIdFile4;
    CPPUNIT_ASSERT(_testObj->cache().id(ReplicaSide::Remote, SyncPath("Dir drive 1/File drive 1.4"), nodeIdFile4, found) &&
                   found);
    CPPUNIT_ASSERT(nodeIdFile4);
    std::optional<NodeId> nodeIdFile5;
    CPPUNIT_ASSERT(_testObj->cache().id(ReplicaSide::Local, SyncPath("Dir loc 1/File loc 1.5"), nodeIdFile5, found) && found);
    CPPUNIT_ASSERT(nodeIdFile5);

    // parent
    NodeId parentNodeidFile3;
    CPPUNIT_ASSERT(_testObj->cache().parent(ReplicaSide::Local, nodeFile3.nodeIdLocal().value(), parentNodeidFile3, found) &&
                   found);
    CPPUNIT_ASSERT(nodeDir1.nodeIdLocal() == parentNodeidFile3);
    CPPUNIT_ASSERT(_testObj->cache().parent(ReplicaSide::Remote, nodeFile3.nodeIdRemote().value(), parentNodeidFile3, found) &&
                   found);
    CPPUNIT_ASSERT(nodeDir1.nodeIdRemote() == parentNodeidFile3);
    NodeId parentNodeidDir1;
    CPPUNIT_ASSERT(_testObj->cache().parent(ReplicaSide::Local, nodeDir1.nodeIdLocal().value(), parentNodeidDir1, found) &&
                   found);
    CPPUNIT_ASSERT(_testObj->rootNode().nodeIdLocal() == parentNodeidDir1);
    CPPUNIT_ASSERT(_testObj->cache().parent(ReplicaSide::Remote, nodeDir1.nodeIdRemote().value(), parentNodeidDir1, found) &&
                   found);
    CPPUNIT_ASSERT(_testObj->rootNode().nodeIdRemote() == parentNodeidDir1);

    // path
    SyncPath pathFile3;
    CPPUNIT_ASSERT(_testObj->cache().path(ReplicaSide::Local, nodeFile3.nodeIdLocal().value(), pathFile3, found) && found);
    CPPUNIT_ASSERT_EQUAL(SyncPath(Str("Dir loc 1/File loc 1.3")), pathFile3);
    CPPUNIT_ASSERT(_testObj->cache().path(ReplicaSide::Remote, nodeFile3.nodeIdRemote().value(), pathFile3, found) && found);
    CPPUNIT_ASSERT_EQUAL(SyncPath(Str("Dir drive 1/File drive 1.3")), pathFile3);
    SyncPath pathDir1;
    CPPUNIT_ASSERT(_testObj->cache().path(ReplicaSide::Local, nodeDir1.nodeIdLocal().value(), pathDir1, found) && found);
    CPPUNIT_ASSERT_EQUAL(SyncPath(Str("Dir loc 1")), pathDir1);
    CPPUNIT_ASSERT(_testObj->cache().path(ReplicaSide::Remote, nodeDir1.nodeIdRemote().value(), pathDir1, found) && found);
    CPPUNIT_ASSERT_EQUAL(SyncPath(Str("Dir drive 1")), pathDir1);
    SyncPath pathDir3;
    CPPUNIT_ASSERT(_testObj->cache().path(ReplicaSide::Local, nodeDir3.nodeIdLocal().value(), pathDir3, found) && found);
    CPPUNIT_ASSERT_EQUAL(SyncPath(Str("家屋香袈睷晦")), pathDir3);
    SyncPath pathRoot;
    CPPUNIT_ASSERT(_testObj->cache().path(ReplicaSide::Local, _testObj->rootNode().nodeIdLocal().value(), pathRoot, found) &&
                   found);
    CPPUNIT_ASSERT_EQUAL(SyncPath(Str("")), pathRoot);
    CPPUNIT_ASSERT(_testObj->cache().path(ReplicaSide::Remote, _testObj->rootNode().nodeIdRemote().value(), pathRoot, found) &&
                   found);
    CPPUNIT_ASSERT_EQUAL(SyncPath(Str("")), pathRoot);

    // ids
    std::vector<NodeId> ids;
    CPPUNIT_ASSERT(_testObj->cache().ids(ReplicaSide::Local, ids, found) && found);
    CPPUNIT_ASSERT_EQUAL(size_t(10), ids.size());
    const auto rootNodeLocIt =
            std::ranges::find_if(ids, [this](const NodeId &id) { return id == _testObj->rootNode().nodeIdLocal(); });
    const auto nodeFile5LocIt =
            std::ranges::find_if(ids, [&nodeFile5](const NodeId &id) { return id == nodeFile5.nodeIdLocal(); });
    CPPUNIT_ASSERT(rootNodeLocIt != ids.end());
    CPPUNIT_ASSERT(nodeFile5LocIt != ids.end());
    ids.clear();
    CPPUNIT_ASSERT(_testObj->cache().ids(ReplicaSide::Remote, ids, found) && found);
    CPPUNIT_ASSERT_EQUAL(size_t(10), ids.size());
    const auto rootNodeRemIt =
            std::ranges::find_if(ids, [this](const NodeId &id) { return id == _testObj->rootNode().nodeIdRemote(); });
    const auto nodeFile5RemIt =
            std::ranges::find_if(ids, [&nodeFile5](const NodeId &id) { return id == nodeFile5.nodeIdRemote(); });
    CPPUNIT_ASSERT(rootNodeRemIt != ids.end());
    CPPUNIT_ASSERT(nodeFile5RemIt != ids.end());

    // dbId
    DbNodeId dbNodeId;
    CPPUNIT_ASSERT(_testObj->cache().dbId(ReplicaSide::Local, _testObj->rootNode().nodeIdLocal().value(), dbNodeId, found) &&
                   found);
    CPPUNIT_ASSERT(dbNodeId == _testObj->rootNode().nodeId());
    CPPUNIT_ASSERT(_testObj->cache().dbId(ReplicaSide::Remote, _testObj->rootNode().nodeIdRemote().value(), dbNodeId, found) &&
                   found);
    CPPUNIT_ASSERT(dbNodeId == _testObj->rootNode().nodeId());
    CPPUNIT_ASSERT(_testObj->cache().dbId(ReplicaSide::Local, nodeFile3.nodeIdLocal().value(), dbNodeId, found) && found);
    CPPUNIT_ASSERT_EQUAL(dbNodeIdFile3, dbNodeId);
    CPPUNIT_ASSERT(_testObj->cache().dbId(ReplicaSide::Remote, nodeFile3.nodeIdRemote().value(), dbNodeId, found) && found);
    CPPUNIT_ASSERT_EQUAL(dbNodeIdFile3, dbNodeId);

    // id
    NodeId nodeId;
    CPPUNIT_ASSERT(_testObj->cache().id(ReplicaSide::Local, _testObj->rootNode().nodeId(), nodeId, found) && found);
    CPPUNIT_ASSERT(nodeId == _testObj->rootNode().nodeIdLocal().value());
    CPPUNIT_ASSERT(_testObj->cache().id(ReplicaSide::Remote, _testObj->rootNode().nodeId(), nodeId, found) && found);
    CPPUNIT_ASSERT(nodeId == _testObj->rootNode().nodeIdRemote().value());
    CPPUNIT_ASSERT(_testObj->cache().id(ReplicaSide::Local, dbNodeIdFile3, nodeId, found) && found);
    CPPUNIT_ASSERT(nodeId == nodeFile3.nodeIdLocal().value());
    CPPUNIT_ASSERT(_testObj->cache().id(ReplicaSide::Remote, dbNodeIdFile3, nodeId, found) && found);
    CPPUNIT_ASSERT(nodeId == nodeFile3.nodeIdRemote().value());

    // node from local ID
    {
        DbNode nodeDirLocal;
        CPPUNIT_ASSERT(_testObj->cache().node(ReplicaSide::Local, nodeDir1.nodeIdLocal().value(), nodeDirLocal, found) && found);
        CPPUNIT_ASSERT_EQUAL(dbNodeIdDir1, nodeDirLocal.nodeId());
        CPPUNIT_ASSERT_EQUAL(nodeDir1.parentNodeId().value(), nodeDirLocal.parentNodeId().value());
        CPPUNIT_ASSERT(nodeDirLocal.nameLocal() == nodeDir1.nameLocal());
        CPPUNIT_ASSERT_EQUAL(nodeDir1.created().value(), nodeDirLocal.created().value());
        CPPUNIT_ASSERT_EQUAL(nodeDir1.lastModifiedLocal().value(), nodeDirLocal.lastModifiedLocal().value());
        CPPUNIT_ASSERT_EQUAL(nodeDir1.type(), nodeDirLocal.type());
        CPPUNIT_ASSERT_EQUAL(nodeDir1.size(), nodeDirLocal.size());

        DbNode nodeDirRemote;
        CPPUNIT_ASSERT(_testObj->cache().node(ReplicaSide::Remote, nodeDir1.nodeIdRemote().value(), nodeDirRemote, found) &&
                       found);
        CPPUNIT_ASSERT(nodeDirRemote.nameRemote() == nodeDir1.nameRemote());
        CPPUNIT_ASSERT_EQUAL(nodeDir1.lastModifiedRemote().value(), nodeDirRemote.lastModifiedRemote().value());

        DbNode nodeFileLocal;
        CPPUNIT_ASSERT(_testObj->cache().node(ReplicaSide::Local, nodeFile1.nodeIdLocal().value(), nodeFileLocal, found) &&
                       found);
        CPPUNIT_ASSERT_EQUAL(dbNodeIdFile1, nodeFileLocal.nodeId());
        CPPUNIT_ASSERT_EQUAL(nodeFile1.parentNodeId().value(), nodeFileLocal.parentNodeId().value());
        CPPUNIT_ASSERT(nodeFileLocal.nameLocal() == nodeFile1.nameLocal());
        CPPUNIT_ASSERT_EQUAL(nodeFile1.created().value(), nodeFileLocal.created().value());
        CPPUNIT_ASSERT_EQUAL(nodeFile1.lastModifiedLocal().value(), nodeFileLocal.lastModifiedLocal().value());
        CPPUNIT_ASSERT_EQUAL(nodeFile1.type(), nodeFileLocal.type());
        CPPUNIT_ASSERT_EQUAL(nodeFile1.size(), nodeFileLocal.size());

        DbNode nodeFileRemote;
        CPPUNIT_ASSERT(_testObj->cache().node(ReplicaSide::Remote, nodeFile1.nodeIdRemote().value(), nodeFileRemote, found) &&
                       found);
        CPPUNIT_ASSERT(nodeFileRemote.nameRemote() == nodeFile1.nameRemote());
        CPPUNIT_ASSERT(nodeFileRemote.lastModifiedRemote() == nodeFile1.lastModifiedRemote());
    }

    // paths from db ID
    {
        SyncPath localPathFile3;
        SyncPath remotePathFile3;
        CPPUNIT_ASSERT(_testObj->cache().path(dbNodeIdFile3, localPathFile3, remotePathFile3, found) && found);
        CPPUNIT_ASSERT_EQUAL(SyncPath(Str("Dir loc 1/File loc 1.3")), localPathFile3);
        CPPUNIT_ASSERT_EQUAL(SyncPath(Str("Dir drive 1/File drive 1.3")), remotePathFile3);

        SyncPath localPathDir1;
        SyncPath remotePathDir1;
        CPPUNIT_ASSERT(_testObj->cache().path(dbNodeIdDir1, localPathDir1, remotePathDir1, found) && found);
        CPPUNIT_ASSERT_EQUAL(SyncPath(Str("Dir loc 1")), localPathDir1);
        CPPUNIT_ASSERT_EQUAL(SyncPath(Str("Dir drive 1")), remotePathDir1);

        SyncPath localPathDir3;
        SyncPath remotePathDir3;
        CPPUNIT_ASSERT(_testObj->cache().path(dbNodeIdDir3, localPathDir3, remotePathDir3, found) && found);
        CPPUNIT_ASSERT_EQUAL(SyncPath(Str("家屋香袈睷晦")), localPathDir3);
        CPPUNIT_ASSERT_EQUAL(SyncPath(Str("家屋香袈睷晦")), remotePathDir3);

        SyncPath localPathRoot;
        SyncPath remotePathRoot;
        CPPUNIT_ASSERT(_testObj->cache().path(_testObj->rootNode().nodeId(), localPathRoot, remotePathRoot, found) && found);
        CPPUNIT_ASSERT_EQUAL(SyncPath(""), localPathRoot);
        CPPUNIT_ASSERT_EQUAL(SyncPath(""), remotePathRoot);
    }

    // node from db ID
    {
        DbNode nodeDir;
        CPPUNIT_ASSERT(_testObj->cache().node(dbNodeIdDir1, nodeDir, found) && found);
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
        CPPUNIT_ASSERT(_testObj->cache().node(dbNodeIdFile3, nodeFile, found) && found);
        CPPUNIT_ASSERT(nodeFile.nameRemote() == nodeFile3.nameRemote());
        CPPUNIT_ASSERT_EQUAL(nodeFile3.lastModifiedLocal().value(), nodeFile.lastModifiedLocal().value());
        CPPUNIT_ASSERT_EQUAL(nodeFile3.type(), nodeFile.type());
        CPPUNIT_ASSERT_EQUAL(nodeFile3.size(), nodeFile.size());
        CPPUNIT_ASSERT_EQUAL(nodeFile3.checksum().value(), nodeFile.checksum().value());
    }

    CPPUNIT_ASSERT(_testObj->clearNodes());
}
} // namespace KDC
