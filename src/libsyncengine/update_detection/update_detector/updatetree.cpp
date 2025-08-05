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

#include "updatetree.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/log/log.h"
#include "requests/parameterscache.h"

#include <log4cplus/loggingmacros.h>

#define MAX_DEPTH 1000

namespace KDC {

UpdateTree::UpdateTree(ReplicaSide side, const DbNode &dbNode) :
    _nodes(std::unordered_map<NodeId, std::shared_ptr<Node>>()),
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
        sentry::Handler::captureMessage(sentry::Level::Warning, "UpdateTree::deleteNode", "Update tree depth is too big");
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
    const auto node = getNodeById(id);
    return deleteNode(node);
}

std::shared_ptr<Node> UpdateTree::getNodeByPath(const SyncPath &path) {
    if (path.empty()) {
        return _rootNode;
    }

    const auto &itemNames = CommonUtility::splitSyncPath(path);
    std::shared_ptr<Node> tmpNode = _rootNode;

    for (const auto &name: itemNames) {
        std::shared_ptr<Node> tmpChildNode = nullptr;

        for (const auto &[_, childNode]: tmpNode->children()) {
            if (name == childNode->name()) {
                tmpChildNode = childNode;
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

std::shared_ptr<Node> UpdateTree::getNodeByPathNormalized(const SyncPath &path) {
    if (path.empty()) {
        return _rootNode;
    }

    const auto &itemNames = CommonUtility::splitSyncPath(path);
    std::shared_ptr<Node> tmpNode = _rootNode;

    for (const auto &name: itemNames) {
        std::shared_ptr<Node> tmpChildNode = nullptr;
        SyncName normalizedSyncName;
        if (!Utility::normalizedSyncName(name, normalizedSyncName)) {
            LOGW_WARN(Log::instance()->getLogger(), L"Error in Utility::normalizedSyncName: " << Utility::formatSyncName(name));
            return nullptr;
        }

        for (const auto &[_, childNode]: tmpNode->children()) {
            if (normalizedSyncName == childNode->normalizedName()) {
                tmpChildNode = childNode;
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

    for (auto &node: _nodes) {
        node.second->setStatus(NodeStatus::Unprocessed);
    }
}

void UpdateTree::init() {
    insertNode(_rootNode);
}

bool UpdateTree::updateNodeId(std::shared_ptr<Node> node, const NodeId &newId) {
    const NodeId oldId = node->id().has_value() ? *node->id() : "";

    if (!node->parentNode()) {
        LOG_WARN(Log::instance()->getLogger(), "Bad parameters");
        assert(false);
        return false;
    }

    node->parentNode()->deleteChildren(node);
    node->setId(newId);

    if (!node->parentNode()->insertChildren(node)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in Node::insertChildren: node "
                                                        << Utility::formatSyncName(node->name()) << L" parent node "
                                                        << Utility::formatSyncName(node->parentNode()->name()));
        return false;
    }

    if (ParametersCache::isExtendedLogEnabled() && newId != oldId) {
        LOGW_DEBUG(Log::instance()->getLogger(), _side << L" update tree: Node ID changed from '" << CommonUtility::s2ws(oldId)
                                                       << L"' to '" << CommonUtility::s2ws(newId) << L"' for node "
                                                       << Utility::formatSyncName(node->name()) << L".");
    }

    if (!oldId.empty() && _nodes.contains(oldId)) {
        auto nodeRef = _nodes.extract(oldId);
        nodeRef.key() = newId;
        _nodes.insert(std::move(nodeRef));
    }
    return true;
}

void UpdateTree::clear() {
    std::unordered_map<NodeId, std::shared_ptr<Node>>::iterator it = _nodes.begin();
    while (it != _nodes.end()) {
        it->second->clear();
        it++;
    }
    _nodes.clear();
    _previousIdSet.clear();
    init();
}

void UpdateTree::drawUpdateTree() {
    if (const std::string drawUpdateTree = CommonUtility::envVarValue("KDRIVE_DEBUG_DRAW_UPDATETREE"); drawUpdateTree.empty()) {
        return;
    }

    SyncName treeStr;
    drawUpdateTreeRow(rootNode(), treeStr);
    LOGW_INFO(Log::instance()->getLogger(), _side << L" update tree:\n" << SyncName2WStr(treeStr));
}

void UpdateTree::drawUpdateTreeRow(const std::shared_ptr<Node> node, SyncName &treeStr, uint64_t depth /*= 0*/) {
    for (uint64_t i = 0; i < depth; i++) {
        treeStr += Str("\t");
    }
    treeStr += Str("'") + node->name() + Str("'");
    treeStr += Str("[");
    treeStr += Str2SyncName(*node->id());
    treeStr += Str(" / ");
    treeStr += node->changeEvents() != OperationType::None ? Str2SyncName(toString(node->changeEvents())).c_str() : Str("-");
    treeStr += Str("]");
    treeStr += Str("\n");

    depth++;
    for (const auto &[_, childNode]: node->children()) {
        drawUpdateTreeRow(childNode, treeStr, depth);
    }
}

} // namespace KDC
