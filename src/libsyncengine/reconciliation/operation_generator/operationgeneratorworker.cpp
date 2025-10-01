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

#include "operationgeneratorworker.h"
#include "update_detection/update_detector/updatetree.h"
#include "libcommonserver/utility/utility.h"
#include "requests/parameterscache.h"
#include "libcommon/log/sentry/ptraces.h"

namespace KDC {

OperationGeneratorWorker::OperationGeneratorWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name,
                                                   const std::string &shortName) :
    OperationProcessor(syncPal, name, shortName) {}

void OperationGeneratorWorker::execute() {
    ExitCode exitCode(ExitCode::Unknown);

    LOG_SYNCPAL_DEBUG(_logger, "Worker started: name=" << name());

    _syncPal->_syncOps->startUpdate();
    _syncPal->_syncOps->clear();
    _bytesToDownload = 0;

    // Mark all nodes "Unprocessed"
    _syncPal->updateTree(ReplicaSide::Local)->markAllNodesUnprocessed();
    _syncPal->updateTree(ReplicaSide::Remote)->markAllNodesUnprocessed();

    _deletedNodes.clear();

    // Initiate breadth-first search with root nodes from both update trees
    _queuedToExplore.push(_syncPal->updateTree(ReplicaSide::Local)->rootNode());
    _queuedToExplore.push(_syncPal->updateTree(ReplicaSide::Remote)->rootNode());

    // Explore both update trees
    sentry::pTraces::counterScoped::GenerateItemOperations perfMonitor(syncDbId());
    while (!_queuedToExplore.empty()) {
        perfMonitor.start();
        if (stopAsked()) {
            exitCode = ExitCode::Ok;
            break;
        }

        std::shared_ptr<Node> currentNode = _queuedToExplore.front();
        _queuedToExplore.pop();

        if (!currentNode) {
            continue;
        }

        // Explore children even if node is processed
        for (const auto &child: currentNode->children()) {
            _queuedToExplore.push(child.second);
        }

        if (currentNode->status() == NodeStatus::Processed) {
            continue;
        }

        std::shared_ptr<Node> correspondingNode = correspondingNodeInOtherTree(currentNode);
        if (!correspondingNode && !currentNode->hasChangeEvent(OperationType::Create) &&
            (currentNode->hasChangeEvent(OperationType::Delete) || currentNode->hasChangeEvent(OperationType::Edit) ||
             currentNode->hasChangeEvent(OperationType::Move))) {
            LOGW_SYNCPAL_WARN(_logger, L"Failed to get corresponding node: " << SyncName2WStr(currentNode->name()));
            exitCode = ExitCode::DataError;
            break;
        }

        if (currentNode->hasChangeEvent(OperationType::Create)) {
            if (!(currentNode->side() == ReplicaSide::Local && currentNode->isSharedFolder())) {
                generateCreateOperation(currentNode, correspondingNode);
            }
        }

        if (currentNode->hasChangeEvent(OperationType::Delete)) {
            generateDeleteOperation(currentNode, correspondingNode);
        }

        if (currentNode->hasChangeEvent(OperationType::Edit)) {
            generateEditOperation(currentNode, correspondingNode);
        }

        if (currentNode->hasChangeEvent(OperationType::Move)) {
            generateMoveOperation(currentNode, correspondingNode);
        }
    }

    if (exitCode == ExitCode::Unknown && _queuedToExplore.empty()) {
        exitCode = ExitCode::Ok;
    }

    if (_bytesToDownload > 0) {
        const int64_t freeBytes = Utility::getFreeDiskSpace(_syncPal->localPath());
        if (freeBytes >= 0) {
            if (freeBytes < _bytesToDownload + Utility::freeDiskSpaceLimit()) {
                LOGW_SYNCPAL_WARN(_logger, L"Disk almost full, only " << freeBytes << L" B available at path "
                                                                      << Utility::formatSyncPath(_syncPal->localPath()) << L", "
                                                                      << _bytesToDownload
                                                                      << L" B to download. Synchronization canceled.");
                exitCode = ExitCode::SystemError;
                setExitCause(ExitCause::NotEnoughDiskSpace);
            }
        } else {
            LOGW_SYNCPAL_WARN(_logger,
                              L"Could not determine free space available at " << Utility::formatSyncPath(_syncPal->localPath()));
        }
    }

    LOG_SYNCPAL_DEBUG(_logger, "Worker stopped: name=" << name());
    setDone(exitCode);
}

void OperationGeneratorWorker::generateCreateOperation(std::shared_ptr<Node> currentNode,
                                                       std::shared_ptr<Node> correspondingNode) {
    SyncOpPtr op = std::make_shared<SyncOperation>();

    // Check for Create-Create pseudo conflict
    if (correspondingNode && isPseudoConflict(currentNode, correspondingNode)) {
        op->setOmit(true);
        op->setCorrespondingNode(correspondingNode);
        correspondingNode->setStatus(NodeStatus::Processed);
    }

    op->setType(OperationType::Create);
    op->setAffectedNode(currentNode);
    const auto targetSide = otherSide(currentNode->side());
    op->setTargetSide(targetSide);
    // We do not set parent node here since it might have been just created as well. In that case, parent node does not exist yet
    // in update tree.
    op->setNewName(currentNode->name());
    currentNode->setStatus(NodeStatus::Processed);
    _syncPal->_syncOps->pushOp(op);

    if (op->omit()) {
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_SYNCPAL_DEBUG(_logger,
                               L"Create-Create pseudo conflict detected. Operation Create to be propagated in DB only for item "
                                       << Utility::formatSyncPath(currentNode->getPath()));
        }
    } else {
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_SYNCPAL_DEBUG(_logger,
                               L"Create operation "
                                       << op->id() << L" to be propagated on " << op->targetSide() << L" replica for item "
                                       << Utility::formatSyncPath(currentNode->getPath()) << L" ("
                                       << CommonUtility::s2ws(currentNode->id() ? currentNode->id().value() : "-1") << L")");
        }

        if (_syncPal->vfsMode() == VirtualFileMode::Off && op->targetSide() == ReplicaSide::Local &&
            currentNode->type() == NodeType::File) {
            _bytesToDownload += currentNode->size();
        }
    }
}

void OperationGeneratorWorker::generateEditOperation(std::shared_ptr<Node> currentNode, std::shared_ptr<Node> correspondingNode) {
    const auto op = std::make_shared<SyncOperation>();

    assert(correspondingNode);

    // Check for Edit-Edit pseudo conflict
    if (isPseudoConflict(currentNode, correspondingNode)) {
        op->setOmit(true);
        correspondingNode->setStatus(NodeStatus::Processed);
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_SYNCPAL_DEBUG(_logger,
                               L"Edit-Edit pseudo conflict detected. Operation Edit to be propagated in DB only for item "
                                       << Utility::formatSyncPath(currentNode->getPath()));
        }
    }

    // If only elements that are not synced with the corresponding side change (e.g., creation date), the operation can be omitted
    bool propagateEdit = true;
    if (auto exitInfo = editChangeShouldBePropagated(currentNode, propagateEdit); !exitInfo) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in OperationProcessor::editChangeShouldBePropagated: "
                                           << Utility::formatSyncPath(currentNode->getPath()) << L" " << exitInfo);
        _syncPal->addError(Error(ERR_ID, exitInfo));
    }

    if (!propagateEdit) {
        // Only update DB and tree
        op->setOmit(true);
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_SYNCPAL_DEBUG(
                    _logger,
                    L"Among dates, only the creation date has changed. Operation Edit to be propagated in DB only for item with "
                            << Utility::formatSyncPath(currentNode->getPath()));
        }
    }

    op->setType(OperationType::Edit);
    op->setAffectedNode(currentNode);
    op->setCorrespondingNode(correspondingNode);
    op->setTargetSide(correspondingNode->side());
    if (currentNode->hasChangeEvent(OperationType::Move) && currentNode->status() == NodeStatus::Unprocessed) {
        currentNode->setStatus(NodeStatus::PartiallyProcessed);
    } else {
        currentNode->setStatus(NodeStatus::Processed);
    }
    _syncPal->_syncOps->pushOp(op);

    if (!op->omit()) {
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_SYNCPAL_DEBUG(_logger,
                               L"Edit operation "
                                       << op->id() << L" to be propagated on " << op->targetSide() << L" replica for item "
                                       << Utility::formatSyncPath(currentNode->getPath()) << L"(ID: "
                                       << CommonUtility::s2ws(currentNode->id() ? currentNode->id().value() : "-1") << L")");
        }

        if (_syncPal->vfsMode() == VirtualFileMode::Off && op->targetSide() == ReplicaSide::Local &&
            currentNode->type() == NodeType::File) {
            // Keep only the difference between remote size and local size
            int64_t diffSize = currentNode->size() - correspondingNode->size();

            _bytesToDownload += diffSize;
        }
    }
}

void OperationGeneratorWorker::generateMoveOperation(std::shared_ptr<Node> currentNode, std::shared_ptr<Node> correspondingNode) {
    SyncOpPtr op = std::make_shared<SyncOperation>();

    assert(correspondingNode);

    // Check for Move-Move (Source) pseudo conflict
    if (isPseudoConflict(currentNode, correspondingNode)) {
        op->setOmit(true);
        correspondingNode->setStatus(NodeStatus::Processed);
    }

    if (currentNode->side() == ReplicaSide::Remote && correspondingNode->name().empty() &&
        currentNode->name() == correspondingNode->name()) {
        // Only update DB and tree
        op->setOmit(true);
    }

    op->setType(OperationType::Move);
    op->setAffectedNode(currentNode);
    op->setCorrespondingNode(correspondingNode);
    op->setTargetSide(correspondingNode->side());
    op->setNewName(currentNode->name());
    if (currentNode->hasChangeEvent(OperationType::Edit) && currentNode->status() == NodeStatus::Unprocessed) {
        currentNode->setStatus(NodeStatus::PartiallyProcessed);
    } else {
        currentNode->setStatus(NodeStatus::Processed);
    }
    _syncPal->_syncOps->pushOp(op);

    if (op->omit()) {
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_SYNCPAL_DEBUG(
                    _logger, L"Move-Move (Source) pseudo conflict detected. Operation Move to be propagated in DB only for item "
                                     << Utility::formatSyncPath(currentNode->getPath()));
        }
    } else {
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_SYNCPAL_DEBUG(_logger, L"Move operation "
                                                << op->id() << L" to be propagated on " << op->targetSide() << L" replica from "
                                                << Utility::formatSyncPath(currentNode->moveOriginInfos().path()) << L" to "
                                                << Utility::formatSyncPath(currentNode->getPath()) << L" (ID: "
                                                << CommonUtility::s2ws(currentNode->id() ? currentNode->id().value() : "-1")
                                                << L")");
        }
    }
}

void OperationGeneratorWorker::generateDeleteOperation(std::shared_ptr<Node> currentNode,
                                                       std::shared_ptr<Node> correspondingNode) {
    auto op = std::make_shared<SyncOperation>();

    assert(correspondingNode);

    // Do not generate delete operation if parent already deleted
    assert(currentNode->parentNode() && currentNode->parentNode()->id().has_value());
    if (_deletedNodes.contains(*currentNode->parentNode()->id())) {
        return;
    }

    // Check if corresponding node has been also deleted
    if (correspondingNode->hasChangeEvent(OperationType::Delete)) {
        op->setOmit(true);
    }

    op->setType(OperationType::Delete);
    findAndMarkAllChildNodes(currentNode);
    currentNode->setStatus(NodeStatus::Processed);
    op->setAffectedNode(currentNode);
    op->setCorrespondingNode(correspondingNode);
    op->setTargetSide(correspondingNode->side());

    // Also mark all corresponding nodes as Processed
    findAndMarkAllChildNodes(correspondingNode);
    correspondingNode->setStatus(NodeStatus::Processed);

    _syncPal->_syncOps->pushOp(op);

    if (op->omit()) {
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_SYNCPAL_DEBUG(_logger, L"Corresponding file already deleted on "
                                                << op->targetSide()
                                                << L" replica. Operation Delete to be propagated in DB only for item "
                                                << Utility::formatSyncPath(currentNode->getPath()));
        }
        _syncPal->setRestart(true);
        // In certain cases (e.g.: directory deleted and re-created with the same name), we need to trigger the start
        // of next sync because nothing has changed but create events are not propagated
    } else {
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_SYNCPAL_DEBUG(_logger,
                               L"Delete operation "
                                       << op->id() << L" to be propagated on " << op->targetSide() << L" replica for item "
                                       << Utility::formatSyncPath(currentNode->getPath()) << L" ("
                                       << CommonUtility::s2ws(currentNode->id() ? currentNode->id().value() : "-1") << L")");
        }
    }

    _deletedNodes.insert(*currentNode->id());
}

void OperationGeneratorWorker::findAndMarkAllChildNodes(std::shared_ptr<Node> parentNode) {
    for (auto &childNode: parentNode->children()) {
        if (childNode.second->type() == NodeType::Directory) {
            findAndMarkAllChildNodes(childNode.second);
        }
        childNode.second->setStatus(NodeStatus::Processed);
    }
}

} // namespace KDC
