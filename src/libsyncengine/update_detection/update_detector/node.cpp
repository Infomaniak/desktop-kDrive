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

#include "node.h"

#include "libcommon/utility/utility.h"
#include "libcommon/utility/logiffail.h"

#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"

namespace KDC {

Node::Node(const std::optional<DbNodeId> &idb, const ReplicaSide &side, const SyncName &name, NodeType type,
           OperationType changeEvents, const std::optional<NodeId> &id, std::optional<SyncTime> createdAt,
           std::optional<SyncTime> lastmodified, int64_t size) :
    _idb(idb),
    _side(side),
    _name(name),
    _type(type),
    _id(id),
    _createdAt(createdAt),
    _lastModified(lastmodified),
    _size(size),
    _conflictsAlreadyConsidered(std::vector<ConflictType>()) {
    setChangeEvents(changeEvents);
}

Node::Node(const std::optional<DbNodeId> &idb, const ReplicaSide &side, const SyncName &name, NodeType type,
           OperationType changeEvents, const std::optional<NodeId> &id, std::optional<SyncTime> createdAt,
           std::optional<SyncTime> lastmodified, int64_t size, const MoveOriginInfos &moveOriginInfos) :
    Node(idb, side, name, type, OperationType::None, id, createdAt, lastmodified, size) {
    setMoveOriginInfos(moveOriginInfos);
    setChangeEvents(changeEvents);
}

Node::Node(const ReplicaSide &side, const SyncName &name, NodeType type, OperationType changeEvents,
           const std::optional<NodeId> &id, std::optional<SyncTime> createdAt, std::optional<SyncTime> lastmodified,
           int64_t size) :
    Node(std::nullopt, side, name, type, changeEvents, id, createdAt, lastmodified, size) {}

Node::Node(const ReplicaSide &side, const SyncName &name, NodeType type) :
    Node(std::nullopt, side, name, type, OperationType::None, "tmp_" + CommonUtility::generateRandomStringAlphaNum(),
         std::nullopt, std::nullopt, 0) {
    _isTmp = true;
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

bool Node::moveTo(std::shared_ptr<Node> parentNode) {
    if (_parentNode == parentNode) return true;

    if (parentNode) {
        if (!_id) {
            LOGW_WARN(Log::instance()->getLogger(),
                      L"A node without ID cannot have a parent: " << Node::formatNode(shared_from_this()));
            assert(false);
            sentry::Handler::captureMessage(sentry::Level::Warning, "Node::moveTo", "Node without ID");
            return false;
        } else if (parentNode->children().contains(*_id)) {
            LOGW_WARN(Log::instance()->getLogger(), L"The parent already has a child with the same node ID: "
                                                            << Node::formatNodeWithParent(shared_from_this(), parentNode));
            assert(false);
            sentry::Handler::captureMessage(sentry::Level::Warning, "Node::moveTo", "Node is a child");
            return false;
        }

        if (!isParentValid(parentNode)) {
            LOGW_WARN(Log::instance()->getLogger(), L"The parent is the node or one of its descendants: "
                                                            << Node::formatNodeWithParent(shared_from_this(), parentNode));
            assert(false);
            sentry::Handler::captureMessage(sentry::Level::Warning, "Node::moveTo", "Parent is a descendant");
            return false;
        }
    }

    detach();

    if (parentNode) {
        // Set the new parent.
        _parentNode = parentNode;

        // Add the node to the list of children of the new parent.
        _parentNode->_childrenById[*_id] = shared_from_this();
    }

    return true;
}

void Node::detach() {
    if (_parentNode) {
        // Remove the node from the list of children of the parent node.
        if (_id) {
            if (auto it = _parentNode->_childrenById.find(*_id);
                it != _parentNode->_childrenById.end() && it->second.get() == this) {
                _parentNode->_childrenById.erase(it);
            } else {
                // This should not happen, the ID is not found or corresponds to another node.
                LOG_IF_FAIL(Log::instance()->getLogger(), false);
            }
        } else {
            // This should not happen, as a node without an ID should not have a parent.
            LOG_IF_FAIL(Log::instance()->getLogger(), false);
        }

        // Unset the parent node.
        _parentNode.reset();
    }
}

std::shared_ptr<Node> Node::getChildExcept(const SyncName &normalizedName, const OperationType except) {
    for (const auto &[_, child]: children()) {
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
    // LOG_IF_FAIL(Log::instance()->getLogger(), !hasInvalidEvents()); TODO: to be reactivated once the tests have been cleaned up
}

void Node::insertChangeEvent(const OperationType op) {
    _changeEvents |= op;
    LOG_IF_FAIL(Log::instance()->getLogger(), (!hasChangeEvent(OperationType::Move) || _moveOriginInfos.isValid()));
    // LOG_IF_FAIL(Log::instance()->getLogger(), !hasInvalidEvents()); TODO: to be reactivated once the tests have been cleaned up
}

bool Node::hasInvalidEvents() const {
    const bool res = (hasChangeEvent(OperationType::Create) && _changeEvents != OperationType::Create) ||
                     (hasChangeEvent(OperationType::Delete) && _changeEvents != OperationType::Delete) ||
                     ((hasChangeEvent(OperationType::Move) || hasChangeEvent(OperationType::Edit)) &&
                      _changeEvents != OperationType::Move && _changeEvents != OperationType::Edit &&
                      _changeEvents != OperationType::MoveEdit);
    return res;
}

std::shared_ptr<Node> Node::findChild(const SyncName &name, const NodeId &nodeId /*= ""*/) {
    // First we search by id if it is not empty
    if (!nodeId.empty()) {
        if (auto res = findChildById(nodeId); res) {
            return res;
        }
    }

    // If this search does not succeed then we search by name
    for (auto &node: _childrenById) {
        // We want to find a tmp node since the search by id gave nothing
        if (!nodeId.empty() && !node.second->isTmp()) continue;
        if (node.second->name() == name) {
            return node.second;
        }
    }

    return nullptr;
}

std::shared_ptr<Node> Node::findChildById(const NodeId &nodeId) {
    auto it = _childrenById.find(nodeId);
    if (it != _childrenById.end()) {
        return it->second;
    }
    return nullptr;
}

bool Node::setId(const std::optional<NodeId> &nodeId) {
    if (_id == nodeId) return true;

    if (_parentNode) {
        if (nodeId && _parentNode->_childrenById.contains(*nodeId)) {
            LOGW_WARN(Log::instance()->getLogger(), L"Parent already has the node with ID: "
                                                            << CommonUtility::s2ws(*nodeId) << L" as a child: "
                                                            << Node::formatNodeWithParent(shared_from_this(), _parentNode));
            assert(false);
            return false;
        }

        // Update the list of children of the parent node.
        if (_id) {
            const auto count = _parentNode->_childrenById.erase(*_id);
            if (count == 0) {
                LOGW_WARN(Log::instance()->getLogger(), L"Parent does not have the node with ID: "
                                                                << CommonUtility::s2ws(*_id) << L" as a child: "
                                                                << Node::formatNodeWithParent(shared_from_this(), _parentNode));
                assert(false);
                return false;
            }
        }

        if (nodeId) {
            const auto [_, inserted] = _parentNode->_childrenById.emplace(*nodeId, shared_from_this());
            if (!inserted) {
                LOGW_WARN(Log::instance()->getLogger(), L"The list of children of the parent node couldn't be updated: "
                                                                << Node::formatNodeWithParent(shared_from_this(), _parentNode));
                assert(false);
                return false;
            }
        }
    }
    _id = nodeId;
    return true;
}

void Node::clear() {
    (void) moveTo(nullptr);

    const auto children = this->children();
    for (const auto &[_, child]: children) {
        child->detach();
    }
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

bool Node::isTmp() const {
    return _isTmp;
}

bool Node::isParentOf(const std::shared_ptr<const Node> &potentialChild) const {
    if (!potentialChild) return false;

    auto logEmptyIdError = [](const std::shared_ptr<const Node> &node) {
        LOGW_WARN(Log::instance()->getLogger(), L"Node has no id: " << Node::formatNode(node));
        assert(false && "Node has no id");
        return false;
    };

    if (!_id) return logEmptyIdError(shared_from_this());
    if (!potentialChild->id()) return logEmptyIdError(potentialChild);

    // potentialChild cannot be this node or have the same ID as this node
    if (potentialChild.get() == this) return false;
    if (*potentialChild->id() == *_id) return false;

    auto tmpNode = potentialChild->parentNode();
    while (tmpNode) {
        if (!tmpNode->id()) {
            return logEmptyIdError(tmpNode);
        }

        if (*tmpNode->id() == *_id) return true; // This node is a parent of `potentialChild`

        tmpNode = tmpNode->parentNode();
    }

    return false;
}

bool Node::isValid() const {
    return _id.has_value() && !_id.value().empty() && !_isTmp;
}

std::wstring Node::formatNode(const std::shared_ptr<const Node> &node) {
    std::wstringstream ss;
    if (node) {
        ss << L"Node '" << SyncName2WStr(node->name()).c_str() << L"' (ID: '" << CommonUtility::s2ws(node->id().value_or(""))
           << L"')";
    } else {
        ss << L"Node 'nullptr'";
    }

    return ss.str();
}

std::wstring Node::formatNodeWithParent(const std::shared_ptr<const Node> &node, const std::shared_ptr<const Node> &parentNode) {
    std::wstringstream ss;
    ss << formatNode(node) << L" parent " << formatNode(parentNode);

    return ss.str();
}

bool Node::isParentValid(const std::shared_ptr<const Node> &parentNode) const {
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
