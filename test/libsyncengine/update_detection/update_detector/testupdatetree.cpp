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

#include "testupdatetree.h"

#include <memory>


using namespace CppUnit;

namespace KDC {

void TestUpdateTree::setUp() {
    _myTree = new UpdateTree(ReplicaSide::Local, SyncDb::driveRootNode());
}

void TestUpdateTree::tearDown() {
    delete _myTree;
}

void TestUpdateTree::testConstructors() {
#ifdef _WIN32
    const SyncName nfdName = Utility::normalizedSyncName(L"éàè", Utility::UnicodeNormalization::NFD);
    const SyncName nfcName = Utility::normalizedSyncName(L"éàè");
#else
    const SyncName nfdName = Utility::normalizedSyncName("éàè", Utility::UnicodeNormalization::NFD);
    const SyncName nfcName = Utility::normalizedSyncName("éàè");
#endif

    {
        Node node(std::nullopt, ReplicaSide::Remote, nfcName, NodeType::Directory, OperationType::None, "1", 0, 0, 123, nullptr);
        CPPUNIT_ASSERT(node.name() == nfcName);
    }

    {
        Node node(std::nullopt, ReplicaSide::Remote, nfdName, NodeType::Directory, OperationType::None, "1", 0, 0, 123, nullptr);
        CPPUNIT_ASSERT(node.name() == nfcName);
    }

    {
        Node node(ReplicaSide::Remote, nfcName, NodeType::Directory, nullptr);
        CPPUNIT_ASSERT(node.name() == nfcName);
    }

    {
        Node node(ReplicaSide::Remote, nfdName, NodeType::Directory, nullptr);
        CPPUNIT_ASSERT(node.name() == nfcName);
    }

    {
        Node node;
        node.setName(nfdName);
        CPPUNIT_ASSERT(node.name() == nfcName);
    }
}

void TestUpdateTree::testAll() {
    CPPUNIT_ASSERT(_myTree->_nodes.empty());
    _myTree->insertNode(std::make_shared<Node>());
    CPPUNIT_ASSERT(_myTree->_nodes.size() == 1);

    auto node1 = std::make_shared<Node>(std::nullopt, _myTree->side(), Str("Dir 1"), NodeType::Directory, OperationType::None,
                                        "l1", 0, 0, 12345, _myTree->rootNode());
    auto node2 = std::make_shared<Node>(std::nullopt, _myTree->side(), Str("Dir 2"), NodeType::Directory, OperationType::None,
                                        "l2", 0, 0, 12345, _myTree->rootNode());
    auto node3 = std::make_shared<Node>(std::nullopt, _myTree->side(), Str("Dir 3"), NodeType::Directory, OperationType::None,
                                        "l3", 0, 0, 12345, _myTree->rootNode());
    auto node4 = std::make_shared<Node>(std::nullopt, _myTree->side(), Str("Dir 4"), NodeType::Directory, OperationType::None,
                                        "l4", 0, 0, 12345, _myTree->rootNode());
    auto node11 = std::make_shared<Node>(std::nullopt, _myTree->side(), Str("Dir 1.1"), NodeType::Directory, OperationType::None,
                                         "l11", 0, 0, 12345, node1);
    auto node111 = std::make_shared<Node>(std::nullopt, _myTree->side(), Str("Dir 1.1.1"), NodeType::Directory,
                                          OperationType::None, "l111", 0, 0, 12345, node11);
    auto node1111 = std::make_shared<Node>(std::nullopt, _myTree->side(), Str("File 1.1.1.1"), NodeType::File,
                                           OperationType::None, "l1111", 0, 0, 12345, node111);
    auto node31 = std::make_shared<Node>(std::nullopt, _myTree->side(), Str("Dir 3.1"), NodeType::Directory, OperationType::None,
                                         "l31", 0, 0, 12345, node3);
    auto node41 = std::make_shared<Node>(std::nullopt, _myTree->side(), Str("Dir 4.1"), NodeType::Directory, OperationType::None,
                                         "l41", 0, 0, 12345, node4);
    auto node411 = std::make_shared<Node>(std::nullopt, _myTree->side(), Str("Dir 4.1.1"), NodeType::Directory,
                                          OperationType::None, "l411", 0, 0, 12345, node41);
    auto node4111 = std::make_shared<Node>(std::nullopt, _myTree->side(), Str("File 4.1.1.1"), NodeType::File,
                                           OperationType::None, "l4111", 0, 0, 12345, node411);

    CPPUNIT_ASSERT(_myTree->rootNode()->insertChildren(node1));
    CPPUNIT_ASSERT(_myTree->rootNode()->insertChildren(node2));
    CPPUNIT_ASSERT(_myTree->rootNode()->insertChildren(node3));
    CPPUNIT_ASSERT(_myTree->rootNode()->insertChildren(node4));
    CPPUNIT_ASSERT(node1->insertChildren(node11));
    CPPUNIT_ASSERT(node11->insertChildren(node111));
    CPPUNIT_ASSERT(node111->insertChildren(node1111));
    CPPUNIT_ASSERT(node3->insertChildren(node31));
    CPPUNIT_ASSERT(node4->insertChildren(node41));
    CPPUNIT_ASSERT(node41->insertChildren(node411));
    CPPUNIT_ASSERT(node411->insertChildren(node4111));

    _myTree->insertNode(node1111);
    _myTree->insertNode(node111);
    _myTree->insertNode(node11);
    _myTree->insertNode(node1);
    _myTree->insertNode(node2);
    _myTree->insertNode(node3);
    _myTree->insertNode(node4);
    _myTree->insertNode(node31);
    _myTree->insertNode(node41);
    _myTree->insertNode(node411);
    _myTree->insertNode(node4111);

    CPPUNIT_ASSERT(_myTree->rootNode()->getPath() == "");
    CPPUNIT_ASSERT(node4->getPath() == "Dir 4");
    CPPUNIT_ASSERT(node41->getPath() == "Dir 4/Dir 4.1");
    CPPUNIT_ASSERT(node411->getPath() == "Dir 4/Dir 4.1/Dir 4.1.1");
    CPPUNIT_ASSERT(node4111->getPath() == "Dir 4/Dir 4.1/Dir 4.1.1/File 4.1.1.1");
    CPPUNIT_ASSERT(node31->getPath() == "Dir 3/Dir 3.1");
}

void TestUpdateTree::testChangeEvents() {
    CPPUNIT_ASSERT(_myTree->_nodes.empty());
    _myTree->insertNode(std::make_shared<Node>());
    CPPUNIT_ASSERT(_myTree->_nodes.size() == 1);

    std::shared_ptr<Node> node = std::make_shared<Node>(std::nullopt, _myTree->side(), Str("Dir 0"), NodeType::Directory,
                                                        OperationType::None, "0", 0, 0, 12345, _myTree->rootNode());
    const std::shared_ptr<Node> nodeCreate =
        std::make_shared<Node>(std::nullopt, _myTree->side(), Str("Dir 1"), NodeType::Directory, OperationType::Create, "l1", 0,
                               0, 12345, _myTree->rootNode());
    const std::shared_ptr<Node> nodeEdit =
        std::make_shared<Node>(std::nullopt, _myTree->side(), Str("Dir 2"), NodeType::Directory, OperationType::Edit, "l2", 0, 0,
                               12345, _myTree->rootNode());
    const std::shared_ptr<Node> nodeMove =
        std::make_shared<Node>(std::nullopt, _myTree->side(), Str("Dir 3"), NodeType::Directory, OperationType::Move, "l3", 0, 0,
                               12345, _myTree->rootNode());
    const std::shared_ptr<Node> nodeDelete =
        std::make_shared<Node>(std::nullopt, _myTree->side(), Str("Dir 4"), NodeType::Directory, OperationType::Delete, "l4", 0,
                               0, 12345, _myTree->rootNode());
    const std::shared_ptr<Node> nodeMoveEdit =
        std::make_shared<Node>(std::nullopt, _myTree->side(), Str("Dir 5"), NodeType::Directory,
                               OperationType::Move | OperationType::Edit, "l5", 0, 0, 12345, _myTree->rootNode());

    CPPUNIT_ASSERT(!node->hasChangeEvent());
    CPPUNIT_ASSERT(!node->hasChangeEvent(OperationType::Create));
    CPPUNIT_ASSERT(nodeCreate->hasChangeEvent(OperationType::Create));
    CPPUNIT_ASSERT(!nodeCreate->hasChangeEvent(OperationType::Edit));
    CPPUNIT_ASSERT(nodeEdit->hasChangeEvent(OperationType::Edit));
    CPPUNIT_ASSERT(nodeMove->hasChangeEvent(OperationType::Move));
    CPPUNIT_ASSERT(nodeDelete->hasChangeEvent(OperationType::Delete));
    CPPUNIT_ASSERT(nodeMoveEdit->hasChangeEvent(OperationType::Edit));
    CPPUNIT_ASSERT(nodeMoveEdit->hasChangeEvent(OperationType::Move));
}

}  // namespace KDC
