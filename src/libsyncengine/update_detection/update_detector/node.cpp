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

#include "node.h"

#include "libcommon/utility/utility.h"
#include "libcommon/utility/logiffail.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/log/log.h"

namespace KDC {

Node::Node(const std::optional<DbNodeId> &idb, const ReplicaSide &side, const SyncName &name, NodeType type,
           OperationType changeEvents, const std::optional<NodeId> &id, std::optional<SyncTime> createdAt,
           std::optional<SyncTime> lastmodified, int64_t size, std::shared_ptr<Node> parentNode) :
    _idb(idb),
    _side(side),
    _name(name),
    _type(type),
    _id(id),
    _createdAt(createdAt),
    _lastModified(lastmodified),
    _size(size),
    _conflictsAlreadyConsidered(std::vector<ConflictType>()) {
    setParentNode(parentNode);
    setChangeEvents(changeEvents);
}

Node::Node(const std::optional<DbNodeId> &idb, const ReplicaSide &side, const SyncName &name, NodeType type,
           OperationType changeEvents, const std::optional<NodeId> &id, std::optional<SyncTime> createdAt,
           std::optional<SyncTime> lastmodified, int64_t size, std::shared_ptr<Node> parentNode,
           const MoveOriginInfos &moveOriginInfos) :
    Node(idb, side, name, type, OperationType::None, id, createdAt, lastmodified, size, parentNode) {
    setParentNode(parentNode);
    setMoveOriginInfos(moveOriginInfos);
    setChangeEvents(changeEvents);
}

Node::Node(const ReplicaSide &side, const SyncName &name, NodeType type, OperationType changeEvents,
           const std::optional<NodeId> &id, std::optional<SyncTime> createdAt, std::optional<SyncTime> lastmodified, int64_t size,
           std::shared_ptr<Node> parentNode) :
    Node(std::nullopt, side, name, type, changeEvents, id, createdAt, lastmodified, size, parentNode) {}

Node::Node(const ReplicaSide &side, const SyncName &name, NodeType type, std::shared_ptr<Node> parentNode) :
    _side(side),
    _name(name),
    _type(type),
    _isTmp(true) {
    _id = "tmp_" + CommonUtility::generateRandomStringAlphaNum();
    setParentNode(parentNode);
}

bool Node::operator==(const Node &n) const {
    return n._idb == _idb && n._name == _name;
}

const SyncName &Node::normalizedName() {
    if (!_normalizedName.empty()) return _normalizedName;
    if (!Utility::normalizedSyncName(_name, _normalizedName)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Failed to normalize: " << Utility::formatSyncName(_name));
        return _name;
    }
    return _normalizedName;
}

void Node::setName(const SyncName &name) {
    _name = name;
    _normalizedName.clear();
}

bool Node::setParentNode(std::shared_ptr<Node> parentNode) {
    if (!parentNode) return true;

    // Check that the parent is not a descendant
    if (!isParentValid(parentNode)) {
        assert(false);
        sentry::Handler::captureMessage(sentry::Level::Warning, "Node::setParentNode", "Parent is a descendant");
        return false;
    }

    _parentNode = parentNode;
    return true;
}

std::shared_ptr<Node> Node::getChildExcept(const SyncName &normalizedName, const OperationType except) {
    for (auto &[_, child]: this->children()) {
        SyncName normalizedChildName;
        if (!Utility::normalizedSyncName(child->name(), normalizedChildName)) {
            LOGW_WARN(Log::instance()->getLogger(), L"Failed to normalize: " << Utility::formatSyncName(child->name()));
            return nullptr;
        }

        // return only non excluded type
        if (normalizedChildName == normalizedName && !child->hasChangeEvent(except)) {
            return child;
        }
    }
    return nullptr;
}

void Node::setChangeEvents(const OperationType ops) {
    _changeEvents = ops;
    LOG_IF_FAIL(Log::instance()->getLogger(), (!hasChangeEvent(OperationType::Move) || _moveOriginInfos.isValid()));
}

void Node::insertChangeEvent(const OperationType op) {
    _changeEvents |= op;
    LOG_IF_FAIL(Log::instance()->getLogger(), (!hasChangeEvent(OperationType::Move) || _moveOriginInfos.isValid()));
}

std::shared_ptr<Node> Node::findChildren(const SyncName &name, const NodeId &nodeId /*= ""*/) {
    if (!nodeId.empty()) {
        auto res = findChildrenById(nodeId);
        if (res) {
            return res;
        }
    }

    for (auto &node: _childrenById) {
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
                sentry::Handler::captureMessage(sentry::Level::Warning, "Node::insertChildren", "Child is an ancestor");
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

void Node::clear() {
    _parentNode.reset();
    _childrenById.clear();
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

bool Node::isSpecialFolder() const {
    return isCommonDocumentsFolder() || isSharedFolder();
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

bool Node::isParentOf(std::shared_ptr<const Node> potentialChild) const {
    if (!potentialChild) return false; // `parentNode` is the root node,
    if (potentialChild->id().has_value() && potentialChild->id() == _id) return false; // potentialChild cannot be its own parent
    while (potentialChild) {
        if (!potentialChild->id().has_value()) {
            LOG_ERROR(Log::instance()->getLogger(), "Error in Node::isParentOf: Node has no id.");
            assert(false && "Node has no id");
            return false;
        }
        if (*potentialChild->id() == *_id) return true; // This node is a parent of `potentialChild`
        potentialChild = potentialChild->parentNode();
    }

    return false;
}

bool Node::isParentValid(std::shared_ptr<const Node> parentNode) const {
    return !isParentOf(parentNode);
}

bool Node::MoveOriginInfos::isValid() const {
    return _isValid;
}

Node::MoveOriginInfos::MoveOriginInfos(const SyncPath &path, const NodeId &parentNodeId) :
    _isValid(true),
    _path(path),
    _parentNodeId(parentNodeId) {
    if (!Utility::normalizedSyncPath(_path, _normalizedPath)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in Utility::normalizedSyncPath: " << Utility::formatSyncPath(_path));
        _normalizedPath = _path;
    }
}

Node::MoveOriginInfos &Node::MoveOriginInfos::operator=(const MoveOriginInfos &other) {
    LOG_IF_FAIL(Log::instance()->getLogger(), other.isValid());
    _isValid = other.isValid();
    _path = other.path();
    _normalizedPath = other.normalizedPath();
    _parentNodeId = other.parentNodeId();
    return *this;
}

const SyncPath &Node::MoveOriginInfos::path() const {
    LOG_IF_FAIL(Log::instance()->getLogger(), isValid());
    return _path;
}

const SyncPath &Node::MoveOriginInfos::normalizedPath() const {
    LOG_IF_FAIL(Log::instance()->getLogger(), isValid());
    return _normalizedPath;
}

const NodeId &Node::MoveOriginInfos::parentNodeId() const {
    LOG_IF_FAIL(Log::instance()->getLogger(), isValid());
    return _parentNodeId;
}

void Node::MoveOriginInfos::clear() {
    _isValid = false;
    _path = defaultInvalidPath;
    _normalizedPath = defaultInvalidPath;
    _parentNodeId = defaultInvalidNodeId;
}

} // namespace KDC
