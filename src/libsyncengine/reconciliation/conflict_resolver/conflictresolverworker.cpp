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
#include "utility/logiffail.h"

namespace KDC {

ConflictResolverWorker::ConflictResolverWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name,
                                               const std::string &shortName) :
    OperationProcessor(syncPal, name, shortName) {}

void ConflictResolverWorker::execute() {
    LOG_SYNCPAL_DEBUG(_logger, "Worker started: name=" << name());

    auto exitCode = ExitCode::Ok;
    _syncPal->syncOps()->startUpdate();

    do {
        bool continueSolving = false;
        exitCode = generateOperations(_syncPal->conflictQueue()->top(), continueSolving);

        if (continueSolving) {
            _syncPal->conflictQueue()->pop();
        } else {
            _syncPal->conflictQueue()->clear();
            break;
        }
    } while (!_syncPal->conflictQueue()->empty() || exitCode != ExitCode::Ok);

    // The sync must be restarted after the execution of the operations that resolve the conflict
    _syncPal->setRestart(true);

    LOG_SYNCPAL_DEBUG(_logger, "Worker stopped: name=" << name());
    setDone(exitCode);
}

ExitCode ConflictResolverWorker::generateOperations(const Conflict &conflict, bool &continueSolving) {
    LOGW_SYNCPAL_INFO(_logger, L"Solving " << conflict.type() << L" conflict for items "
                                           << Utility::formatSyncName(conflict.node()->name()) << L" ("
                                           << CommonUtility::s2ws(*conflict.node()->id()) << L") and "
                                           << Utility::formatSyncName(conflict.otherNode()->name()) << L" ("
                                           << CommonUtility::s2ws(*conflict.otherNode()->id()) << L")");

    continueSolving = false;
    if (auto exitCode = handleConflictOnDehydratedPlaceholder(conflict, continueSolving); exitCode != ExitCode::Ok) {
        LOG_SYNCPAL_WARN(_logger, "Error in handleConflictOnDehydratedPlaceholder: code=" << exitCode);
        return exitCode;
    }

    if (continueSolving) return ExitCode::Ok;

    if (auto exitInfo = handleConflictOnOmittedEdit(conflict, continueSolving); !exitInfo) {
        LOG_SYNCPAL_WARN(_logger, "Error in handleConflictOnOmittedEdit: " << exitInfo);
        return exitInfo.code();
    }

    if (continueSolving) return ExitCode::Ok;

    ExitCode exitCode = ExitCode::Ok;
    switch (conflict.type()) {
        case ConflictType::MoveCreate: {
            generateMoveCreateConflictOperation(conflict, continueSolving);
            break;
        }
        case ConflictType::CreateCreate:
        case ConflictType::EditEdit: {
            exitCode = generateLocalRenameOperation(conflict, continueSolving);
            break;
        }
        case ConflictType::EditDelete: {
            exitCode = generateEditDeleteConflictOperation(conflict, continueSolving);
            break;
        }
        case ConflictType::MoveDelete: {
            exitCode = generateMoveDeleteConflictOperation(conflict, continueSolving);
            break;
        }
        case ConflictType::MoveParentDelete:
        case ConflictType::CreateParentDelete: {
            exitCode = generateParentDeleteConflictOperation(conflict);
            break;
        }
        case ConflictType::MoveMoveSource:
        case ConflictType::MoveMoveDest:
        case ConflictType::MoveMoveCycle: {
            exitCode = generateUndoMoveOperation(conflict, conflict.localNode());
            break;
        }
        default: {
            LOG_SYNCPAL_WARN(_logger, "Unknown conflict type: " << conflict.type());
            exitCode = ExitCode::DataError;
            break;
        }
    }

    return exitCode;
}

ExitInfo ConflictResolverWorker::handleConflictOnOmittedEdit(const Conflict &conflict, bool &continueSolving) {
    if (const auto localNode = conflict.localNode(); localNode->hasChangeEvent(OperationType::Edit)) {
        bool propagateEdit = true;
        if (auto exitInfo = editChangeShouldBePropagated(localNode, propagateEdit); !exitInfo) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in OperationProcessor::editChangeShouldBePropagated: "
                                               << Utility::formatSyncPath(localNode->getPath()) << L" " << exitInfo);
            _syncPal->addError(Error(ERR_ID, exitInfo));
            return exitInfo;
        }

        if (!propagateEdit) {
            const auto editOp = std::make_shared<SyncOperation>();
            editOp->setType(OperationType::Edit);
            editOp->setAffectedNode(localNode);
            editOp->setCorrespondingNode(conflict.remoteNode());
            editOp->setOmit(true);
            editOp->setTargetSide(ReplicaSide::Remote);
            editOp->setConflict(conflict);
            LOGW_SYNCPAL_INFO(_logger, getLogString(editOp));
            (void) _syncPal->syncOps()->pushOp(editOp);
            continueSolving = true;
        }
    }

    return ExitCode::Ok;
}

ExitCode ConflictResolverWorker::handleConflictOnDehydratedPlaceholder(const Conflict &conflict, bool &continueSolving) {
    if (_syncPal->vfsMode() == VirtualFileMode::Off) return ExitCode::Ok;

    const auto localNode = conflict.localNode();
    if (localNode->type() != NodeType::File) return ExitCode::Ok;

    VfsStatus vfsStatus;
    const auto moveNodeRelativePath = localNode->getPath();
    if (const auto exitInfo = _syncPal->vfs()->status(_syncPal->syncInfo().localPath / moveNodeRelativePath, vfsStatus);
        exitInfo.code() != ExitCode::Ok) {
        LOGW_SYNCPAL_WARN(_logger, L"Failed to get VFS status for file " << Utility::formatSyncPath(moveNodeRelativePath));
        return exitInfo;
    }
    if (vfsStatus.isPlaceholder && !vfsStatus.isHydrated && !vfsStatus.isSyncing) {
        // Local node is a dehydrated placeholder, remove it so it will be detected as new on next sync (if still present on
        // remote replica).
        const auto op = std::make_shared<SyncOperation>();
        op->setType(OperationType::Delete);
        op->setAffectedNode(conflict.remoteNode());
        op->setCorrespondingNode(localNode);

        op->setTargetSide(localNode->side());
        op->setConflict(conflict);
        op->setIsDehydratedPlaceholder(true);
        LOGW_SYNCPAL_INFO(_logger, L"The conflict occurred on a dehydrated placeholder. " << getLogString(op));

        (void) _syncPal->syncOps()->pushOp(op);
        continueSolving = true;
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

    const auto originPath = conflict.remoteNode()->getPath();
    op->setRelativeOriginPath(originPath);
    op->setRelativeDestinationPath(originPath.parent_path() / newName);

    // Update node
    conflict.remoteNode()->setMoveOriginInfos({conflict.remoteNode()->getPath(), *conflict.remoteNode()->parentNode()->id()});
    conflict.remoteNode()->setChangeEvents(OperationType::Move);

    LOGW_SYNCPAL_INFO(_logger, getLogString(op));

    (void) _syncPal->syncOps()->pushOp(op);
    continueSolving = true; // solve them all in the same sync
    return ExitCode::Ok;
}

ExitCode ConflictResolverWorker::generateMoveCreateConflictOperation(const Conflict &conflict, bool &continueSolving) {
    ExitInfo res = ExitCode::Ok;
    if (conflict.localNode()->hasChangeEvent(OperationType::Move)) {
        res = generateUndoMoveOperation(conflict, conflict.localNode());
    } else {
        res = generateLocalRenameOperation(conflict, continueSolving);
    }

    return res;
}

ExitCode ConflictResolverWorker::generateEditDeleteConflictOperation(const Conflict &conflict, bool &continueSolving) {
    const auto deleteNode = conflict.node()->hasChangeEvent(OperationType::Delete) ? conflict.node() : conflict.otherNode();
    const auto editNode = conflict.node()->hasChangeEvent(OperationType::Edit) ? conflict.node() : conflict.otherNode();
    if (deleteNode->parentNode()->hasChangeEvent(OperationType::Delete)) {
        if (editNode->side() == ReplicaSide::Local) {
            // Move the items that have been modified locally to the rescue folder.
            // Delete operations will be propagated on next sync.
            rescueModifiedLocalNodes(conflict, editNode);
            continueSolving = true;
        } else {
            // Delete operation wins.
            const auto deleteOp = std::make_shared<SyncOperation>();
            deleteOp->setType(OperationType::Delete);
            deleteOp->setAffectedNode(deleteNode);
            deleteOp->setCorrespondingNode(editNode);
            deleteOp->setTargetSide(ReplicaSide::Remote);
            deleteOp->setConflict(conflict);
            continueSolving = true;
            LOGW_SYNCPAL_INFO(_logger, getLogString(deleteOp));
            (void) _syncPal->syncOps()->pushOp(deleteOp);
        }
    } else {
        // Edit operation wins.
        // Remove the edited node from DB.
        // This will cause the file to be detected as new in the next sync iteration, thus it will be restored.
        const auto deleteOp = std::make_shared<SyncOperation>();
        deleteOp->setType(OperationType::Delete);
        deleteOp->setAffectedNode(editNode);
        deleteOp->setCorrespondingNode(deleteNode);
        deleteOp->setOmit(true);
        deleteOp->setTargetSide(deleteNode->side()); // Target side does not matter when we remove only in DB
        deleteOp->setConflict(conflict);

        LOGW_SYNCPAL_INFO(_logger, getLogString(deleteOp, true));

        (void) _syncPal->syncOps()->pushOp(deleteOp);
    }
    return ExitCode::Ok;
}

ExitCode ConflictResolverWorker::generateMoveDeleteConflictOperation(const Conflict &conflict, bool &continueSolving) {
    const auto deleteNode = conflict.node()->hasChangeEvent(OperationType::Delete) ? conflict.node() : conflict.otherNode();
    const auto moveNode = conflict.node()->hasChangeEvent(OperationType::Move) ? conflict.node() : conflict.otherNode();

    if (const auto correspondingMoveNodeParent = correspondingNodeDirect(moveNode->parentNode());
        correspondingMoveNodeParent && correspondingMoveNodeParent->hasChangeEvent(OperationType::Delete) &&
        _syncPal->conflictQueue()->hasConflict(ConflictType::MoveParentDelete)) {
        // If the move operation happens within a directory that was deleted on the other replica,
        // therefore, we ignore the Move-Delete conflict.
        // This conflict will be handled as a Move-ParentDelete conflict.
        LOG_SYNCPAL_INFO(
                _logger,
                "Move-Delete conflict ignored because it will be solved by solving the Move-ParentDelete conflict instead.");
        continueSolving = true;
        return ExitCode::Ok;
    }

    // Otherwise we need to check if any item has been modified locally
    rescueModifiedLocalNodes(conflict, moveNode);

    // Then propagate the delete operation if the item is a directory or if the file has not been modified locally
    if (moveNode->type() == NodeType::Directory || moveNode->status() != NodeStatus::ConflictOpGenerated) {
        const auto deleteOp = std::make_shared<SyncOperation>();
        deleteOp->setType(OperationType::Delete);
        deleteOp->setAffectedNode(deleteNode);
        deleteOp->setCorrespondingNode(moveNode);
        deleteOp->setTargetSide(moveNode->side());
        deleteOp->setConflict(conflict);

        LOGW_SYNCPAL_INFO(_logger, getLogString(deleteOp));

        (void) _syncPal->syncOps()->pushOp(deleteOp);
    }

    return ExitCode::Ok;
}

ExitCode ConflictResolverWorker::generateParentDeleteConflictOperation(const Conflict &conflict) {
    // We need to check if any item has been modified locally
    rescueModifiedLocalNodes(conflict, conflict.localNode());

    // Then propagate the delete operation (any modified children should have already been rescued since EditDelete and MoveDelete
    // conflicts have higher priority)
    const auto deleteOp = std::make_shared<SyncOperation>();
    deleteOp->setType(OperationType::Delete);
    const auto deleteNode = conflict.node()->hasChangeEvent(OperationType::Delete) ? conflict.node() : conflict.otherNode();
    deleteOp->setAffectedNode(deleteNode);
    const auto &correspondingNode = correspondingNodeInOtherTree(deleteNode);
    deleteOp->setCorrespondingNode(correspondingNode);
    deleteOp->setTargetSide(correspondingNode->side());
    deleteOp->setConflict(conflict);

    LOGW_SYNCPAL_INFO(_logger, getLogString(deleteOp));

    (void) _syncPal->syncOps()->pushOp(deleteOp);
    return ExitCode::Ok;
}

ExitCode ConflictResolverWorker::generateUndoMoveOperation(const Conflict &conflict, const std::shared_ptr<Node> loserNode) {
    // Undo move on the loser replica
    const auto moveOp = std::make_shared<SyncOperation>();
    if (const auto res = undoMove(loserNode, moveOp); res != ExitCode::Ok) {
        return res;
    }
    moveOp->setConflict(conflict);
    LOGW_SYNCPAL_INFO(_logger, getLogString(moveOp));
    (void) _syncPal->syncOps()->pushOp(moveOp);
    return ExitCode::Ok;
}

void ConflictResolverWorker::rescueModifiedLocalNodes(const Conflict &conflict, const std::shared_ptr<Node> node) {
    if (node->side() != ReplicaSide::Local) return;

    generateRescueOperation(conflict, node);
    for (const auto &[_, child]: node->children()) {
        rescueModifiedLocalNodes(conflict, child);
    }
}

void ConflictResolverWorker::generateRescueOperation(const Conflict &conflict, const std::shared_ptr<Node> node) {
    if (node->status() == NodeStatus::ConflictOpGenerated) return; // Already processed
    if (node->type() != NodeType::File) return;
    if (!node->hasChangeEvent(OperationType::Edit) && !node->hasChangeEvent(OperationType::Create)) return;

    // Move the deleted node to the rescue folder
    const auto moveOp = std::make_shared<SyncOperation>();
    moveOp->setType(OperationType::Move);
    moveOp->setAffectedNode(node);
    moveOp->setCorrespondingNode(node);
    moveOp->setTargetSide(ReplicaSide::Local);
    moveOp->setRelativeOriginPath(node->getPath());
    moveOp->setConflict(conflict);
    moveOp->setIsRescueOperation(true);

    // Update node
    node->setMoveOriginInfos({conflict.remoteNode()->getPath(), *conflict.remoteNode()->parentNode()->id()});
    node->setChangeEvents(OperationType::Move);
    node->setStatus(NodeStatus::ConflictOpGenerated);

    LOGW_SYNCPAL_INFO(_logger, getLogString(moveOp) << L" because it has been modified locally.");

    (void) _syncPal->syncOps()->pushOp(moveOp);
}

bool ConflictResolverWorker::generateConflictedName(const std::shared_ptr<Node> node, SyncName &newName,
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

ExitCode ConflictResolverWorker::undoMove(const std::shared_ptr<Node> moveNode, const SyncOpPtr moveOp) {
    LOG_IF_FAIL(moveNode)

    const auto updateTree = _syncPal->updateTree(moveNode->side());
    const auto originParentNode = updateTree->getNodeById(moveNode->moveOriginInfos().parentNodeId());
    const auto originPath = moveNode->moveOriginInfos().path();
    bool undoPossible = true;

    if (!originParentNode) {
        LOG_SYNCPAL_WARN(_logger, "Failed to retrieve origin parent node");
        return ExitCode::DataError;
    }

    if (isABelowB(originParentNode, moveNode) || originParentNode->hasChangeEvent(OperationType::Delete)) {
        undoPossible = false;
    } else {
        auto potentialOriginNode = originParentNode->getChildExcept(originPath.filename().native(), OperationType::Delete);
        if (potentialOriginNode && (potentialOriginNode->hasChangeEvent(OperationType::Create) ||
                                    potentialOriginNode->hasChangeEvent(OperationType::Move))) {
            undoPossible = false;
        }
    }

    SyncPath destinationPath;
    if (undoPossible) {
        moveOp->setNewParentNode(originParentNode);
        moveOp->setNewName(originPath.filename().native());
        destinationPath = originPath.parent_path() / moveOp->newName();
    } else {
        // We cannot undo the move operation, so the file is moved under the root node instead.
        // TODO : move it to the rescue folder instead
        moveOp->setNewParentNode(_syncPal->updateTree(moveNode->side())->rootNode());
        SyncName newName;
        (void) generateConflictedName(moveNode, newName);
        moveOp->setNewName(newName);
        destinationPath = moveOp->newName();
    }

    moveOp->setType(OperationType::Move);
    const auto correspondingNode = correspondingNodeInOtherTree(moveNode);
    correspondingNode->setMoveOriginInfos({moveNode->getPath(), moveNode->parentNode()->id().value_or("")});
    correspondingNode->insertChangeEvent(OperationType::Move);
    moveOp->setAffectedNode(correspondingNode);
    moveOp->setCorrespondingNode(moveNode);
    moveOp->setTargetSide(moveNode->side());
    moveOp->setRelativeOriginPath(moveNode->getPath());
    moveOp->setRelativeDestinationPath(destinationPath);

    return ExitCode::Ok;
}

std::wstring ConflictResolverWorker::getLogString(SyncOpPtr op, bool omit /*= false*/) {
    if (!op->correspondingNode()) return {};
    std::wstringstream ss;
    if (omit) {
        ss << L"Operation " << op->type() << L" to be propagated on DB only for item "
           << Utility::formatSyncName(op->correspondingNode()->name()) << L" ("
           << CommonUtility::s2ws(*op->correspondingNode()->id()) << L")";
    } else {
        ss << L"Operation " << op->type() << L" to be propagated on " << op->targetSide() << L" replica for item "
           << Utility::formatSyncName(op->correspondingNode()->name()) << L" ("
           << CommonUtility::s2ws(*op->correspondingNode()->id()) << L")";
    }
    return ss.str();
}

} // namespace KDC
