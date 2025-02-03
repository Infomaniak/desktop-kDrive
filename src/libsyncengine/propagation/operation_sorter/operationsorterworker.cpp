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

#include "operationsorterworker.h"

#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/utility.h"
#include "requests/parameterscache.h"

namespace KDC {

OperationSorterWorker::OperationSorterWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name,
                                             const std::string &shortName) :
    OperationProcessor(syncPal, name, shortName), _hasOrderChanged(false) {}

void OperationSorterWorker::execute() {
    auto exitCode(ExitCode::Unknown);

    LOG_SYNCPAL_DEBUG(_logger, "Worker started: name=" << name().c_str());

    auto start = std::chrono::steady_clock::now();

    _syncPal->_syncOps->startUpdate();
    exitCode = sortOperations();

    std::chrono::duration<double> elapsed_seconds = std::chrono::steady_clock::now() - start;
    LOG_SYNCPAL_INFO(_logger, "Operation sorting finished in: " << elapsed_seconds.count() << "s");

    LOG_SYNCPAL_DEBUG(_logger, "Worker stopped: name=" << name().c_str());
    setDone(exitCode);
}

ExitCode OperationSorterWorker::sortOperations() {
    _syncPal->_syncOps->startUpdate();
    // Keep a copy of the unsorted list
    _unsortedList = *_syncPal->_syncOps;
    std::list<SyncOperationList> completeCycles;
    while (true) {
        if (stopAsked()) {
            return ExitCode::Ok;
        }
        while (pauseAsked() || isPaused()) {
            if (!isPaused()) {
                setPauseDone();
            }
            Utility::msleep(LOOP_PAUSE_SLEEP_PERIOD);
        }

        _hasOrderChanged = false;
        fixDeleteBeforeMove();
        fixMoveBeforeCreate();
        fixMoveBeforeDelete();
        fixCreateBeforeMove();
        fixDeleteBeforeCreate();
        fixMoveBeforeMoveOccupied();
        fixCreateBeforeCreate();
        fixEditBeforeMove();
        fixMoveBeforeMoveHierarchyFlip();
        if (!_hasOrderChanged) {
            break;
        }

        completeCycles = findCompleteCycles();
        if (!completeCycles.empty()) {
            break;
        }
    }

    if (!completeCycles.empty()) {
        if (auto resolutionOperation = std::make_shared<SyncOperation>();
            breakCycle(completeCycles.front(), resolutionOperation)) {
            _syncPal->_syncOps->setOpList({resolutionOperation});

            _hasOrderChanged = true;

            // If a cycle is discovered, the sync must be restarted after the execution of the operation in _syncOrderedOps
            _syncPal->setRestart(true);

            return ExitCode::Ok;
        }
    }

    if (const auto reshuffledOps = fixImpossibleFirstMoveOp(); reshuffledOps && !reshuffledOps->isEmpty()) {
        *_syncPal->_syncOps = *reshuffledOps;

        // If a cycle is discovered, the sync must be restarted after the execution of the operation in _syncOrderedOps
        _syncPal->setRestart(true);
    }

    return ExitCode::Ok;
}

void OperationSorterWorker::fixDeleteBeforeMove() {
    LOG_SYNCPAL_DEBUG(_logger, "Start fixDeleteBeforeMove");
    const std::unordered_set<UniqueId> deleteOps = _unsortedList.opListIdByType(OperationType::Delete);
    const std::unordered_set<UniqueId> moveOps = _unsortedList.opListIdByType(OperationType::Move);
    if (deleteOps.empty() || moveOps.empty()) return;

    for (const auto &deleteOpId: deleteOps) {
        const auto deleteOp = _unsortedList.getOp(deleteOpId);
        const auto deleteNode = deleteOp->affectedNode();
        if (!deleteNode->id().has_value()) {
            LOGW_SYNCPAL_WARN(_logger, L"Node without id: " << SyncName2WStr(deleteNode->name()).c_str());
            continue;
        }

        const auto deleteNodeParentPath = deleteNode->getPath().parent_path();
        bool found = false;
        std::optional<NodeId> deleteNodeParentId;
        if (!_syncPal->_syncDb->id(deleteNode->side(), deleteNodeParentPath.parent_path(), deleteNodeParentId, found)) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::id");
            return;
        }
        if (!found) {
            LOGW_SYNCPAL_INFO(_logger, L"Node not found for path = " << Path2WStr(deleteNodeParentPath.parent_path()).c_str());
            break;
        }

        for (const auto &moveOpId: moveOps) {
            const auto moveOp = _unsortedList.getOp(moveOpId);
            if (moveOp->targetSide() != deleteOp->targetSide()) {
                continue;
            }

            const auto moveNode = moveOp->affectedNode();
            if (!moveNode->parentNode()->id().has_value()) {
                LOGW_SYNCPAL_WARN(_logger, L"Node without id: " << SyncName2WStr(moveNode->parentNode()->name()).c_str());
                continue;
            }

            if (const auto moveNodeParentId = moveNode->parentNode()->id();
                deleteNodeParentId.value() != moveNodeParentId.value())
                continue;

            if (deleteNode->name() == moveNode->name()) {
                // move only if moveOp is before op
                // moveOp depends on op
                moveFirstAfterSecond(moveOp, deleteOp);
            }
        }
    }
    LOG_SYNCPAL_DEBUG(_logger, "End fixDeleteBeforeMove");
}

void OperationSorterWorker::fixMoveBeforeCreate() {
    LOG_SYNCPAL_DEBUG(_logger, "Start fixMoveBeforeCreate");
    const std::unordered_set<UniqueId> moveOps = _unsortedList.opListIdByType(OperationType::Move);
    const std::unordered_set<UniqueId> createOps = _unsortedList.opListIdByType(OperationType::Create);
    for (const auto &moveOpId: moveOps) {
        SyncOpPtr moveOp = _unsortedList.getOp(moveOpId);

        for (const auto &createOpId: createOps) {
            SyncOpPtr createOp = _unsortedList.getOp(createOpId);
            if (createOp->targetSide() != moveOp->targetSide()) {
                continue;
            }

            bool found;
            std::shared_ptr<Node> moveNode = moveOp->affectedNode();
            std::optional<NodeId> sourceParentId;
            // get with path or with idb
            // TODO : check if it's better with moveOriginParentId
            if (!_syncPal->_syncDb->id(moveNode->side(), moveNode->moveOrigin()->parent_path(), sourceParentId, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::id");
                return;
            }
            if (!found) {
                LOGW_SYNCPAL_INFO(_logger,
                                  L"Node not found for path = " << Path2WStr(moveNode->moveOrigin()->parent_path()).c_str());
                break;
            }

            std::shared_ptr<Node> createNode = createOp->affectedNode();
            if (createNode->parentNode() != nullptr) {
                if (!createNode->parentNode()->id().has_value()) {
                    LOGW_SYNCPAL_WARN(_logger, L"Node without id: " << SyncName2WStr(createNode->parentNode()->name()).c_str());
                    continue;
                }

                NodeId createParentId = *createNode->parentNode()->id();
                if (sourceParentId == createParentId) {
                    if (moveNode->name() == createNode->name()) {
                        // move only if createOp is before op
                        moveFirstAfterSecond(createOp, moveOp);
                        continue;
                    }
                }
            }

            std::shared_ptr<Node> correspondingMoveNode = correspondingNodeInOtherTree(moveNode);
            if (correspondingMoveNode) {
                if (correspondingMoveNode->name() == createOp->affectedNode()->name()) {
                    moveFirstAfterSecond(createOp, moveOp);
                    continue;
                }
            } else {
                LOGW_SYNCPAL_WARN(_logger, L"Failed to get corresponding node: " << SyncName2WStr(moveNode->name()).c_str());
                continue;
            }
        }
    }
    LOG_SYNCPAL_DEBUG(_logger, "End fixMoveBeforeCreate");
}

void OperationSorterWorker::fixMoveBeforeDelete() {
    LOG_SYNCPAL_DEBUG(_logger, "Start fixMoveBeforeDelete");
    const std::unordered_set<UniqueId> deleteOps = _unsortedList.opListIdByType(OperationType::Delete);
    const std::unordered_set<UniqueId> moveOps = _unsortedList.opListIdByType(OperationType::Move);
    for (const auto &deleteOpId: deleteOps) {
        SyncOpPtr deleteOp = _unsortedList.getOp(deleteOpId);
        if (deleteOp->affectedNode()->type() != NodeType::Directory) {
            continue;
        }

        std::shared_ptr<Node> deleteNode = deleteOp->affectedNode();
        if (!deleteNode->id().has_value()) {
            LOGW_SYNCPAL_WARN(_logger, L"Node without id: " << SyncName2WStr(deleteNode->name()).c_str());
            continue;
        }

        for (const auto &moveOpId: moveOps) {
            SyncOpPtr moveOp = _unsortedList.getOp(moveOpId);
            if (moveOp->targetSide() != deleteOp->targetSide()) {
                continue;
            }

            bool found = false;
            SyncPath deleteDirPath;
            if (!_syncPal->_syncDb->path(deleteNode->side(), *deleteNode->id(), deleteDirPath, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::path");
                return;
            }
            if (!found) {
                LOGW_SYNCPAL_INFO(_logger, L"Node not found for id = " << Utility::s2ws(*deleteNode->id()).c_str()
                                                                       << L" and name="
                                                                       << SyncName2WStr(deleteNode->name()).c_str());
                continue;
            }

            if (!moveOp->affectedNode()->moveOrigin().has_value()) {
                LOG_SYNCPAL_WARN(_logger, "Missing origin path");
                return;
            }

            SyncPath sourcePath = moveOp->affectedNode()->moveOrigin().value();
            if (Utility::isDescendantOrEqual(sourcePath.lexically_normal(),
                                             SyncPath(deleteDirPath.native() + Str("/")).lexically_normal())) {
                // move only if op is before moveOp
                moveFirstAfterSecond(deleteOp, moveOp);
            }
        }
    }
    LOG_SYNCPAL_DEBUG(_logger, "End fixMoveBeforeDelete");
}

void OperationSorterWorker::fixCreateBeforeMove() {
    LOG_SYNCPAL_DEBUG(_logger, "Start fixCreateBeforeMove");
    const std::unordered_set<UniqueId> createOps = _unsortedList.opListIdByType(OperationType::Create);
    const std::unordered_set<UniqueId> moveOps = _unsortedList.opListIdByType(OperationType::Move);
    for (const auto &createOpId: createOps) {
        SyncOpPtr createOp = _unsortedList.getOp(createOpId);
        if (createOp->affectedNode()->type() != NodeType::Directory) {
            continue;
        }

        if (!createOp->affectedNode()->id().has_value()) {
            LOGW_SYNCPAL_WARN(_logger, L"Node without id: " << SyncName2WStr(createOp->affectedNode()->name()).c_str());
            continue;
        }

        for (const auto &moveOpId: moveOps) {
            SyncOpPtr moveOp = _unsortedList.getOp(moveOpId);
            if (moveOp->targetSide() != createOp->targetSide()) {
                continue;
            }
            std::shared_ptr<Node> moveNode = moveOp->affectedNode();
            if (moveNode->parentNode() != nullptr) {
                std::optional<NodeId> moveDestParentId = moveNode->parentNode()->id();
                if (moveDestParentId) {
                    if (*moveDestParentId == *createOp->affectedNode()->id()) {
                        // move only if moveOp is before op
                        moveFirstAfterSecond(moveOp, createOp);
                        continue;
                    }
                }
            }
        }
    }
    LOG_SYNCPAL_DEBUG(_logger, "End fixCreateBeforeMove");
}

void OperationSorterWorker::fixDeleteBeforeCreate() {
    LOG_SYNCPAL_DEBUG(_logger, "Start fixDeleteBeforeCreate");
    const std::unordered_set<UniqueId> deleteOps = _unsortedList.opListIdByType(OperationType::Delete);
    const std::unordered_set<UniqueId> createOps = _unsortedList.opListIdByType(OperationType::Create);
    for (const auto &deleteOpId: deleteOps) {
        SyncOpPtr deleteOp = _unsortedList.getOp(deleteOpId);

        std::shared_ptr<Node> deleteNode = deleteOp->affectedNode();
        if (!deleteNode->id().has_value()) {
            LOGW_SYNCPAL_WARN(_logger, L"Node without id: " << SyncName2WStr(deleteNode->name()).c_str());
            continue;
        }

        for (const auto &createOpId: createOps) {
            SyncOpPtr createOp = _unsortedList.getOp(createOpId);
            if (createOp->targetSide() != deleteOp->targetSide()) {
                continue;
            }

            std::shared_ptr<Node> createNode = createOp->affectedNode();
            if (!createNode->id().has_value()) {
                LOGW_SYNCPAL_WARN(_logger, L"Node without id: " << SyncName2WStr(createNode->name()).c_str());
                continue;
            }

            bool found = false;
            NodeId deleteNodeId = deleteNode->id().value();
            if (createNode->id().value() == deleteNode->id().value()) {
                if (!deleteNode->previousId().has_value()) {
                    LOGW_SYNCPAL_WARN(_logger, L"Node without previousId: " << SyncName2WStr(deleteNode->name()).c_str());
                    continue;
                }

                deleteNodeId = deleteNode->previousId().value();
            }

            SyncPath deletePath;
            if (!_syncPal->_syncDb->path(deleteNode->side(), deleteNodeId, deletePath, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::path");
                return;
            }
            if (!found) {
                LOGW_SYNCPAL_INFO(_logger, L"Node not found for id = " << Utility::s2ws(*deleteNode->id()).c_str()
                                                                       << L" and name="
                                                                       << SyncName2WStr(deleteNode->name()).c_str());
                continue;
            }

            std::optional<NodeId> deleteParentId;
            if (!_syncPal->_syncDb->id(deleteNode->side(), deletePath.parent_path(), deleteParentId, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::id");
                return;
            }
            if (!found) {
                LOGW_SYNCPAL_INFO(_logger, L"Node not found for path = " << Path2WStr(deletePath.parent_path()).c_str());
                break;
            }

            if (createNode->parentNode() != nullptr) {
                std::optional<NodeId> createParentId = createNode->parentNode()->id();
                if (deleteParentId == createParentId) {
                    if (createNode->name() == deleteNode->name()) {
                        // move only if createOp is before op
                        moveFirstAfterSecond(createOp, deleteOp);
                    }
                }
            }
        }
    }
    LOG_SYNCPAL_DEBUG(_logger, "End fixDeleteBeforeCreate");
}

void OperationSorterWorker::fixMoveBeforeMoveOccupied() {
    LOG_SYNCPAL_DEBUG(_logger, "Start fixMoveBeforeMoveOccupied");
    std::unordered_set<UniqueId> moveOps = _unsortedList.opListIdByType(OperationType::Move);
    std::unordered_set<UniqueId> moveOps2 = moveOps;
    for (const auto &moveOpId: moveOps) {
        SyncOpPtr moveOp = _unsortedList.getOp(moveOpId);

        for (const auto &moveOpId2: moveOps2) {
            SyncOpPtr moveOp2 = _unsortedList.getOp(moveOpId2);
            if (moveOp == moveOp2 || moveOp2->targetSide() != moveOp->targetSide()) {
                continue;
            }

            std::shared_ptr<Node> moveNode = moveOp2->affectedNode();

            if (!moveOp2->affectedNode()->moveOrigin().has_value()) {
                LOG_SYNCPAL_WARN(_logger, "Missing origin path");
                return;
            }

            SyncPath sourcePath = *moveOp2->affectedNode()->moveOrigin();
            bool found;
            std::optional<NodeId> sourceParentId;
            if (!_syncPal->_syncDb->id(moveNode->side(), sourcePath.parent_path(), sourceParentId, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::id");
                return;
            }
            if (!found) {
                LOGW_SYNCPAL_DEBUG(_logger, L"Node not found for path = " << Path2WStr(sourcePath.parent_path()).c_str());
                break;
            }

            std::shared_ptr<Node> otherMoveNode = moveOp->affectedNode();
            if (otherMoveNode->parentNode() != nullptr) {
                std::optional<NodeId> moveDestParentId = otherMoveNode->parentNode()->id();
                if (sourceParentId == moveDestParentId) {
                    if (moveNode->name() == otherMoveNode->name()) {
                        // move only if moveOp is before op
                        moveFirstAfterSecond(moveOp2, moveOp);
                    }
                }
            }
        }
        moveOps2.erase(moveOpId);
    }
    LOG_SYNCPAL_DEBUG(_logger, "End fixMoveBeforeMoveOccupied");
}

class SyncOpDepthCmp {
    public:
        bool operator()(const std::tuple<SyncOpPtr, SyncOpPtr, int> &a, const std::tuple<SyncOpPtr, SyncOpPtr, int> &b) {
            if (std::get<2>(a) == std::get<2>(b)) {
                // If depth are equal, put op to move with lowest ID first
                return std::get<0>(a)->id() > std::get<0>(b)->id();
            }
            return std::get<2>(a) < std::get<2>(b);
        }
};

void OperationSorterWorker::fixCreateBeforeCreate() {
    LOG_SYNCPAL_DEBUG(_logger, "Start fixCreateBeforeCreate");
    std::priority_queue<std::tuple<SyncOpPtr, SyncOpPtr, int>, std::vector<std::tuple<SyncOpPtr, SyncOpPtr, int>>, SyncOpDepthCmp>
            opsToMove;

    std::unordered_map<UniqueId, int> indexMap;
    _syncPal->_syncOps->getMapIndexToOp(indexMap, OperationType::Create);

    for (const auto &opId: _syncPal->_syncOps->opSortedList()) {
        SyncOpPtr createOp = _syncPal->_syncOps->getOp(opId);
        int createOpIndex = indexMap[opId];
        SyncOpPtr ancestorOp = nullptr;
        int relativeDepth = 0;
        if (hasParentWithHigherIndex(indexMap, createOp, createOpIndex, ancestorOp, relativeDepth)) {
            opsToMove.push(std::make_tuple(createOp, ancestorOp, relativeDepth));
        }
    }

    while (!opsToMove.empty()) {
        std::tuple<SyncOpPtr, SyncOpPtr, int> opTuple = opsToMove.top();
        moveFirstAfterSecond(std::get<0>(opTuple), std::get<1>(opTuple));
        opsToMove.pop();
    }
    LOG_SYNCPAL_DEBUG(_logger, "End fixCreateBeforeCreate");
}

void OperationSorterWorker::fixEditBeforeMove() {
    LOG_SYNCPAL_DEBUG(_logger, "Start fixEditBeforeMove");
    const std::unordered_set<UniqueId> editOps = _unsortedList.opListIdByType(OperationType::Edit);
    const std::unordered_set<UniqueId> moveOps = _unsortedList.opListIdByType(OperationType::Move);
    for (const auto &editOpId: editOps) {
        SyncOpPtr editOp = _unsortedList.getOp(editOpId);

        for (const auto &moveOpId: moveOps) {
            SyncOpPtr moveOp = _unsortedList.getOp(moveOpId);
            if (moveOp->targetSide() != editOp->targetSide() || moveOp->affectedNode()->id() != editOp->affectedNode()->id()) {
                continue;
            }

            // Since in case of move op, the node contains already the new name
            // we always want to execute move operation first
            moveFirstAfterSecond(editOp, moveOp);
        }
    }
    LOG_SYNCPAL_DEBUG(_logger, "End fixEditBeforeMove");
}

bool OperationSorterWorker::hasParentWithHigherIndex(const std::unordered_map<UniqueId, int> &indexMap, const SyncOpPtr op,
                                                     const int createOpIndex, SyncOpPtr &ancestorOp, int &depth) {
    // Move createOp after the ancestor operation on the highest level (if needed)

    // Retrieve parent Op
    std::shared_ptr<Node> createNode = op->affectedNode();
    std::shared_ptr<Node> parentNode = createNode->parentNode();

    if (!parentNode->id().has_value()) {
        return false;
    }

    std::list<UniqueId> parentOpList = _syncPal->_syncOps->getOpIdsFromNodeId(*parentNode->id());
    for (const auto &parentOpId: parentOpList) {
        SyncOpPtr parentOp = _syncPal->_syncOps->getOp(parentOpId);
        // Check that parent has been just created
        if (parentOp->type() == OperationType::Create) {
            // Check that index of parentOp is lower than index of createOp
            int parentOpIndex = indexMap.at(parentOpId);
            if (parentOpIndex > createOpIndex) {
                // Update ancestor
                ancestorOp = parentOp;
            }

            // We need to check higher level ancestors anyway, even if direct parent index is correct
            hasParentWithHigherIndex(indexMap, parentOp, parentOpIndex, ancestorOp, ++depth);
            break;
        }
    }

    if (ancestorOp == nullptr) {
        return false;
    } else {
        return true;
    }
}

void OperationSorterWorker::fixMoveBeforeMoveHierarchyFlip() {
    LOG_SYNCPAL_DEBUG(_logger, "Start fixMoveBeforeMoveHierarchyFlip");
    std::unordered_set<UniqueId> moveOps = _unsortedList.opListIdByType(OperationType::Move);
    for (const auto &xOpId: moveOps) {
        SyncOpPtr xOp = _unsortedList.getOp(xOpId);
        if (xOp->affectedNode()->type() != NodeType::Directory) {
            continue;
        }

        std::shared_ptr<Node> xNode = xOp->affectedNode();
        SyncPath xDestPath = xNode->getPath();
        if (!xNode->moveOrigin().has_value()) {
            continue;
        }
        SyncPath xSourcePath = *xNode->moveOrigin();

        for (const auto &yOpId: moveOps) {
            SyncOpPtr yOp = _unsortedList.getOp(yOpId);
            if (yOp->affectedNode()->type() != NodeType::Directory || xOp == yOp || xOp->targetSide() != yOp->targetSide()) {
                continue;
            }

            std::shared_ptr<Node> yNode = yOp->affectedNode();
            SyncPath yDestPath = yNode->getPath();
            if (!yNode->moveOrigin().has_value()) {
                continue;
            }
            SyncPath ySourcePath = *yNode->moveOrigin();

            bool isXBelowY = Utility::isDescendantOrEqual(xDestPath.lexically_normal(),
                                                          SyncPath(yDestPath.native() + Str("/")).lexically_normal());
            if (isXBelowY) {
                bool isYBelowXInDb = Utility::isDescendantOrEqual(ySourcePath.lexically_normal(),
                                                                  SyncPath(xSourcePath.native() + Str("/")).lexically_normal());
                if (isYBelowXInDb) {
                    moveFirstAfterSecond(xOp, yOp);
                }
            }
        }
    }
    LOG_SYNCPAL_DEBUG(_logger, "End fixMoveBeforeMoveHierarchyFlip");
}

std::optional<SyncOperationList> OperationSorterWorker::fixImpossibleFirstMoveOp() {
    if (_syncPal->_syncOps->isEmpty()) {
        return std::nullopt;
    }

    UniqueId opId1 = _syncPal->_syncOps->opSortedList().front();
    SyncOpPtr o1 = _syncPal->_syncOps->getOp(opId1);
    // check if o1 is move-directory operation
    if (o1->type() != OperationType::Move && o1->affectedNode()->type() != NodeType::Directory) {
        return std::nullopt;
    }

    // computes paths
    // impossible move if dest = source + "/"
    SyncPath source = (o1->affectedNode()->moveOrigin().has_value() ? o1->affectedNode()->moveOrigin().value() : "");
    SyncPath dest = o1->affectedNode()->getPath();
    if (!Utility::isDescendantOrEqual(dest.lexically_normal(), SyncPath(source.native() + Str("/")).lexically_normal())) {
        return std::nullopt;
    }

    std::shared_ptr<Node> correspondingDestinationParentNode = correspondingNodeInOtherTree(o1->affectedNode()->parentNode());
    std::shared_ptr<Node> correspondingSourceNode = correspondingNodeInOtherTree(o1->affectedNode());
    if (correspondingDestinationParentNode == nullptr || correspondingSourceNode == nullptr) {
        return std::nullopt;
    }

    std::list<std::shared_ptr<Node>> moveDirectoryList;
    // Traverse updatetree from source to dest
    std::shared_ptr<Node> tmpNode = correspondingSourceNode;
    while (tmpNode->parentNode() != nullptr && tmpNode != correspondingDestinationParentNode) {
        tmpNode = tmpNode->parentNode();
        // storing all move-directory nodes
        if (tmpNode->type() == NodeType::Directory && tmpNode->hasChangeEvent(OperationType::Move)) {
            moveDirectoryList.push_back(tmpNode);
        }
    }

    // look for oFirst the operation in moveDirectoryList which come first in _sortedOps
    bool found = false;
    SyncOpPtr oFirst;
    for (const auto &opId: _syncPal->_syncOps->opSortedList()) {
        SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
        for (auto n: moveDirectoryList) {
            if (op->affectedNode() == n) {
                oFirst = op;
                found = true;
                break;
            }
        }
        if (found) {
            break;
        }
    }

    // make filtered version of sortedOps
    ReplicaSide targetReplica = correspondingDestinationParentNode->side();
    SyncOperationList reshuffledOps;
    for (const auto &opId: _syncPal->_syncOps->opSortedList()) {
        SyncOpPtr op = _syncPal->_syncOps->getOp(opId);
        // we include the oFirst op
        if (op == oFirst) {
            reshuffledOps.pushOp(op);
            break;
        }
        // op that affect the targetReplica or both
        if (op->targetSide() == targetReplica || op->omit()) {
            reshuffledOps.pushOp(op);
        }
    }

    return reshuffledOps;
}

std::list<SyncOperationList> OperationSorterWorker::findCompleteCycles() {
    std::list<SyncOperationList> completeCycles;

    for (auto &pair: _reorderings) {
        std::list<std::pair<SyncOpPtr, SyncOpPtr>> l1;
        std::list<SyncOpPtr> l2;
        l1 = _reorderings;
        l1.remove(pair);
        l2.push_back(pair.first);
        l2.push_back(pair.second);
        SyncOpPtr opToFind = pair.first;
        SyncOpPtr opTmp = l2.back();
        for (auto it = l1.begin(); it != l1.end(); ++it) {
            if (SyncOpPtr opF = it->first; opF == opTmp) {
                l2.push_back(it->second);
                opTmp = l2.back();
                if (it->second == opToFind) {
                    break;
                }
                it = l1.erase(it);
            }
        }

        if (l2.front() == l2.back()) {
            l2.pop_back();
            SyncOperationList cycle;
            cycle.setOpList(l2);
            completeCycles.push_back(cycle);
            break;
        }
    }
    return completeCycles;
}

bool OperationSorterWorker::breakCycle(SyncOperationList &cycle, SyncOpPtr renameResolutionOp) {
    SyncOpPtr matchOp;
    // Look for delete operation in the cycle
    if (!cycle.opListIdByType(OperationType::Delete).empty()) {
        const auto opId = cycle.opListIdByType(OperationType::Delete).begin();
        matchOp = cycle.getOp(*opId);
    }

    // If there is no delete, look for move op in the cycle
    if (!matchOp && !cycle.opListIdByType(OperationType::Move).empty()) {
        const auto opId = cycle.opListIdByType(OperationType::Move).begin();
        matchOp = cycle.getOp(*opId);
    }

    if (!matchOp) return false;

    // A cycle must contain a Delete or a Move operation
    // Generate a rename resolution operation
    renameResolutionOp->setType(OperationType::Move);
    renameResolutionOp->setOmit(matchOp->omit());
    // Find the corresponding node of `matchOp`
    std::shared_ptr<Node> correspondingNode = correspondingNodeInOtherTree(matchOp->affectedNode());
    if (!correspondingNode) {
        LOG_SYNCPAL_WARN(_logger, "Error in correspondingNode with id = " << matchOp->affectedNode()->id()->c_str()
                                                                          << " - idDb = " << *matchOp->affectedNode()->idb());
        return false;
    }
    renameResolutionOp->setAffectedNode(matchOp->affectedNode());
    renameResolutionOp->setCorrespondingNode(correspondingNode);
    renameResolutionOp->setNewParentNode(correspondingNode->parentNode()); // Parent not changed but needed in Executor
    renameResolutionOp->setIsBreakingCycleOp(true);

    renameResolutionOp->setTargetSide(correspondingNode->side());
    // Append random suffix to name in order to break the cycle
    renameResolutionOp->setNewName(correspondingNode->name() + Str("-") +
                                   Str2SyncName(CommonUtility::generateRandomStringAlphaNum()));
    // and we only execute Opr follow by restart sync
    LOGW_SYNCPAL_INFO(_logger, L"Breaking cycle by renaming temporarily item "
                                       << SyncName2WStr(correspondingNode->name()).c_str() << L" to "
                                       << SyncName2WStr(renameResolutionOp->newName()).c_str());
    return true;
}

void OperationSorterWorker::moveFirstAfterSecond(SyncOpPtr opFirst, SyncOpPtr opSecond) {
    std::list<UniqueId>::const_iterator firstIt;
    std::list<UniqueId>::const_iterator secondIt;
    bool firstFound = false;
    for (auto it = _syncPal->_syncOps->opSortedList().begin(); it != _syncPal->_syncOps->opSortedList().end(); ++it) {
        SyncOpPtr op = _syncPal->_syncOps->getOp(*it);
        if (op == opSecond) {
            if (firstFound) {
                secondIt = it;
            }
            break;
        }
        if (op == opFirst) {
            firstFound = true;
            firstIt = it;
        }
    }

    // Make sure that opFirst is finished before starting opSecond, even if they are in the correct order
    opFirst->setParentId(opSecond->id());

    if (firstFound) {
        // make sure opSecond is executed after opFirst
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_SYNCPAL_DEBUG(_logger, L"Operation " << opFirst->id() << L" (" << opFirst->type() << L" "
                                                      << SyncName2WStr(opFirst->affectedNode()->name()).c_str()
                                                      << L") moved after operation " << opSecond->id() << L" ("
                                                      << opSecond->type() << L" "
                                                      << SyncName2WStr(opSecond->affectedNode()->name()).c_str() << L")");
        }

        _syncPal->_syncOps->deleteOp(firstIt);
        _syncPal->_syncOps->insertOp(++secondIt, opFirst);
        _hasOrderChanged = true;
        addPairToReorderings(opSecond, opFirst);
    }
}

void OperationSorterWorker::addPairToReorderings(SyncOpPtr op, SyncOpPtr opOnFirstDepends) {
    const auto pair = std::make_pair(op, opOnFirstDepends);
    for (auto const &opPair: _reorderings) {
        if (opPair == pair) {
            return;
        }
    }
    _reorderings.push_back(pair);
}

} // namespace KDC
