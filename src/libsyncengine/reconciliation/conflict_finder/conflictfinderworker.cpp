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

#include "conflictfinderworker.h"
#include "libcommonserver/utility/utility.h"

namespace KDC {

ConflictFinderWorker::ConflictFinderWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name,
                                           const std::string &shortName)
    : OperationProcessor(syncPal, name, shortName) {}

void ConflictFinderWorker::execute() {
    ExitCode exitCode(ExitCodeUnknown);

    LOG_SYNCPAL_DEBUG(_logger, "Worker started: name=" << name().c_str());
    _syncPal->_conflictQueue->startUpdate();

    findConflicts();
    exitCode = ExitCodeOk;

    setDone(exitCode);
    LOG_SYNCPAL_DEBUG(_logger, "Worker stopped: name=" << name().c_str());
    return;
}

void ConflictFinderWorker::findConflicts() {
    std::vector<std::shared_ptr<Node>> remoteMoveDirNodes;
    std::vector<std::shared_ptr<Node>> localMoveDirNodes;
    findConflictsInTree(_syncPal->updateTree(ReplicaSideLocal), _syncPal->updateTree(ReplicaSideRemote), localMoveDirNodes,
                        remoteMoveDirNodes);

    // Move-Move Cycle
    std::optional<std::vector<Conflict>> moveMoveCycleList =
        determineMoveMoveCycleConflicts(localMoveDirNodes, remoteMoveDirNodes);
    if (moveMoveCycleList) {
        for (const Conflict &c : *moveMoveCycleList) {
            _syncPal->_conflictQueue->push(c);
            LOGW_SYNCPAL_INFO(_logger, Utility::s2ws(Utility::conflictType2Str(c.type())).c_str()
                                           << L" conflict found between local node "
                                           << SyncName2WStr(c.localNode()->name()).c_str() << L" ("
                                           << Utility::s2ws(*c.localNode()->id()).c_str() << L") and remote node "
                                           << SyncName2WStr(c.remoteNode()->name()).c_str() << L" ("
                                           << Utility::s2ws(*c.remoteNode()->id()).c_str() << L")");
        }
    }
}

void ConflictFinderWorker::findConflictsInTree(std::shared_ptr<UpdateTree> localTree, std::shared_ptr<UpdateTree> remoteTree,
                                               std::vector<std::shared_ptr<Node>> &localMoveDirNodes,
                                               std::vector<std::shared_ptr<Node>> &remoteMoveDirNodes) {
    // starting node
    std::shared_ptr<Node> nodeL = localTree->rootNode();
    std::shared_ptr<Node> nodeR = remoteTree->rootNode();
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
        while (pauseAsked() || isPaused()) {
            if (!isPaused()) {
                setPauseDone();
            }
            Utility::msleep(LOOP_PAUSE_SLEEP_PERIOD);
        }

        // get next node
        node = queue.front();

        if (node->type() == NodeType::NodeTypeDirectory && node->hasChangeEvent(OperationType::OperationTypeMove)) {
            if (node->side() == ReplicaSideLocal) {
                localMoveDirNodes.push_back(node);
            } else {
                remoteMoveDirNodes.push_back(node);
            }
        }
        // Create - Create_Create conflict
        if (node->hasChangeEvent(OperationType::OperationTypeCreate) &&
            !node->hasConflictAlreadyConsidered(ConflictType::ConflictTypeCreateCreate)) {
            std::optional<Conflict> createCreateConf = checkCreateCreateConflict(node);
            if (createCreateConf) {
                _syncPal->_conflictQueue->push(*createCreateConf);
                LOGW_SYNCPAL_INFO(_logger, Utility::s2ws(Utility::conflictType2Str(createCreateConf->type())).c_str()
                                               << L" conflict found between local node "
                                               << SyncName2WStr(createCreateConf->localNode()->name()).c_str() << L" ("
                                               << Utility::s2ws(*createCreateConf->localNode()->id()).c_str()
                                               << L") and remote node "
                                               << SyncName2WStr(createCreateConf->remoteNode()->name()).c_str() << L" ("
                                               << Utility::s2ws(*createCreateConf->remoteNode()->id()).c_str() << L")");
            }
        }
        // Edit - Edit_Edit conflict
        if (node->hasChangeEvent(OperationType::OperationTypeEdit) &&
            !node->hasConflictAlreadyConsidered(ConflictType::ConflictTypeEditEdit)) {
            std::optional<Conflict> editEditConf = checkEditEditConflict(node);
            if (editEditConf) {
                _syncPal->_conflictQueue->push(*editEditConf);
                LOGW_SYNCPAL_INFO(_logger, Utility::s2ws(Utility::conflictType2Str(editEditConf->type())).c_str()
                                               << L" conflict found between local node "
                                               << SyncName2WStr(editEditConf->localNode()->name()).c_str() << L" ("
                                               << Utility::s2ws(*editEditConf->localNode()->id()).c_str() << L") and remote node "
                                               << SyncName2WStr(editEditConf->remoteNode()->name()).c_str() << L" ("
                                               << Utility::s2ws(*editEditConf->remoteNode()->id()).c_str() << L")");
            }
        }
        // Delete
        if (node->hasChangeEvent(OperationType::OperationTypeDelete)) {
            if (node->type() == NodeType::NodeTypeDirectory) {
                std::optional<std::vector<Conflict>> moveParentDeleteConf = checkMoveParentDeleteConflicts(node);
                std::optional<std::vector<Conflict>> createParentDeleteConf = checkCreateParentDeleteConflicts(node);
                if (moveParentDeleteConf) {
                    for (Conflict c : *moveParentDeleteConf) {
                        _syncPal->_conflictQueue->push(c);
                        LOGW_SYNCPAL_INFO(_logger, Utility::s2ws(Utility::conflictType2Str(c.type())).c_str()
                                                       << L" conflict found between local node "
                                                       << SyncName2WStr(c.localNode()->name()).c_str() << L" ("
                                                       << Utility::s2ws(*c.localNode()->id()).c_str() << L") and remote node "
                                                       << SyncName2WStr(c.remoteNode()->name()).c_str() << L" ("
                                                       << Utility::s2ws(*c.remoteNode()->id()).c_str() << L")");
                    }
                }
                if (createParentDeleteConf) {
                    for (Conflict c : *createParentDeleteConf) {
                        _syncPal->_conflictQueue->push(c);
                        LOGW_SYNCPAL_INFO(_logger, Utility::s2ws(Utility::conflictType2Str(c.type())).c_str()
                                                       << L" conflict found between local node "
                                                       << SyncName2WStr(c.localNode()->name()).c_str() << L" ("
                                                       << Utility::s2ws(*c.localNode()->id()).c_str() << L") and remote node "
                                                       << SyncName2WStr(c.remoteNode()->name()).c_str() << L" ("
                                                       << Utility::s2ws(*c.remoteNode()->id()).c_str() << L")");
                    }
                }
            }
            std::optional<Conflict> moveDeleteConf = checkMoveDeleteConflict(node);
            std::optional<Conflict> editDeleteConf = checkEditDeleteConflict(node);
            if (moveDeleteConf) {
                _syncPal->_conflictQueue->push(*moveDeleteConf);
                LOGW_SYNCPAL_INFO(_logger, Utility::s2ws(Utility::conflictType2Str(moveDeleteConf->type())).c_str()
                                               << L" conflict found between local node "
                                               << SyncName2WStr(moveDeleteConf->localNode()->name()).c_str() << L" ("
                                               << Utility::s2ws(*moveDeleteConf->localNode()->id()).c_str()
                                               << L") and remote node "
                                               << SyncName2WStr(moveDeleteConf->remoteNode()->name()).c_str() << L" ("
                                               << Utility::s2ws(*moveDeleteConf->remoteNode()->id()).c_str() << L")");
            }
            if (editDeleteConf) {
                _syncPal->_conflictQueue->push(*editDeleteConf);
                LOGW_SYNCPAL_INFO(_logger, Utility::s2ws(Utility::conflictType2Str(editDeleteConf->type())).c_str()
                                               << L" conflict found between local node "
                                               << SyncName2WStr(editDeleteConf->localNode()->name()).c_str() << L" ("
                                               << Utility::s2ws(*editDeleteConf->localNode()->id()).c_str()
                                               << L") and remote node "
                                               << SyncName2WStr(editDeleteConf->remoteNode()->name()).c_str() << L" ("
                                               << Utility::s2ws(*editDeleteConf->remoteNode()->id()).c_str() << L")");
            }
        }

        // Move
        if (node->hasChangeEvent(OperationType::OperationTypeMove)) {
            std::optional<Conflict> moveCreateConf = checkMoveCreateConflict(node);
            if (moveCreateConf) {
                _syncPal->_conflictQueue->push(*moveCreateConf);
                LOGW_SYNCPAL_INFO(_logger, Utility::s2ws(Utility::conflictType2Str(moveCreateConf->type())).c_str()
                                               << L" conflict found between local node "
                                               << SyncName2WStr(moveCreateConf->localNode()->name()).c_str() << L" ("
                                               << Utility::s2ws(*moveCreateConf->localNode()->id()).c_str()
                                               << L") and remote node "
                                               << SyncName2WStr(moveCreateConf->remoteNode()->name()).c_str() << L" ("
                                               << Utility::s2ws(*moveCreateConf->remoteNode()->id()).c_str() << L")");
            }
            if (!node->hasConflictAlreadyConsidered(ConflictType::ConflictTypeMoveMoveDest)) {
                std::optional<Conflict> moveMoveDestConf = checkMoveMoveDestConflict(node);
                if (moveMoveDestConf) {
                    _syncPal->_conflictQueue->push(*moveMoveDestConf);
                    LOGW_SYNCPAL_INFO(_logger, Utility::s2ws(Utility::conflictType2Str(moveMoveDestConf->type())).c_str()
                                                   << L" conflict found between local node "
                                                   << SyncName2WStr(moveMoveDestConf->localNode()->name()).c_str() << L" ("
                                                   << Utility::s2ws(*moveMoveDestConf->localNode()->id()).c_str()
                                                   << L") and remote node "
                                                   << SyncName2WStr(moveMoveDestConf->remoteNode()->name()).c_str() << L" ("
                                                   << Utility::s2ws(*moveMoveDestConf->remoteNode()->id()).c_str() << L")");
                }
            }
            if (!node->hasConflictAlreadyConsidered(ConflictType::ConflictTypeMoveMoveSource)) {
                std::optional<Conflict> moveMoveSrcConf = checkMoveMoveSourceConflict(node);
                if (moveMoveSrcConf) {
                    _syncPal->_conflictQueue->push(*moveMoveSrcConf);
                    LOGW_SYNCPAL_INFO(_logger, Utility::s2ws(Utility::conflictType2Str(moveMoveSrcConf->type())).c_str()
                                                   << L" conflict found between local node "
                                                   << SyncName2WStr(moveMoveSrcConf->localNode()->name()).c_str() << L" ("
                                                   << Utility::s2ws(*moveMoveSrcConf->localNode()->id()).c_str()
                                                   << L") and remote node "
                                                   << SyncName2WStr(moveMoveSrcConf->remoteNode()->name()).c_str() << L" ("
                                                   << Utility::s2ws(*moveMoveSrcConf->remoteNode()->id()).c_str() << L")");
                }
            }
        }

        // remove it from queue
        queue.pop();

        // visit children
        for (auto &childElt : node->children()) {
            auto child = childElt.second;
            if (!visited[child]) {
                visited[child] = true;
                queue.push(child);
            }
        }
    }
}

std::optional<Conflict> ConflictFinderWorker::checkCreateCreateConflict(std::shared_ptr<Node> createNode) {
    std::shared_ptr<Node> correspondingParentNode;
    if (_syncPal->_syncHasFullyCompleted) {
        correspondingParentNode = correspondingNodeDirect(createNode->parentNode());
    } else {
        correspondingParentNode = correspondingNodeInOtherTree(createNode->parentNode());
    }
    if (correspondingParentNode == nullptr) {
        return std::nullopt;
    }
    std::optional<Conflict> conflict = std::nullopt;
    std::shared_ptr<Node> correspondingCreateNode =
        correspondingParentNode->getChildExcept(createNode->name(), OperationTypeDelete);

    if (correspondingCreateNode != nullptr && correspondingCreateNode->hasChangeEvent(OperationTypeCreate)) {
        if (!isPseudoConflict(createNode, correspondingCreateNode)) {
            conflict = Conflict(createNode, correspondingCreateNode, ConflictType::ConflictTypeCreateCreate);
            correspondingCreateNode->insertConflictAlreadyConsidered(ConflictType::ConflictTypeCreateCreate);
        }
    }
    return conflict;
}

std::optional<Conflict> ConflictFinderWorker::checkEditEditConflict(std::shared_ptr<Node> editNode) {
    std::optional<Conflict> conflict = std::nullopt;
    std::shared_ptr<Node> correspondingNode = correspondingNodeDirect(editNode);
    if (correspondingNode != nullptr && correspondingNode->hasChangeEvent(OperationType::OperationTypeEdit)) {
        if (!isPseudoConflict(editNode, correspondingNode)) {
            conflict = Conflict(editNode, correspondingNode, ConflictType::ConflictTypeEditEdit);
            correspondingNode->insertConflictAlreadyConsidered(ConflictType::ConflictTypeEditEdit);
        }
    }
    return conflict;
}

std::optional<Conflict> ConflictFinderWorker::checkMoveCreateConflict(std::shared_ptr<Node> moveNode) {
    std::optional<Conflict> conflict = std::nullopt;
    std::shared_ptr<Node> moveParentNode = moveNode->parentNode();
    std::shared_ptr<Node> correspondingParentNode = correspondingNodeDirect(moveParentNode);
    if (correspondingParentNode != nullptr) {
        std::shared_ptr<Node> potentialCreateChildNode =
            correspondingParentNode->getChildExcept(moveNode->name(), OperationTypeDelete);
        if (potentialCreateChildNode != nullptr && potentialCreateChildNode->hasChangeEvent(OperationTypeCreate)) {
            conflict = Conflict(moveNode, potentialCreateChildNode, ConflictType::ConflictTypeMoveCreate);
        }
    }
    return conflict;
}

std::optional<Conflict> ConflictFinderWorker::checkEditDeleteConflict(std::shared_ptr<Node> deleteNode) {
    if (deleteNode->type() == NodeType::NodeTypeDirectory) {
        return std::nullopt;
    }

    std::optional<Conflict> conflict = std::nullopt;
    std::shared_ptr<Node> correspondingEditNode = correspondingNodeDirect(deleteNode);
    if (correspondingEditNode != nullptr && correspondingEditNode->hasChangeEvent(OperationTypeEdit)) {
        conflict = Conflict(deleteNode, correspondingEditNode, ConflictType::ConflictTypeEditDelete);
    }
    return conflict;
}

std::optional<Conflict> ConflictFinderWorker::checkMoveDeleteConflict(std::shared_ptr<Node> deleteNode) {
    std::shared_ptr<Node> correspondingMoveNode = correspondingNodeDirect(deleteNode);
    if (correspondingMoveNode == nullptr || !correspondingMoveNode->hasChangeEvent(OperationType::OperationTypeMove)) {
        return std::nullopt;
    }

    return Conflict(deleteNode, correspondingMoveNode, ConflictType::ConflictTypeMoveDelete);
}

std::optional<std::vector<Conflict>> ConflictFinderWorker::checkMoveParentDeleteConflicts(std::shared_ptr<Node> deleteNode) {
    std::shared_ptr<Node> correspondingDirNode = correspondingNodeDirect(deleteNode);

    std::optional<std::vector<Conflict>> moveNodes = std::vector<Conflict>();
    if (correspondingDirNode != nullptr) {
        if (correspondingDirNode->hasChangeEvent(OperationTypeDelete)) {
            return std::nullopt;
        }

        std::optional<std::vector<std::shared_ptr<Node>>> subMoveNodes =
            findChangeEventInSubNodes(OperationTypeMove, correspondingDirNode);
        if (subMoveNodes) {
            for (std::shared_ptr<Node> node : *subMoveNodes) {
                moveNodes->push_back(Conflict(deleteNode, node, ConflictType::ConflictTypeMoveParentDelete));
            }
        }
    }
    return (moveNodes->empty() ? std::nullopt : moveNodes);
}

std::optional<std::vector<Conflict>> ConflictFinderWorker::checkCreateParentDeleteConflicts(std::shared_ptr<Node> deleteNode) {
    std::shared_ptr<Node> correspondingDirNode = correspondingNodeDirect(deleteNode);

    std::optional<std::vector<Conflict>> createNodes = std::vector<Conflict>();
    if (correspondingDirNode != nullptr) {
        if (correspondingDirNode->hasChangeEvent(OperationTypeDelete)) {
            return std::nullopt;
        }

        std::optional<std::vector<std::shared_ptr<Node>>> subMoveNodes =
            findChangeEventInSubNodes(OperationTypeCreate, correspondingDirNode);
        if (subMoveNodes) {
            for (std::shared_ptr<Node> node : *subMoveNodes) {
                createNodes->push_back(Conflict(deleteNode, node, ConflictType::ConflictTypeCreateParentDelete));
            }
        }
    }
    return (createNodes->empty() ? std::nullopt : createNodes);
}

std::optional<Conflict> ConflictFinderWorker::checkMoveMoveSourceConflict(std::shared_ptr<Node> moveNode) {
    std::shared_ptr<Node> correspondingMoveNode = correspondingNodeDirect(moveNode);
    if (correspondingMoveNode != nullptr) {
        if (!correspondingMoveNode->hasChangeEvent(OperationTypeMove)) {
            return std::nullopt;
        }

        if (!isPseudoConflict(moveNode, correspondingMoveNode)) {
            correspondingMoveNode->insertConflictAlreadyConsidered(ConflictType::ConflictTypeMoveMoveSource);
            if (moveNode->name() != correspondingMoveNode->name() ||
                moveNode->parentNode() != correspondingMoveNode->parentNode()) {
                return Conflict(moveNode, correspondingMoveNode, ConflictType::ConflictTypeMoveMoveSource);
            }
        }
    }
    return std::nullopt;
}

std::optional<Conflict> ConflictFinderWorker::checkMoveMoveDestConflict(std::shared_ptr<Node> moveNode) {
    std::shared_ptr<Node> nodeParentInOtherTree = correspondingNodeDirect(moveNode->parentNode());
    std::optional<Conflict> conflict = std::nullopt;
    if (nodeParentInOtherTree != nullptr) {
        std::shared_ptr<Node> potentialMoveChild = nodeParentInOtherTree->getChildExcept(moveNode->name(), OperationTypeDelete);
        if (potentialMoveChild != nullptr && potentialMoveChild->hasChangeEvent(OperationTypeMove) &&
            potentialMoveChild->idb() != moveNode->idb()) {
            conflict = Conflict(moveNode, potentialMoveChild, ConflictType::ConflictTypeMoveMoveDest);
            potentialMoveChild->insertConflictAlreadyConsidered(ConflictType::ConflictTypeMoveMoveDest);
        }
    }
    return conflict;
}

std::optional<std::vector<Conflict>> ConflictFinderWorker::determineMoveMoveCycleConflicts(
    std::vector<std::shared_ptr<Node>> localMoveDirNodes, std::vector<std::shared_ptr<Node>> remoteMoveDirNodes) {
    std::optional<std::vector<Conflict>> conflicts = std::vector<Conflict>();

    for (std::shared_ptr<Node> localNode : localMoveDirNodes) {
        for (std::shared_ptr<Node> remoteNode : remoteMoveDirNodes) {
            if (*localNode->idb() == *remoteNode->idb()) {
                continue;
            }
            // get databases relative paths
            bool found;
            SyncPath localDbPath;
            if (!_syncPal->_syncDb->path(localNode->side(), *localNode->id(), localDbPath, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::path");
                return std::nullopt;
            }
            if (!found) {
                LOG_SYNCPAL_WARN(_logger,
                                 "Node not found for id = " << localNode->id()->c_str() << " side = " << localNode->side());
                // break loop because localNode's path is not found
                break;
            }
            SyncPath remoteDbPath;
            if (!_syncPal->_syncDb->path(remoteNode->side(), *remoteNode->id(), remoteDbPath, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::path");
                return std::nullopt;
            }
            if (!found) {
                LOG_SYNCPAL_WARN(_logger,
                                 "Node not found for id = " << remoteNode->id()->c_str() << " side = " << remoteNode->side());
                // continue loop because remoteNode's path is not found
                continue;
            }

            if (Utility::startsWith(localDbPath.native(), remoteDbPath.native() + Str("/")) ||
                Utility::startsWith(remoteDbPath.native(), localDbPath.native() + Str("/"))) {
                continue;
            }

            std::shared_ptr<Node> correspondingLocalNode = correspondingNodeDirect(remoteNode);
            std::shared_ptr<Node> correspondingRemoteNode = correspondingNodeDirect(localNode);

            if (correspondingLocalNode != nullptr && correspondingRemoteNode != nullptr) {
                if (isABelowB(localNode, correspondingLocalNode) && isABelowB(remoteNode, correspondingRemoteNode)) {
                    conflicts->push_back(Conflict(localNode, remoteNode, ConflictType::ConflictTypeMoveMoveCycle));
                }
            }
        }
    }
    return (conflicts->empty() ? std::nullopt : conflicts);
}

std::optional<std::vector<std::shared_ptr<Node>>> ConflictFinderWorker::findChangeEventInSubNodes(
    OperationType event, std::shared_ptr<Node> parentNode) {
    std::optional<std::vector<std::shared_ptr<Node>>> nodes = std::vector<std::shared_ptr<Node>>();
    std::unordered_map<std::shared_ptr<Node>, bool> visited;
    std::list<std::shared_ptr<Node>> queue;

    visited[parentNode] = true;
    queue.push_back(parentNode);
    std::shared_ptr<Node> node;

    while (!queue.empty()) {
        // get next node
        node = queue.front();

        if (node != parentNode && node->hasChangeEvent(event)) {
            nodes->push_back(node);
        }
        // remove it from queue
        queue.pop_front();

        // visit children
        for (auto child : node->children()) {
            if (!visited[child.second]) {
                visited[child.second] = true;
                queue.push_back(child.second);
            }
        }
    }
    return (nodes->empty() ? std::nullopt : nodes);
}

}  // namespace KDC
