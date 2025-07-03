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

#include "operationprocessor.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/utility.h"

namespace KDC {

OperationProcessor::OperationProcessor(const std::shared_ptr<SyncPal> syncPal, const std::string &name,
                                       const std::string &shortName, bool useSyncDbCache) :
    ISyncWorker(syncPal, name, shortName),
    _useSyncDbCache(useSyncDbCache) {}

bool OperationProcessor::editChangeShouldBePropagated(std::shared_ptr<Node> affectedNode) {
    if (!affectedNode) {
        LOG_SYNCPAL_WARN(_logger, "hasChangeToPropagate: provided node is null");
        return true;
    }

    if (affectedNode->side() != ReplicaSide::Local) return true;

    bool found = false;
    DbNode affectedDbNode;
    if (affectedNode->idb().has_value()) {
        if (!(_useSyncDbCache ? _syncPal->syncDb()->cache().node(affectedNode->idb().value(), affectedDbNode, found)
                              : _syncPal->syncDb()->node(affectedNode->idb().value(), affectedDbNode, found))) {
            LOG_SYNCPAL_WARN(_logger, "hasChangeToPropagate: Failed to retrieve node from DB, id=" << *affectedNode->idb());
            _syncPal->addError(Error(errId(), ExitCode::DbError, ExitCause::DbAccessError));
            return true;
        }
    } else {
        if (!(_useSyncDbCache ? _syncPal->syncDb()->cache().node(ReplicaSide::Local, *affectedNode->id(), affectedDbNode, found)
                              : _syncPal->syncDb()->node(ReplicaSide::Local, *affectedNode->id(), affectedDbNode, found))) {
            LOG_SYNCPAL_WARN(_logger, "hasChangeToPropagate: Failed to retrieve node from DB, id=" << *affectedNode->id());
            _syncPal->addError(Error(errId(), ExitCode::DbError, ExitCause::DbAccessError));
            return true;
        }
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "hasChangeToPropagate: node not found in DB");
        return true;
    }

    if (affectedNode->size() == affectedDbNode.size() && affectedNode->modificationTime() == affectedDbNode.lastModifiedLocal() &&
        affectedNode->createdAt() != affectedDbNode.created()) {
        return false;
    }
    return true;
}

bool OperationProcessor::isPseudoConflict(const std::shared_ptr<Node> node, const std::shared_ptr<Node> correspondingNode) {
    if (!node || !node->hasChangeEvent() || !correspondingNode || !correspondingNode->hasChangeEvent()) {
        // We can have a conflict only if the node on both replica has change events
        return false;
    }

    // Create-Create pseudo-conflict
    if (node->hasChangeEvent(OperationType::Create) && correspondingNode->hasChangeEvent(OperationType::Create) &&
        node->type() == NodeType::Directory && correspondingNode->type() == NodeType::Directory) {
        return true;
    }

    // Move-Move (Source) pseudo-conflict
    bool isEqual = false;
    if (!Utility::checkIfSameNormalization(node->name(), correspondingNode->name(), isEqual)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in Utility::checkIfSameNormalization: "
                                                        << Utility::formatSyncName(node->name()) << L" / "
                                                        << Utility::formatSyncName(correspondingNode->name()));
        return false;
    }

    if (node->hasChangeEvent(OperationType::Move) && correspondingNode->hasChangeEvent(OperationType::Move) &&
        node->parentNode()->idb() == correspondingNode->parentNode()->idb() && isEqual) {
        if (!node->parentNode()->idb().has_value() && !correspondingNode->parentNode()->idb().has_value() &&
            node->parentNode()->getPath() != correspondingNode->parentNode()->getPath()) {
            return false; // The parent nodes are not in DB and have different paths
        }
        return true;
    }

    // Edit-Edit & Create-Create pseudo-conflict
    if (!node->id().has_value() || !correspondingNode->id().has_value()) {
        LOG_SYNCPAL_WARN(_logger, "Error in OperationProcessor::isPseudoConflict, node ID not found");
        return false;
    }

    // Size can differ for links between remote and local replica, do not check it in that case
    const auto snapshot = _syncPal->snapshot(node->side());
    const bool sameSizeAndDate = node->modificationTime() == correspondingNode->modificationTime() &&
                                 (snapshot->isLink(*node->id()) || node->size() == correspondingNode->size());

    const auto nodeChecksum = snapshot->contentChecksum(*node->id());
    const auto otherSnapshot = _syncPal->snapshot(correspondingNode->side());
    const auto correspondingNodeChecksum = otherSnapshot->contentChecksum(*correspondingNode->id());

    const bool useContentChecksum = !nodeChecksum.empty() && !correspondingNodeChecksum.empty();
    const bool hasSameContent = useContentChecksum ? nodeChecksum == correspondingNodeChecksum : sameSizeAndDate;

    const bool hasCreateOrEditChangeEvent =
            (node->hasChangeEvent(OperationType::Create) || node->hasChangeEvent(OperationType::Edit)) &&
            (correspondingNode->hasChangeEvent(OperationType::Create) || correspondingNode->hasChangeEvent(OperationType::Edit));

    if (node->type() == NodeType::File && correspondingNode->type() == node->type() && hasCreateOrEditChangeEvent &&
        hasSameContent) {
        return true;
    }

    return false;
}

std::shared_ptr<Node> OperationProcessor::correspondingNodeInOtherTree(const std::shared_ptr<Node> node) {
    std::optional<DbNodeId> dbNodeId = node->idb();
    if (!dbNodeId && node->id()) {
        // Find node in DB
        DbNodeId tmpDbNodeId = -1;
        bool found = false;
        if (!(_useSyncDbCache ? _syncPal->syncDb()->cache().dbId(node->side(), *node->id(), tmpDbNodeId, found)
                              : _syncPal->syncDb()->dbId(node->side(), *node->id(), tmpDbNodeId, found))) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbId for nodeId=" << (*node->id()));
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

std::shared_ptr<Node> OperationProcessor::findCorrespondingNodeFromPath(const std::shared_ptr<Node> node) {
    std::shared_ptr<Node> parentNode = node;
    std::vector<SyncName> names;
    DbNodeId parentDbNodeId = 0;
    bool found = false;

    while (parentNode != nullptr && !found) {
        if (!parentNode->id()) {
            LOG_SYNCPAL_WARN(_logger, "Parent node has an empty nodeId");
            return nullptr;
        }

        if (!(_useSyncDbCache ? _syncPal->syncDb()->cache().dbId(node->side(), *parentNode->id(), parentDbNodeId, found)
                              : _syncPal->syncDb()->dbId(node->side(), *parentNode->id(), parentDbNodeId, found))) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbId for nodeId=" << (*parentNode->id()));
            return nullptr;
        }
        if (!found) {
            names.push_back(parentNode->name());
            parentNode = parentNode->parentNode();
        }
    }

    // Construct relative path
    SyncPath relativeTraversedPath;
    for (auto nameIt = names.rbegin(); nameIt != names.rend(); ++nameIt) {
        relativeTraversedPath /= *nameIt;
    }

    // Find corresponding ancestor node id
    NodeId parentNodeId;
    if (!(_useSyncDbCache ? _syncPal->syncDb()->cache().id(otherSide(node->side()), parentDbNodeId, parentNodeId, found)
                          : _syncPal->syncDb()->id(otherSide(node->side()), parentDbNodeId, parentNodeId, found))) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::id for dbNodeId=" << parentDbNodeId);
        return nullptr;
    }

    // Find corresponding ancestor node in the other tree
    std::shared_ptr<UpdateTree> otherTree = _syncPal->updateTree(otherSide(node->side()));
    std::shared_ptr<Node> correspondingParentNode = otherTree->getNodeById(parentNodeId);
    if (correspondingParentNode == nullptr) {
        LOG_SYNCPAL_WARN(_logger, "No corresponding node in the other tree for nodeId = " << parentNodeId);
        return nullptr;
    }

    // Construct path with ancestor path / relative path
    SyncPath correspondingPath = correspondingParentNode->getPath() / relativeTraversedPath;
    std::shared_ptr<Node> correspondingNode = otherTree->getNodeByPathNormalized(correspondingPath);
    if (correspondingNode == nullptr) {
        return nullptr;
    }

    return correspondingNode;
}

std::shared_ptr<Node> OperationProcessor::correspondingNodeDirect(const std::shared_ptr<Node> node) {
    if (node->idb() == std::nullopt) {
        return nullptr;
    }

    bool found = false;
    NodeId correspondingId;
    if (!(_useSyncDbCache ? _syncPal->syncDb()->cache().id(otherSide(node->side()), *node->idb(), correspondingId, found)
                          : _syncPal->syncDb()->id(otherSide(node->side()), *node->idb(), correspondingId, found))) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::id");
        return nullptr;
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "nodeId not found");
        return nullptr;
    }

    std::shared_ptr<UpdateTree> otherTree = _syncPal->updateTree(otherSide(node->side()));
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

bool OperationProcessor::isABelowB(const std::shared_ptr<Node> a, const std::shared_ptr<Node> b) {
    std::shared_ptr<Node> tmpNode = a;
    while (tmpNode->parentNode() != nullptr) {
        if (tmpNode == b) {
            return true;
        }
        tmpNode = tmpNode->parentNode();
    }
    return false;
}

} // namespace KDC
