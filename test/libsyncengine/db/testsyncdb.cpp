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
#include "libcommonserver/utility/asserts.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/log/log.h"

#include <time.h>

using namespace CppUnit;

namespace KDC {

void TestSyncDb::setUp() {
    bool alreadyExists;
    std::filesystem::path syncDbPath = Db::makeDbName(1, 1, 1, 1, alreadyExists);

    // Delete previous DB
    std::filesystem::remove(syncDbPath);

    // Create DB
    _testObj = new SyncDb(syncDbPath.string(), "3.4.0");
    _testObj->setAutoDelete(true);
}

void TestSyncDb::tearDown() {
    // Close and delete DB
    _testObj->close();
    delete _testObj;
}

namespace helpers {
SyncName makeNfdSyncName() {
#ifdef _WIN32
    return Utility::normalizedSyncName(L"ééé", Utility::UnicodeNormalization::NFD);
#else
    return Utility::normalizedSyncName("ééé", Utility::UnicodeNormalization::NFD);
#endif
}

SyncName makeNfcSyncName() {
#ifdef _WIN32
    return Utility::normalizedSyncName(L"ééé");
#else
    return Utility::normalizedSyncName("ééé");
#endif
}
}  // namespace helpers

void TestSyncDb::testUpgrade_3_6_3() {
    const time_t tLoc = std::time(0);
    const time_t tDrive = std::time(0);
    const auto rootId = _testObj->rootNode().nodeId();

    // Insert nodes with NFD-normalized names
    const SyncName nfdEncodedName = helpers::makeNfdSyncName();
    const SyncName nfcEncodedName = helpers::makeNfcSyncName();

    DbNode nodeFile1(0, rootId, nfdEncodedName, nfdEncodedName, "id loc 1", "id drive 1", tLoc, tLoc, tDrive,
                     NodeType::NodeTypeFile, 0, "cs 2.2");
    DbNode nodeFile2(0, rootId, nfcEncodedName, nfdEncodedName, "id loc 2", "id drive 2", tLoc, tLoc, tDrive,
                     NodeType::NodeTypeFile, 0, "cs 2.2");
    DbNode nodeFile3(0, rootId, nfcEncodedName, nfcEncodedName, "id loc 3", "id drive 3", tLoc, tLoc, tDrive,
                     NodeType::NodeTypeFile, 0, "cs 2.2");

    bool constraintError = false;
    DbNodeId dbNodeId;

    _testObj->insertNode(nodeFile1, dbNodeId, constraintError);
    _testObj->insertNode(nodeFile2, dbNodeId, constraintError);
    _testObj->insertNode(nodeFile3, dbNodeId, constraintError);

    _testObj->upgrade("3.6.3", "3.6.4");

    // All DB sync names should now be NFC-normalized.
    SyncName localName;
    SyncName remoteName;
    bool found = false;
    for (int i = 1; i <= 3; ++i) {
        CPPUNIT_ASSERT(
            _testObj->name(ReplicaSide::ReplicaSideLocal, std::string("id loc ") + std::to_string(i), localName, found) && found);
        CPPUNIT_ASSERT(localName == nfcEncodedName);
        CPPUNIT_ASSERT(
            _testObj->name(ReplicaSide::ReplicaSideRemote, std::string("id drive ") + std::to_string(i), remoteName, found) &&
            found);
        CPPUNIT_ASSERT(remoteName == nfcEncodedName);
    }
}

void TestSyncDb::testNodes() {
    CPPUNIT_ASSERT(_testObj->exists());
    CPPUNIT_ASSERT(_testObj->clearNodes());

    // Insert node
    time_t tLoc = std::time(0);
    time_t tDrive = std::time(0);

    DbNode nodeDir1(0, _testObj->rootNode().nodeId(), Str("Dir loc 1"), Str("Dir drive 1"), "id loc 1", "id drive 1", tLoc, tLoc,
                    tDrive, NodeType::NodeTypeDirectory, 0, std::nullopt);
    DbNode nodeDir2(0, _testObj->rootNode().nodeId(), Str("Dir loc 2"), Str("Dir drive 2"), "id loc 2", "id drive 2", tLoc, tLoc,
                    tDrive, NodeType::NodeTypeDirectory, 0, std::nullopt);
    DbNode nodeDir3(0, _testObj->rootNode().nodeId(), Str("家屋香袈睷晦"), Str("家屋香袈睷晦"), "id loc 3", "id drive 3", tLoc,
                    tLoc, tDrive, NodeType::NodeTypeDirectory, 0, std::nullopt);
    DbNodeId dbNodeIdDir1;
    DbNodeId dbNodeIdDir2;
    DbNodeId dbNodeIdDir3;
    bool constraintError = false;
    CPPUNIT_ASSERT(_testObj->insertNode(nodeDir1, dbNodeIdDir1, constraintError));
    CPPUNIT_ASSERT(_testObj->insertNode(nodeDir2, dbNodeIdDir2, constraintError));
    CPPUNIT_ASSERT(_testObj->insertNode(nodeDir3, dbNodeIdDir3, constraintError));

    DbNode nodeFile1(0, dbNodeIdDir1, Str("File loc 1.1"), Str("File drive 1.1"), "id loc 1.1", "id drive 1.1", tLoc, tLoc,
                     tDrive, NodeType::NodeTypeFile, 0, "cs 1.1");
    DbNode nodeFile2(0, dbNodeIdDir1, Str("File loc 1.2"), Str("File drive 1.2"), "id loc 1.2", "id drive 1.2", tLoc, tLoc,
                     tDrive, NodeType::NodeTypeFile, 0, "cs 1.2");
    DbNode nodeFile3(0, dbNodeIdDir1, Str("File loc 1.3"), Str("File drive 1.3"), "id loc 1.3", "id drive 1.3", tLoc, tLoc,
                     tDrive, NodeType::NodeTypeFile, 0, "cs 1.3");
    DbNode nodeFile4(0, dbNodeIdDir1, Str("File loc 1.4"), Str("File drive 1.4"), "id loc 1.4", "id drive 1.4", tLoc, tLoc,
                     tDrive, NodeType::NodeTypeFile, 0, "cs 1.4");
    DbNode nodeFile5(0, dbNodeIdDir1, Str("File loc 1.5"), Str("File drive 1.5"), "id loc 1.5", "id drive 1.5", tLoc, tLoc,
                     tDrive, NodeType::NodeTypeFile, 0, "cs 1.5");

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

    DbNode nodeFile6(0, dbNodeIdDir2, Str("File loc 2.1"), Str("File drive 2.1"), "id loc 2.1", "id drive 2.1", tLoc, tLoc,
                     tDrive, NodeType::NodeTypeFile, 0, "cs 2.1");
    DbNodeId dbNodeIdFile6;
    CPPUNIT_ASSERT(_testObj->insertNode(nodeFile6, dbNodeIdFile6, constraintError));

    // Insert node with non normalized name (NFD)
    const SyncName nfdEncodedName = helpers::makeNfdSyncName();
    DbNode nodeFile7(0, dbNodeIdDir1, nfdEncodedName, nfdEncodedName, "id loc 2.2", "id drive 2.2", tLoc, tLoc, tDrive,
                     NodeType::NodeTypeFile, 0, "cs 2.2");
    DbNodeId dbNodeIdFile7;
    CPPUNIT_ASSERT(_testObj->insertNode(nodeFile7, dbNodeIdFile7, constraintError));

    SyncName localName;
    SyncName remoteName;
    bool found;
    CPPUNIT_ASSERT(_testObj->name(ReplicaSide::ReplicaSideLocal, nodeFile7.nodeIdLocal().value(), localName, found) && found);
    CPPUNIT_ASSERT(_testObj->name(ReplicaSide::ReplicaSideRemote, nodeFile7.nodeIdRemote().value(), remoteName, found) && found);

    const SyncName nfcEncodedName = helpers::makeNfcSyncName();
    CPPUNIT_ASSERT(localName == nfcEncodedName);
    CPPUNIT_ASSERT(remoteName == nfcEncodedName);

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
    CPPUNIT_ASSERT(_testObj->name(ReplicaSide::ReplicaSideLocal, nodeFile6.nodeIdLocal().value(), name, found) && found);
    CPPUNIT_ASSERT(name == nodeFile6.nameLocal());
    CPPUNIT_ASSERT(_testObj->name(ReplicaSide::ReplicaSideRemote, nodeFile6.nodeIdRemote().value(), name, found) && found);
    CPPUNIT_ASSERT(name == nodeFile6.nameRemote());
    CPPUNIT_ASSERT(_testObj->lastModified(ReplicaSide::ReplicaSideLocal, nodeFile6.nodeIdLocal().value(), time, found) && found);
    CPPUNIT_ASSERT_EQUAL(nodeFile6.lastModifiedLocal().value(), time.value());
    CPPUNIT_ASSERT(_testObj->lastModified(ReplicaSide::ReplicaSideRemote, nodeFile6.nodeIdRemote().value(), time, found) &&
                   found);
    CPPUNIT_ASSERT_EQUAL(nodeFile6.lastModifiedRemote().value(), time.value());
    CPPUNIT_ASSERT(_testObj->checksum(ReplicaSide::ReplicaSideLocal, nodeFile6.nodeIdLocal().value(), cs, found) && found);
    CPPUNIT_ASSERT_EQUAL(nodeFile6.checksum().value(), cs.value());

    // Update node with non normalized name (NFD)
    nodeFile7.setNodeId(dbNodeIdFile7);
    nodeFile7.setNameLocal(nfdEncodedName);
    nodeFile7.setNameRemote(nfdEncodedName);
    CPPUNIT_ASSERT(_testObj->updateNode(nodeFile7, found) && found);

    CPPUNIT_ASSERT(_testObj->name(ReplicaSide::ReplicaSideLocal, nodeFile7.nodeIdLocal().value(), localName, found) && found);
    CPPUNIT_ASSERT(_testObj->name(ReplicaSide::ReplicaSideRemote, nodeFile7.nodeIdRemote().value(), remoteName, found) && found);

    CPPUNIT_ASSERT(localName == nfcEncodedName);
    CPPUNIT_ASSERT(remoteName == nfcEncodedName);

    // Delete node
    CPPUNIT_ASSERT(_testObj->deleteNode(dbNodeIdFile6, found) && found);

    // id
    std::optional<NodeId> nodeIdRoot;
    CPPUNIT_ASSERT(_testObj->id(ReplicaSide::ReplicaSideLocal, SyncPath(""), nodeIdRoot, found) && found);
    CPPUNIT_ASSERT_EQUAL(nodeIdRoot.value(), _testObj->rootNode().nodeIdLocal().value());
    CPPUNIT_ASSERT(_testObj->id(ReplicaSide::ReplicaSideRemote, SyncPath(""), nodeIdRoot, found) && found);
    CPPUNIT_ASSERT_EQUAL(nodeIdRoot.value(), _testObj->rootNode().nodeIdRemote().value());
    std::optional<NodeId> nodeIdFile3;
    CPPUNIT_ASSERT(_testObj->id(ReplicaSide::ReplicaSideLocal, SyncPath("Dir loc 1/File loc 1.3"), nodeIdFile3, found) && found);
    CPPUNIT_ASSERT_EQUAL(nodeIdFile3.value(), nodeFile3.nodeIdLocal().value());
    CPPUNIT_ASSERT(_testObj->id(ReplicaSide::ReplicaSideRemote, SyncPath("Dir drive 1/File drive 1.3"), nodeIdFile3, found) &&
                   found);
    CPPUNIT_ASSERT_EQUAL(nodeIdFile3.value(), nodeFile3.nodeIdRemote().value());
    std::optional<NodeId> nodeIdFile4;
    CPPUNIT_ASSERT(_testObj->id(ReplicaSide::ReplicaSideRemote, SyncPath("Dir drive 1/File drive 1.4"), nodeIdFile4, found) &&
                   found);
    CPPUNIT_ASSERT(nodeIdFile4);
    std::optional<NodeId> nodeIdFile5;
    CPPUNIT_ASSERT(_testObj->id(ReplicaSide::ReplicaSideLocal, SyncPath("Dir loc 1/File loc 1.5"), nodeIdFile5, found) && found);
    CPPUNIT_ASSERT(nodeIdFile5);

    // type
    NodeType typeDir1;
    CPPUNIT_ASSERT(_testObj->type(ReplicaSide::ReplicaSideLocal, nodeDir1.nodeIdLocal().value(), typeDir1, found) && found);
    CPPUNIT_ASSERT_EQUAL(typeDir1, nodeDir1.type());
    CPPUNIT_ASSERT(_testObj->type(ReplicaSide::ReplicaSideRemote, nodeDir1.nodeIdRemote().value(), typeDir1, found) && found);
    CPPUNIT_ASSERT_EQUAL(typeDir1, nodeDir1.type());
    NodeType typeFile3;
    CPPUNIT_ASSERT(_testObj->type(ReplicaSide::ReplicaSideLocal, nodeFile3.nodeIdLocal().value(), typeFile3, found) && found);
    CPPUNIT_ASSERT_EQUAL(typeFile3, nodeFile3.type());
    CPPUNIT_ASSERT(_testObj->type(ReplicaSide::ReplicaSideRemote, nodeFile3.nodeIdRemote().value(), typeFile3, found) && found);
    CPPUNIT_ASSERT_EQUAL(typeFile3, nodeFile3.type());

    // lastModified
    std::optional<SyncTime> timeDir1;
    CPPUNIT_ASSERT(_testObj->lastModified(ReplicaSide::ReplicaSideLocal, nodeDir1.nodeIdLocal().value(), timeDir1, found) &&
                   found);
    CPPUNIT_ASSERT_EQUAL(timeDir1.value(), nodeDir1.lastModifiedLocal().value());
    CPPUNIT_ASSERT(_testObj->lastModified(ReplicaSide::ReplicaSideRemote, nodeDir1.nodeIdRemote().value(), timeDir1, found) &&
                   found);
    CPPUNIT_ASSERT_EQUAL(timeDir1.value(), nodeDir1.lastModifiedRemote().value());
    std::optional<SyncTime> timeFile3;
    CPPUNIT_ASSERT(_testObj->lastModified(ReplicaSide::ReplicaSideLocal, nodeFile3.nodeIdLocal().value(), timeFile3, found) &&
                   found);
    CPPUNIT_ASSERT_EQUAL(timeFile3.value(), nodeFile3.lastModifiedLocal().value());
    CPPUNIT_ASSERT(_testObj->lastModified(ReplicaSide::ReplicaSideRemote, nodeFile3.nodeIdRemote().value(), timeFile3, found) &&
                   found);
    CPPUNIT_ASSERT_EQUAL(timeFile3.value(), nodeFile3.lastModifiedRemote().value());

    // parent
    NodeId parentNodeidFile3;
    CPPUNIT_ASSERT(_testObj->parent(ReplicaSide::ReplicaSideLocal, nodeFile3.nodeIdLocal().value(), parentNodeidFile3, found) &&
                   found);
    CPPUNIT_ASSERT(nodeDir1.nodeIdLocal() == parentNodeidFile3);
    CPPUNIT_ASSERT(_testObj->parent(ReplicaSide::ReplicaSideRemote, nodeFile3.nodeIdRemote().value(), parentNodeidFile3, found) &&
                   found);
    CPPUNIT_ASSERT(nodeDir1.nodeIdRemote() == parentNodeidFile3);
    NodeId parentNodeidDir1;
    CPPUNIT_ASSERT(_testObj->parent(ReplicaSide::ReplicaSideLocal, nodeDir1.nodeIdLocal().value(), parentNodeidDir1, found) &&
                   found);
    CPPUNIT_ASSERT(_testObj->rootNode().nodeIdLocal() == parentNodeidDir1);
    CPPUNIT_ASSERT(_testObj->parent(ReplicaSide::ReplicaSideRemote, nodeDir1.nodeIdRemote().value(), parentNodeidDir1, found) &&
                   found);
    CPPUNIT_ASSERT(_testObj->rootNode().nodeIdRemote() == parentNodeidDir1);

    // path
    SyncPath pathFile3;
    CPPUNIT_ASSERT(_testObj->path(ReplicaSide::ReplicaSideLocal, nodeFile3.nodeIdLocal().value(), pathFile3, found) && found);
    CPPUNIT_ASSERT_EQUAL(SyncPath(Str("Dir loc 1/File loc 1.3")), pathFile3);
    CPPUNIT_ASSERT(_testObj->path(ReplicaSide::ReplicaSideRemote, nodeFile3.nodeIdRemote().value(), pathFile3, found) && found);
    CPPUNIT_ASSERT_EQUAL(SyncPath(Str("Dir drive 1/File drive 1.3")), pathFile3);
    SyncPath pathDir1;
    CPPUNIT_ASSERT(_testObj->path(ReplicaSide::ReplicaSideLocal, nodeDir1.nodeIdLocal().value(), pathDir1, found) && found);
    CPPUNIT_ASSERT_EQUAL(SyncPath(Str("Dir loc 1")), pathDir1);
    CPPUNIT_ASSERT(_testObj->path(ReplicaSide::ReplicaSideRemote, nodeDir1.nodeIdRemote().value(), pathDir1, found) && found);
    CPPUNIT_ASSERT_EQUAL(SyncPath(Str("Dir drive 1")), pathDir1);
    SyncPath pathDir3;
    CPPUNIT_ASSERT(_testObj->path(ReplicaSide::ReplicaSideLocal, nodeDir3.nodeIdLocal().value(), pathDir3, found) && found);
    CPPUNIT_ASSERT_EQUAL(SyncPath(Str("家屋香袈睷晦")), pathDir3);
    SyncPath pathRoot;
    CPPUNIT_ASSERT(_testObj->path(ReplicaSide::ReplicaSideLocal, _testObj->rootNode().nodeIdLocal().value(), pathRoot, found) &&
                   found);
    CPPUNIT_ASSERT_EQUAL(SyncPath(Str("")), pathRoot);
    CPPUNIT_ASSERT(_testObj->path(ReplicaSide::ReplicaSideRemote, _testObj->rootNode().nodeIdRemote().value(), pathRoot, found) &&
                   found);
    CPPUNIT_ASSERT_EQUAL(SyncPath(Str("")), pathRoot);

    // name
    CPPUNIT_ASSERT(_testObj->name(ReplicaSide::ReplicaSideLocal, nodeFile3.nodeIdLocal().value(), name, found) && found);
    CPPUNIT_ASSERT(name == nodeFile3.nameLocal());
    CPPUNIT_ASSERT(_testObj->name(ReplicaSide::ReplicaSideRemote, nodeFile3.nodeIdRemote().value(), name, found) && found);
    CPPUNIT_ASSERT(name == nodeFile3.nameRemote());
    CPPUNIT_ASSERT(_testObj->name(ReplicaSide::ReplicaSideLocal, nodeDir1.nodeIdLocal().value(), name, found) && found);
    CPPUNIT_ASSERT(name == nodeDir1.nameLocal());
    CPPUNIT_ASSERT(_testObj->name(ReplicaSide::ReplicaSideRemote, nodeDir1.nodeIdRemote().value(), name, found) && found);
    CPPUNIT_ASSERT(name == nodeDir1.nameRemote());
    CPPUNIT_ASSERT(_testObj->name(ReplicaSide::ReplicaSideLocal, nodeDir3.nodeIdLocal().value(), name, found) && found);
    CPPUNIT_ASSERT(name == nodeDir3.nameLocal());
    CPPUNIT_ASSERT(_testObj->name(ReplicaSide::ReplicaSideRemote, nodeDir3.nodeIdRemote().value(), name, found) && found);
    CPPUNIT_ASSERT(name == nodeDir3.nameRemote());

    // checksum
    CPPUNIT_ASSERT(_testObj->checksum(ReplicaSide::ReplicaSideLocal, _testObj->rootNode().nodeIdLocal().value(), cs, found) &&
                   found);
    CPPUNIT_ASSERT(!cs);
    CPPUNIT_ASSERT(_testObj->checksum(ReplicaSide::ReplicaSideRemote, nodeDir1.nodeIdRemote().value(), cs, found) && found);
    CPPUNIT_ASSERT(!cs);
    CPPUNIT_ASSERT(_testObj->checksum(ReplicaSide::ReplicaSideLocal, nodeFile3.nodeIdLocal().value(), cs, found) && found);
    CPPUNIT_ASSERT_EQUAL(nodeFile3.checksum().value(), cs.value());
    CPPUNIT_ASSERT(_testObj->checksum(ReplicaSide::ReplicaSideRemote, nodeFile3.nodeIdRemote().value(), cs, found) && found);
    CPPUNIT_ASSERT_EQUAL(nodeFile3.checksum().value(), cs.value());

    // ids
    std::vector<NodeId> ids;
    CPPUNIT_ASSERT(_testObj->ids(ReplicaSide::ReplicaSideLocal, ids, found) && found);
    CPPUNIT_ASSERT_EQUAL(size_t(10), ids.size());
    CPPUNIT_ASSERT(ids[0] == _testObj->rootNode().nodeIdLocal());
    CPPUNIT_ASSERT(ids[8] == nodeFile5.nodeIdLocal());
    ids.clear();
    CPPUNIT_ASSERT(_testObj->ids(ReplicaSide::ReplicaSideRemote, ids, found) && found);
    CPPUNIT_ASSERT_EQUAL(size_t(10), ids.size());
    CPPUNIT_ASSERT(ids[0] == _testObj->rootNode().nodeIdRemote());
    CPPUNIT_ASSERT(ids[8] == nodeFile5.nodeIdRemote());

    // ancestor
    bool ancestor;
    CPPUNIT_ASSERT(_testObj->ancestor(ReplicaSide::ReplicaSideLocal, _testObj->rootNode().nodeIdLocal().value(),
                                      _testObj->rootNode().nodeIdLocal().value(), ancestor, found) &&
                   found);
    CPPUNIT_ASSERT(ancestor);
    CPPUNIT_ASSERT(_testObj->ancestor(ReplicaSide::ReplicaSideRemote, _testObj->rootNode().nodeIdRemote().value(),
                                      _testObj->rootNode().nodeIdRemote().value(), ancestor, found) &&
                   found);
    CPPUNIT_ASSERT(ancestor);
    CPPUNIT_ASSERT(_testObj->ancestor(ReplicaSide::ReplicaSideLocal, _testObj->rootNode().nodeIdLocal().value(),
                                      nodeDir1.nodeIdLocal().value(), ancestor, found) &&
                   found);
    CPPUNIT_ASSERT(ancestor);
    CPPUNIT_ASSERT(_testObj->ancestor(ReplicaSide::ReplicaSideRemote, _testObj->rootNode().nodeIdRemote().value(),
                                      nodeDir1.nodeIdRemote().value(), ancestor, found) &&
                   found);
    CPPUNIT_ASSERT(ancestor);
    CPPUNIT_ASSERT(_testObj->ancestor(ReplicaSide::ReplicaSideLocal, _testObj->rootNode().nodeIdLocal().value(),
                                      nodeFile3.nodeIdLocal().value(), ancestor, found) &&
                   found);
    CPPUNIT_ASSERT(ancestor);
    CPPUNIT_ASSERT(_testObj->ancestor(ReplicaSide::ReplicaSideRemote, _testObj->rootNode().nodeIdRemote().value(),
                                      nodeFile3.nodeIdRemote().value(), ancestor, found) &&
                   found);
    CPPUNIT_ASSERT(ancestor);
    CPPUNIT_ASSERT(_testObj->ancestor(ReplicaSide::ReplicaSideLocal, nodeDir1.nodeIdLocal().value(),
                                      nodeFile3.nodeIdLocal().value(), ancestor, found) &&
                   found);
    CPPUNIT_ASSERT(ancestor);
    CPPUNIT_ASSERT(_testObj->ancestor(ReplicaSide::ReplicaSideRemote, nodeDir1.nodeIdRemote().value(),
                                      nodeFile3.nodeIdRemote().value(), ancestor, found) &&
                   found);
    CPPUNIT_ASSERT(ancestor);
    CPPUNIT_ASSERT(_testObj->ancestor(ReplicaSide::ReplicaSideLocal, nodeFile2.nodeIdLocal().value(),
                                      nodeDir1.nodeIdLocal().value(), ancestor, found) &&
                   found);
    CPPUNIT_ASSERT(!ancestor);
    CPPUNIT_ASSERT(_testObj->ancestor(ReplicaSide::ReplicaSideRemote, nodeFile2.nodeIdRemote().value(),
                                      nodeDir1.nodeIdRemote().value(), ancestor, found) &&
                   found);
    CPPUNIT_ASSERT(!ancestor);
    CPPUNIT_ASSERT(_testObj->ancestor(ReplicaSide::ReplicaSideLocal, nodeFile1.nodeIdLocal().value(),
                                      nodeFile2.nodeIdLocal().value(), ancestor, found) &&
                   found);
    CPPUNIT_ASSERT(!ancestor);
    CPPUNIT_ASSERT(_testObj->ancestor(ReplicaSide::ReplicaSideRemote, nodeFile1.nodeIdRemote().value(),
                                      nodeFile2.nodeIdRemote().value(), ancestor, found) &&
                   found);
    CPPUNIT_ASSERT(!ancestor);

    // dbId
    DbNodeId dbNodeId;
    CPPUNIT_ASSERT(_testObj->dbId(ReplicaSide::ReplicaSideLocal, _testObj->rootNode().nodeIdLocal().value(), dbNodeId, found) &&
                   found);
    CPPUNIT_ASSERT(dbNodeId == _testObj->rootNode().nodeId());
    CPPUNIT_ASSERT(_testObj->dbId(ReplicaSide::ReplicaSideRemote, _testObj->rootNode().nodeIdRemote().value(), dbNodeId, found) &&
                   found);
    CPPUNIT_ASSERT(dbNodeId == _testObj->rootNode().nodeId());
    CPPUNIT_ASSERT(_testObj->dbId(ReplicaSide::ReplicaSideLocal, nodeFile3.nodeIdLocal().value(), dbNodeId, found) && found);
    CPPUNIT_ASSERT_EQUAL(dbNodeIdFile3, dbNodeId);
    CPPUNIT_ASSERT(_testObj->dbId(ReplicaSide::ReplicaSideRemote, nodeFile3.nodeIdRemote().value(), dbNodeId, found) && found);
    CPPUNIT_ASSERT_EQUAL(dbNodeIdFile3, dbNodeId);

    // id
    NodeId nodeId;
    CPPUNIT_ASSERT(_testObj->id(ReplicaSide::ReplicaSideLocal, _testObj->rootNode().nodeId(), nodeId, found) && found);
    CPPUNIT_ASSERT(nodeId == _testObj->rootNode().nodeIdLocal().value());
    CPPUNIT_ASSERT(_testObj->id(ReplicaSide::ReplicaSideRemote, _testObj->rootNode().nodeId(), nodeId, found) && found);
    CPPUNIT_ASSERT(nodeId == _testObj->rootNode().nodeIdRemote().value());
    CPPUNIT_ASSERT(_testObj->id(ReplicaSide::ReplicaSideLocal, dbNodeIdFile3, nodeId, found) && found);
    CPPUNIT_ASSERT(nodeId == nodeFile3.nodeIdLocal().value());
    CPPUNIT_ASSERT(_testObj->id(ReplicaSide::ReplicaSideRemote, dbNodeIdFile3, nodeId, found) && found);
    CPPUNIT_ASSERT(nodeId == nodeFile3.nodeIdRemote().value());

    // node from local ID
    {
        DbNode nodeDirLocal;
        CPPUNIT_ASSERT(_testObj->node(ReplicaSide::ReplicaSideLocal, nodeDir1.nodeIdLocal().value(), nodeDirLocal, found) &&
                       found);
        CPPUNIT_ASSERT_EQUAL(dbNodeIdDir1, nodeDirLocal.nodeId());
        CPPUNIT_ASSERT_EQUAL(nodeDir1.parentNodeId().value(), nodeDirLocal.parentNodeId().value());
        CPPUNIT_ASSERT(nodeDirLocal.nameLocal() == nodeDir1.nameLocal());
        CPPUNIT_ASSERT_EQUAL(nodeDir1.created().value(), nodeDirLocal.created().value());
        CPPUNIT_ASSERT_EQUAL(nodeDir1.lastModifiedLocal().value(), nodeDirLocal.lastModifiedLocal().value());
        CPPUNIT_ASSERT_EQUAL(nodeDir1.type(), nodeDirLocal.type());
        CPPUNIT_ASSERT_EQUAL(nodeDir1.size(), nodeDirLocal.size());

        DbNode nodeDirRemote;
        CPPUNIT_ASSERT(_testObj->node(ReplicaSide::ReplicaSideRemote, nodeDir1.nodeIdRemote().value(), nodeDirRemote, found) &&
                       found);
        CPPUNIT_ASSERT(nodeDirRemote.nameRemote() == nodeDir1.nameRemote());
        CPPUNIT_ASSERT_EQUAL(nodeDir1.lastModifiedRemote().value(), nodeDirRemote.lastModifiedRemote().value());

        DbNode nodeFileLocal;
        CPPUNIT_ASSERT(_testObj->node(ReplicaSide::ReplicaSideLocal, nodeFile1.nodeIdLocal().value(), nodeFileLocal, found) &&
                       found);
        CPPUNIT_ASSERT_EQUAL(dbNodeIdFile1, nodeFileLocal.nodeId());
        CPPUNIT_ASSERT_EQUAL(nodeFile1.parentNodeId().value(), nodeFileLocal.parentNodeId().value());
        CPPUNIT_ASSERT(nodeFileLocal.nameLocal() == nodeFile1.nameLocal());
        CPPUNIT_ASSERT_EQUAL(nodeFile1.created().value(), nodeFileLocal.created().value());
        CPPUNIT_ASSERT_EQUAL(nodeFile1.lastModifiedLocal().value(), nodeFileLocal.lastModifiedLocal().value());
        CPPUNIT_ASSERT_EQUAL(nodeFile1.type(), nodeFileLocal.type());
        CPPUNIT_ASSERT_EQUAL(nodeFile1.size(), nodeFileLocal.size());

        DbNode nodeFileRemote;
        CPPUNIT_ASSERT(_testObj->node(ReplicaSide::ReplicaSideRemote, nodeFile1.nodeIdRemote().value(), nodeFileRemote, found) &&
                       found);
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

    CPPUNIT_ASSERT(_testObj->updateAllSyncNodes(SyncNodeTypeBlackList, nodeIdSet));
    CPPUNIT_ASSERT(_testObj->updateAllSyncNodes(SyncNodeTypeUndecidedList, nodeIdSet2));

    std::unordered_set<NodeId> nodeIdSet3;
    CPPUNIT_ASSERT(_testObj->selectAllSyncNodes(SyncNodeTypeBlackList, nodeIdSet3));
    CPPUNIT_ASSERT_EQUAL(size_t(5), nodeIdSet3.size());
    CPPUNIT_ASSERT(nodeIdSet3.contains("1"));
    CPPUNIT_ASSERT(nodeIdSet3.contains("2"));
    CPPUNIT_ASSERT(nodeIdSet3.contains("3"));
    CPPUNIT_ASSERT(nodeIdSet3.contains("4"));
    CPPUNIT_ASSERT(nodeIdSet3.contains("5"));
    nodeIdSet3.clear();
    CPPUNIT_ASSERT(_testObj->selectAllSyncNodes(SyncNodeTypeUndecidedList, nodeIdSet3));
    CPPUNIT_ASSERT_EQUAL(size_t(3), nodeIdSet3.size());
    CPPUNIT_ASSERT(nodeIdSet3.contains("11"));
    CPPUNIT_ASSERT(nodeIdSet3.contains("12"));
    CPPUNIT_ASSERT(nodeIdSet3.contains("13"));

    CPPUNIT_ASSERT(_testObj->updateAllSyncNodes(SyncNodeTypeBlackList, std::unordered_set<NodeId>()));
    nodeIdSet3.clear();
    CPPUNIT_ASSERT(_testObj->selectAllSyncNodes(SyncNodeTypeBlackList, nodeIdSet3));
    CPPUNIT_ASSERT_EQUAL(size_t(0), nodeIdSet3.size());
}

void TestSyncDb::testCorrespondingNodeId() {
    time_t tLoc = std::time(0);
    time_t tDrive = std::time(0);
    bool constraintError = false;


    DbNode nodeDir(0, _testObj->rootNode().nodeId(), Str("Dir loc 1"), Str("Dir drive 1"), "id dir loc 1", "id dir drive 1", tLoc,
                   tLoc, tDrive, NodeType::NodeTypeDirectory, 0, std::nullopt);
    DbNodeId dbNodeIdDir;
    CPPUNIT_ASSERT(_testObj->insertNode(nodeDir, dbNodeIdDir, constraintError));
    CPPUNIT_ASSERT(!constraintError);

    DbNodeId dbNodeIdFile;
    DbNode nodeFile(0, _testObj->rootNode().nodeId(), Str("File loc 1"), Str("File drive 1"), "id file loc 1", "id file drive 1",
                    tLoc, tLoc, tDrive, NodeType::NodeTypeDirectory, 0, std::nullopt);
    CPPUNIT_ASSERT(_testObj->insertNode(nodeFile, dbNodeIdFile, constraintError));
    CPPUNIT_ASSERT(!constraintError);

    // Normal case
    NodeId correspondingNodeId;
    bool found = false;
    CPPUNIT_ASSERT(_testObj->correspondingNodeId(ReplicaSideLocal, "id dir loc 1", correspondingNodeId, found));
    CPPUNIT_ASSERT(found);
    CPPUNIT_ASSERT_EQUAL(std::string("id dir drive 1"), correspondingNodeId);

    CPPUNIT_ASSERT(_testObj->correspondingNodeId(ReplicaSideRemote, "id dir drive 1", correspondingNodeId, found));
    CPPUNIT_ASSERT(found);
    CPPUNIT_ASSERT_EQUAL(std::string("id dir loc 1"), correspondingNodeId);

    // Wrong Side case
    CPPUNIT_ASSERT(_testObj->correspondingNodeId(ReplicaSideRemote, "id dir loc 1", correspondingNodeId, found));
    CPPUNIT_ASSERT(!found);

    CPPUNIT_ASSERT(_testObj->correspondingNodeId(ReplicaSideLocal, "id dir drive 1", correspondingNodeId, found));
    CPPUNIT_ASSERT(!found);

    // Wrong id case
    CPPUNIT_ASSERT(_testObj->correspondingNodeId(ReplicaSideLocal, "id dir loc 2", correspondingNodeId, found));
    CPPUNIT_ASSERT(!found);

    CPPUNIT_ASSERT(_testObj->correspondingNodeId(ReplicaSideRemote, "id dir drive 2", correspondingNodeId, found));
    CPPUNIT_ASSERT(!found);

    // Unknow side case
    CPPUNIT_ASSERT(!_testObj->correspondingNodeId(ReplicaSideUnknown, "id dir loc 1", correspondingNodeId, found));
    CPPUNIT_ASSERT(!found);
}
}  // namespace KDC
