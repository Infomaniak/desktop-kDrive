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

#include "updatetree.h"
#include "libcommon/utility/utility.h"

#define MAX_DEPTH 1000

namespace KDC {

UpdateTree::UpdateTree(ReplicaSide side, const DbNode &dbNode)
    : _nodes(std::unordered_map<NodeId, std::shared_ptr<Node>>()),
      _rootNode(std::shared_ptr<Node>(
          new Node(dbNode.nodeId(), side, (side == ReplicaSide::Local ? dbNode.nameLocal() : dbNode.nameRemote()),
                   NodeType::Directory, {}, (side == ReplicaSide::Local ? dbNode.nodeIdLocal() : dbNode.nodeIdRemote()),
                   (side == ReplicaSide::Local ? dbNode.created() : dbNode.created()),
                   (side == ReplicaSide::Local ? dbNode.lastModifiedLocal() : dbNode.lastModifiedRemote()), 0, nullptr))),
      _side(side) {}

UpdateTree::~UpdateTree() {
    clear();
}

void UpdateTree::insertNode(std::shared_ptr<Node> node) {
    if (!node) {
        return;
    }

    _nodes[*node->id()] = node;
}

bool UpdateTree::deleteNode(std::shared_ptr<Node> node, int depth) {
    if (!node) {
        return false;
    }

    if (depth > MAX_DEPTH) {
        assert(false);
        SentryHandler::instance()->captureMessage(SentryLevel::Warning, "UpdateTree::deleteNode", "UpdateTree loop");
        return false;
    }

    // Recursively remove all children
    while (node->children().size() > 0) {
        if (!deleteNode((node->children().begin())->second, depth + 1)) {
            return false;
        }
    }

    // Remove node from tree
    node->parentNode()->deleteChildren(node);
    _nodes.erase(*node->id());

    return true;
}

bool UpdateTree::deleteNode(const NodeId &id) {
    std::shared_ptr<Node> node = getNodeById(id);
    return deleteNode(node);
}

std::shared_ptr<Node> UpdateTree::getNodeByPath(const SyncPath &path) {
    if (path.empty()) {
        return _rootNode;
    }

    // Split path
    std::vector<SyncName> names;
    SyncPath pathTmp(path);
    while (pathTmp != pathTmp.root_path()) {
        names.push_back(pathTmp.filename().native());
        pathTmp = pathTmp.parent_path();
    }

    std::shared_ptr<Node> tmpNode = _rootNode;
    for (std::vector<SyncName>::reverse_iterator nameIt = names.rbegin(); nameIt != names.rend(); ++nameIt) {
        std::shared_ptr<Node> tmpChildNode = nullptr;
        for (const auto &childNode : tmpNode->children()) {
            if (*nameIt == childNode.second->name()) {
                tmpChildNode = childNode.second;
                break;
            }
        }
        if (tmpChildNode == nullptr) {
            return nullptr;
        }
        tmpNode = tmpChildNode;
    }
    return tmpNode;
}

std::shared_ptr<Node> UpdateTree::getNodeById(const NodeId &nodeId) {
    auto it = _nodes.find(nodeId);
    if (it != _nodes.end()) {
        return it->second;
    }
    return nullptr;
}

bool UpdateTree::exists(const NodeId &id) {
    return getNodeById(id) != nullptr;
}

bool UpdateTree::isAncestor(const NodeId &nodeId, const NodeId &ancestorNodeId) const {
    auto it = _nodes.find(nodeId);
    if (it == _nodes.end()) {
        return false;
    }

    std::shared_ptr<Node> directParent = it->second->parentNode();
    if (directParent->id() == ancestorNodeId) {
        return true;
    }

    if (directParent == _rootNode) {
        // We have reached the root directory
        return false;
    }

    return isAncestor(*directParent->id(), ancestorNodeId);
}

void UpdateTree::markAllNodesUnprocessed() {
    startUpdate();

    for (auto &node : _nodes) {
        node.second->setStatus(NodeStatus::Unprocessed);
    }
}

void UpdateTree::init() {
    clear();

    insertNode(_rootNode);
    _inconsistencyCheckDone = false;
}

void UpdateTree::clear() {
    std::unordered_map<NodeId, std::shared_ptr<Node>>::iterator it = _nodes.begin();
    while (it != _nodes.end()) {
        it->second->parentNode().reset();
        it->second->children().clear();
        it++;
    }
    _nodes.clear();
}

}  // namespace KDC
