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

#include "conflictresolverworker.h"
#include "reconciliation/platform_inconsistency_checker/platforminconsistencycheckerutility.h"
#include "libcommonserver/utility/utility.h"

namespace KDC {

static const SyncPath rescueFolderName = ".rescueFolder";

ConflictResolverWorker::ConflictResolverWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name,
                                               const std::string &shortName) : OperationProcessor(syncPal, name, shortName) {}

void ConflictResolverWorker::execute() {
    LOG_SYNCPAL_DEBUG(_logger, "Worker started: name=" << name());

    auto exitCode = ExitCode::Ok;
    _syncPal->_syncOps->startUpdate();

    do {
        bool continueSolving = false;
        exitCode = generateOperations(_syncPal->_conflictQueue->top(), continueSolving);

        if (continueSolving) {
            _syncPal->_conflictQueue->pop();
        } else {
            _syncPal->_conflictQueue->clear();
            break;
        }
    } while (!_syncPal->_conflictQueue->empty() || exitCode != ExitCode::Ok);

    // The sync must be restarted after the execution of the operations that resolve the conflict
    _syncPal->setRestart(true);

    LOG_SYNCPAL_DEBUG(_logger, "Worker stopped: name=" << name());
    setDone(exitCode);
}

ExitCode ConflictResolverWorker::generateOperations(const Conflict &conflict, bool &continueSolving) {
    LOGW_SYNCPAL_INFO(_logger, L"Solving " << conflict.type() << L" conflict for items " << SyncName2WStr(conflict.node()->name())
                                           << L" (" << Utility::s2ws(*conflict.node()->id()) << L") and "
                                           << SyncName2WStr(conflict.correspondingNode()->name()) << L" ("
                                           << Utility::s2ws(*conflict.correspondingNode()->id()) << L")");

    continueSolving = false;
    auto res = handleConflictOnDehydratedPlaceholder(conflict, continueSolving);
    if (res != ExitCode::Ok) {
        return res;
    }
    if (continueSolving) return ExitCode::Ok;

    switch (conflict.type()) {
        case ConflictType::CreateCreate:
        case ConflictType::EditEdit: {
            res = generateLocalRenameOperation(conflict, continueSolving);
            break;
        }
        case ConflictType::EditDelete: {
            res = generateEditDeleteConflictOperation(conflict, continueSolving);
            break;
        }
        case ConflictType::MoveDelete: {
            res = generateMoveDeleteConflictOperation(conflict, continueSolving);
            break;
        }
        case ConflictType::MoveParentDelete: {
            res = generateMoveParentDeleteConflictOperation(conflict);
            break;
        }
        case ConflictType::CreateParentDelete: {
            res = generateCreateParentDeleteConflictOperation(conflict);
            break;
        }
        case ConflictType::MoveMoveSource:
        case ConflictType::MoveMoveDest:
        case ConflictType::MoveMoveCycle:
        case ConflictType::MoveCreate: {
            res = generateUndoMoveOperation(conflict, getLoserNode(conflict));
            break;
        }
        default: {
            LOG_SYNCPAL_WARN(_logger, "Unknown conflict type: " << conflict.type());
            res = ExitCode::DataError;
            break;
        }
    }

    return res;
}

ExitCode ConflictResolverWorker::handleConflictOnDehydratedPlaceholder(const Conflict &conflict, bool &continueSolving) {
    if (_syncPal->vfsMode() != VirtualFileMode::Off) {
        if (const auto localNode = conflict.localNode(); localNode->type() == NodeType::File) {
            VfsStatus vfsStatus;
            const auto moveNodeRelativePath = localNode->getPath();
            if (const auto exitInfo = _syncPal->vfs()->status(_syncPal->syncInfo().localPath / moveNodeRelativePath, vfsStatus);
                exitInfo.code() != ExitCode::Ok) {
                LOGW_SYNCPAL_WARN(_logger,
                                  L"Failed to get VFS status for file " << Utility::formatSyncPath(moveNodeRelativePath));
                return exitInfo;
            }
            if (vfsStatus.isPlaceholder && !vfsStatus.isHydrated && !vfsStatus.isSyncing) {
                // Local node is a dehydrated placeholder, remove it so it will be detected as new on next sync
                const auto op = std::make_shared<SyncOperation>();
                op->setType(OperationType::Delete);
                op->setAffectedNode(conflict.remoteNode());
                op->setCorrespondingNode(localNode);

                op->setTargetSide(localNode->side());
                op->setConflict(conflict);
                op->setIsDehydratedPlaceholder(true);
                LOGW_SYNCPAL_INFO(_logger, L"The conflict occurred on a dehydrated placeholder. Operation "
                                                   << op->type() << L" to be propagated on " << op->targetSide()
                                                   << L" replica for item " << SyncName2WStr(localNode->name()) << L" ("
                                                   << Utility::s2ws(*localNode->id()) << L")");

                _syncPal->_syncOps->pushOp(op);
                continueSolving = true;
            }
        }
    }
    return ExitCode::Ok;
}

ExitCode ConflictResolverWorker::generateLocalRenameOperation(const Conflict &conflict, bool &continueSolving) {
    // Rename the file on the local replica and remove it from DB
    const auto op = std::make_shared<SyncOperation>();
    op->setType(OperationType::Move);
    op->setAffectedNode(conflict.remoteNode());
    op->setCorrespondingNode(conflict.localNode());
    op->setTargetSide(ReplicaSide::Local);

    SyncName newName;
    if (!generateConflictedName(conflict.localNode(), newName)) {
        // New path is too long, move file to the root directory
        // TODO : we need to discuss this behavior again!
        op->setNewParentNode(_syncPal->updateTree(ReplicaSide::Local)->rootNode());
    }
    op->setNewName(newName);
    op->setConflict(conflict);

    LOGW_SYNCPAL_INFO(_logger, L"Operation " << op->type() << L" to be propagated on " << op->targetSide()
                                             << L" replica for item " << SyncName2WStr(op->correspondingNode()->name()) << L" ("
                                             << Utility::s2ws(*op->correspondingNode()->id()) << L")");

    _syncPal->_syncOps->pushOp(op);
    continueSolving = true; // solve them all in the same sync
    return ExitCode::Ok;
}

ExitCode ConflictResolverWorker::generateEditDeleteConflictOperation(const Conflict &conflict, bool &continueSolving) {
    const auto deleteNode =
            conflict.node()->hasChangeEvent(OperationType::Delete) ? conflict.node() : conflict.correspondingNode();
    const auto editNode = conflict.node()->hasChangeEvent(OperationType::Edit) ? conflict.node() : conflict.correspondingNode();
    if (deleteNode->parentNode()->hasChangeEvent(OperationType::Delete)) {
        // The move operation happen in a deleted directory.
        // Move the items that have been modified locally (if any).
        // Delete operations will be propagated on next sync.
        rescueModifiedLocalNodes(conflict, editNode);
        continueSolving = true;
    } else {
        // Edit operation wins.
        // Delete the edit node from DB
        // This will cause the file to be detected as new in the next sync iteration, thus it will be restored
        const auto deleteOp = std::make_shared<SyncOperation>();
        deleteOp->setType(OperationType::Delete);
        deleteOp->setAffectedNode(editNode);
        deleteOp->setCorrespondingNode(deleteNode);
        deleteOp->setTargetSide(deleteNode->side());
        deleteOp->setOmit(true); // Target side does not matter when we remove only in DB
        deleteOp->setConflict(conflict);

        LOGW_SYNCPAL_INFO(_logger, L"Operation " << deleteOp->type() << L" to be propagated in DB only for item "
                                                 << SyncName2WStr(deleteOp->correspondingNode()->name()) << L" ("
                                                 << Utility::s2ws(*deleteOp->correspondingNode()->id()) << L")");

        _syncPal->_syncOps->pushOp(deleteOp);
    }
    return ExitCode::Ok;
}

ExitCode ConflictResolverWorker::generateMoveDeleteConflictOperation(const Conflict &conflict, bool &continueSolving) {
    const auto deleteNode =
            conflict.node()->hasChangeEvent(OperationType::Delete) ? conflict.node() : conflict.correspondingNode();
    const auto moveNode = conflict.node()->hasChangeEvent(OperationType::Move) ? conflict.node() : conflict.correspondingNode();

    if (const auto correspondingMoveNodeParent = correspondingNodeDirect(moveNode->parentNode());
        correspondingMoveNodeParent && correspondingMoveNodeParent->hasChangeEvent(OperationType::Delete) &&
        _syncPal->_conflictQueue->hasConflict(ConflictType::MoveParentDelete)) {
        // If the move operation happen within a directory that was deleted on the other replica,
        // therefor, we ignore the Move-Delete conflict
        // This conflict will be handled as Move-ParentDelete conflict
        LOG_SYNCPAL_INFO(_logger, "Move-Delete conflict ignored because it will be solved by solving Move-ParentDelete conflict");
        continueSolving = true;
        return ExitCode::Ok;
    }

    // Otherwise we need to check if any item has been modified locally
    if (moveNode->side() == ReplicaSide::Local) {
        rescueModifiedLocalNodes(conflict, moveNode);
    }

    // Then propagate the delete operation if the item is a directory or if the file has not been modified locally
    if (moveNode->type() == NodeType::Directory || moveNode->status() != NodeStatus::ConflictOpGenerated) {
        const auto deleteOp = std::make_shared<SyncOperation>();
        deleteOp->setType(OperationType::Delete);
        deleteOp->setAffectedNode(deleteNode);
        deleteOp->setCorrespondingNode(moveNode);
        deleteOp->setTargetSide(moveNode->side());
        deleteOp->setConflict(conflict);

        LOGW_SYNCPAL_INFO(_logger, L"Operation " << deleteOp->type() << L" to be propagated for item "
                                                 << SyncName2WStr(deleteOp->correspondingNode()->name()) << L" ("
                                                 << Utility::s2ws(*deleteOp->correspondingNode()->id()) << L")");


        _syncPal->_syncOps->pushOp(deleteOp);
    }

    return ExitCode::Ok;
}

ExitCode ConflictResolverWorker::generateMoveParentDeleteConflictOperation(const Conflict &conflict) {
    const auto deleteNode =
            conflict.node()->hasChangeEvent(OperationType::Delete) ? conflict.node() : conflict.correspondingNode();
    const auto moveNode = conflict.node()->hasChangeEvent(OperationType::Move) ? conflict.node() : conflict.correspondingNode();

    // We need to check if any item has been modified locally
    rescueModifiedLocalNodes(conflict, moveNode);

    // Then propagate the delete operation (any modified children should have already been rescued since EditDelete and MoveDelete
    // conflicts have higher priority)
    const auto deleteOp = std::make_shared<SyncOperation>();
    deleteOp->setType(OperationType::Delete);
    deleteOp->setAffectedNode(deleteNode);
    const auto &correspondingNode = correspondingNodeInOtherTree(deleteNode);
    deleteOp->setCorrespondingNode(correspondingNode);
    deleteOp->setTargetSide(correspondingNode->side());
    deleteOp->setConflict(conflict);

    LOGW_SYNCPAL_INFO(_logger, L"Operation " << deleteOp->type() << L" to be propagated for item "
                                             << SyncName2WStr(deleteOp->correspondingNode()->name()) << L" ("
                                             << Utility::s2ws(*deleteOp->correspondingNode()->id()) << L")");

    _syncPal->_syncOps->pushOp(deleteOp);
    return ExitCode::Ok;
}

ExitCode ConflictResolverWorker::generateCreateParentDeleteConflictOperation(const Conflict &conflict) {
    const auto deleteNode =
            conflict.node()->hasChangeEvent(OperationType::Delete) ? conflict.node() : conflict.correspondingNode();
    const auto correspondingNode = correspondingNodeInOtherTree(deleteNode);

    // First rescue modified items
    rescueModifiedLocalNodes(conflict, correspondingNode);

    // Then propagate delete operation
    const auto deleteOp = std::make_shared<SyncOperation>();
    deleteOp->setType(OperationType::Delete);
    deleteOp->setAffectedNode(deleteNode);

    deleteOp->setCorrespondingNode(correspondingNode);
    deleteOp->setTargetSide(correspondingNode->side());
    deleteOp->setConflict(conflict);

    LOGW_SYNCPAL_INFO(_logger, L"Operation " << deleteOp->type() << L" to be propagated on " << deleteOp->targetSide()
                                             << L" replica for item " << SyncName2WStr(deleteNode->name()) << L" ("
                                             << Utility::s2ws(*deleteNode->id()) << L")");

    _syncPal->_syncOps->pushOp(deleteOp);
    return ExitCode::Ok;
}

ExitCode ConflictResolverWorker::generateUndoMoveOperation(const Conflict &conflict, const std::shared_ptr<Node> &loserNode) {
    // Undo move on the loser replica
    const auto moveOp = std::make_shared<SyncOperation>();
    if (const auto res = undoMove(loserNode, moveOp); res != ExitCode::Ok) {
        return res;
    }
    moveOp->setConflict(conflict);
    LOGW_SYNCPAL_INFO(_logger, L"Operation " << moveOp->type() << L" to be propagated on " << moveOp->targetSide()
                                             << L" replica for item " << SyncName2WStr(moveOp->correspondingNode()->name())
                                             << L" (" << Utility::s2ws(*moveOp->correspondingNode()->id()) << L")");
    _syncPal->_syncOps->pushOp(moveOp);
    return ExitCode::Ok;
}

void ConflictResolverWorker::rescueModifiedLocalNodes(const Conflict &conflict, const std::shared_ptr<Node> &parentNode) {
    if (parentNode->side() != ReplicaSide::Local) return;

    for (const auto &[_, child]: parentNode->children()) {
        rescueModifiedLocalNodes(conflict, child);
        generateRescueOperation(conflict, child);
    }
}

void ConflictResolverWorker::generateRescueOperation(const Conflict &conflict, const std::shared_ptr<Node> &node) {
    if (node->status() == NodeStatus::ConflictOpGenerated) return; // Already processed
    if (!node->hasChangeEvent(OperationType::Edit) && !node->hasChangeEvent(OperationType::Create)) return;

    createRescueFolderIfNeeded();

    // Move the deleted node to the rescue folder
    const auto moveOp = std::make_shared<SyncOperation>();
    moveOp->setType(OperationType::Move);
    moveOp->setAffectedNode(node);
    moveOp->setCorrespondingNode(node);
    moveOp->setTargetSide(ReplicaSide::Local);
    // TO DO : check for existing names
    // SyncName newName;
    // (void) generateConflictedName(conflict.localNode(), newName);
    // moveOp->setNewName(newName);
    moveOp->setRelativeOriginPath(node->getPath());
    moveOp->setRelativeDestinationPath(rescueFolderName / node->name());
    moveOp->setConflict(conflict);
    node->setStatus(NodeStatus::ConflictOpGenerated);

    LOGW_SYNCPAL_INFO(_logger, L"Operation " << moveOp->type() << L" to be propagated on " << moveOp->targetSide()
                                             << L" replica from " << Utility::formatSyncPath(moveOp->relativeOriginPath())
                                             << L" to " << Utility::formatSyncPath(moveOp->relativeDestinationPath()) << L" ("
                                             << Utility::s2ws(*moveOp->correspondingNode()->id())
                                             << L") because it has been modified locally.");

    _syncPal->_syncOps->pushOp(moveOp);
}

void ConflictResolverWorker::createRescueFolderIfNeeded() {
    // TODO : improve that. Probably move it to Executor
    if (_rescueFolderExists) return;
    if (auto ioError = IoError::Unknown;
        !IoHelper::createDirectory(_syncPal->localPath() / rescueFolderName, ioError) || ioError != IoError::Success) {
        LOGW_SYNCPAL_INFO(_logger, L"Pas de bol...");
        return;
    }
    _rescueFolderExists = true;
}

std::shared_ptr<Node> ConflictResolverWorker::getLoserNode(const Conflict &conflict) {
    auto loserNode = conflict.localNode();
    if (!loserNode->hasChangeEvent(OperationType::Move)) {
        loserNode = conflict.remoteNode();
    }

    // Check if this node is a registered orphan
    if (conflict.type() == ConflictType::MoveMoveSource && _registeredOrphans.contains(*conflict.node()->idb())) {
        loserNode = _registeredOrphans.find(*conflict.node()->idb())->second == ReplicaSide::Local ? conflict.remoteNode()
                                                                                                   : conflict.localNode();
        LOGW_SYNCPAL_INFO(_logger, L"Undoing move operation on orphan node " << SyncName2WStr(loserNode->name()));
    }

    return loserNode;
}

bool ConflictResolverWorker::generateConflictedName(const std::shared_ptr<Node> &node, SyncName &newName,
                                                    const bool isOrphanNode /*= false*/) const {
    const auto absoluteLocalFilePath = _syncPal->localPath() / node->getPath();
    newName = PlatformInconsistencyCheckerUtility::instance()->generateNewValidName(
            absoluteLocalFilePath, isOrphanNode ? PlatformInconsistencyCheckerUtility::SuffixType::Orphan
                                                : PlatformInconsistencyCheckerUtility::SuffixType::Conflict);

    // Check path size
    if (const auto pathSize = absoluteLocalFilePath.parent_path().native().size() + 1 + newName.size();
        PlatformInconsistencyCheckerUtility::instance()->isPathTooLong(pathSize)) {
        // Path is now too long, file needs to be moved to root directory
        return false;
    }

    return true;
}

void ConflictResolverWorker::findAllChildNodes(const std::shared_ptr<Node> &parentNode,
                                               std::unordered_set<std::shared_ptr<Node>> &children) {
    for (auto const &[_, childNode]: parentNode->children()) {
        if (childNode->type() == NodeType::Directory) {
            findAllChildNodes(childNode, children);
        }
        (void) children.insert(childNode);
    }
}

ExitCode ConflictResolverWorker::findAllChildNodeIdsFromDb(const std::shared_ptr<Node> &parentNode,
                                                           std::unordered_set<DbNodeId> &childrenDbIds) {
    std::vector<NodeId> nodeIds;
    bool found = false;
    if (!_syncPal->_syncDb->ids(parentNode->side(), nodeIds, found)) {
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "Failed to retrieve node IDs in DB");
        return ExitCode::DataError;
    }

    for (const auto &nodeId: nodeIds) {
        if (nodeId == *parentNode->id()) {
            continue;
        }

        bool isAncestor = false;
        if (!_syncPal->_syncDb->ancestor(parentNode->side(), *parentNode->id(), nodeId, isAncestor, found)) {
            return ExitCode::DbError;
        }
        if (!found) {
            LOG_SYNCPAL_WARN(_logger, "Failed to retrieve ancestor for node ID: " << nodeId << " in DB");
            return ExitCode::DataError;
        }

        if (isAncestor) {
            DbNodeId dbNodeId = 0;
            if (!_syncPal->_syncDb->dbId(parentNode->side(), nodeId, dbNodeId, found)) {
                return ExitCode::DbError;
            }
            if (!found) {
                LOG_SYNCPAL_WARN(_logger, "Failed to retrieve DB node ID for node ID=" << nodeId);
                return ExitCode::DataError;
            }

            (void) childrenDbIds.insert(dbNodeId);
        }
    }
    return ExitCode::Ok;
}

ExitCode ConflictResolverWorker::undoMove(const std::shared_ptr<Node> &moveNode, const SyncOpPtr &moveOp) {
    if (!moveNode->moveOrigin().has_value()) {
        LOG_SYNCPAL_WARN(_logger, "Failed to retrieve origin parent path");
        return ExitCode::DataError;
    }

    const auto updateTree = _syncPal->updateTree(moveNode->side());
    const auto originParentNode = updateTree->getNodeByPath(moveNode->moveOrigin()->parent_path());
    auto originPath = moveNode->moveOrigin();
    bool undoPossible = true;

    if (!originParentNode) {
        LOG_SYNCPAL_WARN(_logger, "Failed to retrieve origin parent node");
        return ExitCode::DataError;
    }

    if (isABelowB(originParentNode, moveNode) || originParentNode->hasChangeEvent(OperationType::Delete)) {
        undoPossible = false;
    } else {
        if (const auto potentialOriginNode =
                    originParentNode->getChildExcept(originPath->filename().native(), OperationType::Delete);
            potentialOriginNode && (potentialOriginNode->hasChangeEvent(OperationType::Create) ||
                                    potentialOriginNode->hasChangeEvent(OperationType::Move))) {
            undoPossible = false;
        }
    }

    if (undoPossible) {
        moveOp->setNewParentNode(originParentNode);
        moveOp->setNewName(originPath->filename().native());
    } else {
        // We cannot undo the move operation, so the file is moved under the root node instead.
        moveOp->setNewParentNode(_syncPal->updateTree(moveNode->side())->rootNode());
        SyncName newName;
        (void) generateConflictedName(moveNode, newName);
        moveOp->setNewName(newName);
    }

    moveOp->setType(OperationType::Move);
    const auto correspondingNode = correspondingNodeInOtherTree(moveNode);
    moveOp->setAffectedNode(correspondingNode);
    moveOp->setCorrespondingNode(moveNode);
    moveOp->setTargetSide(moveNode->side());

    return ExitCode::Ok;
}

} // namespace KDC
