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

#include "node.h"

#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/utility.h"

namespace KDC {

Node::Node(const std::optional<DbNodeId> &idb, const ReplicaSide &side, const SyncName &name, NodeType type,
           OperationType changeEvents, const std::optional<NodeId> &id, std::optional<SyncTime> createdAt,
           std::optional<SyncTime> lastmodified, int64_t size, std::shared_ptr<Node> parentNode,
           std::optional<SyncPath> moveOrigin, std::optional<DbNodeId> moveOriginParentDbId)
    : _idb(idb),
      _side(side),
      _name(Utility::normalizedSyncName(name)),
      _type(type),
      _changeEvents(changeEvents),
      _id(id),
      _createdAt(createdAt),
      _lastModified(lastmodified),
      _size(size),
      _moveOrigin(moveOrigin),
      _moveOriginParentDbId(moveOriginParentDbId),
      _conflictsAlreadyConsidered(std::vector<ConflictType>()) {
    setParentNode(parentNode);
}

Node::Node(const ReplicaSide &side, const SyncName &name, NodeType type, std::shared_ptr<Node> parentNode)
    : _side(side), _name(Utility::normalizedSyncName(name)), _type(type), _isTmp(true) {
    _id = "tmp_" + CommonUtility::generateRandomStringAlphaNum();
    setParentNode(parentNode);
}

bool Node::operator==(const Node &n) const {
    return n._idb == _idb && n._name == _name;
}

void Node::setName(const SyncName &name) {
    _name = Utility::normalizedSyncName(name);
}

bool Node::setParentNode(const std::shared_ptr<Node> &parentNode) {
    if (!parentNode) return true;

    // Check that the parent is not a descendant
    if (!isParentValid(parentNode)) {
        assert(false);
#ifdef NDEBUG
        sentry_capture_event(
            sentry_value_new_message_event(SENTRY_LEVEL_WARNING, "Node::setParentNode", "Parent is a descendant"));
#endif
        return false;
    }

    _parentNode = parentNode;
    return true;
}

std::shared_ptr<Node> Node::getChildExcept(SyncName name, OperationType except) {
    for (auto &child : this->children()) {
        // return only non excluded type
        if (child.second->name() == name && !child.second->hasChangeEvent(except)) {
            return child.second;
        }
    }
    return nullptr;
}

std::shared_ptr<Node> Node::findChildren(const SyncName &name, const NodeId &nodeId /*= ""*/) {
    if (!nodeId.empty()) {
        auto res = findChildrenById(nodeId);
        if (res) {
            return res;
        }
    }

    for (auto &node : _childrenById) {
        if (node.second->name() == name) {
            return node.second;
        }
    }

    return nullptr;
}

std::shared_ptr<Node> Node::findChildrenById(const NodeId &nodeId) {
    auto it = _childrenById.find(nodeId);
    if (it != _childrenById.end()) {
        return it->second;
    }
    return nullptr;
}

bool Node::insertChildren(std::shared_ptr<Node> child) {
    if (!child->id().has_value()) {
        return false;
    }

    // Check that the child is not an ancestor
    if (std::shared_ptr<Node> tmpNode = parentNode(); tmpNode) {
        while (tmpNode->parentNode() != nullptr) {
            if (child == tmpNode) {
                assert(false);
#ifdef NDEBUG
                sentry_capture_event(
                    sentry_value_new_message_event(SENTRY_LEVEL_WARNING, "Node::insertChildren", "Child is an ancestor"));
#endif
                return false;
            }

            tmpNode = tmpNode->parentNode();
        }
    }

    _childrenById[*child->id()] = child;

    return true;
}

size_t Node::deleteChildren(std::shared_ptr<Node> child) {
    if (!child->id().has_value()) {
        return 0;
    }
    return deleteChildren(*child->id());
}

size_t Node::deleteChildren(const NodeId &childId) {
    return _childrenById.erase(childId);
}

bool Node::isEditFromDeleteCreate() const {
    if (hasChangeEvent(OperationType::Edit) && _previousId.has_value()) {
        return true;
    }
    return false;
}

bool Node::isRoot() const {
    if (!_parentNode) {
        return true;
    }
    return false;
}

bool Node::isCommonDocumentsFolder() const {
    if (_parentNode && _parentNode->isRoot() && _name == Utility::commonDocumentsFolderName()) {
        return true;
    }
    return false;
}

bool Node::isSharedFolder() const {
    if (_parentNode && _parentNode->isRoot() && _name == Utility::sharedFolderName()) {
        return true;
    }
    return false;
}

SyncPath Node::getPath() const {
    std::vector<SyncName> names;
    names.push_back(name());

    if (std::shared_ptr<Node> tmpNode = parentNode(); tmpNode) {
        while (tmpNode->parentNode() != nullptr) {
            names.push_back(tmpNode->name());
            tmpNode = tmpNode->parentNode();
        }
    }

    SyncPath path;
    for (auto nameIt = names.rbegin(); nameIt != names.rend(); ++nameIt) {
        path /= *nameIt;
    }

    return path;
}

bool Node::isParentValid(std::shared_ptr<const Node> parentNode) const {
    if (!parentNode) return true;  // `parentNode` is the root node, hence a valid parent. Stop climbing up the tree.

    while (parentNode) {
        if (parentNode->id() == _id)
            return false;  // This node is a parent of `parentNode`, hence `parentNode` is not a valid parent.
        parentNode = parentNode->parentNode();
    }

    return true;
}

}  // namespace KDC
