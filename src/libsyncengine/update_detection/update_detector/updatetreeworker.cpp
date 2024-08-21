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
#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/utility.h"
#include "requests/parameterscache.h"

#include <iostream>
#include <log4cplus/loggingmacros.h>


namespace KDC {

UpdateTreeWorker::UpdateTreeWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName,
                                   ReplicaSide side)
    : ISyncWorker(syncPal, name, shortName),
      _syncDb(syncPal->_syncDb),
      _operationSet(syncPal->operationSet(side)),
      _updateTree(syncPal->updateTree(side)),
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
    ExitCode exitCode(ExitCode::Unknown);

    LOG_SYNCPAL_DEBUG(_logger, "Worker started: name=" << name().c_str());

    auto start = std::chrono::steady_clock::now();

    _updateTree->startUpdate();

    // Reset nodes working properties
    for (const auto &nodeItem : _updateTree->nodes()) {
        nodeItem.second->clearChangeEvents();
        nodeItem.second->clearConflictAlreadyConsidered();
        nodeItem.second->setInconsistencyType(InconsistencyType::None);
        nodeItem.second->setPreviousId(std::nullopt);
    }

    _updateTree->previousIdSet().clear();

    std::vector<stepptr> steptab = {&UpdateTreeWorker::step1MoveDirectory,   &UpdateTreeWorker::step2MoveFile,
                                    &UpdateTreeWorker::step3DeleteDirectory, &UpdateTreeWorker::step4DeleteFile,
                                    &UpdateTreeWorker::step5CreateDirectory, &UpdateTreeWorker::step6CreateFile,
                                    &UpdateTreeWorker::step7EditFile,        &UpdateTreeWorker::step8CompleteUpdateTree};

    for (auto stepn : steptab) {
        exitCode = (this->*stepn)();
        if (exitCode != ExitCode::Ok) {
            setDone(exitCode);
            return;
        }
        if (stopAsked()) {
            setDone(ExitCode::Ok);
            return;
        }
    }

    integrityCheck();
    drawUpdateTree();

    std::chrono::duration<double> elapsed_seconds = std::chrono::steady_clock::now() - start;

    if (exitCode == ExitCode::Ok) {
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
    return createMoveNodes(NodeType::Directory);
}

ExitCode UpdateTreeWorker::step2MoveFile() {
    return createMoveNodes(NodeType::File);
}

ExitCode UpdateTreeWorker::searchForParentNode(const SyncPath &nodePath, std::shared_ptr<Node> &parentNode) {
    parentNode.reset();
    std::optional<NodeId> parentNodeId;
    bool found = false;
    if (!_syncDb->id(_side, nodePath.parent_path(), parentNodeId, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::id");
        return ExitCode::DbError;
    }

    if (found && parentNodeId) {
        if (auto parentNodeIt = _updateTree->nodes().find(*parentNodeId); parentNodeIt != _updateTree->nodes().end()) {
            // The parent node exists.
            parentNode = parentNodeIt->second;
        }
    }

    return ExitCode::Ok;
}

ExitCode UpdateTreeWorker::step3DeleteDirectory() {
    std::unordered_set<UniqueId> deleteOpsIds = _operationSet->getOpsByType(OperationType::Delete);
    for (const auto &deleteOpId : deleteOpsIds) {
        // worker stop or pause
        if (stopAsked()) {
            return ExitCode::Ok;
        }

        while (pauseAsked() || isPaused()) {
            if (!isPaused()) {
                setPauseDone();
            }

            Utility::msleep(LOOP_PAUSE_SLEEP_PERIOD);
        }

        FSOpPtr deleteOp = nullptr;
        _operationSet->getOp(deleteOpId, deleteOp);

        if (deleteOp->objectType() != NodeType::Directory) {
            continue;
        }

        auto currentNodeIt = _updateTree->nodes().find(deleteOp->nodeId());
        if (currentNodeIt != _updateTree->nodes().end()) {
            // Node exists
            currentNodeIt->second->insertChangeEvent(OperationType::Delete);
            currentNodeIt->second->setCreatedAt(deleteOp->createdAt());
            currentNodeIt->second->setLastModified(deleteOp->lastModified());
            currentNodeIt->second->setSize(deleteOp->size());
            currentNodeIt->second->setIsTmp(false);
            if (ParametersCache::isExtendedLogEnabled()) {
                LOGW_SYNCPAL_DEBUG(
                    _logger,
                    Utility::s2ws(Utility::side2Str(_side)).c_str()
                        << L" update tree: Node '" << SyncName2WStr(currentNodeIt->second->name()).c_str() << L"' (node ID: '"
                        << Utility::s2ws(currentNodeIt->second->id().has_value() ? *currentNodeIt->second->id() : NodeId())
                               .c_str()
                        << L"', DB ID: '" << (currentNodeIt->second->idb().has_value() ? *currentNodeIt->second->idb() : -1)
                        << L"') updated. Operation DELETE inserted in change events.");
            }
        } else {
            std::shared_ptr<Node> parentNode;
            if (const auto searchExitCode = searchForParentNode(deleteOp->path(), parentNode); searchExitCode != ExitCode::Ok) {
                return searchExitCode;
            };

            if (!parentNode) {
                SyncPath newPath;
                if (const auto newPathExitCode = getNewPathAfterMove(deleteOp->path(), newPath);
                    newPathExitCode != ExitCode::Ok) {
                    LOG_SYNCPAL_WARN(_logger, "Error in UpdateTreeWorker::getNewPathAfterMove");
                    return newPathExitCode;
                }
                parentNode = getOrCreateNodeFromDeletedPath(newPath.parent_path());
            }

            // Find dbNodeId
            DbNodeId idb = 0;
            bool found = false;
            if (!_syncDb->dbId(_side, deleteOp->nodeId(), idb, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbId");
                return ExitCode::DbError;
            }
            if (!found) {
                LOG_SYNCPAL_WARN(_logger, "Node not found for id = " << deleteOp->nodeId().c_str());
                return ExitCode::DataError;
            }

            // Check if parentNode has got a child with the same name
            std::shared_ptr<Node> newNode = parentNode->findChildrenById(deleteOp->nodeId());
            if (newNode != nullptr) {
                // Node already exists, update it
                newNode->setIdb(idb);
                if (!updateNodeId(newNode, deleteOp->nodeId())) {
                    LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTreeWorker::updateNodeId");
                    return ExitCode::DataError;
                }
                newNode->setCreatedAt(deleteOp->createdAt());
                newNode->setLastModified(deleteOp->lastModified());
                newNode->setSize(deleteOp->size());
                newNode->insertChangeEvent(OperationType::Delete);
                newNode->setIsTmp(false);
                _updateTree->nodes()[deleteOp->nodeId()] = newNode;
                if (ParametersCache::isExtendedLogEnabled()) {
                    LOGW_SYNCPAL_DEBUG(
                        _logger, Utility::s2ws(Utility::side2Str(_side)).c_str()
                                     << L" update tree: Node '" << SyncName2WStr(newNode->name()).c_str() << L"' (node ID: '"
                                     << Utility::s2ws(newNode->id().has_value() ? *newNode->id() : NodeId()).c_str()
                                     << L"', DB ID: '" << (newNode->idb().has_value() ? *newNode->idb() : -1)
                                     << L"') updated. Operation DELETE inserted in change events.");
                }
            } else {
                // create node
                newNode = std::shared_ptr<Node>(new Node(idb, _side, deleteOp->path().filename().native(), deleteOp->objectType(),
                                                         OperationType::Delete, deleteOp->nodeId(), deleteOp->createdAt(),
                                                         deleteOp->lastModified(), deleteOp->size(), parentNode));
                if (newNode == nullptr) {
                    std::cout << "Failed to allocate memory" << std::endl;
                    LOG_SYNCPAL_ERROR(_logger, "Failed to allocate memory");
                    return ExitCode::SystemError;
                }

                if (!parentNode->insertChildren(newNode)) {
                    LOGW_SYNCPAL_WARN(_logger, L"Error in Node::insertChildren: node name="
                                                   << SyncName2WStr(newNode->name()).c_str()
                                                   << " parent node name=" << SyncName2WStr(parentNode->name()).c_str());
                    return ExitCode::DataError;
                }

                _updateTree->nodes()[deleteOp->nodeId()] = newNode;
                if (ParametersCache::isExtendedLogEnabled()) {
                    LOGW_SYNCPAL_DEBUG(
                        _logger, Utility::s2ws(Utility::side2Str(_side)).c_str()
                                     << L" update tree: Node '" << SyncName2WStr(newNode->name()).c_str() << L"' (node ID: '"
                                     << Utility::s2ws(newNode->id().has_value() ? *newNode->id() : NodeId()).c_str()
                                     << L"', DB ID: '" << (newNode->idb().has_value() ? *newNode->idb() : -1)
                                     << L"', parent ID: '"
                                     << Utility::s2ws(parentNode->id().has_value() ? *parentNode->id() : NodeId()).c_str()
                                     << L"') inserted. Operation DELETE inserted in change events.");
                }
            }
        }
    }
    return ExitCode::Ok;
}

ExitCode UpdateTreeWorker::handleCreateOperationsWithSamePath() {
    _createFileOperationSet.clear();
    FSOpPtrMap createDirectoryOperationSet;
    std::unordered_set<UniqueId> createOpsIds = _operationSet->getOpsByType(OperationType::Create);

    bool isSnapshotRebuildRequired = false;

    for (const auto &createOpId : createOpsIds) {
        // worker stop or pause
        if (stopAsked()) {
            return ExitCode::Ok;
        }
        while (pauseAsked() || isPaused()) {
            if (!isPaused()) {
                setPauseDone();
            }

            Utility::msleep(LOOP_PAUSE_SLEEP_PERIOD);
        }

        FSOpPtr createOp;
        _operationSet->getOp(createOpId, createOp);
        const auto normalizedPath = createOp->path();

        std::pair<FSOpPtrMap::iterator, bool> insertionResult;
        switch (createOp->objectType()) {
            case NodeType::File:
                insertionResult = _createFileOperationSet.try_emplace(normalizedPath, createOp);
                break;
            case NodeType::Directory:
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

            if (_syncPal) {  // `_syncPal`can be set with `nullptr` in tests.
                _syncPal->increaseErrorCount(createOp->nodeId(), createOp->objectType(), createOp->path(), _side);
                if (_syncPal->getErrorCount(createOp->nodeId(), _side) > 1) {
                    // We are in situation (2), i.e. duplicate normalized names.
                    // We display to the user an explicit error message about item name inconsistency.
                    Error err(_syncPal->syncDbId(), "", createOp->nodeId(), createOp->objectType(), createOp->path(),
                              ConflictType::None, InconsistencyType::DuplicateNames, CancelType::None);
                    _syncPal->addError(err);
                }
            }

            isSnapshotRebuildRequired = true;
        }
    }

    return isSnapshotRebuildRequired ? ExitCode::DataError : ExitCode::Ok;
}

void UpdateTreeWorker::logUpdate(const std::shared_ptr<Node> node, const OperationType opType,
                                 const std::shared_ptr<Node> parentNode) {
    if (!ParametersCache::isExtendedLogEnabled()) return;

    std::wstring parentIdStr;
    std::wstring updateTypeStr = L"updated";
    if (parentNode) {
        parentIdStr = L"parent ID: '" + Utility::s2ws(parentNode->id() ? *parentNode->id() : NodeId()) + L"'";
        updateTypeStr = L"inserted";
    }

    const std::wstring updateTreeStr = Utility::s2ws(Utility::side2Str(_side)) + L" update tree";
    const std::wstring nodeStr = L"Node '" + SyncName2WStr(node->name()) + L"'";
    const std::wstring nodeIdStr = L"node ID: '" + Utility::s2ws(node->id() ? *node->id() : NodeId()) + L"'";
    const std::wstring dbIdStr = L"DB ID: '" + std::to_wstring(node->idb() ? *node->idb() : -1) + L"'";
    const std::wstring opTypeStr = Utility::s2ws(Utility::opType2Str(opType));

    LOGW_SYNCPAL_DEBUG(_logger, updateTreeStr.c_str()
                                    << L": " << nodeStr.c_str() << L" (" << nodeIdStr.c_str() << L", " << dbIdStr.c_str() << L", "
                                    << parentIdStr.c_str() << L") " << updateTypeStr.c_str() << L". Operation "
                                    << opTypeStr.c_str() << L" inserted in change events.");
}

bool UpdateTreeWorker::updateTmpFileNode(std::shared_ptr<Node> newNode, const FSOpPtr op, const FSOpPtr deleteOp,
                                         OperationType opType) {
    assert(newNode != nullptr && newNode->isTmp());

    if (!updateNodeId(newNode, op->nodeId())) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTreeWorker::updateNodeId");
        return false;
    }
    newNode->setCreatedAt(op->createdAt());
    newNode->setLastModified(op->lastModified());
    newNode->setSize(op->size());
    newNode->insertChangeEvent(opType);
    newNode->setIsTmp(false);

    if (opType == OperationType::Edit) {
        newNode->setPreviousId(deleteOp->nodeId());
        _updateTree->previousIdSet()[deleteOp->nodeId()] = op->nodeId();
    }

    _updateTree->nodes()[op->nodeId()] = newNode;
    logUpdate(newNode, opType);

    return true;
}

ExitCode UpdateTreeWorker::step4DeleteFile() {
    const ExitCode exitCode = handleCreateOperationsWithSamePath();
    if (exitCode != ExitCode::Ok) return exitCode;  // Rebuild the snapshot.

    std::unordered_set<UniqueId> deleteOpsIds = _operationSet->getOpsByType(OperationType::Delete);
    for (const auto &deleteOpId : deleteOpsIds) {
        // worker stop or pause
        if (stopAsked()) {
            return ExitCode::Ok;
        }
        while (pauseAsked() || isPaused()) {
            if (!isPaused()) {
                setPauseDone();
            }

            Utility::msleep(LOOP_PAUSE_SLEEP_PERIOD);
        }

        FSOpPtr deleteOp = nullptr;
        _operationSet->getOp(deleteOpId, deleteOp);
        if (deleteOp->objectType() != NodeType::File) {
            continue;
        }

        FSOpPtr op = deleteOp;
        if (_side == ReplicaSide::Local) {
            // Transform a Delete and a Create operations into one Edit operation.
            // Some software, such as Excel, keeps the current version into a temporary directory and move it to the destination,
            // replacing the original file. However, this behavior should be applied only on local side.
            if (auto createFileOpSetIt = _createFileOperationSet.find(deleteOp->path());
                createFileOpSetIt != _createFileOperationSet.end()) {
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

        const OperationType opType = op->operationType() == OperationType::Create ? OperationType::Edit : OperationType::Delete;
        if (auto currentNodeIt = _updateTree->nodes().find(deleteOp->nodeId()); currentNodeIt != _updateTree->nodes().end()) {
            // Node is already in the update tree, it can be a Delete or an Edit
            std::shared_ptr<Node> currentNode = currentNodeIt->second;
            if (!updateNodeId(currentNode, op->nodeId())) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTreeWorker::updateNodeId");
                return ExitCode::DataError;
            }
            currentNode->setCreatedAt(op->createdAt());
            currentNode->setLastModified(op->lastModified());
            currentNode->setSize(op->size());
            currentNode->insertChangeEvent(opType);
            currentNode->setIsTmp(false);

            // TODO: refactor other OperationType::Edit conditions

            // If it's an edit (Delete-Create) we change it's id
            if (opType == OperationType::Edit) {
                currentNode->setPreviousId(deleteOp->nodeId());
                _updateTree->previousIdSet()[deleteOp->nodeId()] = op->nodeId();

                // replace node in _nodes map because id changed
                auto node = _updateTree->nodes().extract(deleteOp->nodeId());
                node.key() = op->nodeId();
                _updateTree->nodes().insert(std::move(node));
            }

            logUpdate(currentNode, opType);
        } else {
            std::shared_ptr<Node> parentNode;
            if (const auto exitCode = searchForParentNode(op->path(), parentNode); exitCode != ExitCode::Ok) {
                return exitCode;
            };

            if (!parentNode) {
                parentNode = getOrCreateNodeFromDeletedPath(op->path().parent_path());
            }

            // find child node
            std::shared_ptr<Node> newNode = parentNode->findChildrenById(deleteOp->nodeId());
            if (newNode != nullptr && newNode->isTmp()) {
                // Tmp node already exists, update it
                if (!updateTmpFileNode(newNode, op, deleteOp, opType)) {
                    LOG_SYNCPAL_WARN(_logger, "Error in UpdateTreeWorker::updateTmpFileNode");
                    return ExitCode::DataError;
                }
            } else {
                // create node
                DbNodeId idb = 0;
                bool found = false;
                if (!_syncDb->dbId(_side, deleteOp->nodeId(), idb, found)) {
                    LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbId");
                    return ExitCode::DbError;
                }
                if (!found) {
                    LOG_SYNCPAL_WARN(_logger, "Node not found for id = " << deleteOp->nodeId().c_str());
                    return ExitCode::DataError;
                }

                newNode =
                    std::shared_ptr<Node>(new Node(idb, _side, op->path().filename().native(), op->objectType(), opType,
                                                   op->nodeId(), op->createdAt(), op->lastModified(), op->size(), parentNode));
                if (newNode == nullptr) {
                    std::cout << "Failed to allocate memory" << std::endl;
                    LOG_SYNCPAL_ERROR(_logger, "Failed to allocate memory");
                    return ExitCode::SystemError;
                }

                // store old NodeId to retrieve node in Db
                if (opType == OperationType::Edit) {
                    newNode->setPreviousId(deleteOp->nodeId());
                    _updateTree->previousIdSet()[deleteOp->nodeId()] = op->nodeId();
                }

                if (!parentNode->insertChildren(newNode)) {
                    LOGW_SYNCPAL_WARN(_logger, L"Error in Node::insertChildren: node name="
                                                   << SyncName2WStr(newNode->name()).c_str()
                                                   << " parent node name=" << SyncName2WStr(parentNode->name()).c_str());
                    return ExitCode::DataError;
                }

                _updateTree->nodes()[op->nodeId()] = newNode;
                logUpdate(newNode, opType, parentNode);
            }
        }
    }

    return ExitCode::Ok;
}

ExitCode UpdateTreeWorker::step5CreateDirectory() {
    std::unordered_set<UniqueId> createOpsIds = _operationSet->getOpsByType(OperationType::Create);
    for (const auto &createOpId : createOpsIds) {
        // worker stop or pause
        if (stopAsked()) {
            return ExitCode::Ok;
        }

        while (pauseAsked() || isPaused()) {
            if (!isPaused()) {
                setPauseDone();
            }

            Utility::msleep(LOOP_PAUSE_SLEEP_PERIOD);
        }

        FSOpPtr createOp = nullptr;
        _operationSet->getOp(createOpId, createOp);
        if (createOp->objectType() != NodeType::Directory) {
            continue;
        }

        // find node by path because it may have been created before
        std::shared_ptr<Node> currentNode = getOrCreateNodeFromExistingPath(createOp->path());
        if (currentNode->hasChangeEvent(OperationType::Delete)) {
            // A directory has been deleted and another one has been created with the same name
            currentNode->setPreviousId(currentNode->id());
        }
        if (!updateNodeId(currentNode, createOp->nodeId())) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTreeWorker::updateNodeId");
            return ExitCode::DataError;
        }
        currentNode->setCreatedAt(createOp->createdAt());
        currentNode->setLastModified(createOp->lastModified());
        currentNode->setSize(createOp->size());
        currentNode->insertChangeEvent(createOp->operationType());
        currentNode->setIsTmp(false);
        _updateTree->nodes()[createOp->nodeId()] = currentNode;
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_SYNCPAL_DEBUG(
                _logger,
                Utility::s2ws(Utility::side2Str(_side)).c_str()
                    << L" update tree: Node '" << SyncName2WStr(currentNode->name()).c_str() << L"' (node ID: '"
                    << Utility::s2ws(currentNode->id().has_value() ? *currentNode->id() : std::string()).c_str() << L"', DB ID: '"
                    << (currentNode->idb().has_value() ? *currentNode->idb() : -1) << L"') inserted. Operation "
                    << Utility::s2ws(Utility::opType2Str(createOp->operationType())).c_str() << L" inserted in change events.");
        }
    }
    return ExitCode::Ok;
}

ExitCode UpdateTreeWorker::step6CreateFile() {
    for (const auto &op : _createFileOperationSet) {
        // worker stop or pause
        if (stopAsked()) {
            return ExitCode::Ok;
        }
        while (pauseAsked() || isPaused()) {
            if (!isPaused()) {
                setPauseDone();
            }

            Utility::msleep(LOOP_PAUSE_SLEEP_PERIOD);
        }

        FSOpPtr operation = op.second;

        // find parentNode by path
        std::shared_ptr<Node> parentNode = getOrCreateNodeFromExistingPath(operation->path().parent_path());
        std::shared_ptr<Node> newNode = parentNode->findChildrenById(operation->nodeId());
        if (newNode != nullptr) {
            // Node already exists, update it
            if (newNode->name() == operation->path().filename().native()) {
                if (!updateNodeId(newNode, operation->nodeId())) {
                    LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTreeWorker::updateNodeId");
                    return ExitCode::DataError;
                }
                newNode->setCreatedAt(operation->createdAt());
                newNode->setLastModified(operation->lastModified());
                newNode->setSize(operation->size());
                newNode->insertChangeEvent(operation->operationType());
                newNode->setIsTmp(false);

                _updateTree->nodes()[operation->nodeId()] = newNode;
                if (ParametersCache::isExtendedLogEnabled()) {
                    LOGW_SYNCPAL_DEBUG(
                        _logger,
                        Utility::s2ws(Utility::side2Str(_side)).c_str()
                            << L" update tree: Node '" << SyncName2WStr(newNode->name()).c_str() << L"' (node ID: '"
                            << Utility::s2ws(newNode->id().has_value() ? *newNode->id() : std::string()).c_str() << L"', DB ID: '"
                            << (newNode->idb().has_value() ? *newNode->idb() : -1) << L"', parent ID: '"
                            << Utility::s2ws(parentNode->id().has_value() ? *parentNode->id() : "-1").c_str()
                            << L"') updated. Operation " << Utility::s2ws(Utility::opType2Str(operation->operationType())).c_str()
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
            return ExitCode::SystemError;
        }

        if (!parentNode->insertChildren(newNode)) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in Node::insertChildren: node name="
                                           << SyncName2WStr(newNode->name()).c_str()
                                           << " parent node name=" << SyncName2WStr(parentNode->name()).c_str());
            return ExitCode::DataError;
        }

        _updateTree->nodes()[operation->nodeId()] = newNode;
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_SYNCPAL_DEBUG(_logger,
                               Utility::s2ws(Utility::side2Str(_side)).c_str()
                                   << L" update tree: Node '" << SyncName2WStr(newNode->name()).c_str() << L"' (node ID: '"
                                   << Utility::s2ws(newNode->id().has_value() ? *newNode->id() : "").c_str() << L"', DB ID: '"
                                   << (newNode->idb().has_value() ? *newNode->idb() : -1) << L"', parent ID: '"
                                   << Utility::s2ws(parentNode->id().has_value() ? *parentNode->id() : "-1").c_str()
                                   << L"') inserted. Operation "
                                   << Utility::s2ws(Utility::opType2Str(operation->operationType())).c_str()
                                   << L" inserted in change events.");
        }
    }
    return ExitCode::Ok;
}

ExitCode UpdateTreeWorker::step7EditFile() {
    std::unordered_set<UniqueId> editOpsIds = _operationSet->getOpsByType(OperationType::Edit);
    for (const auto &editOpId : editOpsIds) {
        // worker stop or pause
        if (stopAsked()) {
            return ExitCode::Ok;
        }

        while (pauseAsked() || isPaused()) {
            if (!isPaused()) {
                setPauseDone();
            }

            Utility::msleep(LOOP_PAUSE_SLEEP_PERIOD);
        }

        FSOpPtr editOp = nullptr;
        _operationSet->getOp(editOpId, editOp);
        if (editOp->objectType() != NodeType::File) {
            continue;
        }
        // find parentNode by path because should have been created
        std::shared_ptr<Node> parentNode = getOrCreateNodeFromExistingPath(editOp->path().parent_path());
        std::shared_ptr<Node> newNode = parentNode->findChildrenById(editOp->nodeId());
        if (newNode != nullptr) {
            // Node already exists, update it
            newNode->setCreatedAt(editOp->createdAt());
            newNode->setLastModified(editOp->lastModified());
            newNode->setSize(editOp->size());
            newNode->insertChangeEvent(editOp->operationType());
            if (!updateNodeId(newNode, editOp->nodeId())) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTreeWorker::updateNodeId");
                return ExitCode::DataError;
            }
            newNode->setIsTmp(false);

            _updateTree->nodes()[editOp->nodeId()] = newNode;
            if (ParametersCache::isExtendedLogEnabled()) {
                LOGW_SYNCPAL_DEBUG(
                    _logger,
                    Utility::s2ws(Utility::side2Str(_side)).c_str()
                        << L" update tree: Node '" << SyncName2WStr(newNode->name()).c_str() << L"' (node ID: '"
                        << Utility::s2ws(newNode->id().has_value() ? *newNode->id() : std::string()).c_str() << L"', DB ID: '"
                        << (newNode->idb().has_value() ? *newNode->idb() : -1) << L"') updated. Operation "
                        << Utility::s2ws(Utility::opType2Str(editOp->operationType())).c_str() << L" inserted in change events.");
            }
            continue;
        }

        // create node
        DbNodeId idb;
        bool found = false;
        if (!_syncDb->dbId(_side, editOp->nodeId(), idb, found)) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbId");
            return ExitCode::DbError;
        }
        if (!found) {
            LOG_SYNCPAL_WARN(_logger, "Node not found for id = " << editOp->nodeId().c_str());
            return ExitCode::DataError;
        }

        newNode = std::shared_ptr<Node>(new Node(idb, _side, editOp->path().filename().native(), editOp->objectType(),
                                                 editOp->operationType(), editOp->nodeId(), editOp->createdAt(),
                                                 editOp->lastModified(), editOp->size(), parentNode));
        if (newNode == nullptr) {
            std::cout << "Failed to allocate memory" << std::endl;
            LOG_SYNCPAL_ERROR(_logger, "Failed to allocate memory");
            return ExitCode::SystemError;
        }

        if (!parentNode->insertChildren(newNode)) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in Node::insertChildren: node name="
                                           << SyncName2WStr(newNode->name()).c_str()
                                           << " parent node name=" << SyncName2WStr(parentNode->name()).c_str());
            return ExitCode::DataError;
        }

        _updateTree->nodes()[editOp->nodeId()] = newNode;
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_SYNCPAL_DEBUG(
                _logger, Utility::s2ws(Utility::side2Str(_side)).c_str()
                             << L" update tree: Node '" << SyncName2WStr(newNode->name()).c_str() << L"' (node ID: '"
                             << Utility::s2ws(newNode->id().has_value() ? *newNode->id() : std::string()).c_str()
                             << L"', DB ID: '" << (newNode->idb().has_value() ? *newNode->idb() : -1) << L"', parent ID: '"
                             << Utility::s2ws(parentNode->id().has_value() ? *parentNode->id() : std::string()).c_str()
                             << L"') inserted. editOp " << Utility::s2ws(Utility::opType2Str(editOp->operationType())).c_str()
                             << L" inserted in change events.");
        }
    }

    return ExitCode::Ok;
}

ExitCode UpdateTreeWorker::step8CompleteUpdateTree() {
    if (stopAsked()) {
        return ExitCode::Ok;
    }

    bool found = false;
    std::vector<NodeId> dbNodeIds;
    if (!_syncDb->ids(_side, dbNodeIds, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbNodeIds");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_SYNCPAL_DEBUG(_logger, "There is no dbNodeIds for side=" << enumClassToInt(_side));
        return ExitCode::Ok;
    }

    // update each existing nodes
    ExitCode exitCode = ExitCode::Unknown;
    try {
        exitCode = updateNodeWithDb(_updateTree->rootNode());
    } catch (std::exception &e) {
        LOG_WARN(_logger, "updateNodeWithDb failed - err= " << e.what());
        return ExitCode::DataError;
    }

    if (exitCode != ExitCode::Ok) {
        LOG_SYNCPAL_WARN(_logger, "Error in updateNodeWithDb");
        return exitCode;
    }

    std::optional<NodeId> rootNodeId =
        (_side == ReplicaSide::Local ? _syncDb->rootNode().nodeIdLocal() : _syncDb->rootNode().nodeIdRemote());

    // creating missing nodes
    for (const NodeId &dbNodeId : dbNodeIds) {
        // worker stop or pause
        if (stopAsked()) {
            return ExitCode::Ok;
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
                return ExitCode::DbError;
            }
            if (!found) {
                LOG_SYNCPAL_WARN(_logger, "Node or parent node not found for dbNodeId = " << previousNodeId.c_str());
                return ExitCode::DataError;
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
                return ExitCode::DbError;
            }
            if (!found) {
                LOG_SYNCPAL_WARN(_logger, "Failed to retrieve node for dbId=" << previousNodeId.c_str());
                return ExitCode::DataError;
            }

            SyncTime lastModified =
                _side == ReplicaSide::Local ? dbNode.lastModifiedLocal().value() : dbNode.lastModifiedRemote().value();
            SyncName name = dbNode.nameRemote();
            std::shared_ptr<Node> newNode =
                std::shared_ptr<Node>(new Node(dbNode.nodeId(), _side, name, dbNode.type(), {}, newNodeId, dbNode.created(),
                                               lastModified, dbNode.size(), parentNode));
            if (newNode == nullptr) {
                std::cout << "Failed to allocate memory" << std::endl;
                LOG_SYNCPAL_ERROR(_logger, "Failed to allocate memory");
                return ExitCode::SystemError;
            }

            if (!parentNode->insertChildren(newNode)) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in Node::insertChildren: node name="
                                               << SyncName2WStr(newNode->name()).c_str()
                                               << " parent node name=" << SyncName2WStr(parentNode->name()).c_str());
                return ExitCode::DataError;
            }

            _updateTree->nodes()[newNodeId] = newNode;
            if (ParametersCache::isExtendedLogEnabled()) {
                LOGW_SYNCPAL_DEBUG(
                    _logger, Utility::s2ws(Utility::side2Str(_side)).c_str()
                                 << L" update tree: Node '" << SyncName2WStr(newNode->name()).c_str() << L"' (node ID: '"
                                 << Utility::s2ws(newNode->id().has_value() ? *newNode->id() : std::string()).c_str()
                                 << L"', DB ID: '" << (newNode->idb().has_value() ? *newNode->idb() : 1) << L"', parent ID: '"
                                 << Utility::s2ws(parentNode->id().has_value() ? *parentNode->id() : std::string()).c_str()
                                 << L"') inserted. Without change events.");
            }
        }
    }

    return exitCode;
}

ExitCode UpdateTreeWorker::createMoveNodes(const NodeType &nodeType) {
    std::unordered_set<UniqueId> moveOpsIds = _operationSet->getOpsByType(OperationType::Move);
    for (const auto &moveOpId : moveOpsIds) {
        if (stopAsked()) {
            return ExitCode::Ok;
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
                if (!mergingTempNodeToRealNode(alreadyExistNode, currentNode)) {
                    LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTreeWorker::mergingTempNodeToRealNode");
                    return ExitCode::DataError;
                }
            }

            // create node if not exist
            std::shared_ptr<Node> parentNode = getOrCreateNodeFromExistingPath(moveOp->destinationPath().parent_path());

            currentNode->insertChangeEvent(OperationType::Move);
            currentNode->setCreatedAt(moveOp->createdAt());
            currentNode->setLastModified(moveOp->lastModified());
            currentNode->setSize(moveOp->size());
            currentNode->setMoveOriginParentDbId(currentNode->parentNode()->idb());
            currentNode->setIsTmp(false);
            ExitCode tmpExitCode = checkNodeIsFoundInDbForMoveOp(currentNode, moveOp);
            if (tmpExitCode != ExitCode::Ok) {
                return tmpExitCode;
            }

            // delete the current Node from children list of old parent
            if (!currentNode->parentNode()->deleteChildren(currentNode)) {
                LOG_SYNCPAL_WARN(_logger, "children " << currentNodeIt->first.c_str() << " was not affected to parent "
                                                      << (currentNodeIt->second->parentNode()->id().has_value()
                                                              ? *currentNodeIt->second->parentNode()->id()
                                                              : std::string())
                                                             .c_str());
                return ExitCode::DataError;
            }

            currentNode->setName(moveOp->destinationPath().filename().native());

            // set new parent
            if (!currentNode->setParentNode(parentNode)) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in Node::setParentNode: node name="
                                               << SyncName2WStr(parentNode->name()).c_str()
                                               << " parent node name=" << SyncName2WStr(currentNode->name()).c_str());
                return ExitCode::DataError;
            }

            currentNode->setMoveOrigin(moveOp->path());

            // insert currentNode into children list of new parent
            if (!parentNode->insertChildren(currentNode)) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in Node::insertChildren: node name="
                                               << SyncName2WStr(currentNode->name()).c_str()
                                               << " parent node name=" << SyncName2WStr(parentNode->name()).c_str());
                return ExitCode::DataError;
            }

            if (ParametersCache::isExtendedLogEnabled()) {
                LOGW_SYNCPAL_DEBUG(
                    _logger,
                    Utility::s2ws(Utility::side2Str(_side)).c_str()
                        << L" update tree: Node '" << SyncName2WStr(currentNode->name()).c_str() << L"' (node ID: '"
                        << Utility::s2ws(currentNode->id().has_value() ? *currentNode->id() : std::string()).c_str()
                        << L"', DB ID: '" << (currentNode->idb().has_value() ? *currentNode->idb() : -1) << L"', parent ID: '"
                        << Utility::s2ws(parentNode->id().has_value() ? *parentNode->id() : std::string()).c_str()
                        << L"') inserted. Operation " << Utility::s2ws(Utility::opType2Str(moveOp->operationType())).c_str()
                        << L" inserted in change events.");
            }
        } else {
            // get parentNode
            std::shared_ptr<Node> parentNode = getOrCreateNodeFromExistingPath(moveOp->destinationPath().parent_path());

            // create node
            DbNodeId idb;
            bool found = false;
            if (!_syncDb->dbId(_side, moveOp->nodeId(), idb, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbId");
                return ExitCode::DbError;
            }
            if (!found) {
                LOG_SYNCPAL_WARN(_logger, "Node not found for id = " << moveOp->nodeId().c_str());
                return ExitCode::DataError;
            }

            std::shared_ptr<Node> newNode =
                std::make_shared<Node>(idb, _side, moveOp->destinationPath().filename().native(), moveOp->objectType(),
                                       OperationType::Move, moveOp->nodeId(), moveOp->createdAt(), moveOp->lastModified(),
                                       moveOp->size(), parentNode, moveOp->path(), std::nullopt);
            if (newNode == nullptr) {
                std::cout << "Failed to allocate memory" << std::endl;
                LOG_SYNCPAL_ERROR(_logger, "Failed to allocate memory");
                return ExitCode::SystemError;
            }

            ExitCode tmpExitCode = checkNodeIsFoundInDbForMoveOp(newNode, moveOp);
            if (tmpExitCode != ExitCode::Ok) {
                return tmpExitCode;
            }

            for (auto &child : parentNode->children()) {
                if (child.second->name() == moveOp->destinationPath().filename() && child.second->isTmp()) {
                    if (!mergingTempNodeToRealNode(child.second, newNode)) {
                        LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTreeWorker::mergingTempNodeToRealNode");
                        return ExitCode::DataError;
                    }

                    break;
                }
            }

            if (!parentNode->insertChildren(newNode)) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in Node::insertChildren: node name="
                                               << SyncName2WStr(newNode->name()).c_str()
                                               << " parent node name=" << SyncName2WStr(parentNode->name()).c_str());
                return ExitCode::DataError;
            }

            _updateTree->nodes()[*newNode->id()] = newNode;
            if (ParametersCache::isExtendedLogEnabled()) {
                LOGW_SYNCPAL_DEBUG(
                    _logger, Utility::s2ws(Utility::side2Str(_side)).c_str()
                                 << L" update tree: Node '" << SyncName2WStr(newNode->name()).c_str() << L"' (node ID: '"
                                 << Utility::s2ws(newNode->id().has_value() ? *newNode->id() : std::string()).c_str()
                                 << L"', DB ID: '" << (newNode->idb().has_value() ? *newNode->idb() : -1) << L"', parent ID: '"
                                 << Utility::s2ws(parentNode->id().has_value() ? *parentNode->id() : std::string()).c_str()
                                 << L"') inserted. Operation "
                                 << Utility::s2ws(Utility::opType2Str(moveOp->operationType())).c_str()
                                 << L" inserted in change events.");
            }
        }
    }

    return ExitCode::Ok;
}

bool UpdateTreeWorker::updateNodeId(std::shared_ptr<Node> node, const NodeId &newId) {
    const NodeId oldId = node->id().has_value() ? *node->id() : "";

    node->parentNode()->deleteChildren(node);
    node->setId(newId);

    if (!node->parentNode()->insertChildren(node)) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in Node::insertChildren: node name="
                                       << SyncName2WStr(node->name()).c_str()
                                       << " parent node name=" << SyncName2WStr(node->parentNode()->name()).c_str());
        return false;
    }

    if (ParametersCache::isExtendedLogEnabled() && newId != oldId) {
        LOGW_SYNCPAL_DEBUG(_logger, Utility::s2ws(Utility::side2Str(_side)).c_str()
                                        << L" update tree: Node ID changed from '" << Utility::s2ws(oldId).c_str() << L"' to '"
                                        << Utility::s2ws(newId).c_str() << L"' for node '" << SyncName2WStr(node->name()).c_str()
                                        << L"'.");
    }

    return true;
}

std::shared_ptr<Node> UpdateTreeWorker::getOrCreateNodeFromPath(const SyncPath &path, bool isDeleted) {
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
    for (auto nameIt = names.rbegin(); nameIt != names.rend(); ++nameIt) {
        std::shared_ptr<Node> tmpChildNode = nullptr;
        for (auto &[_, childNode] : tmpNode->children()) {
            if (childNode->type() == NodeType::Directory && *nameIt == childNode->name()) {
                if (!isDeleted && childNode->hasChangeEvent(OperationType::Delete)) {
                    // An item on a deleted branch can only have a DELETE change event. If it has any other
                    // change event, it means its parent is on a different branch.
                    continue;
                }
                tmpChildNode = childNode;
                break;
            }
        }

        if (tmpChildNode == nullptr) {
            // create tmp Node
            tmpChildNode = std::make_shared<Node>(_side, *nameIt, NodeType::Directory, tmpNode);

            if (tmpChildNode == nullptr) {
                std::cout << "Failed to allocate memory" << std::endl;
                LOG_SYNCPAL_ERROR(_logger, "Failed to allocate memory");
                return nullptr;
            }

            if (!tmpNode->insertChildren(tmpChildNode)) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in Node::insertChildren: node name="
                                               << SyncName2WStr(tmpChildNode->name()).c_str()
                                               << " parent node name=" << SyncName2WStr(tmpNode->name()).c_str());
                return nullptr;
            }
        }
        tmpNode = tmpChildNode;
    }
    return tmpNode;
}

bool UpdateTreeWorker::mergingTempNodeToRealNode(std::shared_ptr<Node> tmpNode, std::shared_ptr<Node> realNode) {
    // merging ids
    if (tmpNode->id().has_value() && !realNode->id().has_value()) {
        // TODO: How is this possible?? Tmp node should NEVER have a valid id and real node should ALWAYS have a valid id
        if (!updateNodeId(realNode, tmpNode->id().value())) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTreeWorker::updateNodeId");
            return false;
        }
        LOGW_SYNCPAL_DEBUG(_logger, Utility::s2ws(Utility::side2Str(_side)).c_str()
                                        << L" update tree: Real node ID updated with tmp node ID. Should never happen...");
    }

    // merging tmpNode's children to realNode
    for (auto &child : tmpNode->children()) {
        if (!child.second->setParentNode(realNode)) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in Node::setParentNode: node name="
                                           << SyncName2WStr(realNode->name()).c_str()
                                           << " parent node name=" << SyncName2WStr(child.second->name()).c_str());
            return false;
        }

        if (!realNode->insertChildren(child.second)) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in Node::insertChildren: node name="
                                           << SyncName2WStr(child.second->name()).c_str()
                                           << " parent node name=" << SyncName2WStr(realNode->name()).c_str());
            return false;
        }
    }

    // temp node removed from children list
    std::shared_ptr<Node> parentTmpNode = tmpNode->parentNode();
    parentTmpNode->deleteChildren(tmpNode);

    // Real node added as child of parent node
    if (!realNode->parentNode()->insertChildren(realNode)) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in Node::insertChildren: node name="
                                       << SyncName2WStr(realNode->name()).c_str()
                                       << " parent node name=" << SyncName2WStr(realNode->parentNode()->name()).c_str());
        return false;
    }

    return true;
}

bool UpdateTreeWorker::integrityCheck() {
    // TODO : check if this does not slow the process too much
    LOGW_SYNCPAL_INFO(_logger, Utility::s2ws(Utility::side2Str(_side)).c_str() << L" update tree integrity check started");
    for (const auto &node : _updateTree->nodes()) {
        if (node.second->isTmp() || Utility::startsWith(*node.second->id(), "tmp_")) {
            LOGW_SYNCPAL_WARN(
                _logger, Utility::s2ws(Utility::side2Str(_side)).c_str()
                             << L" update tree integrity check failed. A temporary node remains in the update tree: "
                             << L" (node ID: '"
                             << Utility::s2ws(node.second->id().has_value() ? *node.second->id() : std::string()).c_str()
                             << L"', DB ID: '" << (node.second->idb().has_value() ? *node.second->idb() : -1) << L"', name: '"
                             << SyncName2WStr(node.second->name()).c_str() << L"')");
            return false;
        }
    }

    LOGW_SYNCPAL_INFO(_logger, Utility::s2ws(Utility::side2Str(_side)).c_str() << L" update tree integrity check finished");
    return true;
}

void UpdateTreeWorker::drawUpdateTree() {
    const std::string drawUpdateTree = CommonUtility::envVarValue("KDRIVE_DEBUG_DRAW_UPDATETREE");
    if (drawUpdateTree.empty()) {
        return;
    }

    SyncName treeStr;
    drawUpdateTreeRow(_updateTree->rootNode(), treeStr);
    LOGW_SYNCPAL_INFO(_logger, Utility::s2ws(Utility::side2Str(_side)).c_str() << L" update tree:\n"
                                                                               << SyncName2WStr(treeStr).c_str());
}

void UpdateTreeWorker::drawUpdateTreeRow(const std::shared_ptr<Node> node, SyncName &treeStr, uint64_t depth /*= 0*/) {
    for (int i = 0; i < depth; i++) {
        treeStr += Str(" ");
    }
    treeStr += Str("'") + node->name() + Str("'");
    treeStr += Str("[");
    treeStr += Str2SyncName(*node->id());
    treeStr += Str(" / ");
    treeStr += node->changeEvents() != OperationType::None ? Str2SyncName(Utility::opType2Str(node->changeEvents())) : Str("-");
    treeStr += Str("]");
    treeStr += Str("\n");

    depth++;
    for (const auto &[_, childNode] : node->children()) {
        drawUpdateTreeRow(childNode, treeStr, depth);
    }
}

ExitCode UpdateTreeWorker::getNewPathAfterMove(const SyncPath &path, SyncPath &newPath) {
    // Split path
    std::vector<std::pair<SyncName, NodeId>> names;
    SyncPath pathTmp(path);
    while (pathTmp != pathTmp.root_path()) {
        names.emplace_back(pathTmp.filename(), "0");
        pathTmp = pathTmp.parent_path();
    }

    // Vector ID
    SyncPath tmpPath;
    for (auto nameIt = names.rbegin(); nameIt != names.rend(); ++nameIt) {
        tmpPath.append(nameIt->first);
        bool found = false;
        std::optional<NodeId> tmpNodeId{};
        if (!_syncDb->id(_side, tmpPath, tmpNodeId, found)) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::id");
            return ExitCode::DbError;
        }
        if (!found || !tmpNodeId.has_value()) {
            LOGW_SYNCPAL_WARN(_logger, L"Node not found for " << Utility::formatSyncPath(tmpPath).c_str());
            return ExitCode::DataError;
        }
        nameIt->second = *tmpNodeId;
    }

    for (auto nameIt = names.rbegin(); nameIt != names.rend(); ++nameIt) {
        FSOpPtr op = nullptr;
        if (_operationSet->findOp(nameIt->second, OperationType::Move, op)) {
            newPath = op->destinationPath();
        } else {
            newPath.append(nameIt->first);
        }
    }
    return ExitCode::Ok;
}

ExitCode UpdateTreeWorker::updateNodeWithDb(const std::shared_ptr<Node> parentNode) {
    std::queue<std::shared_ptr<Node>> nodeQueue;
    nodeQueue.push(parentNode);

    while (!nodeQueue.empty()) {
        std::shared_ptr<Node> node = nodeQueue.front();
        nodeQueue.pop();

        if (stopAsked()) {
            return ExitCode::Ok;
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
        if (node->hasChangeEvent(OperationType::Create)) {
            continue;
        }

        // if node is temporary node
        if (node->isTmp()) {
            if (ExitCode exitCode = updateTmpNode(node); exitCode != ExitCode::Ok) {
                return exitCode;
            }
        }

        // use previous nodeId if it's an Edit from Delete-Create
        if (!node->id().has_value()) {
            LOGW_SYNCPAL_WARN(_logger, L"Failed to retrieve ID for node= " << SyncName2WStr(node->name()).c_str());
            return ExitCode::DataError;
        }

        NodeId usableNodeId = node->id().value();
        if (node->isEditFromDeleteCreate()) {
            if (!node->previousId().has_value()) {
                LOGW_SYNCPAL_WARN(_logger, L"Failed to retrieve previousId for node= " << SyncName2WStr(node->name()).c_str());
                return ExitCode::DataError;
            }
            usableNodeId = node->previousId().value();
        }

        DbNode dbNode;
        if (!_syncDb->node(_side, usableNodeId, dbNode, found)) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::node");
            return ExitCode::DbError;
        }
        if (!found) {
            LOG_SYNCPAL_WARN(_logger, "Failed to retrieve node for id=" << usableNodeId.c_str());
            return ExitCode::DataError;
        }

        // if it's a Move event
        if (node->hasChangeEvent(OperationType::Move)) {
            // update parentDbId
            node->setMoveOriginParentDbId(dbNode.parentNodeId());

            SyncPath dbPath;
            if (!_syncDb->path(node->idb().value(), dbPath, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::path");
                return ExitCode::DbError;
            }
            if (!found) {
                LOG_SYNCPAL_WARN(_logger, "Failed to retrieve node for DB ID=" << node->idb().value());
                return ExitCode::DataError;
            }
            node->setMoveOrigin(dbPath);
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
            node->setLastModified(_side == ReplicaSide::Local ? dbNode.lastModifiedLocal() : dbNode.lastModifiedRemote());
        }
        if (node->size() == 0) {
            node->setSize(dbNode.size());
        }

        for (auto &nodeChild : node->children()) {
            nodeQueue.push(nodeChild.second);
        }
    }

    return ExitCode::Ok;
}

ExitCode UpdateTreeWorker::updateTmpNode(const std::shared_ptr<Node> tmpNode) {
    SyncPath dbPath;
    // If it's a move, we need the previous path to be able to retrieve database data from path
    if (ExitCode exitCode = getOriginPath(tmpNode, dbPath); exitCode != ExitCode::Ok) {
        return exitCode;
    }

    std::optional<NodeId> id;
    bool found = false;
    if (!_syncDb->id(_side, dbPath, id, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::id");
        return ExitCode::DbError;
    }
    if (!found) {
        LOGW_SYNCPAL_WARN(_logger, L"Node not found in DB for " << Utility::formatSyncPath(dbPath).c_str() << L" (Node name: '"
                                                                << SyncName2WStr(tmpNode->name()).c_str() << L"') on side"
                                                                << Utility::s2ws(Utility::side2Str(_side)).c_str());
        return ExitCode::DataError;
    }
    if (!id.has_value()) {
        LOGW_SYNCPAL_WARN(_logger, L"Failed to retrieve ID for node= " << SyncName2WStr(tmpNode->name()).c_str());
        return ExitCode::DataError;
    }

    if (!updateNodeId(tmpNode, id.value())) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTreeWorker::updateNodeId");
        return ExitCode::DataError;
    }

    DbNodeId dbId;
    if (!_syncDb->dbId(_side, *id, dbId, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbId");
        return ExitCode::DbError;
    }
    if (!found) {
        LOGW_SYNCPAL_WARN(_logger, L"Node not found for ID = " << Utility::s2ws(*id).c_str() << L" (Node name: '"
                                                               << SyncName2WStr(tmpNode->name()).c_str()
                                                               << L"', node valid name: '"
                                                               << SyncName2WStr(tmpNode->name()).c_str() << L"') on side"
                                                               << Utility::s2ws(Utility::side2Str(_side)).c_str());
        return ExitCode::DataError;
    }
    tmpNode->setIdb(dbId);
    tmpNode->setIsTmp(false);

    const auto &prevNode = _updateTree->nodes()[*id];
    if (prevNode) {
        // Update children list
        for (const auto &[_, childNode] : prevNode->children()) {
            if (!tmpNode->insertChildren(childNode)) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in Node::insertChildren: node name="
                                               << SyncName2WStr(childNode->name()).c_str()
                                               << " parent node name=" << SyncName2WStr(tmpNode->name()).c_str());
                return ExitCode::DataError;
            }

            // set new parent
            if (!childNode->setParentNode(tmpNode)) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in Node::setParentNode: node name="
                                               << SyncName2WStr(tmpNode->name()).c_str()
                                               << " parent node name=" << SyncName2WStr(childNode->name()).c_str());
                return ExitCode::DataError;
            }

            LOGW_SYNCPAL_DEBUG(
                _logger, Utility::s2ws(Utility::side2Str(_side)).c_str()
                             << L" update tree: Node '" << SyncName2WStr(childNode->name()).c_str() << L"' (node ID: '"
                             << Utility::s2ws((childNode->id().has_value() ? *childNode->id() : std::string())).c_str()
                             << L"') inserted in children list of node '" << SyncName2WStr(tmpNode->name()).c_str()
                             << L"' (node ID: '"
                             << Utility::s2ws((tmpNode->id().has_value() ? *tmpNode->id() : std::string())).c_str() << L"')");
        }

        // Update events
        if (tmpNode->changeEvents() != prevNode->changeEvents()) {
            tmpNode->setChangeEvents(prevNode->changeEvents());
            LOGW_SYNCPAL_DEBUG(_logger, Utility::s2ws(Utility::side2Str(_side)).c_str()
                                            << L" update tree: Changed events to '"
                                            << Utility::opType2Str((OperationType)prevNode->changeEvents()).c_str()
                                            << "' for node '" << SyncName2WStr(tmpNode->name()).c_str() << L"' (node ID: '"
                                            << Utility::s2ws((tmpNode->id().has_value() ? *tmpNode->id() : std::string())).c_str()
                                            << L"')");
        }
    }

    _updateTree->nodes()[*id] = tmpNode;
    if (ParametersCache::isExtendedLogEnabled()) {
        LOGW_SYNCPAL_DEBUG(
            _logger,
            Utility::s2ws(Utility::side2Str(_side)).c_str()
                << L" update tree: Node '" << SyncName2WStr(tmpNode->name()).c_str() << L"' (node ID: '"
                << Utility::s2ws((tmpNode->id().has_value() ? *tmpNode->id() : std::string())).c_str() << L"', DB ID: '"
                << (tmpNode->idb().has_value() ? *tmpNode->idb() : -1) << L"', parent ID: '"
                << Utility::s2ws(tmpNode->parentNode()->id().has_value() ? *tmpNode->parentNode()->id() : std::string()).c_str()
                << L"') updated. Node updated with DB");
    }

    return ExitCode::Ok;
}

ExitCode UpdateTreeWorker::getOriginPath(const std::shared_ptr<Node> node, SyncPath &path) {
    path.clear();

    if (!node) {
        return ExitCode::DataError;
    }

    std::vector<SyncName> names;
    std::shared_ptr<Node> tmpNode = node;
    while (tmpNode && tmpNode->parentNode() != nullptr) {
        if (tmpNode->moveOriginParentDbId().has_value() && tmpNode->moveOrigin().has_value()) {
            // Save origin file name
            names.push_back(tmpNode->moveOrigin().value().filename());

            // Get origin parent
            DbNode dbNode;
            bool found = false;
            if (!_syncDb->node(tmpNode->moveOriginParentDbId().value(), dbNode, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::node");
                return ExitCode::DbError;
            }
            if (!found) {
                LOG_SYNCPAL_WARN(
                    _logger, "Failed to retrieve node for id=" << (node->id().has_value() ? *node->id() : std::string()).c_str());
                return ExitCode::DataError;
            }

            SyncPath dbPath;
            if (!_syncDb->path(dbNode.nodeId(), dbPath, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::path");
                return ExitCode::DbError;
            }
            if (!found) {
                LOG_SYNCPAL_WARN(_logger, "Failed to retrieve node path for nodeId=" << dbNode.nodeId());
                return ExitCode::DataError;
            }

            path = dbPath;
            break;
        } else {
            names.push_back(tmpNode->name());
            tmpNode = tmpNode->parentNode();
        }
    }

    for (std::vector<SyncName>::reverse_iterator nameIt = names.rbegin(); nameIt != names.rend(); ++nameIt) {
        path /= *nameIt;
    }

    return ExitCode::Ok;
}

ExitCode UpdateTreeWorker::checkNodeIsFoundInDbForMoveOp(const std::shared_ptr<Node> node, FSOpPtr moveOp) {
    if (_side == ReplicaSide::Remote || moveOp->operationType() != OperationType::Move) {
        return ExitCode::Ok;
    }

    // Does the file has a valid name in DB?
    DbNode dbNode;
    bool found = false;
    if (!_syncDb->node(_side, node->id() ? *node->id() : std::string(), dbNode, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::node");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "Failed to retrieve node for id=" << (node->id() ? *node->id() : std::string()).c_str());
        return ExitCode::DataError;
    }

    return ExitCode::Ok;
}

}  // namespace KDC
