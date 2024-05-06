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

#include "operationprocessor.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/utility.h"

namespace KDC {

OperationProcessor::OperationProcessor(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName)
    : ISyncWorker(syncPal, name, shortName) {}

bool OperationProcessor::isPseudoConflict(std::shared_ptr<Node> node, std::shared_ptr<Node> correspondingNode) {
    if (!node || !node->hasChangeEvent() || !correspondingNode || !correspondingNode->hasChangeEvent()) {
        // We can have a conflict only if the node on both replica has change events
        return false;
    }

    std::shared_ptr<Snapshot> snapshot =
        (node->side() == ReplicaSide::ReplicaSideLocal ? _syncPal->_localSnapshot : _syncPal->_remoteSnapshot);
    std::shared_ptr<Snapshot> otherSnapshot =
        (correspondingNode->side() == ReplicaSide::ReplicaSideLocal ? _syncPal->_localSnapshot : _syncPal->_remoteSnapshot);

    // Create-Create pseudo-conflict
    if (node->hasChangeEvent(OperationTypeCreate) && correspondingNode->hasChangeEvent(OperationTypeCreate) &&
        node->type() == NodeTypeDirectory && correspondingNode->type() == NodeTypeDirectory) {
        return true;
    }

    // Move-Move (Source) pseudo-conflict
    if (node->hasChangeEvent(OperationTypeMove) && correspondingNode->hasChangeEvent(OperationTypeMove) &&
        node->parentNode()->idb() == correspondingNode->parentNode()->idb() && node->name() == correspondingNode->name()) {
        return true;
    }

    // Edit-Edit & Create-Create pseudo-conflict
    if (!node->id().has_value() || !correspondingNode->id().has_value()) {
        LOG_SYNCPAL_WARN(_logger, "Error in OperationProcessor::isPseudoConflict, node ID not found");
        return false;
    }

    bool useContentChecksum =
        !snapshot->contentChecksum(*node->id()).empty() && !otherSnapshot->contentChecksum(*correspondingNode->id()).empty();
    bool sameSizeAndDate = snapshot->lastModifed(*node->id()) == otherSnapshot->lastModifed(*correspondingNode->id()) &&
                           snapshot->size(*node->id()) == otherSnapshot->size(*correspondingNode->id());
    if (node->type() == NodeType::NodeTypeFile && correspondingNode->type() == node->type() &&
        node->hasChangeEvent((OperationTypeCreate | OperationTypeEdit)) &&
        correspondingNode->hasChangeEvent(OperationTypeCreate | OperationTypeEdit) &&
        (useContentChecksum ? snapshot->contentChecksum(*node->id()) == otherSnapshot->contentChecksum(*correspondingNode->id())
                            : sameSizeAndDate)) {
        return true;
    }

    return false;
}

std::shared_ptr<Node> OperationProcessor::correspondingNodeInOtherTree(std::shared_ptr<Node> node) {
    std::optional<DbNodeId> dbNodeId = node->idb();
    if (!dbNodeId && node->id()) {
        // Find node in DB
        DbNodeId tmpDbNodeId = -1;
        bool found = false;
        if (!_syncPal->_syncDb->dbId(node->side(), *node->id(), tmpDbNodeId, found)) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbId for nodeId=" << (*node->id()).c_str());
            return nullptr;
        }
        if (found) {
            dbNodeId = tmpDbNodeId;
        }
    }

    if (dbNodeId) return correspondingNodeDirect(node);

    // The node is not in DB => find an ancestor
    return findCorrespondingNodeFromPath(node);
}

constexpr auto otherSide = [](ReplicaSide side) {
    return side == ReplicaSide::ReplicaSideLocal ? ReplicaSide::ReplicaSideRemote : ReplicaSide::ReplicaSideLocal;
};

std::shared_ptr<Node> OperationProcessor::findCorrespondingNodeFromPath(std::shared_ptr<Node> node) {
    std::shared_ptr<Node> parentNode = node;
    std::vector<SyncName> names;
    DbNodeId parentDbNodeId;
    bool found = false;
    while (parentNode != nullptr && !found) {
        if (!parentNode->id()) {
            LOG_SYNCPAL_WARN(_logger, "Parent node has an empty nodeId");
            return nullptr;
        }

        if (!_syncPal->_syncDb->dbId(node->side(), *parentNode->id(), parentDbNodeId, found)) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbId for nodeId=" << (*parentNode->id()).c_str());
            return nullptr;
        }
        if (!found) {
            names.push_back(parentNode->name());
            parentNode = parentNode->parentNode();
        }
    }

    // Construct relative path
    SyncPath relativeTraversedPath;
    for (std::vector<SyncName>::reverse_iterator nameIt = names.rbegin(); nameIt != names.rend(); ++nameIt) {
        relativeTraversedPath /= *nameIt;
    }

    // Find corresponding ancestor node id
    NodeId parentNodeId;
    if (!_syncPal->_syncDb->id(otherSide(node->side()), parentDbNodeId, parentNodeId, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::id for dbNodeId=" << parentDbNodeId);
        return nullptr;
    }

    // Find corresponding ancestor node in the other tree
    std::shared_ptr<UpdateTree> otherTree = _syncPal->getUpdateTree(otherSide(node->side()));
    std::shared_ptr<Node> correspondingParentNode = otherTree->getNodeById(parentNodeId);
    if (correspondingParentNode == nullptr) {
        LOG_SYNCPAL_WARN(_logger, "No corresponding node in the other tree for nodeId = " << parentNodeId.c_str());
        return nullptr;
    }

    // Construct path with ancestor path / relative path
    SyncPath correspondingPath = correspondingParentNode->getPath() / relativeTraversedPath;
    std::shared_ptr<Node> correspondingNode = otherTree->getNodeByPath(correspondingPath);
    if (correspondingNode == nullptr) {
        return nullptr;
    }

    return correspondingNode;
}

std::shared_ptr<Node> OperationProcessor::correspondingNodeDirect(std::shared_ptr<Node> node) {
    if (node->idb() == std::nullopt) {
        return nullptr;
    }

    bool found = false;
    NodeId correspondingId;
    if (!_syncPal->_syncDb->id(otherSide(node->side()), *node->idb(), correspondingId, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::id");
        return nullptr;
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "nodeId not found");
        return nullptr;
    }

    std::shared_ptr<UpdateTree> otherTree = _syncPal->getUpdateTree(otherSide(node->side()));
    NodeId effectiveCorrespondingId = correspondingId;

    auto previousIdIt = otherTree->previousIdSet().find(correspondingId);
    if (previousIdIt != otherTree->previousIdSet().end()) {
        effectiveCorrespondingId = previousIdIt->second;
    }

    auto it = otherTree->nodes().find(effectiveCorrespondingId);
    if (it != otherTree->nodes().end()) {
        return it->second;
    }
    return nullptr;
}

bool OperationProcessor::isABelowB(std::shared_ptr<Node> a, std::shared_ptr<Node> b) {
    std::shared_ptr<Node> tmpNode = a;
    while (tmpNode->parentNode() != nullptr) {
        if (tmpNode == b) {
            return true;
        }
        tmpNode = tmpNode->parentNode();
    }
    return false;
}

}  // namespace KDC
