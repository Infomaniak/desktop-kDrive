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

#include "cyclefinder.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/utility.h"
#include "requests/parameterscache.h"
#include "utility/logiffail.h"

namespace KDC {

OperationSorterWorker::OperationSorterWorker(const std::shared_ptr<SyncPal> &syncPal, const std::string &name,
                                             const std::string &shortName) : OperationProcessor(syncPal, name, shortName) {}

void OperationSorterWorker::execute() {
    LOG_SYNCPAL_DEBUG(_logger, "Worker started: name=" << name());

    const auto start = std::chrono::steady_clock::now();

    _reorderings.clear();
    _syncPal->_syncOps->startUpdate();
    sortOperations();

    const auto elapsed_seconds = std::chrono::steady_clock::now() - start;
    LOG_SYNCPAL_INFO(_logger, "Operation sorting finished in: " << elapsed_seconds.count() << "s");

    LOG_SYNCPAL_DEBUG(_logger, "Worker stopped: name=" << name());
    setDone(ExitCode::Ok);
}

void OperationSorterWorker::sortOperations() {
    _syncPal->_syncOps->startUpdate();
    SyncOperationList completeCycle;
    bool cycleFound = false;
    _hasOrderChanged = true;
    while (_hasOrderChanged) {
        if (stopAsked()) {
            return;
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

        CycleFinder cycleFinder(_reorderings);
        cycleFinder.findCompleteCycle();
        if (cycleFinder.hasCompleteCycle()) {
            completeCycle = cycleFinder.completeCycle();
            cycleFound = true;
            break;
        }
    }

    if (cycleFound) {
        if (const auto resolutionOperation = std::make_shared<SyncOperation>(); breakCycle(completeCycle, resolutionOperation)) {
            _syncPal->_syncOps->setOpList({resolutionOperation});
            _hasOrderChanged = true;
            // If a cycle is discovered, the sync must be restarted after the execution of the operation in _syncOps
            _syncPal->setRestart(true);
            return;
        }
    }

    if (const auto reshuffledOps = fixImpossibleFirstMoveOp(); reshuffledOps && !reshuffledOps->isEmpty()) {
        *_syncPal->_syncOps = *reshuffledOps;
        // If a cycle is discovered, the sync must be restarted after the execution of the operation in _syncOps
        _syncPal->setRestart(true);
    }
}

void OperationSorterWorker::fixDeleteBeforeMove() {
    LOG_SYNCPAL_DEBUG(_logger, "Start fixDeleteBeforeMove");
    const std::unordered_set<UniqueId> deleteOps = _syncPal->_syncOps->opListIdByType(OperationType::Delete);
    const std::unordered_set<UniqueId> moveOps = _syncPal->_syncOps->opListIdByType(OperationType::Move);
    if (deleteOps.empty() || moveOps.empty()) return;

    for (const auto &deleteOpId: deleteOps) {
        const auto deleteOp = _syncPal->_syncOps->getOp(deleteOpId);
        LOG_IF_FAIL(deleteOp)
        const auto deleteNode = deleteOp->affectedNode();
        LOG_IF_FAIL(deleteNode)
        const auto deleteNodeParentPath = deleteNode->getPath().parent_path();
        NodeId deleteNodeParentId;
        if (!getIdFromDb(deleteNode->side(), deleteNodeParentPath.parent_path(), deleteNodeParentId)) continue;

        for (const auto &moveOpId: moveOps) {
            const auto moveOp = _syncPal->_syncOps->getOp(moveOpId);
            LOG_IF_FAIL(moveOp)
            if (moveOp->targetSide() != deleteOp->targetSide()) {
                continue;
            }

            const auto moveNode = moveOp->affectedNode();
            LOG_IF_FAIL(moveNode)
            const auto moveNodeParent = moveNode->parentNode();
            LOG_IF_FAIL(moveNodeParent)
            if (!moveNodeParent->id().has_value()) {
                LOGW_SYNCPAL_WARN(_logger, L"Node without id: " << SyncName2WStr(moveNodeParent->name()));
                continue;
            }

            if (const auto moveNodeParentId = moveNodeParent->id(); deleteNodeParentId != moveNodeParentId.value()) {
                continue;
            }

            if (deleteNode->name() == moveNode->name()) {
                // move only if moveOp is before deleteOp
                moveFirstAfterSecond(moveOp, deleteOp);
            }
        }
    }
    LOG_SYNCPAL_DEBUG(_logger, "End fixDeleteBeforeMove");
}

void OperationSorterWorker::fixMoveBeforeCreate() {
    LOG_SYNCPAL_DEBUG(_logger, "Start fixMoveBeforeCreate");
    const std::unordered_set<UniqueId> moveOps = _syncPal->_syncOps->opListIdByType(OperationType::Move);
    const std::unordered_set<UniqueId> createOps = _syncPal->_syncOps->opListIdByType(OperationType::Create);
    for (const auto &moveOpId: moveOps) {
        const auto moveOp = _syncPal->_syncOps->getOp(moveOpId);
        LOG_IF_FAIL(moveOp)
        const auto moveNode = moveOp->affectedNode();
        LOG_IF_FAIL(moveNode)

        for (const auto &createOpId: createOps) {
            const auto createOp = _syncPal->_syncOps->getOp(createOpId);
            LOG_IF_FAIL(createOp)
            if (createOp->targetSide() != moveOp->targetSide()) {
                continue;
            }

            NodeId moveNodeOriginParentId;
            if (!getIdFromDb(moveNode->side(), moveNode->moveOriginInfos().path().parent_path(), moveNodeOriginParentId))
                continue;

            const auto createNode = createOp->affectedNode();
            LOG_IF_FAIL(createNode)
            const auto createParentNode = createNode->parentNode();
            LOG_IF_FAIL(createParentNode)
            if (!createParentNode->id().has_value()) {
                LOGW_SYNCPAL_WARN(_logger, L"Node without id: " << SyncName2WStr(createParentNode->name()));
                continue;
            }

            if (const auto createParentId = *createParentNode->id(); moveNodeOriginParentId != createParentId) {
                continue;
            }

            if (moveNode->moveOriginInfos().path().filename() == createNode->name()) {
                // move only if createOp is before moveOp
                moveFirstAfterSecond(createOp, moveOp);
            }
        }
    }
    LOG_SYNCPAL_DEBUG(_logger, "End fixMoveBeforeCreate");
}

void OperationSorterWorker::fixMoveBeforeDelete() {
    LOG_SYNCPAL_DEBUG(_logger, "Start fixMoveBeforeDelete");
    const std::unordered_set<UniqueId> deleteOps = _syncPal->_syncOps->opListIdByType(OperationType::Delete);
    const std::unordered_set<UniqueId> moveOps = _syncPal->_syncOps->opListIdByType(OperationType::Move);
    for (const auto &deleteOpId: deleteOps) {
        const auto deleteOp = _syncPal->_syncOps->getOp(deleteOpId);
        LOG_IF_FAIL(deleteOp)
        if (deleteOp->affectedNode()->type() != NodeType::Directory) {
            continue;
        }
        const auto deleteNode = deleteOp->affectedNode();
        LOG_IF_FAIL(deleteNode)
        const auto deleteNodePath = deleteNode->getPath();

        for (const auto &moveOpId: moveOps) {
            const auto moveOp = _syncPal->_syncOps->getOp(moveOpId);
            LOG_IF_FAIL(moveOp)
            if (moveOp->targetSide() != deleteOp->targetSide()) {
                continue;
            }

            LOG_IF_FAIL(moveOp->affectedNode())
            if (const auto moveNodeOriginPath = moveOp->affectedNode()->moveOriginInfos().path();
                Utility::isDescendantOrEqual(moveNodeOriginPath, deleteNodePath)) {
                // move only if deleteOp is before moveOp
                moveFirstAfterSecond(deleteOp, moveOp);
            }
        }
    }
    LOG_SYNCPAL_DEBUG(_logger, "End fixMoveBeforeDelete");
}

void OperationSorterWorker::fixCreateBeforeMove() {
    LOG_SYNCPAL_DEBUG(_logger, "Start fixCreateBeforeMove");
    const std::unordered_set<UniqueId> createOps = _syncPal->_syncOps->opListIdByType(OperationType::Create);
    const std::unordered_set<UniqueId> moveOps = _syncPal->_syncOps->opListIdByType(OperationType::Move);
    for (const auto &createOpId: createOps) {
        const auto createOp = _syncPal->_syncOps->getOp(createOpId);
        LOG_IF_FAIL(createOp)
        if (createOp->affectedNode()->type() != NodeType::Directory) {
            continue;
        }

        const auto createNode = createOp->affectedNode();
        LOG_IF_FAIL(createNode)
        if (!createNode->id().has_value()) {
            LOGW_SYNCPAL_WARN(_logger, L"Node without id: " << SyncName2WStr(createNode->name()));
            continue;
        }

        for (const auto &moveOpId: moveOps) {
            const auto moveOp = _syncPal->_syncOps->getOp(moveOpId);
            if (moveOp->targetSide() != createOp->targetSide()) {
                continue;
            }

            const auto moveNode = moveOp->affectedNode();
            LOG_IF_FAIL(moveNode)
            const auto moveParentNode = moveNode->parentNode();
            LOG_IF_FAIL(moveParentNode)
            if (!moveParentNode->id().has_value()) {
                LOGW_SYNCPAL_WARN(_logger, L"Node without id: " << SyncName2WStr(moveParentNode->name()));
                continue;
            }

            if (*moveParentNode->id() == *createNode->id()) {
                // move only if moveOp is before createOp
                moveFirstAfterSecond(moveOp, createOp);
            }
        }
    }
    LOG_SYNCPAL_DEBUG(_logger, "End fixCreateBeforeMove");
}

void OperationSorterWorker::fixDeleteBeforeCreate() {
    LOG_SYNCPAL_DEBUG(_logger, "Start fixDeleteBeforeCreate");
    const std::unordered_set<UniqueId> deleteOps = _syncPal->_syncOps->opListIdByType(OperationType::Delete);
    const std::unordered_set<UniqueId> createOps = _syncPal->_syncOps->opListIdByType(OperationType::Create);
    for (const auto &deleteOpId: deleteOps) {
        const auto deleteOp = _syncPal->_syncOps->getOp(deleteOpId);
        LOG_IF_FAIL(deleteOp)
        const auto deleteNode = deleteOp->affectedNode();
        LOG_IF_FAIL(deleteNode)
        const auto deleteNodeParentPath = deleteNode->getPath().parent_path();
        NodeId deleteNodeParentId;
        if (!getIdFromDb(deleteNode->side(), deleteNodeParentPath, deleteNodeParentId)) continue;

        for (const auto &createOpId: createOps) {
            const auto createOp = _syncPal->_syncOps->getOp(createOpId);
            LOG_IF_FAIL(createOp)
            if (createOp->targetSide() != deleteOp->targetSide()) {
                continue;
            }

            const auto createNode = createOp->affectedNode();
            LOG_IF_FAIL(createNode)
            if (!createNode->parentNode()) {
                continue;
            }

            if (const auto createNodeParentId = createNode->parentNode()->id(); deleteNodeParentId != createNodeParentId) {
                continue;
            }

            if (createNode->name() == deleteNode->name()) {
                // move only if createOp is before deleteOp
                moveFirstAfterSecond(createOp, deleteOp);
            }
        }
    }
    LOG_SYNCPAL_DEBUG(_logger, "End fixDeleteBeforeCreate");
}

void OperationSorterWorker::fixMoveBeforeMoveOccupied() {
    LOG_SYNCPAL_DEBUG(_logger, "Start fixMoveBeforeMoveOccupied");
    for (const auto opIds = _syncPal->_syncOps->opListIdByType(OperationType::Move); const auto &opId: opIds) {
        const auto op = _syncPal->_syncOps->getOp(opId);
        LOG_IF_FAIL(op)
        const auto node = op->affectedNode();
        LOG_IF_FAIL(node)
        if (!node->parentNode()) continue;
        const auto nodePath = node->getPath();
        const auto nodeParentId = node->parentNode()->id();

        for (const auto &otherOpId: opIds) {
            const auto otherOp = _syncPal->_syncOps->getOp(otherOpId);
            LOG_IF_FAIL(otherOp)
            if (op == otherOp || otherOp->targetSide() != op->targetSide()) {
                continue;
            }

            const auto otherNode = otherOp->affectedNode();
            LOG_IF_FAIL(otherNode)
            const auto otherNodeOriginPath = otherNode->moveOriginInfos().path();
            NodeId otherNodeOriginParentId;
            if (!getIdFromDb(otherNode->side(), otherNodeOriginPath.parent_path(), otherNodeOriginParentId)) continue;

            if (nodeParentId == otherNodeOriginParentId && nodePath.filename() == otherNodeOriginPath.filename()) {
                // move only if op is before otherOp
                moveFirstAfterSecond(op, otherOp);
            }
        }
    }
    LOG_SYNCPAL_DEBUG(_logger, "End fixMoveBeforeMoveOccupied");
}

class SyncOpDepthCmp {
    public:
        bool operator()(const std::tuple<SyncOpPtr, SyncOpPtr, int32_t> &a,
                        const std::tuple<SyncOpPtr, SyncOpPtr, int32_t> &b) const {
            if (std::get<2>(a) == std::get<2>(b)) {
                // If depth are equal, put op to move with lowest ID first
                return std::get<0>(a)->id() > std::get<0>(b)->id();
            }
            return std::get<2>(a) < std::get<2>(b);
        }
};

void OperationSorterWorker::fixCreateBeforeCreate() {
    LOG_SYNCPAL_DEBUG(_logger, "Start fixCreateBeforeCreate");
    // The method described in the thesis is way too slow. Therefor, a std::priority_queue is used to efficiently sort the
    // operations before moving them in the sorted list.
    std::priority_queue<std::tuple<SyncOpPtr, SyncOpPtr, int32_t>, std::vector<std::tuple<SyncOpPtr, SyncOpPtr, int32_t>>,
                        SyncOpDepthCmp>
            opsToMove;

    std::unordered_map<UniqueId, int32_t> opIdToIndexMap;
    _syncPal->_syncOps->getOpIdToIndexMap(opIdToIndexMap, OperationType::Create);

    for (const auto &opId: _syncPal->_syncOps->opSortedList()) {
        SyncOpPtr createOp = _syncPal->_syncOps->getOp(opId);
        LOG_IF_FAIL(createOp)
        if (createOp->type() != OperationType::Create) continue;

        SyncOpPtr ancestorOpWithHighestDistance = nullptr;
        if (int32_t relativeDepth = 0;
            hasParentWithHigherIndex(opIdToIndexMap, createOp, ancestorOpWithHighestDistance, relativeDepth)) {
            opsToMove.emplace(createOp, ancestorOpWithHighestDistance, relativeDepth);
        }
    }

    while (!opsToMove.empty()) {
        const auto [op, ancestorOp, depth] = opsToMove.top();
        LOGW_SYNCPAL_DEBUG(_logger, L"op: " << Utility::formatSyncName(op->affectedNode()->name()) << L", ancestorOp: "
                                            << Utility::formatSyncName(ancestorOp->affectedNode()->name()) << L", depth; "
                                            << depth);
        moveFirstAfterSecond(op, ancestorOp);
        opsToMove.pop();
    }
    LOG_SYNCPAL_DEBUG(_logger, "End fixCreateBeforeCreate");
}

bool OperationSorterWorker::hasParentWithHigherIndex(const std::unordered_map<UniqueId, int32_t> &opIdToIndexMap,
                                                     const SyncOpPtr &op, SyncOpPtr &ancestorOpWithHighestDistance,
                                                     int32_t &relativeDepth) const {
    ancestorOpWithHighestDistance = nullptr;
    relativeDepth = 0;
    const auto node = op->affectedNode();
    auto parentNode = node->parentNode();

    bool again = true;
    while (again) {
        again = false;
        if (!parentNode || parentNode == _syncPal->updateTree(parentNode->side())->rootNode()) {
            break;
        }

        for (const auto parentOpIdList = _syncPal->_syncOps->getOpIdsFromNodeId(*parentNode->id());
             const auto &parentOpId: parentOpIdList) {
            const auto parentOp = _syncPal->_syncOps->getOp(parentOpId);
            if (parentOp->type() != OperationType::Create) continue;

            // Check that index of parentOp is lower than index of op
            if (opIdToIndexMap.at(parentOpId) > opIdToIndexMap.at(op->id())) {
                // parentOp has higher index than op. Save it in `ancestorOpWithHighestDistance` and check its parent.
                ancestorOpWithHighestDistance = parentOp;
                parentNode = parentNode->parentNode();
                ++relativeDepth;
                again = true;
                break;
            }
        }
    }

    return ancestorOpWithHighestDistance != nullptr;
}

void OperationSorterWorker::fixEditBeforeMove() {
    LOG_SYNCPAL_DEBUG(_logger, "Start fixEditBeforeMove");
    const auto moveOpIds = _syncPal->_syncOps->opListIdByType(OperationType::Move);
    for (const auto editOpIds = _syncPal->_syncOps->opListIdByType(OperationType::Edit); const auto &editOpId: editOpIds) {
        const auto editOp = _syncPal->_syncOps->getOp(editOpId);
        LOG_IF_FAIL(editOp)

        for (const auto &moveOpId: moveOpIds) {
            const auto moveOp = _syncPal->_syncOps->getOp(moveOpId);
            LOG_IF_FAIL(moveOp)
            if (moveOp->targetSide() != editOp->targetSide() || moveOp->affectedNode()->id() != editOp->affectedNode()->id()) {
                continue;
            }

            // Since in case of move op, the node already contains the new name
            // we always want to execute move operation first
            moveFirstAfterSecond(editOp, moveOp);
        }
    }
    LOG_SYNCPAL_DEBUG(_logger, "End fixEditBeforeMove");
}

void OperationSorterWorker::fixMoveBeforeMoveHierarchyFlip() {
    LOG_SYNCPAL_DEBUG(_logger, "Start fixMoveBeforeMoveHierarchyFlip");
    for (const auto moveOpIds = _syncPal->_syncOps->opListIdByType(OperationType::Move); const auto &opIdX: moveOpIds) {
        const auto opX = _syncPal->_syncOps->getOp(opIdX);
        LOG_IF_FAIL(opX)
        if (opX->affectedNode()->type() != NodeType::Directory) {
            continue;
        }

        const auto nodeX = opX->affectedNode();
        LOG_IF_FAIL(nodeX);
        const auto nodeOriginPathX = nodeX->moveOriginInfos().path();
        const auto nodeDestinationPathX = nodeX->getPath();

        for (const auto &opIdY: moveOpIds) {
            const auto opY = _syncPal->_syncOps->getOp(opIdY);
            LOG_IF_FAIL(opY)
            if (opY->affectedNode()->type() != NodeType::Directory || opX == opY || opX->targetSide() != opY->targetSide()) {
                continue;
            }

            const auto nodeY = opY->affectedNode();
            LOG_IF_FAIL(nodeY)

            if (!Utility::isDescendantOrEqual(nodeDestinationPathX, nodeY->getPath())) continue;
            if (!Utility::isDescendantOrEqual(nodeY->moveOriginInfos().path(), nodeOriginPathX)) continue;

            moveFirstAfterSecond(opX, opY);
        }
    }
    LOG_SYNCPAL_DEBUG(_logger, "End fixMoveBeforeMoveHierarchyFlip");
}

std::optional<SyncOperationList> OperationSorterWorker::fixImpossibleFirstMoveOp() {
    if (_syncPal->_syncOps->isEmpty()) {
        return std::nullopt;
    }

    const auto firstOp = _syncPal->_syncOps->getOp(_syncPal->_syncOps->opSortedList().front());
    LOG_IF_FAIL(firstOp)
    const auto node = firstOp->affectedNode();
    LOG_IF_FAIL(node)
    // Check if firstOp is move-directory operation.
    if (firstOp->type() != OperationType::Move || node->type() != NodeType::Directory) {
        return std::nullopt; // firstOp is possible
    }

    // firstOp is an impossible move if dest starts with source + "/".

    if (!Utility::isDescendantOrEqual(node->getPath(), node->moveOriginInfos().path())) {
        return std::nullopt; // firstOp is possible
    }

    const auto correspondingDestinationParentNode = correspondingNodeInOtherTree(node->parentNode());
    const auto correspondingSourceNode = correspondingNodeInOtherTree(node);
    if (correspondingDestinationParentNode == nullptr || correspondingSourceNode == nullptr) {
        LOGW_SYNCPAL_ERROR(_logger, L"Missing corresponding nodes for node " << Utility::formatSyncName(node->name()) << L" ("
                                                                             << Utility::s2ws(*node->id()) << L")");
        return std::nullopt; // Should never happen
    }

    std::list<std::shared_ptr<Node>> moveDirectoryList;
    // Traverse update tree, starting from the corresponding destination parent directory node, up to the corresponding source
    // node.
    std::shared_ptr<Node> tmpNode = correspondingDestinationParentNode;
    while (tmpNode->parentNode() != nullptr && tmpNode != correspondingSourceNode) {
        tmpNode = tmpNode->parentNode();
        // Storing all move-directory nodes
        if (tmpNode->type() == NodeType::Directory && tmpNode->hasChangeEvent(OperationType::Move)) {
            moveDirectoryList.push_back(tmpNode);
        }
    }

    // Find the operation in moveDirectoryList which come first in _sortedOps
    std::unordered_map<UniqueId, int32_t> opIdToIndexMap;
    _syncPal->_syncOps->getOpIdToIndexMap(opIdToIndexMap, OperationType::Move);

    int32_t lowestIndex = INT32_MAX;
    SyncOpPtr selectedOp = nullptr;
    for (const auto &n: moveDirectoryList) {
        for (const auto opIds = _syncPal->_syncOps->getOpIdsFromNodeId(*n->id()); const auto opId: opIds) {
            const auto op = _syncPal->_syncOps->getOp(opId);
            LOG_IF_FAIL(op)
            if (op->type() != OperationType::Move) continue;

            if (const auto currentIndex = opIdToIndexMap[opId]; currentIndex < lowestIndex) {
                lowestIndex = currentIndex;
                selectedOp = op;
            }
        }
    }

    // Make a list of all operations on target replica up to selectedOp
    const auto targetReplica = correspondingDestinationParentNode->side();
    SyncOperationList reshuffledOps;
    for (const auto &opId: _syncPal->_syncOps->opSortedList()) {
        const auto op = _syncPal->_syncOps->getOp(opId);
        LOG_IF_FAIL(op)
        // Include selectedOp
        if (op == selectedOp) {
            (void) reshuffledOps.pushOp(op);
            break;
        }
        // Operations that affect the targetReplica or both (omit flag)
        if (op->affectedNode()->side() == targetReplica || op->omit()) {
            (void) reshuffledOps.pushOp(op);
        }
    }

    return reshuffledOps;
}

bool OperationSorterWorker::breakCycle(SyncOperationList &cycle, const SyncOpPtr &renameResolutionOp) {
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
    const auto correspondingNode = correspondingNodeInOtherTree(matchOp->affectedNode());
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
    LOGW_SYNCPAL_INFO(_logger, L"Breaking cycle by renaming temporarily item " << SyncName2WStr(correspondingNode->name())
                                                                               << L" to "
                                                                               << SyncName2WStr(renameResolutionOp->newName()));
    return true;
}

void OperationSorterWorker::moveFirstAfterSecond(const SyncOpPtr &opFirst, const SyncOpPtr &opSecond) {
    std::list<UniqueId>::const_iterator firstIt;
    std::list<UniqueId>::const_iterator secondIt;
    bool firstFound = false;
    for (auto it = _syncPal->_syncOps->opSortedList().begin(); it != _syncPal->_syncOps->opSortedList().end(); ++it) {
        const auto op = _syncPal->_syncOps->getOp(*it);
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
                                                      << Utility::formatSyncName(opFirst->affectedNode()->name())
                                                      << L") moved after operation " << opSecond->id() << L" ("
                                                      << opSecond->type() << L" "
                                                      << Utility::formatSyncName(opSecond->affectedNode()->name()) << L")");
        }

        _syncPal->_syncOps->deleteOp(firstIt);
        _syncPal->_syncOps->insertOp(++secondIt, opFirst);
        _hasOrderChanged = true;
        addPairToReorderings(opFirst, opSecond);
    }
}

void OperationSorterWorker::addPairToReorderings(const SyncOpPtr &op1, const SyncOpPtr &op2 /*opOnFirstDepends*/) {
    const auto pair = std::make_pair(op1, op2);
    for (auto const &opPair: _reorderings) {
        // Check that the pair does not already exist in the list
        if (opPair == pair) {
            return; // Usefull?
        }
    }
    _reorderings.push_back(pair);
}

bool OperationSorterWorker::getIdFromDb(const ReplicaSide side, const SyncPath &path, NodeId &id) const {
    bool found = false;
    std::optional<NodeId> tmpId;

    if (!_syncPal->syncDb()->id(side, path, tmpId, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::id");
        return false;
    }

    if (!found || !tmpId) {
        LOGW_SYNCPAL_WARN(_logger, L"Node not found for " << Utility::formatSyncPath(path));
        return false;
    }

    LOG_IF_FAIL(tmpId)
    id = *tmpId;

    return true;
}

} // namespace KDC
