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
#include "utility/timerutility.h"

namespace KDC {

OperationSorterWorker::OperationSorterWorker(const std::shared_ptr<SyncPal> &syncPal, const std::string &name,
                                             const std::string &shortName) :
    OperationProcessor(syncPal, name, shortName),
    _filter(syncPal->_syncOps->allOps()) {}

void OperationSorterWorker::execute() {
    LOG_SYNCPAL_DEBUG(_logger, "Worker started: name=" << name());

    const TimerUtility timer;

    _reorderings.clear();
    _syncPal->_syncOps->startUpdate();
    _filter.filterOperations();
    sortOperations();

    LOG_SYNCPAL_INFO(_logger, "Operation sorting finished in: " << timer.elapsed<DoubleSeconds>().count() << "s");

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
    for (const auto &[op1, op2]: _filter.fixDeleteBeforeMoveCandidates()) {
        const auto [deleteOp, moveOp] = extractOpsByType(OperationType::Delete, OperationType::Move, op1, op2);

        const auto deleteNode = deleteOp->affectedNode();
        LOG_IF_FAIL(deleteNode)
        NodeId deleteNodeParentId;
        if (!getIdFromDb(deleteNode->side(), deleteNode->getPath().parent_path(), deleteNodeParentId)) {
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

        if (deleteNode->normalizedName() == moveNode->normalizedName()) {
            // move only if moveOp is before deleteOp
            moveFirstAfterSecond(moveOp, deleteOp);
        }
    }
    LOG_SYNCPAL_DEBUG(_logger, "End fixDeleteBeforeMove");
}

void OperationSorterWorker::fixMoveBeforeCreate() {
    LOG_SYNCPAL_DEBUG(_logger, "Start fixMoveBeforeCreate");
    for (const auto &[op1, op2]: _filter.fixMoveBeforeCreateCandidates()) {
        const auto [moveOp, createOp] = extractOpsByType(OperationType::Move, OperationType::Create, op1, op2);

        const auto moveNode = moveOp->affectedNode();
        LOG_IF_FAIL(moveNode)
        NodeId moveNodeOriginParentId;
        if (!getIdFromDb(moveNode->side(), moveNode->moveOriginInfos().path().parent_path(), moveNodeOriginParentId)) {
            continue;
        }

        const auto createNode = createOp->affectedNode();
        LOG_IF_FAIL(createNode)
        const auto createParentNode = createNode->parentNode();
        LOG_IF_FAIL(createParentNode)
        if (!createParentNode->id().has_value()) {
            LOGW_SYNCPAL_WARN(_logger, L"Node without id: " << SyncName2WStr(createParentNode->normalizedName()));
            continue;
        }

        if (const auto createParentId = *createParentNode->id(); moveNodeOriginParentId != createParentId) {
            continue;
        }

        if (moveNode->moveOriginInfos().normalizedPath().filename() == createNode->normalizedName()) {
            // move only if createOp is before moveOp
            moveFirstAfterSecond(createOp, moveOp);
        }
    }
    LOG_SYNCPAL_DEBUG(_logger, "End fixMoveBeforeCreate");
}

void OperationSorterWorker::fixMoveBeforeDelete() {
    LOG_SYNCPAL_DEBUG(_logger, "Start fixMoveBeforeDelete");
    for (const auto &[op1, op2]: _filter.fixMoveBeforeDeleteCandidates()) {
        const auto [moveOp, deleteOp] = extractOpsByType(OperationType::Move, OperationType::Delete, op1, op2);
        // move only if deleteOp is before moveOp
        moveFirstAfterSecond(deleteOp, moveOp);
    }
    LOG_SYNCPAL_DEBUG(_logger, "End fixMoveBeforeDelete");
}

void OperationSorterWorker::fixCreateBeforeMove() {
    LOG_SYNCPAL_DEBUG(_logger, "Start fixCreateBeforeMove");
    for (const auto &[op1, op2]: _filter.fixCreateBeforeMoveCandidates()) {
        const auto [createOp, moveOp] = extractOpsByType(OperationType::Create, OperationType::Move, op1, op2);
        // move only if moveOp is before createOp
        moveFirstAfterSecond(moveOp, createOp);
    }
    LOG_SYNCPAL_DEBUG(_logger, "End fixCreateBeforeMove");
}

void OperationSorterWorker::fixDeleteBeforeCreate() {
    LOG_SYNCPAL_DEBUG(_logger, "Start fixDeleteBeforeCreate");
    for (const auto &[op1, op2]: _filter.fixDeleteBeforeCreateCandidates()) {
        const auto [deleteOp, createOp] = extractOpsByType(OperationType::Delete, OperationType::Create, op1, op2);

        const auto deleteNode = deleteOp->affectedNode();
        LOG_IF_FAIL(deleteNode)
        NodeId deleteNodeParentId;
        if (!getIdFromDb(deleteNode->side(), deleteNode->getPath().parent_path(), deleteNodeParentId)) {
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

        if (createNode->normalizedName() == deleteNode->normalizedName()) {
            // move only if createOp is before deleteOp
            moveFirstAfterSecond(createOp, deleteOp);
        }
    }
    LOG_SYNCPAL_DEBUG(_logger, "End fixDeleteBeforeCreate");
}

void OperationSorterWorker::fixMoveBeforeMoveOccupied() {
    LOG_SYNCPAL_DEBUG(_logger, "Start fixMoveBeforeMoveOccupied");
    for (const auto &[moveOp, otherMoveOp]: _filter.fixMoveBeforeMoveOccupiedCandidates()) {
        const auto node = moveOp->affectedNode();
        LOG_IF_FAIL(node)
        if (!node->parentNode()) {
            continue;
        }

        const auto otherNode = otherMoveOp->affectedNode();
        LOG_IF_FAIL(otherNode)

        NodeId otherNodeOriginParentId;
        if (!getIdFromDb(otherNode->side(), otherNode->moveOriginInfos().path().parent_path(), otherNodeOriginParentId)) {
            continue;
        }

        const auto nodeParentId = node->parentNode()->id();
        const auto otherNodeOriginPath = otherNode->moveOriginInfos().normalizedPath();
        if (nodeParentId == otherNodeOriginParentId && node->normalizedName() == otherNodeOriginPath.filename()) {
            // move only if op is before otherOp
            moveFirstAfterSecond(moveOp, otherMoveOp);
        }
    }
    LOG_SYNCPAL_DEBUG(_logger, "End fixMoveBeforeMoveOccupied");
}


void OperationSorterWorker::fixCreateBeforeCreate() {
    LOG_SYNCPAL_DEBUG(_logger, "Start fixCreateBeforeCreate");
    // The method described in the thesis slow. Therefore, we use a std::priority_queue to efficiently sort the
    // operations before moving them in the sorted list.
    CreatePairQueue opsToMove;

    std::unordered_map<UniqueId, int32_t> opIdToIndexMap;
    _syncPal->_syncOps->getOpIdToIndexMap(opIdToIndexMap, OperationType::Create);

    for (const auto &opId: _syncPal->_syncOps->opSortedList()) {
        SyncOpPtr createOp = _syncPal->_syncOps->getOp(opId);
        LOG_IF_FAIL(createOp)
        if (createOp->type() != OperationType::Create) continue;

        if (int32_t depth = 0; auto ancestorOpWithHighestIndex = getAncestorOpWithHighestIndex(opIdToIndexMap, createOp, depth))
            opsToMove.emplace(createOp, depth, ancestorOpWithHighestIndex);
    }

    while (!opsToMove.empty()) {
        const auto createPair = opsToMove.top();
        LOGW_SYNCPAL_DEBUG(_logger, L"op: " << Utility::formatSyncName(createPair.op->affectedNode()->name()) << L", ancestorOp: "
                                            << Utility::formatSyncName(createPair.ancestorOp->affectedNode()->name())
                                            << L", depth; " << createPair.opNodeDepth);
        moveFirstAfterSecond(createPair.op, createPair.ancestorOp);
        opsToMove.pop();
    }
    LOG_SYNCPAL_DEBUG(_logger, "End fixCreateBeforeCreate");
}

SyncOpPtr OperationSorterWorker::getAncestorOpWithHighestIndex(const std::unordered_map<UniqueId, int32_t> &opIdToIndexMap,
                                                               const SyncOpPtr &op, int32_t &depth) const {
    SyncOpPtr ancestorOpWithHighestIndex = nullptr;
    int32_t highestIndex = opIdToIndexMap.at(op->id());

    const auto node = op->affectedNode();
    std::shared_ptr<const Node> ancestorNode = node->parentNode();
    depth = ancestorNode ? 1 : 0;

    while (ancestorNode && ancestorNode != _syncPal->updateTree(ancestorNode->side())->rootNode()) {
        for (const auto parentOpIdList = _syncPal->_syncOps->getOpIdsFromNodeId(*ancestorNode->id());
             const auto &parentOpId: parentOpIdList) {
            const auto parentOp = _syncPal->_syncOps->getOp(parentOpId);
            if (parentOp->type() != OperationType::Create) {
                continue;
            }

            // Check whether the index of `parentOp` is greater than the index of `op`.
            if (opIdToIndexMap.at(parentOpId) > highestIndex) {
                highestIndex = opIdToIndexMap.at(parentOpId);
                ancestorOpWithHighestIndex = parentOp;
            }
        }
        ancestorNode = ancestorNode->parentNode();
        ++depth;
    }

    return ancestorOpWithHighestIndex;
}

void OperationSorterWorker::fixEditBeforeMove() {
    LOG_SYNCPAL_DEBUG(_logger, "Start fixEditBeforeMove");
    for (const auto &[_, opList]: _filter.fixEditBeforeMoveCandidates()) {
        if (opList.size() < 2) {
            continue; // We are looking for nodes affected by both EDIT and MOVE operations
        }
        // opList contains one MOVE operation and all the EDIT operations that are under the node affected by the MOVE

        auto moveOpIt =
                std::find_if(opList.begin(), opList.end(), [](const SyncOpPtr &op) { return op->type() == OperationType::Move; });
        if (moveOpIt == opList.end()) {
            LOG_IF_FAIL("fixEditBeforeMove: No MOVE operation found in the list of operations. This should not happen." && false)
            continue;
        }

        // Ensure that all EDIT operations under the node affected by the MOVE operation are executed after the MOVE operation
        // Otherwise, this may cause issues with path changes between the edit job generation and its execution (due to the
        // MOVE) in the executor step.
        for (auto &op: opList) {
            if (op->type() == OperationType::Edit) {
                moveFirstAfterSecond(op, *moveOpIt);
            }
        }
    }
    LOG_SYNCPAL_DEBUG(_logger, "End fixEditBeforeMove");
}

void OperationSorterWorker::fixMoveBeforeMoveHierarchyFlip() {
    LOG_SYNCPAL_DEBUG(_logger, "Start fixMoveBeforeMoveHierarchyFlip");
    for (const auto &[op, otherOp]: _filter.fixMoveBeforeMoveHierarchyFlipCandidates()) {
        // move only if op is before opOtherOp
        moveFirstAfterSecond(op, otherOp);
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
    const auto path = node->getPath();
    SyncPath normalizedPath;
    if (!Utility::normalizedSyncPath(path, normalizedPath)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Failed to normalize: " << Utility::formatSyncPath(path));
        normalizedPath = path;
    }

    if (!CommonUtility::isDescendantOrEqual(normalizedPath, node->moveOriginInfos().normalizedPath())) {
        return std::nullopt; // firstOp is possible
    }

    const auto correspondingDestinationParentNode = correspondingNodeInOtherTree(node->parentNode());
    const auto correspondingSourceNode = correspondingNodeInOtherTree(node);
    if (correspondingDestinationParentNode == nullptr || correspondingSourceNode == nullptr) {
        LOGW_SYNCPAL_ERROR(_logger, L"Missing corresponding nodes for node " << Utility::formatSyncName(node->name()) << L" ("
                                                                             << CommonUtility::s2ws(*node->id()) << L")");
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
            if (op->type() != OperationType::Move) {
                continue;
            }

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

    if (!matchOp) {
        return false;
    }

    // A cycle must contain a Delete or a Move operation
    // Generate a rename resolution operation
    renameResolutionOp->setType(OperationType::Move);
    const auto affectedNode = matchOp->affectedNode();
    affectedNode->setMoveOriginInfos({affectedNode->getPath(), affectedNode->parentNode()->id().value()});
    renameResolutionOp->setOmit(matchOp->omit());
    // Find the corresponding node of `matchOp`
    const auto correspondingNode = correspondingNodeInOtherTree(affectedNode);
    if (!correspondingNode) {
        LOG_SYNCPAL_WARN(_logger,
                         "Error in correspondingNode with id=" << *affectedNode->id() << " - idDb = " << *affectedNode->idb());
        return false;
    }
    renameResolutionOp->setAffectedNode(affectedNode);
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
    // After breaking a cycle, the update tree is not up to date anymore and needs to be rebuilt.
    _syncPal->setUpdateTreesNeedToBeCleared(true);
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

    if (!_syncPal->syncDb()->cache().id(side, path, tmpId, found)) {
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

std::pair<SyncOpPtr, SyncOpPtr> OperationSorterWorker::extractOpsByType(const OperationType type1, const OperationType type2,
                                                                        const SyncOpPtr op, const SyncOpPtr otherOp) const {
    const auto op1 = op->type() == type1 ? op : otherOp;
    const auto op2 = otherOp->type() == type2 ? otherOp : op;
    LOG_IF_FAIL(op1->type() == type1 && op2->type() == type2)
    return {op1, op2};
}

} // namespace KDC
