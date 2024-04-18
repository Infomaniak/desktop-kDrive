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

#include "updatetreeworker.h"
#include "libcommonserver/utility/utility.h"
#include "requests/parameterscache.h"

#include <iostream>
#include <log4cplus/loggingmacros.h>


namespace KDC {

UpdateTreeWorker::UpdateTreeWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName,
                                   ReplicaSide side)
    : ISyncWorker(syncPal, name, shortName),
      _syncDb(syncPal->_syncDb),
      _operationSet(side == ReplicaSideLocal ? syncPal->_localOperationSet : syncPal->_remoteOperationSet),
      _updateTree(side == ReplicaSideLocal ? syncPal->_localUpdateTree : syncPal->_remoteUpdateTree),
      _side(side) {}

UpdateTreeWorker::UpdateTreeWorker(std::shared_ptr<SyncDb> syncDb, std::shared_ptr<FSOperationSet> operationSet,
                                   std::shared_ptr<UpdateTree> updateTree, const std::string &name, const std::string &shortName,
                                   ReplicaSide side)
    : ISyncWorker(nullptr, name, shortName), _syncDb(syncDb), _operationSet(operationSet), _updateTree(updateTree), _side(side) {}

UpdateTreeWorker::~UpdateTreeWorker() {
    _operationSet.reset();
    _updateTree.reset();
}

void UpdateTreeWorker::execute() {
    ExitCode exitCode(ExitCodeUnknown);

    LOG_SYNCPAL_DEBUG(_logger, "Worker started: name=" << name().c_str());

    auto start = std::chrono::steady_clock::now();

    _updateTree->startUpdate();

    // Reset nodes working properties
    for (const auto &nodeItem : _updateTree->nodes()) {
        nodeItem.second->clearChangeEvents();
        nodeItem.second->clearConflictAlreadyConsidered();
        nodeItem.second->setInconsistencyType(InconsistencyTypeNone);
        nodeItem.second->setPreviousId(std::nullopt);
    }

    _updateTree->previousIdSet().clear();

    std::vector<stepptr> steptab = {&UpdateTreeWorker::step1MoveDirectory,   &UpdateTreeWorker::step2MoveFile,
                                    &UpdateTreeWorker::step3DeleteDirectory, &UpdateTreeWorker::step4DeleteFile,
                                    &UpdateTreeWorker::step5CreateDirectory, &UpdateTreeWorker::step6CreateFile,
                                    &UpdateTreeWorker::step7EditFile,        &UpdateTreeWorker::step8CompleteUpdateTree};

    for (auto stepn : steptab) {
        exitCode = (this->*stepn)();
        if (exitCode != ExitCodeOk) {
            setDone(exitCode);
            return;
        }
        if (stopAsked()) {
            setDone(ExitCodeOk);
            return;
        }
    }

    integrityCheck();

    std::chrono::duration<double> elapsed_seconds = std::chrono::steady_clock::now() - start;

    if (exitCode == ExitCodeOk) {
        LOG_SYNCPAL_DEBUG(
            _logger, "Update Tree " << Utility::side2Str(_side).c_str() << " updated in: " << elapsed_seconds.count() << "s");
    } else {
        LOG_SYNCPAL_WARN(_logger, "Update Tree " << Utility::side2Str(_side).c_str()
                                                 << " generation failed after: " << elapsed_seconds.count() << "s");
    }

    // Clear unexpected operation set once used
    _operationSet->clear();

    setDone(exitCode);
    LOG_SYNCPAL_DEBUG(_logger, "Worker stopped: name=" << name().c_str());
}

ExitCode UpdateTreeWorker::step1MoveDirectory() {
    return createMoveNodes(NodeTypeDirectory);
}

ExitCode UpdateTreeWorker::step2MoveFile() {
    return createMoveNodes(NodeTypeFile);
}

ExitCode UpdateTreeWorker::step3DeleteDirectory() {
    std::unordered_set<UniqueId> deleteOpsIds;
    _operationSet->getOpsByType(OperationTypeDelete, deleteOpsIds);
    for (const auto &deleteOpId : deleteOpsIds) {
        // worker stop or pause
        if (stopAsked()) {
            return ExitCodeOk;
        }

        while (pauseAsked() || isPaused()) {
            if (!isPaused()) {
                setPauseDone();
            }

            Utility::msleep(LOOP_PAUSE_SLEEP_PERIOD);
        }

        FSOpPtr deleteOp = nullptr;
        _operationSet->getOp(deleteOpId, deleteOp);

        if (deleteOp->objectType() != NodeTypeDirectory) {
            continue;
        }

        auto currentNodeIt = _updateTree->nodes().find(deleteOp->nodeId());
        if (currentNodeIt != _updateTree->nodes().end()) {
            // Node exists
            std::shared_ptr<Node> testNode = currentNodeIt->second;

            currentNodeIt->second->insertChangeEvent(OperationTypeDelete);
            currentNodeIt->second->setCreatedAt(deleteOp->createdAt());
            currentNodeIt->second->setLastModified(deleteOp->lastModified());
            currentNodeIt->second->setSize(deleteOp->size());
            currentNodeIt->second->setIsTmp(false);
            if (ParametersCache::instance()->parameters().extendedLog()) {
                LOG_SYNCPAL_DEBUG(
                    _logger,
                    Utility::s2ws(Utility::side2Str(_side)).c_str()
                        << L" update tree: Node " << SyncName2WStr(currentNodeIt->second->name()).c_str() << L" (node ID: "
                        << Utility::s2ws(currentNodeIt->second->id().has_value() ? *currentNodeIt->second->id() : NodeId())
                               .c_str()
                        << L", DB ID: " << (currentNodeIt->second->idb().has_value() ? *currentNodeIt->second->idb() : -1)
                        << L") updated. Operation DELETE inserted in change events.");
            }
        } else {
            // Look for parentNodeId in db
            std::optional<NodeId> parentNodeId;
            bool found = false;
            if (!_syncDb->id(_side, deleteOp->path().parent_path(), parentNodeId, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::id");
                return ExitCodeDbError;
            }
            std::shared_ptr<Node> parentNode = nullptr;
            bool ok = false;
            if (found && parentNodeId.has_value()) {
                auto nodeIt = _updateTree->nodes().find(*parentNodeId);
                if (nodeIt != _updateTree->nodes().end()) {
                    parentNode = nodeIt->second;
                    ok = true;
                }
            }

            if (!ok) {
                SyncPath newPath;
                ExitCode exitCode = getNewPathAfterMove(deleteOp->path(), newPath);
                if (exitCode != ExitCodeOk) {
                    if (exitCode == ExitCodeDbError) {
                        LOG_SYNCPAL_WARN(_logger, "Error in UpdateTreeWorker::getNewPathAfterMove");
                        return exitCode;
                    }
                }
                // get parentNode
                parentNode = getOrCreateNodeFromPath(newPath.parent_path());
            }

            // Find dbNodeId
            DbNodeId idb = 0;
            if (!_syncDb->dbId(_side, deleteOp->nodeId(), idb, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbId");
                return ExitCodeDbError;
            }
            if (!found) {
                LOG_SYNCPAL_WARN(_logger, "Node not found for id = " << deleteOp->nodeId().c_str());
                return ExitCodeDataError;
            }

            // Check if parentNode has got a child with the same name
            std::shared_ptr<Node> newNode = parentNode->findChildrenById(deleteOp->nodeId());
            if (newNode != nullptr) {
                // Node already exists, update it
                newNode->setIdb(idb);
                updateNodeId(newNode, deleteOp->nodeId());
                newNode->setCreatedAt(deleteOp->createdAt());
                newNode->setLastModified(deleteOp->lastModified());
                newNode->setSize(deleteOp->size());
                newNode->insertChangeEvent(OperationTypeDelete);
                newNode->setIsTmp(false);
                _updateTree->nodes()[deleteOp->nodeId()] = newNode;
                if (ParametersCache::instance()->parameters().extendedLog()) {
                    LOG_SYNCPAL_DEBUG(_logger, Utility::s2ws(Utility::side2Str(_side)).c_str()
                                                   << L" update tree: Node " << SyncName2WStr(newNode->name()).c_str()
                                                   << L" (node ID: "
                                                   << Utility::s2ws(newNode->id().has_value() ? *newNode->id() : NodeId()).c_str()
                                                   << L", DB ID: " << (newNode->idb().has_value() ? *newNode->idb() : -1)
                                                   << L") updated. Operation DELETE inserted in change events.");
                }
            } else {
                // create node
                newNode = std::shared_ptr<Node>(new Node(idb, _side, deleteOp->path().filename().native(), deleteOp->objectType(),
                                                         OperationTypeDelete, deleteOp->nodeId(), deleteOp->createdAt(),
                                                         deleteOp->lastModified(), deleteOp->size(), parentNode));
                if (newNode == nullptr) {
                    std::cout << "Failed to allocate memory" << std::endl;
                    LOG_SYNCPAL_ERROR(_logger, "Failed to allocate memory");
                    return ExitCodeSystemError;
                }

                parentNode->insertChildren(newNode);
                _updateTree->nodes()[deleteOp->nodeId()] = newNode;
                if (ParametersCache::instance()->parameters().extendedLog()) {
                    LOG_SYNCPAL_DEBUG(
                        _logger, Utility::s2ws(Utility::side2Str(_side)).c_str()
                                     << L" update tree: Node " << SyncName2WStr(newNode->name()).c_str() << L" (node ID: "
                                     << Utility::s2ws(newNode->id().has_value() ? *newNode->id() : NodeId()).c_str()
                                     << L", DB ID: " << (newNode->idb().has_value() ? *newNode->idb() : -1) << L", parent ID: "
                                     << Utility::s2ws(parentNode->id().has_value() ? *parentNode->id() : NodeId()).c_str()
                                     << L") inserted. Operation DELETE inserted in change events.");
                }
            }
        }
    }
    return ExitCodeOk;
}

ExitCode UpdateTreeWorker::handleCreateOperationsWithSamePath() {
    _createFileOperationSet.clear();
    FSOpPtrMap createDirectoryOperationSet;
    std::unordered_set<UniqueId> createOpsIds;
    _operationSet->getOpsByType(OperationTypeCreate, createOpsIds);

    bool isSnapshotRebuildRequired = false;

    for (const auto &createOpId : createOpsIds) {
        // worker stop or pause
        if (stopAsked()) {
            return ExitCodeOk;
        }
        while (pauseAsked() || isPaused()) {
            if (!isPaused()) {
                setPauseDone();
            }

            Utility::msleep(LOOP_PAUSE_SLEEP_PERIOD);
        }

        FSOpPtr createOp;
        _operationSet->getOp(createOpId, createOp);
        const auto normalizedPath = Utility::normalizedSyncPath(createOp->path());

        std::pair<FSOpPtrMap::iterator, bool> insertionResult;
        switch (createOp->objectType()) {
            case NodeTypeFile:
                insertionResult = _createFileOperationSet.try_emplace(normalizedPath, createOp);
                break;
            case NodeTypeDirectory:
                insertionResult = createDirectoryOperationSet.try_emplace(normalizedPath, createOp);
                break;
            default:
                break;
        }

        if (!insertionResult.second) {
            // Failed to insert Create operation. A full rebuild of the snapshot is required.
            //
            // Two issues have been identified:
            // - Either (1) the operating system missed a delete operation, in which case a snapshot rebuild is both
            // required and sufficient.
            // - Or (2) the file system allows file or directory names with different encodings but the same normalization,
            // in which case an action of the user is required. In such a situation, we trigger a snapshot rebuild
            // on the first pass in this function. Then we temporarily blacklist the item during the second pass and
            // display an error message.

            LOGW_SYNCPAL_WARN(_logger, Utility::s2ws(Utility::side2Str(_side)).c_str()
                                           << L" update tree: Operation Create already exists on item with "
                                           << Utility::formatSyncPath(createOp->path()).c_str());

#ifdef NDEBUG
            sentry_capture_event(sentry_value_new_message_event(SENTRY_LEVEL_WARNING, "UpdateTreeWorker::step4",
                                                                "2 Create operations detected on the same item"));
#endif

            _syncPal->increaseErrorCount(createOp->nodeId(), createOp->objectType(), createOp->path(), _side);
            if (_syncPal->getErrorCount(createOp->nodeId(), _side) > 1) {
                // We are in situation (2), i.e. duplicate normalized names.
                // We display to the user an explicit error message about item name inconsistency.
                Error err(_syncPal->syncDbId(), "", createOp->nodeId(), createOp->objectType(), createOp->path(),
                          ConflictTypeNone, InconsistencyTypeDuplicateNames, CancelTypeNone);
                _syncPal->addError(err);
            }

            isSnapshotRebuildRequired = true;
        }
    }

    return isSnapshotRebuildRequired ? ExitCodeDataError : ExitCodeOk;
}
ExitCode UpdateTreeWorker::step4DeleteFile() {
    const ExitCode exitCode = handleCreateOperationsWithSamePath();
    if (exitCode != ExitCodeOk) return exitCode;  // Rebuild the snapshot.

    std::unordered_set<UniqueId> deleteOpsIds;
    _operationSet->getOpsByType(OperationTypeDelete, deleteOpsIds);
    for (const auto &deleteOpId : deleteOpsIds) {
        // worker stop or pause
        if (stopAsked()) {
            return ExitCodeOk;
        }
        while (pauseAsked() || isPaused()) {
            if (!isPaused()) {
                setPauseDone();
            }

            Utility::msleep(LOOP_PAUSE_SLEEP_PERIOD);
        }

        FSOpPtr deleteOp = nullptr;
        _operationSet->getOp(deleteOpId, deleteOp);
        if (deleteOp->objectType() != NodeTypeFile) {
            continue;
        }

        FSOpPtr op = deleteOp;
        if (_side == ReplicaSideLocal) {
            // Transform a Delete and a Create operations into one Edit operation.
            // Some software, such as Excel, keeps the current version into a temporary directory and move it to the destination,
            // replacing the original file. However, this behavior should be applied only on local side.
            auto createFileOpSetIt = _createFileOperationSet.find(deleteOp->path());
            if (createFileOpSetIt != _createFileOperationSet.end()) {
                FSOpPtr tmp = nullptr;
                if (!_operationSet->findOp(createFileOpSetIt->second->nodeId(), createFileOpSetIt->second->operationType(),
                                           tmp)) {
                    continue;
                }

                // op is now the createOperation
                op = tmp;
                _createFileOperationSet.erase(deleteOp->path());
            }
        }
        OperationType opType = op->operationType() == OperationTypeCreate ? OperationTypeEdit : OperationTypeDelete;

        auto currentNodeIt = _updateTree->nodes().find(deleteOp->nodeId());
        if (currentNodeIt != _updateTree->nodes().end()) {
            // Node is already in the update tree, it can be a Delete or an Edit
            std::shared_ptr<Node> currentNode = currentNodeIt->second;
            updateNodeId(currentNode, op->nodeId());
            currentNode->setCreatedAt(op->createdAt());
            currentNode->setLastModified(op->lastModified());
            currentNode->setSize(op->size());
            currentNode->insertChangeEvent(opType);
            currentNode->setIsTmp(false);

            // TODO: refactor other OperationTypeEdit conditions

            // If it's an edit (Delete-Create) we change it's id
            if (opType == OperationTypeEdit) {
                currentNode->setPreviousId(deleteOp->nodeId());
                _updateTree->previousIdSet()[deleteOp->nodeId()] = op->nodeId();

                // replace node in _nodes map because id changed
                auto node = _updateTree->nodes().extract(deleteOp->nodeId());
                node.key() = op->nodeId();
                _updateTree->nodes().insert(std::move(node));
            }

            if (ParametersCache::instance()->parameters().extendedLog()) {
                LOG_SYNCPAL_DEBUG(_logger,
                                  Utility::s2ws(Utility::side2Str(_side)).c_str()
                                      << L" update tree: Node " << SyncName2WStr(currentNode->name()).c_str() << L" (node ID: "
                                      << Utility::s2ws(currentNode->id().has_value() ? *currentNode->id() : NodeId()).c_str()
                                      << L", DB ID: " << (currentNode->idb().has_value() ? *currentNode->idb() : -1)
                                      << L") updated. Operation " << Utility::s2ws(Utility::opType2Str(opType)).c_str()
                                      << L" inserted in change events.");
            }
        } else {
            // find parentNodeId in db
            std::optional<NodeId> parentNodeId;
            bool found = false;
            if (!_syncDb->id(_side, op->path().parent_path(), parentNodeId, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::id");
                return ExitCodeDbError;
            }
            std::shared_ptr<Node> parentNode;
            bool ok = false;
            if (found && parentNodeId.has_value()) {
                auto parentNodeIt = _updateTree->nodes().find(*parentNodeId);
                if (parentNodeIt != _updateTree->nodes().end()) {
                    // if node exist
                    parentNode = parentNodeIt->second;
                    ok = true;
                }
            }

            if (!ok) {
                // find parentNode
                parentNode = getOrCreateNodeFromPath(op->path().parent_path());
            }

            // find child node
            std::shared_ptr<Node> newNode = parentNode->findChildrenById(deleteOp->nodeId());
            if (newNode != nullptr && newNode->isTmp()) {
                // Tmp node already exists, update it
                updateNodeId(newNode, op->nodeId());
                newNode->setCreatedAt(op->createdAt());
                newNode->setLastModified(op->lastModified());
                newNode->setSize(op->size());
                newNode->insertChangeEvent(opType);
                newNode->setIsTmp(false);
                if (opType == OperationTypeEdit) {
                    newNode->setPreviousId(deleteOp->nodeId());
                    _updateTree->previousIdSet()[deleteOp->nodeId()] = op->nodeId();
                }

                _updateTree->nodes()[op->nodeId()] = newNode;
                if (ParametersCache::instance()->parameters().extendedLog()) {
                    LOG_SYNCPAL_DEBUG(_logger,
                                      Utility::s2ws(Utility::side2Str(_side)).c_str()
                                          << L" update tree: Node " << SyncName2WStr(newNode->name()).c_str() << L" (node ID: "
                                          << Utility::s2ws(newNode->id().has_value() ? *newNode->id() : NodeId()).c_str()
                                          << L", DB ID: " << (newNode->idb().has_value() ? *newNode->idb() : -1)
                                          << L") updated. Operation " << Utility::s2ws(Utility::opType2Str(opType)).c_str()
                                          << L" inserted in change events.");
                }
            } else {
                // create node
                DbNodeId idb;
                bool found = false;
                if (!_syncDb->dbId(_side, deleteOp->nodeId(), idb, found)) {
                    LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbId");
                    return ExitCodeDbError;
                }
                if (!found) {
                    LOG_SYNCPAL_WARN(_logger, "Node not found for id = " << deleteOp->nodeId().c_str());
                    return ExitCodeDataError;
                }

                newNode =
                    std::shared_ptr<Node>(new Node(idb, _side, op->path().filename().native(), op->objectType(), opType,
                                                   op->nodeId(), op->createdAt(), op->lastModified(), op->size(), parentNode));
                if (newNode == nullptr) {
                    std::cout << "Failed to allocate memory" << std::endl;
                    LOG_SYNCPAL_ERROR(_logger, "Failed to allocate memory");
                    return ExitCodeSystemError;
                }

                // store old NodeId to retrieve node in Db
                if (opType == OperationTypeEdit) {
                    newNode->setPreviousId(deleteOp->nodeId());
                    _updateTree->previousIdSet()[deleteOp->nodeId()] = op->nodeId();
                }

                parentNode->insertChildren(newNode);
                _updateTree->nodes()[op->nodeId()] = newNode;
                if (ParametersCache::instance()->parameters().extendedLog()) {
                    LOG_SYNCPAL_DEBUG(
                        _logger, Utility::s2ws(Utility::side2Str(_side)).c_str()
                                     << L" update tree: Node " << SyncName2WStr(newNode->name()).c_str() << L" (node ID: "
                                     << Utility::s2ws(newNode->id().has_value() ? *newNode->id() : NodeId()).c_str()
                                     << L", DB ID: " << (newNode->idb().has_value() ? *newNode->idb() : -1) << L", parent ID: "
                                     << Utility::s2ws(parentNode->id().has_value() ? *parentNode->id() : NodeId()).c_str()
                                     << L") inserted. Operation " << Utility::s2ws(Utility::opType2Str(opType)).c_str()
                                     << L" inserted in change events.");
                }
            }
        }
    }

    return ExitCodeOk;
}

ExitCode UpdateTreeWorker::step5CreateDirectory() {
    std::unordered_set<UniqueId> createOpsIds;
    _operationSet->getOpsByType(OperationTypeCreate, createOpsIds);
    for (const auto &createOpId : createOpsIds) {
        // worker stop or pause
        if (stopAsked()) {
            return ExitCodeOk;
        }

        while (pauseAsked() || isPaused()) {
            if (!isPaused()) {
                setPauseDone();
            }

            Utility::msleep(LOOP_PAUSE_SLEEP_PERIOD);
        }

        FSOpPtr createOp = nullptr;
        _operationSet->getOp(createOpId, createOp);
        if (createOp->objectType() != NodeTypeDirectory) {
            continue;
        }

        // find node by path because it may have been created before
        std::shared_ptr<Node> currentNode = getOrCreateNodeFromPath(createOp->path());
        if (currentNode->hasChangeEvent(OperationTypeDelete)) {
            // A directory has been deleted and another one has been created with the same name
            currentNode->setPreviousId(currentNode->id());
        }
        updateNodeId(currentNode, createOp->nodeId());
        currentNode->setCreatedAt(createOp->createdAt());
        currentNode->setLastModified(createOp->lastModified());
        currentNode->setSize(createOp->size());
        currentNode->insertChangeEvent(createOp->operationType());
        currentNode->setIsTmp(false);
        _updateTree->nodes()[createOp->nodeId()] = currentNode;
        if (ParametersCache::instance()->parameters().extendedLog()) {
            LOG_SYNCPAL_DEBUG(
                _logger, Utility::s2ws(Utility::side2Str(_side)).c_str()
                             << L" update tree: Node " << SyncName2WStr(currentNode->name()).c_str() << L" (node ID: "
                             << Utility::s2ws(currentNode->id().has_value() ? *currentNode->id() : std::string()).c_str()
                             << L", DB ID: " << (currentNode->idb().has_value() ? *currentNode->idb() : -1)
                             << L") inserted. Operation " << Utility::s2ws(Utility::opType2Str(createOp->operationType())).c_str()
                             << L" inserted in change events.");
        }
    }
    return ExitCodeOk;
}

ExitCode UpdateTreeWorker::step6CreateFile() {
    for (const auto &op : _createFileOperationSet) {
        // worker stop or pause
        if (stopAsked()) {
            return ExitCodeOk;
        }
        while (pauseAsked() || isPaused()) {
            if (!isPaused()) {
                setPauseDone();
            }

            Utility::msleep(LOOP_PAUSE_SLEEP_PERIOD);
        }

        FSOpPtr operation = op.second;

        // find parentNode by path
        std::shared_ptr<Node> parentNode = getOrCreateNodeFromPath(operation->path().parent_path());
        std::shared_ptr<Node> newNode = parentNode->findChildrenById(operation->nodeId());
        if (newNode != nullptr) {
            // Node already exists, update it
            if (newNode->name() == operation->path().filename().native()) {
                updateNodeId(newNode, operation->nodeId());
                newNode->setCreatedAt(operation->createdAt());
                newNode->setLastModified(operation->lastModified());
                newNode->setSize(operation->size());
                newNode->insertChangeEvent(operation->operationType());
                newNode->setIsTmp(false);

                _updateTree->nodes()[operation->nodeId()] = newNode;
                if (ParametersCache::instance()->parameters().extendedLog()) {
                    LOG_SYNCPAL_DEBUG(
                        _logger, Utility::s2ws(Utility::side2Str(_side)).c_str()
                                     << L" update tree: Node " << SyncName2WStr(newNode->name()).c_str() << L" (node ID: "
                                     << Utility::s2ws(newNode->id().has_value() ? *newNode->id() : std::string()).c_str()
                                     << L", DB ID: " << (newNode->idb().has_value() ? *newNode->idb() : -1) << L", parent ID: "
                                     << Utility::s2ws(parentNode->id().has_value() ? *parentNode->id() : "-1").c_str()
                                     << L") updated. Operation "
                                     << Utility::s2ws(Utility::opType2Str(operation->operationType())).c_str()
                                     << L" inserted in change events.");
                }
                continue;
            }
        }

        // create node
        newNode = std::shared_ptr<Node>(new Node(
            std::nullopt, _side, operation->path().filename().native(), operation->objectType(), operation->operationType(),
            operation->nodeId(), operation->createdAt(), operation->lastModified(), operation->size(), parentNode));
        if (newNode == nullptr) {
            std::cout << "Failed to allocate memory" << std::endl;
            LOG_SYNCPAL_ERROR(_logger, "Failed to allocate memory");
            return ExitCodeSystemError;
        }

        parentNode->insertChildren(newNode);
        _updateTree->nodes()[operation->nodeId()] = newNode;
        if (ParametersCache::instance()->parameters().extendedLog()) {
            LOG_SYNCPAL_DEBUG(
                _logger,
                Utility::s2ws(Utility::side2Str(_side)).c_str()
                    << L" update tree: Node " << SyncName2WStr(newNode->name()).c_str() << L" (node ID: "
                    << Utility::s2ws(newNode->id().has_value() ? *newNode->id() : "").c_str() << L", DB ID: "
                    << (newNode->idb().has_value() ? *newNode->idb() : -1) << L", parent ID: "
                    << Utility::s2ws(parentNode->id().has_value() ? *parentNode->id() : "-1").c_str() << L") inserted. Operation "
                    << Utility::s2ws(Utility::opType2Str(operation->operationType())).c_str() << L" inserted in change events.");
        }
    }
    return ExitCodeOk;
}

ExitCode UpdateTreeWorker::step7EditFile() {
    std::unordered_set<UniqueId> editOpsIds;
    _operationSet->getOpsByType(OperationTypeEdit, editOpsIds);
    for (const auto &editOpId : editOpsIds) {
        // worker stop or pause
        if (stopAsked()) {
            return ExitCodeOk;
        }

        while (pauseAsked() || isPaused()) {
            if (!isPaused()) {
                setPauseDone();
            }

            Utility::msleep(LOOP_PAUSE_SLEEP_PERIOD);
        }

        FSOpPtr editOp = nullptr;
        _operationSet->getOp(editOpId, editOp);
        if (editOp->objectType() != NodeTypeFile) {
            continue;
        }
        // find parentNode by path because should have been created
        std::shared_ptr<Node> parentNode = getOrCreateNodeFromPath(editOp->path().parent_path());
        std::shared_ptr<Node> newNode = parentNode->findChildrenById(editOp->nodeId());
        if (newNode != nullptr) {
            // Node already exists, update it
            newNode->setCreatedAt(editOp->createdAt());
            newNode->setLastModified(editOp->lastModified());
            newNode->setSize(editOp->size());
            newNode->insertChangeEvent(editOp->operationType());
            updateNodeId(newNode, editOp->nodeId());
            newNode->setIsTmp(false);

            _updateTree->nodes()[editOp->nodeId()] = newNode;
            if (ParametersCache::instance()->parameters().extendedLog()) {
                LOG_SYNCPAL_DEBUG(
                    _logger,
                    Utility::s2ws(Utility::side2Str(_side)).c_str()
                        << L" update tree: Node " << SyncName2WStr(newNode->name()).c_str() << L" (node ID: "
                        << Utility::s2ws(newNode->id().has_value() ? *newNode->id() : std::string()).c_str() << L", DB ID: "
                        << (newNode->idb().has_value() ? *newNode->idb() : -1) << L") updated. Operation "
                        << Utility::s2ws(Utility::opType2Str(editOp->operationType())).c_str() << L" inserted in change events.");
            }
            continue;
        }

        // create node
        DbNodeId idb;
        bool found = false;
        if (!_syncDb->dbId(_side, editOp->nodeId(), idb, found)) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbId");
            return ExitCodeDbError;
        }
        if (!found) {
            LOG_SYNCPAL_WARN(_logger, "Node not found for id = " << editOp->nodeId().c_str());
            return ExitCodeDataError;
        }

        newNode = std::shared_ptr<Node>(new Node(idb, _side, editOp->path().filename().native(), editOp->objectType(),
                                                 editOp->operationType(), editOp->nodeId(), editOp->createdAt(),
                                                 editOp->lastModified(), editOp->size(), parentNode));
        if (newNode == nullptr) {
            std::cout << "Failed to allocate memory" << std::endl;
            LOG_SYNCPAL_ERROR(_logger, "Failed to allocate memory");
            return ExitCodeSystemError;
        }

        parentNode->insertChildren(newNode);
        _updateTree->nodes()[editOp->nodeId()] = newNode;
        if (ParametersCache::instance()->parameters().extendedLog()) {
            LOG_SYNCPAL_DEBUG(
                _logger, Utility::s2ws(Utility::side2Str(_side)).c_str()
                             << L" update tree: Node " << SyncName2WStr(newNode->name()).c_str() << L" (node ID: "
                             << Utility::s2ws(newNode->id().has_value() ? *newNode->id() : std::string()).c_str() << L", DB ID: "
                             << (newNode->idb().has_value() ? *newNode->idb() : -1) << L", parent ID: "
                             << Utility::s2ws(parentNode->id().has_value() ? *parentNode->id() : std::string()).c_str()
                             << L") inserted. editOp " << Utility::s2ws(Utility::opType2Str(editOp->operationType())).c_str()
                             << L" inserted in change events.");
        }
    }

    return ExitCodeOk;
}

ExitCode UpdateTreeWorker::step8CompleteUpdateTree() {
    if (stopAsked()) {
        return ExitCodeOk;
    }

    bool found = false;
    std::vector<NodeId> dbNodeIds;
    if (!_syncDb->ids(_side, dbNodeIds, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbNodeIds");
        return ExitCodeDbError;
    }
    if (!found) {
        LOG_SYNCPAL_DEBUG(_logger, "There is no dbNodeIds for side=" << _side);
        return ExitCodeOk;
    }

    // update each existing nodes
    ExitCode exitCode = ExitCodeUnknown;
    try {
        exitCode = updateNodeWithDb(_updateTree->rootNode());
    } catch (std::exception &e) {
        LOG_WARN(_logger, "updateNodeWithDb failed - err= " << e.what());
        return ExitCodeDataError;
    }

    if (exitCode != ExitCodeOk) {
        LOG_SYNCPAL_WARN(_logger, "Error in updateNodeWithDb");
        return exitCode;
    }

    std::optional<NodeId> rootNodeId =
        (_side == ReplicaSideLocal ? _syncDb->rootNode().nodeIdLocal() : _syncDb->rootNode().nodeIdRemote());

    // creating missing nodes
    for (const NodeId &dbNodeId : dbNodeIds) {
        // worker stop or pause
        if (stopAsked()) {
            return ExitCodeOk;
        }
        while (pauseAsked() || isPaused()) {
            if (!isPaused()) {
                setPauseDone();
            }

            Utility::msleep(LOOP_PAUSE_SLEEP_PERIOD);
        }

        if (dbNodeId == rootNodeId.value()) {
            // Ignore root folder
            continue;
        }

        NodeId previousNodeId = dbNodeId;

        NodeId newNodeId = dbNodeId;
        auto previousToNewIdIt = _updateTree->previousIdSet().find(dbNodeId);
        if (previousToNewIdIt != _updateTree->previousIdSet().end()) {
            newNodeId = previousToNewIdIt->second;
        }

        if (_updateTree->nodes().find(newNodeId) == _updateTree->nodes().end()) {
            // create node
            NodeId parentId;
            if (!_syncDb->parent(_side, previousNodeId, parentId, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::parent");
                return ExitCodeDbError;
            }
            if (!found) {
                LOG_SYNCPAL_WARN(_logger, "Node or parent node not found for dbNodeId = " << previousNodeId.c_str());
                return ExitCodeDataError;
            }
            // not sure that parentId is already in _nodes ??
            std::shared_ptr<Node> parentNode;  // parentNode could be null
            auto parentIt = _updateTree->nodes().find(parentId);
            if (parentIt != _updateTree->nodes().end()) {
                parentNode = parentIt->second;
            }

            DbNode dbNode;
            if (!_syncDb->node(_side, previousNodeId, dbNode, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::node");
                return ExitCodeDbError;
            }
            if (!found) {
                LOG_SYNCPAL_WARN(_logger, "Failed to retrieve node for dbId=" << previousNodeId.c_str());
                return ExitCodeDataError;
            }

            SyncTime lastModified =
                _side == ReplicaSideLocal ? dbNode.lastModifiedLocal().value() : dbNode.lastModifiedRemote().value();
            SyncName name = dbNode.nameRemote();
            std::shared_ptr<Node> n = std::shared_ptr<Node>(new Node(dbNode.nodeId(), _side, name, dbNode.type(), {}, newNodeId,
                                                                     dbNode.created(), lastModified, dbNode.size(), parentNode));
            if (n == nullptr) {
                std::cout << "Failed to allocate memory" << std::endl;
                LOG_SYNCPAL_ERROR(_logger, "Failed to allocate memory");
                return ExitCodeSystemError;
            }

            if (dbNode.nameLocal() != dbNode.nameRemote()) {
                n->setValidLocalName(dbNode.nameLocal());
            }

            parentNode->insertChildren(n);
            _updateTree->nodes()[newNodeId] = n;
            if (ParametersCache::instance()->parameters().extendedLog()) {
                LOG_SYNCPAL_DEBUG(_logger,
                                  Utility::s2ws(Utility::side2Str(_side)).c_str()
                                      << L" update tree: Node " << SyncName2WStr(n->name()).c_str() << L" (node ID: "
                                      << Utility::s2ws(n->id().has_value() ? *n->id() : std::string()).c_str() << L", DB ID: "
                                      << (n->idb().has_value() ? *n->idb() : 1) << L", parent ID: "
                                      << Utility::s2ws(parentNode->id().has_value() ? *parentNode->id() : std::string()).c_str()
                                      << L") inserted. Without change events.");
            }
        }
    }

    return exitCode;
}

ExitCode UpdateTreeWorker::createMoveNodes(const NodeType &nodeType) {
    std::unordered_set<UniqueId> moveOpsIds;
    _operationSet->getOpsByType(OperationTypeMove, moveOpsIds);
    for (const auto &moveOpId : moveOpsIds) {
        if (stopAsked()) {
            return ExitCodeOk;
        }

        while (pauseAsked() || isPaused()) {
            if (!isPaused()) {
                setPauseDone();
            }

            Utility::msleep(LOOP_PAUSE_SLEEP_PERIOD);
        }

        FSOpPtr moveOp = nullptr;
        _operationSet->getOp(moveOpId, moveOp);
        if (moveOp->objectType() != nodeType) {
            continue;
        }

        auto currentNodeIt = _updateTree->nodes().find(moveOp->nodeId());
        // if node exist
        if (currentNodeIt != _updateTree->nodes().end()) {
            // verify if the same node exist
            std::shared_ptr<Node> currentNode = currentNodeIt->second;
            std::shared_ptr<Node> alreadyExistNode = _updateTree->getNodeByPath(moveOp->destinationPath());
            if (alreadyExistNode && alreadyExistNode->isTmp()) {
                // merging nodes we keep currentNode
                mergingTempNodeToRealNode(alreadyExistNode, currentNode);
            }

            // create node if not exist
            std::shared_ptr<Node> parentNode = getOrCreateNodeFromPath(moveOp->destinationPath().parent_path());

            currentNode->insertChangeEvent(OperationTypeMove);
            currentNode->setCreatedAt(moveOp->createdAt());
            currentNode->setLastModified(moveOp->lastModified());
            currentNode->setSize(moveOp->size());
            currentNode->setMoveOriginParentDbId(currentNode->parentNode()->idb());
            currentNode->setIsTmp(false);
            ExitCode tmpExitCode = updateNameFromDbForMoveOp(currentNode, moveOp);
            if (tmpExitCode != ExitCodeOk) {
                return tmpExitCode;
            }

            // delete the current Node from children list of old parent
            if (!currentNode->parentNode()->deleteChildren(currentNode)) {
                LOG_SYNCPAL_WARN(_logger, "children " << currentNodeIt->first.c_str() << " was not affected to parent "
                                                      << (currentNodeIt->second->parentNode()->id().has_value()
                                                              ? *currentNodeIt->second->parentNode()->id()
                                                              : std::string())
                                                             .c_str());
                return ExitCodeDataError;
            }

            currentNode->setName(moveOp->destinationPath().filename().native());

            if (_side == ReplicaSideRemote) {
                currentNode->setValidLocalName(
                    Str(""));  // Clear valid name. If remote name is not valid, it will be fixed by InconsistencyChecker later.
            }

            // set new parent
            currentNode->setParentNode(parentNode);
            currentNode->setMoveOrigin(moveOp->path());
            // insert currentNode into children list of new parent
            parentNode->insertChildren(currentNode);

            if (ParametersCache::instance()->parameters().extendedLog()) {
                LOG_SYNCPAL_DEBUG(
                    _logger,
                    Utility::s2ws(Utility::side2Str(_side)).c_str()
                        << L" update tree: Node " << SyncName2WStr(currentNode->name()).c_str() << L" (node ID: "
                        << Utility::s2ws(currentNode->id().has_value() ? *currentNode->id() : std::string()).c_str()
                        << L", DB ID: " << (currentNode->idb().has_value() ? *currentNode->idb() : -1) << L", parent ID: "
                        << Utility::s2ws(parentNode->id().has_value() ? *parentNode->id() : std::string()).c_str()
                        << L") inserted. Operation " << Utility::s2ws(Utility::opType2Str(moveOp->operationType())).c_str()
                        << L" inserted in change events.");
            }
        } else {
            // get parentNode
            std::shared_ptr<Node> parentNode = getOrCreateNodeFromPath(moveOp->destinationPath().parent_path());

            // create node
            DbNodeId idb;
            bool found = false;
            if (!_syncDb->dbId(_side, moveOp->nodeId(), idb, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbId");
                return ExitCodeDbError;
            }
            if (!found) {
                LOG_SYNCPAL_WARN(_logger, "Node not found for id = " << moveOp->nodeId().c_str());
                return ExitCodeDataError;
            }

            std::shared_ptr<Node> n =
                std::shared_ptr<Node>(new Node(idb, _side, moveOp->destinationPath().filename().native(), moveOp->objectType(),
                                               OperationTypeMove, moveOp->nodeId(), moveOp->createdAt(), moveOp->lastModified(),
                                               moveOp->size(), parentNode, moveOp->path(), std::nullopt));
            if (n == nullptr) {
                std::cout << "Failed to allocate memory" << std::endl;
                LOG_SYNCPAL_ERROR(_logger, "Failed to allocate memory");
                return ExitCodeSystemError;
            }

            ExitCode tmpExitCode = updateNameFromDbForMoveOp(n, moveOp);
            if (tmpExitCode != ExitCodeOk) {
                return tmpExitCode;
            }

            for (auto &child : parentNode->children()) {
                if (child.second->name() == moveOp->destinationPath().filename() && child.second->isTmp()) {
                    mergingTempNodeToRealNode(child.second, n);
                    break;
                }
            }

            parentNode->insertChildren(n);
            _updateTree->nodes()[*n->id()] = n;
            if (ParametersCache::instance()->parameters().extendedLog()) {
                LOG_SYNCPAL_DEBUG(_logger,
                                  Utility::s2ws(Utility::side2Str(_side)).c_str()
                                      << L" update tree: Node " << SyncName2WStr(n->name()).c_str() << L" (node ID: "
                                      << Utility::s2ws(n->id().has_value() ? *n->id() : std::string()).c_str() << L", DB ID: "
                                      << (n->idb().has_value() ? *n->idb() : -1) << L", parent ID: "
                                      << Utility::s2ws(parentNode->id().has_value() ? *parentNode->id() : std::string()).c_str()
                                      << L") inserted. Operation "
                                      << Utility::s2ws(Utility::opType2Str(moveOp->operationType())).c_str()
                                      << L" inserted in change events.");
            }
        }
    }

    return ExitCodeOk;
}

void UpdateTreeWorker::updateNodeId(std::shared_ptr<Node> node, const NodeId &newId) {
    node->parentNode()->deleteChildren(node);
    node->setId(newId);
    node->parentNode()->insertChildren(node);
}

std::shared_ptr<Node> UpdateTreeWorker::getOrCreateNodeFromPath(const SyncPath &path) {
    if (path.empty()) {
        return _updateTree->rootNode();
    }

    // Split path
    std::vector<SyncName> names;
    SyncPath pathTmp(path);
    while (pathTmp != pathTmp.root_path()) {
        names.push_back(
            pathTmp.filename().native());  // TODO : not optimized to do push back on vector, use queue or deque instead
        pathTmp = pathTmp.parent_path();
    }

    // create intermediate nodes if needed
    std::shared_ptr<Node> tmpNode = _updateTree->rootNode();
    for (std::vector<SyncName>::reverse_iterator nameIt = names.rbegin(); nameIt != names.rend(); ++nameIt) {
        std::shared_ptr<Node> tmpChildNode = nullptr;
        for (auto &childNode : tmpNode->children()) {
            if (childNode.second->type() == NodeTypeDirectory && *nameIt == childNode.second->name()) {
                tmpChildNode = childNode.second;
                break;
            }
        }

        if (tmpChildNode == nullptr) {
            // create tmp Node
            tmpChildNode = std::shared_ptr<Node>(new Node(_side, *nameIt, NodeTypeDirectory, tmpNode));

            if (tmpChildNode == nullptr) {
                std::cout << "Failed to allocate memory" << std::endl;
                LOG_SYNCPAL_ERROR(_logger, "Failed to allocate memory");
                return nullptr;
            }

            tmpNode->insertChildren(tmpChildNode);
        }
        tmpNode = tmpChildNode;
    }
    return tmpNode;
}

void UpdateTreeWorker::mergingTempNodeToRealNode(std::shared_ptr<Node> tmpNode, std::shared_ptr<Node> realNode) {
    // merging ids
    if (tmpNode->id().has_value() && !realNode->id().has_value()) {
        // TODO: How is this possible?? Tmp node should NEVER have a valid id and real node should ALWAYS have a valid id
        updateNodeId(realNode, tmpNode->id().value());
        LOG_SYNCPAL_DEBUG(_logger, Utility::s2ws(Utility::side2Str(_side)).c_str()
                                       << L" update tree: Real node ID updated with tmp node ID. Should never happen...");
    }

    // merging tmpNode's children to realNode
    for (auto &child : tmpNode->children()) {
        child.second->setParentNode(realNode);
        realNode->insertChildren(child.second);
    }

    // temp node removed from children list
    std::shared_ptr<Node> parentTmpNode = tmpNode->parentNode();
    parentTmpNode->deleteChildren(tmpNode);

    // real added as child of parent node
    realNode->parentNode()->insertChildren(realNode);
}

bool UpdateTreeWorker::integrityCheck() {
    // TODO : check if this does not slow the process too much
    LOGW_SYNCPAL_INFO(_logger, Utility::side2Str(_side).c_str() << L" update tree integrity check started");
    for (const auto &node : _updateTree->nodes()) {
        if (node.second->isTmp() || Utility::startsWith(*node.second->id(), "tmp_")) {
            LOG_SYNCPAL_WARN(_logger,
                             Utility::s2ws(Utility::side2Str(_side)).c_str()
                                 << L" update tree integrity check failed. A temporary node remains in the update tree: "
                                 << L" (node ID: "
                                 << Utility::s2ws(node.second->id().has_value() ? *node.second->id() : std::string()).c_str()
                                 << L", DB ID: " << (node.second->idb().has_value() ? *node.second->idb() : -1) << L", name: "
                                 << SyncName2WStr(node.second->name()).c_str() << L")");
            return false;
        }
    }
    LOGW_SYNCPAL_INFO(_logger, Utility::side2Str(_side).c_str() << L" update tree integrity check finished");
    return true;
}

ExitCode UpdateTreeWorker::getNewPathAfterMove(const SyncPath &path, SyncPath &newPath) {
    // Split path
    std::vector<std::pair<SyncName, NodeId>> names;
    SyncPath pathTmp(path);
    while (pathTmp != pathTmp.root_path()) {
        names.push_back({pathTmp.filename().native(), "0"});
        pathTmp = pathTmp.parent_path();
    }

    // Vector Id
    SyncPath tmpPath;
    for (std::vector<std::pair<SyncName, NodeId>>::reverse_iterator nameIt = names.rbegin(); nameIt != names.rend(); ++nameIt) {
        tmpPath.append(nameIt->first);
        bool found;
        std::optional<NodeId> tmpNodeId;
        if (!_syncDb->id(_side, tmpPath, tmpNodeId, found)) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::id");
            return ExitCodeDbError;
        }
        if (!found || !tmpNodeId.has_value()) {
            LOGW_SYNCPAL_WARN(_logger, L"Node not found for path=" << Path2WStr(tmpPath).c_str());
            return ExitCodeDataError;
        }
        nameIt->second = *tmpNodeId;
    }

    for (std::vector<std::pair<SyncName, NodeId>>::reverse_iterator nameIt = names.rbegin(); nameIt != names.rend(); ++nameIt) {
        FSOpPtr op = nullptr;
        if (_operationSet->findOp(nameIt->second, OperationTypeMove, op)) {
            newPath = op->destinationPath();
        } else {
            newPath.append(nameIt->first);
        }
    }
    return ExitCodeOk;
}

ExitCode UpdateTreeWorker::updateNodeWithDb(const std::shared_ptr<Node> parentNode) {
    std::queue<std::shared_ptr<Node>> nodeQueue;
    nodeQueue.push(parentNode);

    while (!nodeQueue.empty()) {
        std::shared_ptr<Node> node = nodeQueue.front();
        nodeQueue.pop();

        if (stopAsked()) {
            return ExitCodeOk;
        }

        while (pauseAsked() || isPaused()) {
            if (!isPaused()) {
                setPauseDone();
            }

            Utility::msleep(LOOP_PAUSE_SLEEP_PERIOD);
        }

        bool found = false;

        // update myself
        // if it's a create we don't have node's database data
        if (node->hasChangeEvent(OperationTypeCreate)) {
            continue;
        }

        // if node is temporary node
        if (node->isTmp()) {
            SyncPath dbPath;
            ExitCode tmpExitCode = ExitCodeOk;
            // if it's a move, we need the previous path to be able to retrieve database data from path
            tmpExitCode = getOriginPath(node, dbPath);
            if (tmpExitCode != ExitCodeOk) {
                return tmpExitCode;
            }

            std::optional<NodeId> id;
            if (!_syncDb->id(_side, dbPath, id, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::id");
                return ExitCodeDbError;
            }
            if (!found) {
                LOGW_SYNCPAL_WARN(_logger, L"Node not found in DB for path = "
                                               << Path2WStr(dbPath).c_str() << L" (Node name: "
                                               << SyncName2WStr(node->name()).c_str() << L") on side"
                                               << Utility::s2ws(Utility::side2Str(_side)).c_str());
                return ExitCodeDataError;
            }
            if (!id.has_value()) {
                LOGW_SYNCPAL_WARN(_logger, L"Failed to retrieve ID for node= " << SyncName2WStr(node->name()).c_str());
                return ExitCodeDataError;
            }
            updateNodeId(node, id.value());

            DbNodeId dbId;
            if (!_syncDb->dbId(_side, *id, dbId, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbId");
                return ExitCodeDbError;
            }
            if (!found) {
                LOGW_SYNCPAL_WARN(_logger, L"Node not found for ID = "
                                               << Utility::s2ws(*id).c_str() << L" (Node name: "
                                               << SyncName2WStr(node->name()).c_str() << L", node valid name: "
                                               << SyncName2WStr(node->validLocalName()).c_str() << L") on side"
                                               << Utility::s2ws(Utility::side2Str(_side)).c_str());
                return ExitCodeDataError;
            }
            node->setIdb(dbId);
            node->setIsTmp(false);

            _updateTree->nodes()[*id] = node;
            if (ParametersCache::instance()->parameters().extendedLog()) {
                LOG_SYNCPAL_DEBUG(
                    _logger,
                    Utility::s2ws(Utility::side2Str(_side)).c_str()
                        << L" update tree: Node " << SyncName2WStr(node->name()).c_str() << L" (node ID: "
                        << Utility::s2ws((node->id().has_value() ? *node->id() : std::string())).c_str() << L", DB ID: "
                        << (node->idb().has_value() ? *node->idb() : -1) << L", parent ID: "
                        << Utility::s2ws(node->parentNode()->id().has_value() ? *node->parentNode()->id() : std::string()).c_str()
                        << L") updated. Node updated with DB");
            }
        }

        // use previous nodeId if it's an Edit from Delete-Create
        if (!node->id().has_value()) {
            LOGW_SYNCPAL_WARN(_logger, L"Failed to retrieve ID for node= " << SyncName2WStr(node->name()).c_str());
            return ExitCodeDataError;
        }

        NodeId usableNodeId = node->id().value();
        if (node->isEditFromDeleteCreate()) {
            if (!node->previousId().has_value()) {
                LOGW_SYNCPAL_WARN(_logger, L"Failed to retrieve previousId for node= " << SyncName2WStr(node->name()).c_str());
                return ExitCodeDataError;
            }
            usableNodeId = node->previousId().value();
        }

        DbNode dbNode;
        if (!_syncDb->node(_side, usableNodeId, dbNode, found)) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::node");
            return ExitCodeDbError;
        }
        if (!found) {
            LOG_SYNCPAL_WARN(_logger, "Failed to retrieve node for id=" << usableNodeId.c_str());
            return ExitCodeDataError;
        }

        // if it's a Move event
        if (node->hasChangeEvent(OperationTypeMove)) {
            // update parentDbId
            node->setMoveOriginParentDbId(dbNode.parentNodeId());
            // TODO : should we set moveOrigin too?
        } else {
            if (dbNode.nameLocal() != dbNode.nameRemote()) {
                node->setName(dbNode.nameRemote());
                node->setValidLocalName(dbNode.nameLocal());
            }
        }

        // if it's dbNodeId is null
        if (!node->idb().has_value() && dbNode.nodeId()) {
            node->setIdb(dbNode.nodeId());
        }

        // if it's meta-data is null
        if (!node->createdAt().has_value()) {
            node->setCreatedAt(dbNode.created());
        }
        if (!node->lastmodified().has_value()) {
            node->setLastModified(_side == ReplicaSideLocal ? dbNode.lastModifiedLocal() : dbNode.lastModifiedRemote());
        }
        if (node->size() == 0) {
            node->setSize(dbNode.size());
        }

        for (auto &nodeChild : node->children()) {
            nodeQueue.push(nodeChild.second);
        }
    }

    return ExitCodeOk;
}

ExitCode UpdateTreeWorker::getOriginPath(const std::shared_ptr<Node> node, SyncPath &path) {
    path.clear();

    if (!node) {
        return ExitCodeDataError;
    }

    std::vector<SyncName> names;
    std::shared_ptr<Node> tmpNode = node;
    while (tmpNode && tmpNode->parentNode() != nullptr) {
        if (tmpNode->moveOriginParentDbId().has_value()) {
            // Save origin file name
            names.push_back(tmpNode->moveOrigin().value().filename());

            // Get origin parent
            DbNode dbNode;
            bool found = false;
            if (!_syncDb->node(tmpNode->moveOriginParentDbId().value(), dbNode, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::node");
                return ExitCodeDbError;
            }
            if (!found) {
                LOG_SYNCPAL_WARN(
                    _logger, "Failed to retrieve node for id=" << (node->id().has_value() ? *node->id() : std::string()).c_str());
                return ExitCodeDataError;
            }

            SyncPath localPath;
            SyncPath remotePath;
            if (!_syncDb->path(dbNode.nodeId(), localPath, remotePath, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::path");
                return ExitCodeDbError;
            }
            if (!found) {
                LOG_SYNCPAL_WARN(_logger, "Failed to retrieve node path for nodeId=" << dbNode.nodeId());
                return ExitCodeDataError;
            }

            path = localPath;
            break;
        } else {
            names.push_back(_side == ReplicaSideRemote ? tmpNode->name() : tmpNode->finalLocalName());
            tmpNode = tmpNode->parentNode();
        }
    }

    for (std::vector<SyncName>::reverse_iterator nameIt = names.rbegin(); nameIt != names.rend(); ++nameIt) {
        path /= *nameIt;
    }

    return ExitCodeOk;
}

ExitCode UpdateTreeWorker::updateNameFromDbForMoveOp(const std::shared_ptr<Node> node, FSOpPtr moveOp) {
    if (_side == ReplicaSideRemote) {
        return ExitCodeOk;
    }

    if (moveOp->operationType() != OperationTypeMove) {
        return ExitCodeOk;
    }

    // Does the file has a valid name in DB?
    DbNode dbNode;
    bool found = false;
    if (!_syncDb->node(_side, node->id().has_value() ? *node->id() : std::string(), dbNode, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::node");
        return ExitCodeDbError;
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger,
                         "Failed to retrieve node for id=" << (node->id().has_value() ? *node->id() : std::string()).c_str());
        return ExitCodeDataError;
    }

    if (dbNode.nameLocal() != dbNode.nameRemote()) {  // Useless?? Now the local and remote name are always the same
        // Check if the file has been renamed locally
        if (moveOp->destinationPath().filename() != dbNode.nameLocal()) {
            // The file has been renamed locally, propagate the change on remote
            node->setName(moveOp->destinationPath().filename().native());
            node->setValidLocalName(
                Str(""));  // Clear valid name. Since the change come from the local replica, the name is valid.
        } else {
            // The file has been moved but not renamed, keep the names from DB
            node->setName(dbNode.nameRemote());
            node->setValidLocalName(dbNode.nameLocal());
        }
    }

    return ExitCodeOk;
}

}  // namespace KDC
