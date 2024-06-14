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

using namespace CppUnit;

namespace KDC {

void TestUpdateTree::setUp() {
    _myTree = new UpdateTree(ReplicaSide::Local, SyncDb::driveRootNode());
}

void TestUpdateTree::tearDown() {
    delete _myTree;
}

void TestUpdateTree::testAll() {
    CPPUNIT_ASSERT(_myTree->_nodes.empty());
    _myTree->insertNode(std::shared_ptr<Node>(new Node()));
    CPPUNIT_ASSERT(_myTree->_nodes.size() == 1);

    std::shared_ptr<Node> node1 = std::shared_ptr<Node>(new Node(std::nullopt, _myTree->side(), Str("Dir 1"), NodeType::Directory,
                                                                 OperationTypeNone, "1", 0, 0, 12345, _myTree->rootNode()));
    std::shared_ptr<Node> node2 = std::shared_ptr<Node>(new Node(std::nullopt, _myTree->side(), Str("Dir 2"), NodeType::Directory,
                                                                 OperationTypeNone, "2", 0, 0, 12345, _myTree->rootNode()));
    std::shared_ptr<Node> node3 = std::shared_ptr<Node>(new Node(std::nullopt, _myTree->side(), Str("Dir 3"), NodeType::Directory,
                                                                 OperationTypeNone, "3", 0, 0, 12345, _myTree->rootNode()));
    std::shared_ptr<Node> node4 = std::shared_ptr<Node>(new Node(std::nullopt, _myTree->side(), Str("Dir 4"), NodeType::Directory,
                                                                 OperationTypeNone, "4", 0, 0, 12345, _myTree->rootNode()));
    std::shared_ptr<Node> node11 = std::shared_ptr<Node>(
        new Node(std::nullopt, _myTree->side(), Str("Dir 1.1"), NodeType::Directory, OperationTypeNone, "11", 0, 0, 12345, node1));
    std::shared_ptr<Node> node111 = std::shared_ptr<Node>(new Node(
        std::nullopt, _myTree->side(), Str("Dir 1.1.1"), NodeType::Directory, OperationTypeNone, "111", 0, 0, 12345, node11));
    std::shared_ptr<Node> node1111 = std::shared_ptr<Node>(new Node(
        std::nullopt, _myTree->side(), Str("File 1.1.1.1"), NodeType::File, OperationTypeNone, "1111", 0, 0, 12345, node111));
    std::shared_ptr<Node> node31 = std::shared_ptr<Node>(
        new Node(std::nullopt, _myTree->side(), Str("Dir 3.1"), NodeType::Directory, OperationTypeNone, "31", 0, 0, 12345, node3));
    std::shared_ptr<Node> node41 = std::shared_ptr<Node>(
        new Node(std::nullopt, _myTree->side(), Str("Dir 4.1"), NodeType::Directory, OperationTypeNone, "41", 0, 0, 12345, node4));
    std::shared_ptr<Node> node411 = std::shared_ptr<Node>(new Node(
        std::nullopt, _myTree->side(), Str("Dir 4.1.1"), NodeType::Directory, OperationTypeNone, "411", 0, 0, 12345, node41));
    std::shared_ptr<Node> node4111 = std::shared_ptr<Node>(new Node(
        std::nullopt, _myTree->side(), Str("File 4.1.1.1"), NodeType::File, OperationTypeNone, "4111", 0, 0, 12345, node411));

    _myTree->rootNode()->insertChildren(node1);
    _myTree->rootNode()->insertChildren(node2);
    _myTree->rootNode()->insertChildren(node3);
    _myTree->rootNode()->insertChildren(node4);
    node1->insertChildren(node11);
    node11->insertChildren(node111);
    node111->insertChildren(node1111);
    node3->insertChildren(node31);
    node4->insertChildren(node41);
    node41->insertChildren(node411);
    node411->insertChildren(node4111);

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
    _myTree->insertNode(std::shared_ptr<Node>(new Node()));
    CPPUNIT_ASSERT(_myTree->_nodes.size() == 1);

    std::shared_ptr<Node> node = std::shared_ptr<Node>(new Node(std::nullopt, _myTree->side(), Str("Dir 0"), NodeType::Directory,
                                                                OperationTypeNone, "0", 0, 0, 12345, _myTree->rootNode()));
    std::shared_ptr<Node> nodeCreate =
        std::shared_ptr<Node>(new Node(std::nullopt, _myTree->side(), Str("Dir 1"), NodeType::Directory, OperationTypeCreate, "1",
                                       0, 0, 12345, _myTree->rootNode()));
    std::shared_ptr<Node> nodeEdit =
        std::shared_ptr<Node>(new Node(std::nullopt, _myTree->side(), Str("Dir 2"), NodeType::Directory, OperationTypeEdit, "2", 0,
                                       0, 12345, _myTree->rootNode()));
    std::shared_ptr<Node> nodeMove =
        std::shared_ptr<Node>(new Node(std::nullopt, _myTree->side(), Str("Dir 3"), NodeType::Directory, OperationTypeMove, "3", 0,
                                       0, 12345, _myTree->rootNode()));
    std::shared_ptr<Node> nodeDelete =
        std::shared_ptr<Node>(new Node(std::nullopt, _myTree->side(), Str("Dir 4"), NodeType::Directory, OperationTypeDelete, "4",
                                       0, 0, 12345, _myTree->rootNode()));
    std::shared_ptr<Node> nodeMoveEdit =
        std::shared_ptr<Node>(new Node(std::nullopt, _myTree->side(), Str("Dir 5"), NodeType::Directory,
                                       OperationTypeMove | OperationTypeEdit, "5", 0, 0, 12345, _myTree->rootNode()));

    CPPUNIT_ASSERT(!node->hasChangeEvent());
    CPPUNIT_ASSERT(!node->hasChangeEvent(OperationTypeCreate));
    CPPUNIT_ASSERT(nodeCreate->hasChangeEvent(OperationTypeCreate));
    CPPUNIT_ASSERT(!nodeCreate->hasChangeEvent(OperationTypeEdit));
    CPPUNIT_ASSERT(nodeEdit->hasChangeEvent(OperationTypeEdit));
    CPPUNIT_ASSERT(nodeMove->hasChangeEvent(OperationTypeMove));
    CPPUNIT_ASSERT(nodeDelete->hasChangeEvent(OperationTypeDelete));
    CPPUNIT_ASSERT(nodeMoveEdit->hasChangeEvent(OperationTypeEdit));
    CPPUNIT_ASSERT(nodeMoveEdit->hasChangeEvent(OperationTypeMove));
}

}  // namespace KDC
