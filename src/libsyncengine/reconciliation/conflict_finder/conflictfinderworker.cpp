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

#include "conflictfinderworker.h"
#include "libcommonserver/utility/utility.h"

namespace KDC {

ConflictFinderWorker::ConflictFinderWorker(const std::shared_ptr<SyncPal> syncPal, const std::string &name,
                                           const std::string &shortName) :
    OperationProcessor(syncPal, name, shortName) {}

void ConflictFinderWorker::execute() {
    auto exitCode(ExitCode::Unknown);

    LOG_SYNCPAL_DEBUG(_logger, "Worker started: name=" << name());
    _syncPal->_conflictQueue->startUpdate();

    findConflicts();
    exitCode = ExitCode::Ok;

    LOG_SYNCPAL_DEBUG(_logger, "Worker stopped: name=" << name());
    setDone(exitCode);
}

void ConflictFinderWorker::findConflicts() {
    std::vector<std::shared_ptr<Node>> remoteMoveDirNodes;
    std::vector<std::shared_ptr<Node>> localMoveDirNodes;
    findConflictsInTree(_syncPal->updateTree(ReplicaSide::Local), _syncPal->updateTree(ReplicaSide::Remote), localMoveDirNodes,
                        remoteMoveDirNodes);

    // Move-Move Cycle
    const std::optional<std::vector<Conflict>> moveMoveCycleList =
            determineMoveMoveCycleConflicts(localMoveDirNodes, remoteMoveDirNodes);
    if (moveMoveCycleList) {
        for (const Conflict &c: *moveMoveCycleList) {
            _syncPal->_conflictQueue->push(c);
            LOGW_SYNCPAL_INFO(_logger, c.type() << L" conflict found between local node "
                                                << Utility::formatSyncName(c.localNode()->name()) << L" ("
                                                << CommonUtility::s2ws(*c.localNode()->id()) << L") and remote node "
                                                << Utility::formatSyncName(c.remoteNode()->name()) << L" ("
                                                << CommonUtility::s2ws(*c.remoteNode()->id()) << L")");
        }
    }
}

void ConflictFinderWorker::findConflictsInTree(const std::shared_ptr<UpdateTree> localTree,
                                               const std::shared_ptr<UpdateTree> remoteTree,
                                               std::vector<std::shared_ptr<Node>> &localMoveDirNodes,
                                               std::vector<std::shared_ptr<Node>> &remoteMoveDirNodes) {
    // starting node
    const auto nodeL = localTree->rootNode();
    const auto nodeR = remoteTree->rootNode();
    // visited map and queue to make BFS
    std::unordered_map<std::shared_ptr<Node>, bool> visited;
    std::queue<std::shared_ptr<Node>> queue;

    visited[nodeL] = true;
    visited[nodeR] = true;
    queue.push(nodeL);
    queue.push(nodeR);

    std::shared_ptr<Node> node;
    while (!queue.empty()) {
        if (stopAsked()) {
            return;
        }

        // get next node
        node = queue.front();

        if (node->type() == NodeType::Directory && node->hasChangeEvent(OperationType::Move)) {
            if (node->side() == ReplicaSide::Local) {
                localMoveDirNodes.push_back(node);
            } else {
                remoteMoveDirNodes.push_back(node);
            }
        }
        // Create - Create_Create conflict
        if (node->hasChangeEvent(OperationType::Create) && !node->hasConflictAlreadyConsidered(ConflictType::CreateCreate)) {
            if (std::optional<Conflict> createCreateConf = checkCreateCreateConflict(node)) {
                _syncPal->_conflictQueue->push(*createCreateConf);
                LOGW_SYNCPAL_INFO(_logger, createCreateConf->type()
                                                   << L" conflict found between local node "
                                                   << Utility::formatSyncName(createCreateConf->localNode()->name()) << L" ("
                                                   << CommonUtility::s2ws(*createCreateConf->localNode()->id()) << L") and remote node "
                                                   << Utility::formatSyncName(createCreateConf->remoteNode()->name()) << L" ("
                                                   << CommonUtility::s2ws(*createCreateConf->remoteNode()->id()) << L")");
            }
        }
        // Edit - Edit_Edit conflict
        if (node->hasChangeEvent(OperationType::Edit) && !node->hasConflictAlreadyConsidered(ConflictType::EditEdit)) {
            if (std::optional<Conflict> editEditConf = checkEditEditConflict(node)) {
                _syncPal->_conflictQueue->push(*editEditConf);
                LOGW_SYNCPAL_INFO(_logger, editEditConf->type()
                                                   << L" conflict found between local node "
                                                   << Utility::formatSyncName(editEditConf->localNode()->name()) << L" ("
                                                   << CommonUtility::s2ws(*editEditConf->localNode()->id()) << L") and remote node "
                                                   << Utility::formatSyncName(editEditConf->remoteNode()->name()) << L" ("
                                                   << CommonUtility::s2ws(*editEditConf->remoteNode()->id()) << L")");
            }
        }
        // Delete
        if (node->hasChangeEvent(OperationType::Delete)) {
            if (node->type() == NodeType::Directory) {
                std::optional<std::vector<Conflict>> moveParentDeleteConf = checkMoveParentDeleteConflicts(node);
                std::optional<std::vector<Conflict>> createParentDeleteConf = checkCreateParentDeleteConflicts(node);
                if (moveParentDeleteConf) {
                    for (const auto &conflict: *moveParentDeleteConf) {
                        _syncPal->_conflictQueue->push(conflict);
                        LOGW_SYNCPAL_INFO(_logger, conflict.type()
                                                           << L" conflict found between local node "
                                                           << Utility::formatSyncName(conflict.localNode()->name()) << L" ("
                                                           << CommonUtility::s2ws(*conflict.localNode()->id()) << L") and remote node "
                                                           << Utility::formatSyncName(conflict.remoteNode()->name()) << L" ("
                                                           << CommonUtility::s2ws(*conflict.remoteNode()->id()) << L")");
                    }
                }
                if (createParentDeleteConf) {
                    for (const auto &conflict: *createParentDeleteConf) {
                        _syncPal->_conflictQueue->push(conflict);
                        LOGW_SYNCPAL_INFO(_logger, conflict.type()
                                                           << L" conflict found between local node "
                                                           << Utility::formatSyncName(conflict.localNode()->name()) << L" ("
                                                           << CommonUtility::s2ws(*conflict.localNode()->id()) << L") and remote node "
                                                           << Utility::formatSyncName(conflict.remoteNode()->name()) << L" ("
                                                           << CommonUtility::s2ws(*conflict.remoteNode()->id()) << L")");
                    }
                }
            }
            std::optional<Conflict> moveDeleteConf = checkMoveDeleteConflict(node);
            std::optional<Conflict> editDeleteConf = checkEditDeleteConflict(node);
            if (moveDeleteConf) {
                _syncPal->_conflictQueue->push(*moveDeleteConf);
                LOGW_SYNCPAL_INFO(_logger, moveDeleteConf->type()
                                                   << L" conflict found between local node "
                                                   << Utility::formatSyncName(moveDeleteConf->localNode()->name()) << L" ("
                                                   << CommonUtility::s2ws(*moveDeleteConf->localNode()->id()) << L") and remote node "
                                                   << Utility::formatSyncName(moveDeleteConf->remoteNode()->name()) << L" ("
                                                   << CommonUtility::s2ws(*moveDeleteConf->remoteNode()->id()) << L")");
            }
            if (editDeleteConf) {
                _syncPal->_conflictQueue->push(*editDeleteConf);
                LOGW_SYNCPAL_INFO(_logger, editDeleteConf->type()
                                                   << L" conflict found between local node "
                                                   << Utility::formatSyncName(editDeleteConf->localNode()->name()) << L" ("
                                                   << CommonUtility::s2ws(*editDeleteConf->localNode()->id()) << L") and remote node "
                                                   << Utility::formatSyncName(editDeleteConf->remoteNode()->name()) << L" ("
                                                   << CommonUtility::s2ws(*editDeleteConf->remoteNode()->id()) << L")");
            }
        }

        // Move
        if (node->hasChangeEvent(OperationType::Move)) {
            if (std::optional<Conflict> moveCreateConf = checkMoveCreateConflict(node)) {
                _syncPal->_conflictQueue->push(*moveCreateConf);
                LOGW_SYNCPAL_INFO(_logger, moveCreateConf->type()
                                                   << L" conflict found between local node "
                                                   << Utility::formatSyncName(moveCreateConf->localNode()->name()) << L" ("
                                                   << CommonUtility::s2ws(*moveCreateConf->localNode()->id()) << L") and remote node "
                                                   << Utility::formatSyncName(moveCreateConf->remoteNode()->name()) << L" ("
                                                   << CommonUtility::s2ws(*moveCreateConf->remoteNode()->id()) << L")");
            }
            if (!node->hasConflictAlreadyConsidered(ConflictType::MoveMoveDest)) {
                if (std::optional<Conflict> moveMoveDestConf = checkMoveMoveDestConflict(node)) {
                    _syncPal->_conflictQueue->push(*moveMoveDestConf);
                    LOGW_SYNCPAL_INFO(_logger, moveMoveDestConf->type()
                                                       << L" conflict found between local node "
                                                       << Utility::formatSyncName(moveMoveDestConf->localNode()->name()) << L" ("
                                                       << CommonUtility::s2ws(*moveMoveDestConf->localNode()->id())
                                                       << L") and remote node "
                                                       << Utility::formatSyncName(moveMoveDestConf->remoteNode()->name()) << L" ("
                                                       << CommonUtility::s2ws(*moveMoveDestConf->remoteNode()->id()) << L")");
                }
            }
            if (!node->hasConflictAlreadyConsidered(ConflictType::MoveMoveSource)) {
                if (std::optional<Conflict> moveMoveSrcConf = checkMoveMoveSourceConflict(node)) {
                    _syncPal->_conflictQueue->push(*moveMoveSrcConf);
                    LOGW_SYNCPAL_INFO(_logger, moveMoveSrcConf->type()
                                                       << L" conflict found between local node "
                                                       << Utility::formatSyncName(moveMoveSrcConf->localNode()->name()) << L" ("
                                                       << CommonUtility::s2ws(*moveMoveSrcConf->localNode()->id())
                                                       << L") and remote node "
                                                       << Utility::formatSyncName(moveMoveSrcConf->remoteNode()->name()) << L" ("
                                                       << CommonUtility::s2ws(*moveMoveSrcConf->remoteNode()->id()) << L")");
                }
            }
        }

        // remove it from queue
        queue.pop();

        // visit children
        for (auto &[_, child]: node->children()) {
            if (!visited[child]) {
                visited[child] = true;
                queue.push(child);
            }
        }
    }
}

std::optional<Conflict> ConflictFinderWorker::checkCreateCreateConflict(const std::shared_ptr<Node> createNode) {
    std::shared_ptr<Node> correspondingParentNode;
    if (_syncPal->syncHasFullyCompleted()) {
        correspondingParentNode = correspondingNodeDirect(createNode->parentNode());
    } else {
        correspondingParentNode = correspondingNodeInOtherTree(createNode->parentNode());
    }
    if (correspondingParentNode == nullptr) {
        return std::nullopt;
    }
    std::optional<Conflict> conflict = std::nullopt;
    SyncName normalizedName;
    if (!Utility::normalizedSyncName(createNode->name(), normalizedName)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Failed to normalize: " << Utility::formatSyncName(createNode->name()));
        return std::nullopt;
    }

    if (const auto correspondingCreateNode = correspondingParentNode->getChildExcept(normalizedName, OperationType::Delete);
        correspondingCreateNode && correspondingCreateNode->hasChangeEvent(OperationType::Create)) {
        if (!isPseudoConflict(createNode, correspondingCreateNode)) {
            conflict = Conflict(createNode, correspondingCreateNode, ConflictType::CreateCreate);
            correspondingCreateNode->insertConflictAlreadyConsidered(ConflictType::CreateCreate);
        }
    }
    return conflict;
}

std::optional<Conflict> ConflictFinderWorker::checkEditEditConflict(const std::shared_ptr<Node> editNode) {
    std::optional<Conflict> conflict = std::nullopt;
    if (const auto correspondingNode = correspondingNodeDirect(editNode);
        correspondingNode != nullptr && correspondingNode->hasChangeEvent(OperationType::Edit)) {
        if (!isPseudoConflict(editNode, correspondingNode)) {
            conflict = Conflict(editNode, correspondingNode, ConflictType::EditEdit);
            correspondingNode->insertConflictAlreadyConsidered(ConflictType::EditEdit);
        }
    }
    return conflict;
}

std::optional<Conflict> ConflictFinderWorker::checkMoveCreateConflict(const std::shared_ptr<Node> moveNode) {
    std::optional<Conflict> conflict = std::nullopt;
    const auto moveParentNode = moveNode->parentNode();
    if (const auto correspondingParentNode = correspondingNodeDirect(moveParentNode); correspondingParentNode) {
        SyncName normalizedName;
        if (!Utility::normalizedSyncName(moveNode->name(), normalizedName)) {
            LOGW_WARN(Log::instance()->getLogger(), L"Failed to normalize: " << Utility::formatSyncName(moveNode->name()));
            return std::nullopt;
        }
        if (const auto potentialCreateChildNode = correspondingParentNode->getChildExcept(normalizedName, OperationType::Delete);
            potentialCreateChildNode && potentialCreateChildNode->hasChangeEvent(OperationType::Create)) {
            conflict = Conflict(moveNode, potentialCreateChildNode, ConflictType::MoveCreate);
        }
    }
    return conflict;
}

std::optional<Conflict> ConflictFinderWorker::checkEditDeleteConflict(const std::shared_ptr<Node> deleteNode) {
    if (deleteNode->type() == NodeType::Directory) {
        return std::nullopt;
    }

    std::optional<Conflict> conflict = std::nullopt;
    if (const auto correspondingEditNode = correspondingNodeDirect(deleteNode);
        correspondingEditNode != nullptr && correspondingEditNode->hasChangeEvent(OperationType::Edit)) {
        conflict = Conflict(deleteNode, correspondingEditNode, ConflictType::EditDelete);
    }
    return conflict;
}

std::optional<Conflict> ConflictFinderWorker::checkMoveDeleteConflict(const std::shared_ptr<Node> deleteNode) {
    const auto correspondingMoveNode = correspondingNodeDirect(deleteNode);
    if (correspondingMoveNode == nullptr || !correspondingMoveNode->hasChangeEvent(OperationType::Move)) {
        return std::nullopt;
    }

    return Conflict(deleteNode, correspondingMoveNode, ConflictType::MoveDelete);
}

std::optional<std::vector<Conflict>> ConflictFinderWorker::checkMoveParentDeleteConflicts(
        const std::shared_ptr<Node> deleteNode) {
    const auto correspondingDirNode = correspondingNodeDirect(deleteNode);
    std::optional<std::vector<Conflict>> moveNodes = std::vector<Conflict>();
    if (correspondingDirNode != nullptr) {
        if (correspondingDirNode->hasChangeEvent(OperationType::Delete)) {
            return std::nullopt;
        }

        const std::optional<std::vector<std::shared_ptr<Node>>> subMoveNodes =
                findChangeEventInSubNodes(OperationType::Move, correspondingDirNode);
        if (subMoveNodes) {
            for (const auto &node: *subMoveNodes) {
                moveNodes->push_back(Conflict(deleteNode, node, ConflictType::MoveParentDelete));
            }
        }
    }
    return (moveNodes->empty() ? std::nullopt : moveNodes);
}

std::optional<std::vector<Conflict>> ConflictFinderWorker::checkCreateParentDeleteConflicts(
        const std::shared_ptr<Node> deleteNode) {
    const auto correspondingDirNode = correspondingNodeDirect(deleteNode);

    std::optional createNodes = std::vector<Conflict>();
    if (correspondingDirNode != nullptr) {
        if (correspondingDirNode->hasChangeEvent(OperationType::Delete)) {
            return std::nullopt;
        }

        std::optional<std::vector<std::shared_ptr<Node>>> subMoveNodes =
                findChangeEventInSubNodes(OperationType::Create, correspondingDirNode);
        if (subMoveNodes) {
            for (const auto &node: *subMoveNodes) {
                createNodes->push_back(Conflict(deleteNode, node, ConflictType::CreateParentDelete));
            }
        }
    }
    return (createNodes->empty() ? std::nullopt : createNodes);
}

std::optional<Conflict> ConflictFinderWorker::checkMoveMoveSourceConflict(const std::shared_ptr<Node> moveNode) {
    if (const auto correspondingMoveNode = correspondingNodeDirect(moveNode); correspondingMoveNode != nullptr) {
        if (!correspondingMoveNode->hasChangeEvent(OperationType::Move)) {
            return std::nullopt;
        }

        if (!isPseudoConflict(moveNode, correspondingMoveNode)) {
            correspondingMoveNode->insertConflictAlreadyConsidered(ConflictType::MoveMoveSource);
            if (moveNode->name() != correspondingMoveNode->name() ||
                moveNode->parentNode() != correspondingMoveNode->parentNode()) {
                return Conflict(moveNode, correspondingMoveNode, ConflictType::MoveMoveSource);
            }
        }
    }
    return std::nullopt;
}

std::optional<Conflict> ConflictFinderWorker::checkMoveMoveDestConflict(const std::shared_ptr<Node> moveNode) {
    const auto nodeParentInOtherTree = correspondingNodeDirect(moveNode->parentNode());
    std::optional<Conflict> conflict = std::nullopt;
    if (nodeParentInOtherTree) {
        SyncName normalizedName;
        if (!Utility::normalizedSyncName(moveNode->name(), normalizedName)) {
            LOGW_WARN(Log::instance()->getLogger(), L"Failed to normalize: " << Utility::formatSyncName(moveNode->name()));
            return std::nullopt;
        }
        if (const auto potentialMoveChild = nodeParentInOtherTree->getChildExcept(normalizedName, OperationType::Delete);
            potentialMoveChild && potentialMoveChild->hasChangeEvent(OperationType::Move) &&
            potentialMoveChild->idb() != moveNode->idb()) {
            conflict = Conflict(moveNode, potentialMoveChild, ConflictType::MoveMoveDest);
            potentialMoveChild->insertConflictAlreadyConsidered(ConflictType::MoveMoveDest);
        }
    }
    return conflict;
}

std::optional<std::vector<Conflict>> ConflictFinderWorker::determineMoveMoveCycleConflicts(
        const std::vector<std::shared_ptr<Node>> &localMoveDirNodes,
        const std::vector<std::shared_ptr<Node>> &remoteMoveDirNodes) {
    std::optional<std::vector<Conflict>> conflicts = std::vector<Conflict>();

    for (const auto &localNode: localMoveDirNodes) {
        for (const auto &remoteNode: remoteMoveDirNodes) {
            if (*localNode->idb() == *remoteNode->idb()) {
                continue;
            }
            // get databases relative paths
            bool found = false;
            SyncPath localDbPath;
            if (!_syncPal->syncDb()->cache().path(localNode->side(), *localNode->id(), localDbPath, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::path");
                return std::nullopt;
            }
            if (!found) {
                LOG_SYNCPAL_WARN(_logger,
                                 "Node not found for id='" << localNode->id()->c_str() << "' side='" << localNode->side() << "'");
                // break loop because localNode's path is not found
                break;
            }
            SyncPath remoteDbPath;
            if (!_syncPal->syncDb()->cache().path(remoteNode->side(), *remoteNode->id(), remoteDbPath, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::path");
                return std::nullopt;
            }
            if (!found) {
                LOG_SYNCPAL_WARN(_logger, "Node not found for id='" << remoteNode->id()->c_str() << "' side='"
                                                                    << remoteNode->side() << "'");
                // continue loop because remoteNode's path is not found
                continue;
            }

            if (CommonUtility::isDescendantOrEqual(SyncPath(localDbPath).lexically_normal(),
                                                   SyncPath(remoteDbPath.native() + Str("/")).lexically_normal()) ||
                CommonUtility::isDescendantOrEqual(SyncPath(remoteDbPath).lexically_normal(),
                                                   SyncPath(localDbPath.native() + Str("/")).lexically_normal())) {
                continue;
            }

            const auto correspondingLocalNode = correspondingNodeDirect(remoteNode);
            const auto correspondingRemoteNode = correspondingNodeDirect(localNode);
            if (correspondingLocalNode != nullptr && correspondingRemoteNode != nullptr) {
                if (isABelowB(localNode, correspondingLocalNode) && isABelowB(remoteNode, correspondingRemoteNode)) {
                    conflicts->push_back(Conflict(localNode, remoteNode, ConflictType::MoveMoveCycle));
                }
            }
        }
    }
    return (conflicts->empty() ? std::nullopt : conflicts);
}

std::optional<std::vector<std::shared_ptr<Node>>> ConflictFinderWorker::findChangeEventInSubNodes(
        const OperationType event, const std::shared_ptr<Node> parentNode) {
    std::optional<std::vector<std::shared_ptr<Node>>> nodes = std::vector<std::shared_ptr<Node>>();
    std::unordered_map<std::shared_ptr<Node>, bool> visited;
    std::list<std::shared_ptr<Node>> queue;

    visited[parentNode] = true;
    queue.push_back(parentNode);
    while (!queue.empty()) {
        // get next node
        const auto node = queue.front();

        if (node != parentNode && node->hasChangeEvent(event)) {
            nodes->push_back(node);
        }
        // remove it from queue
        queue.pop_front();

        // visit children
        for (const auto &child: node->children()) {
            if (!visited[child.second]) {
                visited[child.second] = true;
                queue.push_back(child.second);
            }
        }
    }
    return (nodes->empty() ? std::nullopt : nodes);
}

} // namespace KDC
