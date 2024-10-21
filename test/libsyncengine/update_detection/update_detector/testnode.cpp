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


#include "testnode.h"
#include "update_detection/update_detector/node.h"

using namespace CppUnit;

namespace KDC {

void TestNode::testIsParentValid() {
    const auto node1 = std::make_shared<Node>(std::nullopt, ReplicaSide::Local, Str("Dir 1"), NodeType::Directory,
                                              OperationType::None, "l1", 0, 0, 12345, nullptr);
    const auto node11 = std::make_shared<Node>(std::nullopt, ReplicaSide::Local, Str("Dir 1.1"), NodeType::Directory,
                                               OperationType::None, "l11", 0, 0, 12345, node1);

    CPPUNIT_ASSERT(node11->isParentValid(node1));
    CPPUNIT_ASSERT(!node1->isParentValid(node11));
}

void TestNode::testIsParentOf() {

    const auto node1 = std::make_shared<Node>(std::nullopt, ReplicaSide::Local, Str("Dir 1"), NodeType::Directory,
                                              OperationType::None, "l1", 0, 0, 12345, nullptr);
    const auto node11 = std::make_shared<Node>(std::nullopt, ReplicaSide::Local, Str("Dir 1.1"), NodeType::Directory,
                                               OperationType::None, "l11", 0, 0, 12345, node1);
    const auto node111 = std::make_shared<Node>(std::nullopt, ReplicaSide::Local, Str("Dir 1.1.1"), NodeType::Directory,
                                               OperationType::None, "l111", 0, 0, 12345, node11);


    CPPUNIT_ASSERT(node1->isParentOf(node11));
    CPPUNIT_ASSERT(!node11->isParentOf(node1));
    CPPUNIT_ASSERT(!node11->isParentOf(node11));
    CPPUNIT_ASSERT(node1->isParentOf(node111));
    CPPUNIT_ASSERT(node11->isParentOf(node111));
}

} // namespace KDC
