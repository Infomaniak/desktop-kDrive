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

#include "updatetreeworker.h"
#include "libcommon/log/sentry/ptraces.h"
#include "libcommon/utility/utility.h"
#include "libcommon/utility/logiffail.h"
#include "libcommonserver/utility/utility.h"
#include "requests/parameterscache.h"

#include <iostream>
#include <log4cplus/loggingmacros.h>


namespace KDC {

UpdateTreeWorker::UpdateTreeWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName,
                                   ReplicaSide side) :
    ISyncWorker(syncPal, name, shortName), _syncDb(syncPal->_syncDb), _operationSet(syncPal->operationSet(side)),
    _updateTree(syncPal->updateTree(side)), _side(side), _syncDbCache(syncPal->syncDb()) {}

UpdateTreeWorker::UpdateTreeWorker(std::shared_ptr<SyncDb> syncDb, std::shared_ptr<FSOperationSet> operationSet,
                                   std::shared_ptr<UpdateTree> updateTree, const std::string &name, const std::string &shortName,
                                   ReplicaSide side) :
    ISyncWorker(nullptr, name, shortName), _syncDb(syncDb), _operationSet(operationSet), _updateTree(updateTree), _side(side),
    _syncDbCache(syncDb) {}

UpdateTreeWorker::~UpdateTreeWorker() {
    _operationSet.reset();
    _updateTree.reset();
}

void UpdateTreeWorker::execute() {
    ExitCode exitCode(ExitCode::Unknown);

    LOG_SYNCPAL_DEBUG(_logger, "Worker started: name=" << name().c_str());

    const auto start = std::chrono::steady_clock::now();

    _updateTree->startUpdate();

    // Reset nodes working properties
    for (const auto &[_, nodeItem]: _updateTree->nodes()) {
        nodeItem->clearChangeEvents();
        nodeItem->clearConflictAlreadyConsidered();
        nodeItem->setInconsistencyType(InconsistencyType::None);
        nodeItem->setPreviousId(std::nullopt);
        nodeItem->setStatus(NodeStatus::Unprocessed);
    }

    _updateTree->previousIdSet().clear();

    const std::vector<stepptr> steptab = {&UpdateTreeWorker::step1MoveDirectory,   &UpdateTreeWorker::step2MoveFile,
                                          &UpdateTreeWorker::step3DeleteDirectory, &UpdateTreeWorker::step4DeleteFile,
                                          &UpdateTreeWorker::step5CreateDirectory, &UpdateTreeWorker::step6CreateFile,
                                          &UpdateTreeWorker::step7EditFile,        &UpdateTreeWorker::step8CompleteUpdateTree};

    for (const auto stepn: steptab) {
        exitCode = (this->*stepn)();
        if (exitCode != ExitCode::Ok) break;

        if (stopAsked()) {
            setDone(ExitCode::Ok);
            return;
        }
    }

    if (exitCode == ExitCode::Ok) {
        if (!integrityCheck()) {
            exitCode = ExitCode::DataError;
        }
        _updateTree->drawUpdateTree();
    }

    const auto elapsed_seconds = std::chrono::steady_clock::now() - start;

    if (exitCode == ExitCode::Ok) {
        LOG_SYNCPAL_DEBUG(_logger, "Update Tree " << _side << " updated in: " << elapsed_seconds.count() << "s");
    } else {
        LOG_SYNCPAL_WARN(_logger, "Update Tree " << _side << " generation failed after: " << elapsed_seconds.count() << "s");
    }

    // Clear unexpected operation set once used
    _operationSet->clear();

    // Clear sync db cache
    _syncDbCache.clearCache();
    LOG_SYNCPAL_DEBUG(_logger, "Worker stopped: name=" << name().c_str());
    setDone(exitCode);
}

ExitCode UpdateTreeWorker::step1MoveDirectory() {
    auto perfMonitor = sentry::pTraces::scoped::Step1MoveDirectory(syncDbId());
    return createMoveNodes(NodeType::Directory);
}

ExitCode UpdateTreeWorker::step2MoveFile() {
    auto perfMonitor = sentry::pTraces::scoped::Step2MoveFile(syncDbId());
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
    auto perfMonitor = sentry::pTraces::scoped::Step3DeleteDirectory(syncDbId());

    const std::unordered_set<UniqueId> deleteOpsIds = _operationSet->getOpsByType(OperationType::Delete);
    for (const auto &deleteOpId: deleteOpsIds) {
        // worker stop or pause
        if (stopAsked()) {
            return ExitCode::Ok;
        }

        FSOpPtr deleteOp = nullptr;
        _operationSet->getOp(deleteOpId, deleteOp);

        if (deleteOp->objectType() != NodeType::Directory) {
            continue;
        }

        const auto currentNodeIt = _updateTree->nodes().find(deleteOp->nodeId());
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
                        _side << L" update tree: Node '" << SyncName2WStr(currentNodeIt->second->name()).c_str()
                              << L"' (node ID: '"
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

                if (const auto exitCode = getOrCreateNodeFromDeletedPath(newPath.parent_path(), parentNode);
                    exitCode != ExitCode::Ok) {
                    LOG_SYNCPAL_WARN(_logger, "Error in UpdateTreeWorker::getOrCreateNodeFromDeletedPath");
                    return exitCode;
                }
            }

            // Find dbNodeId
            DbNodeId idb = 0;
            bool found = false;
            if (!_syncDbCache.dbId(_side, deleteOp->nodeId(), idb, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbId");
                return ExitCode::DbError;
            }
            if (!found) {
                LOG_SYNCPAL_WARN(_logger, "Node not found for id = " << deleteOp->nodeId().c_str());
                return ExitCode::DataError;
            }

            // Check if parentNode has got a child with the same name
            std::shared_ptr<Node> newNode = parentNode->findChildren(deleteOp->path().filename(), deleteOp->nodeId());
            if (newNode != nullptr) {
                // Node already exists, update it
                newNode->setIdb(idb);
                if (!_updateTree->updateNodeId(newNode, deleteOp->nodeId())) {
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
                    LOGW_SYNCPAL_DEBUG(_logger,
                                       _side << L" update tree: Node '" << SyncName2WStr(newNode->name()).c_str()
                                             << L"' (node ID: '"
                                             << Utility::s2ws(newNode->id().has_value() ? *newNode->id() : NodeId()).c_str()
                                             << L"', DB ID: '" << (newNode->idb().has_value() ? *newNode->idb() : -1)
                                             << L"') updated. Operation DELETE inserted in change events.");
                }
            } else {
                // create node
                newNode = std::make_shared<Node>(idb, _side, deleteOp->path().filename().native(), deleteOp->objectType(),
                                                 OperationType::Delete, deleteOp->nodeId(), deleteOp->createdAt(),
                                                 deleteOp->lastModified(), deleteOp->size(), parentNode);
                if (newNode == nullptr) {
                    std::cout << "Failed to allocate memory" << std::endl;
                    LOG_SYNCPAL_ERROR(_logger, "Failed to allocate memory");
                    return ExitCode::SystemError;
                }

                if (!parentNode->insertChildren(newNode)) {
                    LOGW_SYNCPAL_WARN(_logger, L"Error in Node::insertChildren: node name="
                                                       << SyncName2WStr(newNode->name()).c_str() << L" parent node name="
                                                       << SyncName2WStr(parentNode->name()).c_str());
                    return ExitCode::DataError;
                }

                _updateTree->nodes()[deleteOp->nodeId()] = newNode;
                if (ParametersCache::isExtendedLogEnabled()) {
                    LOGW_SYNCPAL_DEBUG(
                            _logger,
                            _side << L" update tree: Node '" << SyncName2WStr(newNode->name()).c_str() << L"' (node ID: '"
                                  << Utility::s2ws(newNode->id().has_value() ? *newNode->id() : NodeId()).c_str()
                                  << L"', DB ID: '" << (newNode->idb().has_value() ? *newNode->idb() : -1) << L"', parent ID: '"
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

    for (const auto &createOpId: createOpsIds) {
        // worker stop or pause
        if (stopAsked()) {
            return ExitCode::Ok;
        }

        FSOpPtr createOp;
        _operationSet->getOp(createOpId, createOp);

        std::pair<FSOpPtrMap::iterator, bool> insertionResult;
        switch (createOp->objectType()) {
            case NodeType::File: {
                SyncPath normalizedPath;
                if (!Utility::normalizedSyncPath(createOp->path(), normalizedPath)) {
                    normalizedPath = createOp->path();
                    LOGW_SYNCPAL_WARN(_logger, L"Failed to normalize: " << Utility::formatSyncPath(createOp->path()));
                }
                insertionResult = _createFileOperationSet.try_emplace(normalizedPath, createOp);
                break;
            }
            case NodeType::Directory:
                insertionResult = createDirectoryOperationSet.try_emplace(createOp->path(), createOp);
                break;
            default:
                break;
        }

        if (!insertionResult.second) {
            // Failed to insert Create operation. A full rebuild of the snapshot is required.
            // The following issue has been identified: the operating system missed a delete operation, in which case a snapshot
            // rebuild is both required and sufficient.


            LOGW_SYNCPAL_WARN(_logger, _side << L" update tree: Operation Create already exists on item with "
                                             << Utility::formatSyncPath(createOp->path()).c_str());

            sentry::Handler::captureMessage(sentry::Level::Warning, "UpdateTreeWorker::step4",
                                            "2 Create operations detected on the same item");
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

    const std::wstring updateTreeStr = Utility::s2ws(toString(_side)) + L" update tree";
    const std::wstring nodeStr = L"Node '" + SyncName2WStr(node->name()) + L"'";
    const std::wstring nodeIdStr = L"node ID: '" + Utility::s2ws(node->id() ? *node->id() : NodeId()) + L"'";
    const std::wstring dbIdStr = L"DB ID: '" + std::to_wstring(node->idb() ? *node->idb() : -1) + L"'";
    const std::wstring opTypeStr = Utility::s2ws(toString(opType));

    LOGW_SYNCPAL_DEBUG(_logger, updateTreeStr.c_str()
                                        << L": " << nodeStr.c_str() << L" (" << nodeIdStr.c_str() << L", " << dbIdStr.c_str()
                                        << L", " << parentIdStr.c_str() << L") " << updateTypeStr.c_str() << L". Operation "
                                        << opTypeStr.c_str() << L" inserted in change events.");
}

bool UpdateTreeWorker::updateTmpFileNode(std::shared_ptr<Node> newNode, const FSOpPtr op, const FSOpPtr deleteOp,
                                         OperationType opType) {
    assert(newNode != nullptr && newNode->isTmp());

    if (!_updateTree->updateNodeId(newNode, op->nodeId())) {
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
    auto perfMonitor = sentry::pTraces::scoped::Step4DeleteFile(syncDbId());

    const ExitCode exitCode = handleCreateOperationsWithSamePath();
    if (exitCode != ExitCode::Ok) return exitCode; // Rebuild the snapshot.

    std::unordered_set<UniqueId> deleteOpsIds = _operationSet->getOpsByType(OperationType::Delete);
    for (const auto &deleteOpId: deleteOpsIds) {
        // worker stop or pause
        if (stopAsked()) {
            return ExitCode::Ok;
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
            SyncPath normalizedPath;
            if (!Utility::normalizedSyncPath(deleteOp->path(), normalizedPath)) {
                normalizedPath = deleteOp->path();
                LOGW_SYNCPAL_WARN(_logger, L"Failed to normalize: " << Utility::formatSyncPath(deleteOp->path()));
            }
            if (auto createFileOpSetIt = _createFileOperationSet.find(normalizedPath);
                createFileOpSetIt != _createFileOperationSet.end()) {
                FSOpPtr tmp = nullptr;
                if (!_operationSet->findOp(createFileOpSetIt->second->nodeId(), createFileOpSetIt->second->operationType(),
                                           tmp)) {
                    continue;
                }

                // op is now the createOperation
                op = tmp;
                _createFileOperationSet.erase(normalizedPath);
            }
        }

        const OperationType opType = op->operationType() == OperationType::Create ? OperationType::Edit : OperationType::Delete;
        if (auto currentNodeIt = _updateTree->nodes().find(deleteOp->nodeId()); currentNodeIt != _updateTree->nodes().end()) {
            // Node is already in the update tree, it can be a Delete or an Edit
            std::shared_ptr<Node> currentNode = currentNodeIt->second;
            if (!_updateTree->updateNodeId(currentNode, op->nodeId())) {
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
                currentNode->setName(op->path().filename().native());
            }

            logUpdate(currentNode, opType);
        } else {
            std::shared_ptr<Node> parentNode;
            if (const auto exitCode = searchForParentNode(op->path(), parentNode); exitCode != ExitCode::Ok) {
                return exitCode;
            }

            if (!parentNode) {
                SyncPath newPath;
                if (const auto newPathExitCode = getNewPathAfterMove(deleteOp->path(), newPath);
                    newPathExitCode != ExitCode::Ok) {
                    LOG_SYNCPAL_WARN(_logger, "Error in UpdateTreeWorker::getNewPathAfterMove");
                    return newPathExitCode;
                }

                if (const auto exitCode = getOrCreateNodeFromDeletedPath(newPath.parent_path(), parentNode);
                    exitCode != ExitCode::Ok) {
                    LOG_SYNCPAL_WARN(_logger, "Error in UpdateTreeWorker::getOrCreateNodeFromDeletedPath");
                    return exitCode;
                }
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
                if (!_syncDbCache.dbId(_side, deleteOp->nodeId(), idb, found)) {
                    LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbId");
                    return ExitCode::DbError;
                }
                if (!found) {
                    LOG_SYNCPAL_WARN(_logger, "Node not found for id = " << deleteOp->nodeId().c_str());
                    return ExitCode::DataError;
                }

                newNode = std::make_shared<Node>(idb, _side, op->path().filename().native(), op->objectType(), opType,
                                                 op->nodeId(), op->createdAt(), op->lastModified(), op->size(), parentNode);
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
                                                       << SyncName2WStr(newNode->name()).c_str() << L" parent node name="
                                                       << SyncName2WStr(parentNode->name()).c_str());
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
    auto perfMonitor = sentry::pTraces::scoped::Step5CreateDirectory(syncDbId());

    std::unordered_set<UniqueId> createOpsIds = _operationSet->getOpsByType(OperationType::Create);
    for (const auto &createOpId: createOpsIds) {
        // worker stop or pause
        if (stopAsked()) {
            return ExitCode::Ok;
        }

        FSOpPtr createOp = nullptr;
        _operationSet->getOp(createOpId, createOp);
        if (createOp->objectType() != NodeType::Directory) {
            continue;
        }

        if (createOp->path().empty()) {
            LOG_SYNCPAL_WARN(_logger, "Invalid create operation on nodeId=" << createOp->nodeId().c_str());
            assert(false);
            sentry::Handler::captureMessage(sentry::Level::Warning, "UpdateTreeWorker::step5CreateDirectory",
                                            "Invalid create operation");
            continue;
        }

        // find node by path because it may have been created before
        std::shared_ptr<Node> currentNode;
        if (const auto exitCode = getOrCreateNodeFromExistingPath(createOp->path(), currentNode); exitCode != ExitCode::Ok) {
            LOG_SYNCPAL_WARN(_logger, "Error in UpdateTreeWorker::getOrCreateNodeFromExistingPath");
            return exitCode;
        }

        if (currentNode == _updateTree->rootNode()) {
            LOG_SYNCPAL_WARN(_logger, "No operation allowed on the root node");
            assert(false);
            return ExitCode::DataError;
        }

        if (currentNode->hasChangeEvent(OperationType::Delete)) {
            // A directory has been deleted and another one has been created with the same name
            currentNode->setPreviousId(currentNode->id());
        }

        if (!_updateTree->updateNodeId(currentNode, createOp->nodeId())) {
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
                    _logger, _side << L" update tree: Node '" << SyncName2WStr(currentNode->name()).c_str() << L"' (node ID: '"
                                   << Utility::s2ws(currentNode->id().has_value() ? *currentNode->id() : std::string()).c_str()
                                   << L"', DB ID: '" << (currentNode->idb().has_value() ? *currentNode->idb() : -1)
                                   << L"') inserted. Operation " << createOp->operationType() << L" inserted in change events.");
        }
    }
    return ExitCode::Ok;
}

ExitCode UpdateTreeWorker::step6CreateFile() {
    auto perfMonitor = sentry::pTraces::scoped::Step6CreateFile(syncDbId());
    for (const auto &op: _createFileOperationSet) {
        // worker stop or pause
        if (stopAsked()) {
            return ExitCode::Ok;
        }

        FSOpPtr operation = op.second;

        // find parentNode by path
        std::shared_ptr<Node> parentNode;
        if (const auto exitCode = getOrCreateNodeFromExistingPath(operation->path().parent_path(), parentNode);
            exitCode != ExitCode::Ok) {
            LOG_SYNCPAL_WARN(_logger, "Error in UpdateTreeWorker::getOrCreateNodeFromExistingPath");
            return exitCode;
        }

        std::shared_ptr<Node> newNode = parentNode->findChildrenById(operation->nodeId());
        if (newNode != nullptr) {
            // Node already exists, update it
            if (newNode->name() == operation->path().filename().native()) {
                if (!_updateTree->updateNodeId(newNode, operation->nodeId())) {
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
                            _side << L" update tree: Node '" << SyncName2WStr(newNode->name()).c_str() << L"' (node ID: '"
                                  << Utility::s2ws(newNode->id().has_value() ? *newNode->id() : std::string()).c_str()
                                  << L"', DB ID: '" << (newNode->idb().has_value() ? *newNode->idb() : -1) << L"', parent ID: '"
                                  << Utility::s2ws(parentNode->id().has_value() ? *parentNode->id() : "-1").c_str()
                                  << L"') updated. Operation " << operation->operationType() << L" inserted in change events.");
                }
                continue;
            }
        }

        // create node
        newNode = std::make_shared<Node>(std::nullopt, _side, operation->path().filename().native(), operation->objectType(),
                                         operation->operationType(), operation->nodeId(), operation->createdAt(),
                                         operation->lastModified(), operation->size(), parentNode);
        if (newNode == nullptr) {
            std::cout << "Failed to allocate memory" << std::endl;
            LOG_SYNCPAL_ERROR(_logger, "Failed to allocate memory");
            return ExitCode::SystemError;
        }

        if (!parentNode->insertChildren(newNode)) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in Node::insertChildren: node name=" << SyncName2WStr(newNode->name()).c_str()
                                                                                    << L" parent node name="
                                                                                    << SyncName2WStr(parentNode->name()).c_str());
            return ExitCode::DataError;
        }

        _updateTree->nodes()[operation->nodeId()] = newNode;
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_SYNCPAL_DEBUG(
                    _logger, _side << L" update tree: Node '" << SyncName2WStr(newNode->name()).c_str() << L"' (node ID: '"
                                   << Utility::s2ws(newNode->id().has_value() ? *newNode->id() : "").c_str() << L"', DB ID: '"
                                   << (newNode->idb().has_value() ? *newNode->idb() : -1) << L"', parent ID: '"
                                   << Utility::s2ws(parentNode->id().has_value() ? *parentNode->id() : "-1").c_str()
                                   << L"') inserted. Operation " << operation->operationType() << L" inserted in change events.");
        }
    }
    return ExitCode::Ok;
}

ExitCode UpdateTreeWorker::step7EditFile() {
    auto perfMonitor = sentry::pTraces::scoped::Step7EditFile(syncDbId());
    std::unordered_set<UniqueId> editOpsIds = _operationSet->getOpsByType(OperationType::Edit);
    for (const auto &editOpId: editOpsIds) {
        // worker stop or pause
        if (stopAsked()) {
            return ExitCode::Ok;
        }

        FSOpPtr editOp = nullptr;
        _operationSet->getOp(editOpId, editOp);
        if (editOp->objectType() != NodeType::File) {
            continue;
        }
        // find parentNode by path because should have been created
        std::shared_ptr<Node> parentNode;
        if (const auto exitCode = getOrCreateNodeFromExistingPath(editOp->path().parent_path(), parentNode);
            exitCode != ExitCode::Ok) {
            LOG_SYNCPAL_WARN(_logger, "Error in UpdateTreeWorker::getOrCreateNodeFromExistingPath");
            return exitCode;
        }

        std::shared_ptr<Node> newNode = parentNode->findChildrenById(editOp->nodeId());
        if (newNode != nullptr) {
            // Node already exists, update it
            newNode->setCreatedAt(editOp->createdAt());
            newNode->setLastModified(editOp->lastModified());
            newNode->setSize(editOp->size());
            newNode->insertChangeEvent(editOp->operationType());
            if (!_updateTree->updateNodeId(newNode, editOp->nodeId())) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTreeWorker::updateNodeId");
                return ExitCode::DataError;
            }
            newNode->setIsTmp(false);

            _updateTree->nodes()[editOp->nodeId()] = newNode;
            if (ParametersCache::isExtendedLogEnabled()) {
                LOGW_SYNCPAL_DEBUG(
                        _logger, _side << L" update tree: Node '" << SyncName2WStr(newNode->name()).c_str() << L"' (node ID: '"
                                       << Utility::s2ws(newNode->id().has_value() ? *newNode->id() : std::string()).c_str()
                                       << L"', DB ID: '" << (newNode->idb().has_value() ? *newNode->idb() : -1)
                                       << L"') updated. Operation " << editOp->operationType() << L" inserted in change events.");
            }
            continue;
        }

        // create node
        DbNodeId idb;
        bool found = false;
        if (!_syncDbCache.dbId(_side, editOp->nodeId(), idb, found)) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbId");
            return ExitCode::DbError;
        }
        if (!found) {
            LOG_SYNCPAL_WARN(_logger, "Node not found for id = " << editOp->nodeId().c_str());
            return ExitCode::DataError;
        }

        newNode = std::make_shared<Node>(idb, _side, editOp->path().filename().native(), editOp->objectType(),
                                         editOp->operationType(), editOp->nodeId(), editOp->createdAt(), editOp->lastModified(),
                                         editOp->size(), parentNode);
        if (newNode == nullptr) {
            std::cout << "Failed to allocate memory" << std::endl;
            LOG_SYNCPAL_ERROR(_logger, "Failed to allocate memory");
            return ExitCode::SystemError;
        }

        if (!parentNode->insertChildren(newNode)) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in Node::insertChildren: node name=" << SyncName2WStr(newNode->name()).c_str()
                                                                                    << L" parent node name="
                                                                                    << SyncName2WStr(parentNode->name()).c_str());
            return ExitCode::DataError;
        }

        _updateTree->nodes()[editOp->nodeId()] = newNode;
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_SYNCPAL_DEBUG(
                    _logger, _side << L" update tree: Node '" << SyncName2WStr(newNode->name()).c_str() << L"' (node ID: '"
                                   << Utility::s2ws(newNode->id().has_value() ? *newNode->id() : std::string()).c_str()
                                   << L"', DB ID: '" << (newNode->idb().has_value() ? *newNode->idb() : -1) << L"', parent ID: '"
                                   << Utility::s2ws(parentNode->id().has_value() ? *parentNode->id() : std::string()).c_str()
                                   << L"') inserted. editOp " << editOp->operationType() << L" inserted in change events.");
        }
    }

    return ExitCode::Ok;
}

ExitCode UpdateTreeWorker::step8CompleteUpdateTree() {
    auto perfMonitor = sentry::pTraces::scoped::Step8CompleteUpdateTree(syncDbId());
    if (stopAsked()) {
        return ExitCode::Ok;
    }

    bool found = false;
    std::set<NodeId> dbNodeIds;
    if (!_syncDbCache.ids(_side, dbNodeIds, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbNodeIds");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_SYNCPAL_DEBUG(_logger, "There is no dbNodeIds for side=" << _side);
        return ExitCode::Ok;
    }

    // update each existing nodes
    ExitCode exitCode = ExitCode::Unknown;
    try {
        exitCode = updateNodeWithDb(_updateTree->rootNode());
    } catch (std::exception &e) {
        LOG_WARN(_logger, "updateNodeWithDb failed: error=" << e.what());
        return ExitCode::DataError;
    }

    if (exitCode != ExitCode::Ok) {
        LOG_SYNCPAL_WARN(_logger, "Error in updateNodeWithDb");
        return exitCode;
    }

    std::optional<NodeId> rootNodeId =
            (_side == ReplicaSide::Local ? _syncDb->rootNode().nodeIdLocal() : _syncDb->rootNode().nodeIdRemote());

    // creating missing nodes
    auto dbNodeIdIt = dbNodeIds.begin();
    while (dbNodeIds.size() != 0) {
        // worker stop or pause
        if (stopAsked()) {
            return ExitCode::Ok;
        }
        if (dbNodeIdIt == dbNodeIds.end()) {
            dbNodeIdIt = dbNodeIds.begin();
        }
        const NodeId dbNodeId = *dbNodeIdIt;
        if (dbNodeId == rootNodeId.value()) {
            // Ignore root folder
            dbNodeIdIt = dbNodeIds.erase(dbNodeIdIt);
            continue;
        }
        const NodeId previousNodeId = dbNodeId;

        NodeId newNodeId = dbNodeId;
        auto previousToNewIdIt = _updateTree->previousIdSet().find(dbNodeId);
        if (previousToNewIdIt != _updateTree->previousIdSet().end()) {
            newNodeId = previousToNewIdIt->second;
        }

        if (_updateTree->nodes().find(newNodeId) == _updateTree->nodes().end()) {
            // create node
            NodeId parentId;
            if (!_syncDbCache.parent(_side, previousNodeId, parentId, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::parent");
                return ExitCode::DbError;
            }
            if (!found) {
                LOG_SYNCPAL_WARN(_logger, "Node or parent node not found for dbNodeId = " << previousNodeId.c_str());
                return ExitCode::DataError;
            }
            // not sure that parentId is already in _nodes ??
            std::shared_ptr<Node> parentNode; // parentNode could be null
            auto parentIt = _updateTree->nodes().find(parentId);
            if (parentIt != _updateTree->nodes().end()) {
                parentNode = parentIt->second;
            } else {
                const auto dbNodeIdIt2 = dbNodeIds.find(parentId);
                if (dbNodeIdIt2 != dbNodeIds.end()) {
                    dbNodeIdIt = dbNodeIdIt2;
                }
                continue; // Create the parent node first
            }

            DbNode dbNode;
            if (!_syncDbCache.node(_side, previousNodeId, dbNode, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::node");
                return ExitCode::DbError;
            }
            if (!found) {
                LOG_SYNCPAL_WARN(_logger, "Failed to retrieve node for dbId=" << previousNodeId.c_str());
                return ExitCode::DataError;
            }

            SyncTime lastModified =
                    _side == ReplicaSide::Local ? dbNode.lastModifiedLocal().value() : dbNode.lastModifiedRemote().value();
            SyncName name = _side == ReplicaSide::Local ? dbNode.nameLocal() : dbNode.nameRemote();
            const auto newNode = std::make_shared<Node>(dbNode.nodeId(), _side, name, dbNode.type(), OperationType::None,
                                                        newNodeId, dbNode.created(), lastModified, dbNode.size(), parentNode);
            if (newNode == nullptr) {
                std::cout << "Failed to allocate memory" << std::endl;
                LOG_SYNCPAL_ERROR(_logger, "Failed to allocate memory");
                return ExitCode::SystemError;
            }

            if (!parentNode->insertChildren(newNode)) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in Node::insertChildren: node name="
                                                   << SyncName2WStr(newNode->name()).c_str() << L" parent node name="
                                                   << SyncName2WStr(parentNode->name()).c_str());
                return ExitCode::DataError;
            }

            _updateTree->nodes()[newNodeId] = newNode;
            if (ParametersCache::isExtendedLogEnabled()) {
                LOGW_SYNCPAL_DEBUG(
                        _logger, _side << L" update tree: Node '" << SyncName2WStr(newNode->name()).c_str() << L"' (node ID: '"
                                       << Utility::s2ws(newNode->id().has_value() ? *newNode->id() : std::string()).c_str()
                                       << L"', DB ID: '" << (newNode->idb().has_value() ? *newNode->idb() : 1)
                                       << L"', parent ID: '"
                                       << Utility::s2ws(parentNode->id().has_value() ? *parentNode->id() : std::string()).c_str()
                                       << L"') inserted. Without change events.");
            }
        }
        dbNodeIdIt = dbNodeIds.erase(dbNodeIdIt);
    }

    return exitCode;
}

ExitCode UpdateTreeWorker::createMoveNodes(const NodeType &nodeType) {
    std::unordered_set<UniqueId> moveOpsIds = _operationSet->getOpsByType(OperationType::Move);
    for (const auto &moveOpId: moveOpsIds) {
        if (stopAsked()) {
            return ExitCode::Ok;
        }

        FSOpPtr moveOp = nullptr;
        _operationSet->getOp(moveOpId, moveOp);
        if (moveOp->objectType() != nodeType) {
            continue;
        }

        auto currentNodeIt = _updateTree->nodes().find(moveOp->nodeId());
        std::optional<NodeId> moveOriginParentId; // Current parent can be a tmp node at this point, so we need
                                                  // to fetch its id from the db.
        if (bool found = false; !_syncDb->id(_side, moveOp->path().parent_path(), moveOriginParentId, found)) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbId");
            return ExitCode::DbError;
        } else if (!found) {
            LOG_SYNCPAL_WARN(_logger, "Parent node not found for id=" << moveOp->nodeId());
            return ExitCode::DataError;
        }

        if (!moveOriginParentId.has_value()) {
            LOGW_SYNCPAL_WARN(_logger, L"Parent node id is empty for node with " << Utility::formatSyncPath(moveOp->path()));
            return ExitCode::DataError;
        }

        // if node exist
        if (currentNodeIt != _updateTree->nodes().end()) {
            // verify if the same node exist
            const std::shared_ptr<Node> currentNode = currentNodeIt->second;
            const std::shared_ptr<Node> alreadyExistNode = _updateTree->getNodeByPath(moveOp->destinationPath());
            if (alreadyExistNode && alreadyExistNode->isTmp()) {
                // merging nodes we keep currentNode
                if (!mergingTempNodeToRealNode(alreadyExistNode, currentNode)) {
                    LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTreeWorker::mergingTempNodeToRealNode");
                    return ExitCode::DataError;
                }
            }

            // create node if not exist
            std::shared_ptr<Node> parentNode;
            if (const auto exitCode = getOrCreateNodeFromExistingPath(moveOp->destinationPath().parent_path(), parentNode);
                exitCode != ExitCode::Ok) {
                LOG_SYNCPAL_WARN(_logger, "Error in UpdateTreeWorker::getOrCreateNodeFromExistingPath");
                return exitCode;
            }

            currentNode->setMoveOriginInfos({moveOp->path(), moveOriginParentId.value()});
            currentNode->insertChangeEvent(OperationType::Move);
            currentNode->setCreatedAt(moveOp->createdAt());
            currentNode->setLastModified(moveOp->lastModified());
            currentNode->setSize(moveOp->size());
            currentNode->setIsTmp(false);

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
                                                   << SyncName2WStr(parentNode->name()).c_str() << L" parent node name="
                                                   << SyncName2WStr(currentNode->name()).c_str());
                return ExitCode::DataError;
            }

            // insert currentNode into children list of new parent
            if (!parentNode->insertChildren(currentNode)) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in Node::insertChildren: node name="
                                                   << SyncName2WStr(currentNode->name()).c_str() << L" parent node name="
                                                   << SyncName2WStr(parentNode->name()).c_str());
                return ExitCode::DataError;
            }

            if (ParametersCache::isExtendedLogEnabled()) {
                LOGW_SYNCPAL_DEBUG(
                        _logger,
                        _side << L" update tree: Node '" << SyncName2WStr(currentNode->name()).c_str() << L"' (node ID: '"
                              << Utility::s2ws(currentNode->id().has_value() ? *currentNode->id() : std::string()).c_str()
                              << L"', DB ID: '" << (currentNode->idb().has_value() ? *currentNode->idb() : -1)
                              << L"', parent ID: '"
                              << Utility::s2ws(parentNode->id().has_value() ? *parentNode->id() : std::string()).c_str()
                              << L"') inserted. Operation " << moveOp->operationType() << L" inserted in change events.");
            }
        } else {
            // get parentNode
            std::shared_ptr<Node> parentNode;
            if (const auto exitCode = getOrCreateNodeFromExistingPath(moveOp->destinationPath().parent_path(), parentNode);
                exitCode != ExitCode::Ok) {
                LOG_SYNCPAL_WARN(_logger, "Error in UpdateTreeWorker::getOrCreateNodeFromExistingPath");
                return exitCode;
            }

            // create node
            DbNodeId idb = 0;
            bool found = false;
            if (!_syncDbCache.dbId(_side, moveOp->nodeId(), idb, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbId");
                return ExitCode::DbError;
            }
            if (!found) {
                LOG_SYNCPAL_WARN(_logger, "Node not found for id = " << moveOp->nodeId());
                return ExitCode::DataError;
            }

            const auto newNode = std::make_shared<Node>(
                    idb, _side, moveOp->destinationPath().filename().native(), moveOp->objectType(), OperationType::Move,
                    moveOp->nodeId(), moveOp->createdAt(), moveOp->lastModified(), moveOp->size(), parentNode,
                    Node::MoveOriginInfos(moveOp->path(), NodeId(moveOriginParentId.value())));

            if (newNode == nullptr) {
                std::cout << "Failed to allocate memory" << std::endl;
                LOG_SYNCPAL_ERROR(_logger, "Failed to allocate memory");
                return ExitCode::SystemError;
            }

            if (const auto exitCode = mergeNodeToParentChildren(parentNode, newNode); exitCode != ExitCode::Ok) {
                LOG_SYNCPAL_WARN(_logger, "Error in UpdateTreeWorker::mergeNodeToParentChildren");
                return exitCode;
            }

            _updateTree->nodes()[*newNode->id()] = newNode;
            if (ParametersCache::isExtendedLogEnabled()) {
                LOGW_SYNCPAL_DEBUG(
                        _logger,
                        _side << L" update tree: Node '" << SyncName2WStr(newNode->name()).c_str() << L"' (node ID: '"
                              << Utility::s2ws(newNode->id().has_value() ? *newNode->id() : std::string()).c_str()
                              << L"', DB ID: '" << (newNode->idb().has_value() ? *newNode->idb() : -1) << L"', parent ID: '"
                              << Utility::s2ws(parentNode->id().has_value() ? *parentNode->id() : std::string()).c_str()
                              << L"') inserted. Operation " << moveOp->operationType() << L" inserted in change events.");
            }
        }
    }

    return ExitCode::Ok;
}

ExitCode UpdateTreeWorker::getOrCreateNodeFromPath(const SyncPath &path, bool isDeleted, std::shared_ptr<Node> &node) {
    node = nullptr;

    if (path.empty()) {
        node = _updateTree->rootNode();
        return ExitCode::Ok;
    }

    std::vector<SyncName> names = Utility::splitPath(path);

    // create intermediate nodes if needed
    std::shared_ptr<Node> tmpNode = _updateTree->rootNode();
    for (auto nameIt = names.rbegin(); nameIt != names.rend(); ++nameIt) {
        std::shared_ptr<Node> tmpChildNode = nullptr;
        for (auto &[_, childNode]: tmpNode->children()) {
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
                return ExitCode::SystemError;
            }

            if (!tmpNode->insertChildren(tmpChildNode)) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in Node::insertChildren: node name="
                                                   << SyncName2WStr(tmpChildNode->name()).c_str() << L" parent node name="
                                                   << SyncName2WStr(tmpNode->name()).c_str());
                return ExitCode::DataError;
            }
        }
        tmpNode = tmpChildNode;
    }

    node = tmpNode;

    return ExitCode::Ok;
}

bool UpdateTreeWorker::mergingTempNodeToRealNode(std::shared_ptr<Node> tmpNode, std::shared_ptr<Node> realNode) {
    // merging ids
    if (tmpNode->id().has_value() && !realNode->id().has_value()) {
        // TODO: How is this possible?? Tmp node should NEVER have a valid id and real node should ALWAYS have a valid id
        if (!_updateTree->updateNodeId(realNode, tmpNode->id().value())) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTreeWorker::updateNodeId");
            return false;
        }
        LOGW_SYNCPAL_DEBUG(_logger, _side << L" update tree: Real node ID updated with tmp node ID. Should never happen...");
    }

    // merging tmpNode's children to realNode
    for (auto &child: tmpNode->children()) {
        if (!child.second->setParentNode(realNode)) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in Node::setParentNode: node name="
                                               << SyncName2WStr(realNode->name()).c_str() << L" parent node name="
                                               << SyncName2WStr(child.second->name()).c_str());
            return false;
        }

        if (!realNode->insertChildren(child.second)) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in Node::insertChildren: node name=" << SyncName2WStr(child.second->name()).c_str()
                                                                                    << L" parent node name="
                                                                                    << SyncName2WStr(realNode->name()).c_str());
            return false;
        }
    }

    // temp node removed from children list
    std::shared_ptr<Node> parentTmpNode = tmpNode->parentNode();
    parentTmpNode->deleteChildren(tmpNode);

    // Real node added as child of parent node
    if (!realNode->parentNode()->insertChildren(realNode)) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in Node::insertChildren: node name="
                                           << SyncName2WStr(realNode->name()).c_str() << L" parent node name="
                                           << SyncName2WStr(realNode->parentNode()->name()).c_str());
        return false;
    }

    return true;
}

bool UpdateTreeWorker::integrityCheck() {
    // TODO : check if this does not slow the process too much
    LOGW_SYNCPAL_INFO(_logger, _side << L" update tree integrity check started");
    for (const auto &node: _updateTree->nodes()) {
        if (node.second->isTmp() || Utility::startsWith(*node.second->id(), "tmp_")) {
            LOGW_SYNCPAL_WARN(_logger,
                              _side << L" update tree integrity check failed. A temporary node remains in the update tree: "
                                    << L" (node ID: '" << Utility::s2ws(node.second->id().value_or("")) << L"', DB ID: '"
                                    << node.second->idb().value_or(-1) << L"', name: '" << SyncName2WStr(node.second->name())
                                    << L"')");
            sentry::Handler::captureMessage(sentry::Level::Warning, "UpdateTreeWorker::integrityCheck",
                                            "A temporary node remains in the update tree");
            return false;
        }
    }
    return true;
}

ExitCode UpdateTreeWorker::getNewPathAfterMove(const SyncPath &path, SyncPath &newPath) {
    const std::vector<SyncName> itemNames = Utility::splitPath(path);
    std::vector<NodeId> nodeIds(itemNames.size());

    // Vector ID
    SyncPath tmpPath;
    auto nameIt = itemNames.rbegin();
    auto nodeIdIt = nodeIds.rbegin();

    for (; nameIt != itemNames.rend() && nodeIdIt != nodeIds.rend(); ++nameIt, ++nodeIdIt) {
        tmpPath /= *nameIt;
        bool found = false;
        std::optional<NodeId> tmpNodeId{std::nullopt};
        if (!_syncDb->id(_side, tmpPath, tmpNodeId, found)) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::id");
            return ExitCode::DbError;
        }
        if (!found || !tmpNodeId.has_value()) {
            LOGW_SYNCPAL_WARN(_logger, L"Node not found for " << Utility::formatSyncPath(tmpPath).c_str());
            return ExitCode::DataError;
        }
        *nodeIdIt = *tmpNodeId;
    }

    nameIt = itemNames.rbegin();
    nodeIdIt = nodeIds.rbegin();

    for (; nameIt != itemNames.rend() && nodeIdIt != nodeIds.rend(); ++nameIt, ++nodeIdIt) {
        if (FSOpPtr op = nullptr; _operationSet->findOp(*nodeIdIt, OperationType::Move, op)) {
            newPath = op->destinationPath();
        } else {
            newPath /= *nameIt;
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

        bool found = false;

        // update myself
        // if it's a create we don't have node's database data
        if (node->hasChangeEvent(OperationType::Create)) {
            continue;
        }

        // if node is temporary node
        if (node->isTmp()) {
            if (const ExitCode exitCode = updateTmpNode(node); exitCode != ExitCode::Ok) {
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
        if (!_syncDbCache.node(_side, usableNodeId, dbNode, found)) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::node");
            return ExitCode::DbError;
        }
        if (!found) {
            LOG_SYNCPAL_WARN(_logger, "Failed to retrieve node for id=" << usableNodeId.c_str());
            return ExitCode::DataError;
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

        for (auto &nodeChild: node->children()) {
            nodeQueue.push(nodeChild.second);
        }
    }

    return ExitCode::Ok;
}

ExitCode UpdateTreeWorker::mergeNodeToParentChildren(std::shared_ptr<Node> parentNode, const std::shared_ptr<Node> node) {
    LOG_IF_FAIL(parentNode)
    LOG_IF_FAIL(node)

    for (auto &child: parentNode->children()) {
        if (child.second->name() == node->name() && child.second->isTmp()) {
            if (!mergingTempNodeToRealNode(child.second, node)) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTreeWorker::mergingTempNodeToRealNode");
                return ExitCode::DataError;
            }

            break;
        }
    }

    if (!parentNode->insertChildren(node)) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in Node::insertChildren: node name=" << SyncName2WStr(node->name()).c_str()
                                                                                << L" parent node name="
                                                                                << SyncName2WStr(parentNode->name()).c_str());
        return ExitCode::DataError;
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
                                                                << _side);
        return ExitCode::DataError;
    }
    if (!id.has_value()) {
        LOGW_SYNCPAL_WARN(_logger, L"Failed to retrieve ID for node= " << SyncName2WStr(tmpNode->name()).c_str());
        return ExitCode::DataError;
    }

    if (!_updateTree->updateNodeId(tmpNode, id.value())) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTreeWorker::updateNodeId");
        return ExitCode::DataError;
    }

    DbNodeId dbId;
    if (!_syncDbCache.dbId(_side, *id, dbId, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbId");
        return ExitCode::DbError;
    }
    if (!found) {
        LOGW_SYNCPAL_WARN(_logger, L"Node not found for ID = "
                                           << Utility::s2ws(*id).c_str() << L" (Node name: '"
                                           << SyncName2WStr(tmpNode->name()).c_str() << L"', node valid name: '"
                                           << SyncName2WStr(tmpNode->name()).c_str() << L"') on side" << _side);
        return ExitCode::DataError;
    }
    tmpNode->setIdb(dbId);
    tmpNode->setIsTmp(false);

    const auto &prevNode = _updateTree->nodes()[*id];
    if (prevNode) {
        // Update children list
        for (const auto &[_, childNode]: prevNode->children()) {
            if (!tmpNode->insertChildren(childNode)) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in Node::insertChildren: node name="
                                                   << SyncName2WStr(childNode->name()).c_str() << L" parent node name="
                                                   << SyncName2WStr(tmpNode->name()).c_str());
                return ExitCode::DataError;
            }

            // set new parent
            if (!childNode->setParentNode(tmpNode)) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in Node::setParentNode: node name="
                                                   << SyncName2WStr(tmpNode->name()).c_str() << L" parent node name="
                                                   << SyncName2WStr(childNode->name()).c_str());
                return ExitCode::DataError;
            }

            LOGW_SYNCPAL_DEBUG(_logger,
                               _side << L" update tree: Node '" << SyncName2WStr(childNode->name()).c_str() << L"' (node ID: '"
                                     << Utility::s2ws((childNode->id().has_value() ? *childNode->id() : std::string())).c_str()
                                     << L"') inserted in children list of node '" << SyncName2WStr(tmpNode->name()).c_str()
                                     << L"' (node ID: '"
                                     << Utility::s2ws((tmpNode->id().has_value() ? *tmpNode->id() : std::string())).c_str()
                                     << L"')");
        }

        // Update events
        if (tmpNode->changeEvents() != prevNode->changeEvents()) {
            // if it's a Move event
            if (prevNode->hasChangeEvent(OperationType::Move)) {
                tmpNode->setName(prevNode->name());
                tmpNode->setMoveOriginInfos(prevNode->moveOriginInfos());
            }

            // Update change events
            tmpNode->setChangeEvents(prevNode->changeEvents());

            LOGW_SYNCPAL_DEBUG(_logger,
                               _side << L" update tree: Changed events to '" << prevNode->changeEvents() << L"' for node '"
                                     << SyncName2WStr(tmpNode->name()).c_str() << L"' (node ID: '"
                                     << Utility::s2ws((tmpNode->id().has_value() ? *tmpNode->id() : std::string())).c_str()
                                     << L"')");
        }
    }

    _updateTree->nodes()[*id] = tmpNode;
    if (ParametersCache::isExtendedLogEnabled()) {
        LOGW_SYNCPAL_DEBUG(
                _logger,
                _side << L" update tree: Node '" << SyncName2WStr(tmpNode->name()).c_str() << L"' (node ID: '"
                      << Utility::s2ws((tmpNode->id().has_value() ? *tmpNode->id() : std::string())).c_str() << L"', DB ID: '"
                      << (tmpNode->idb().has_value() ? *tmpNode->idb() : -1) << L"', parent ID: '"
                      << Utility::s2ws(tmpNode->parentNode()->id().has_value() ? *tmpNode->parentNode()->id() : std::string())
                                 .c_str()
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
        if (tmpNode->hasChangeEvent(OperationType::Move)) {
            // Save origin file name
            names.push_back(tmpNode->moveOriginInfos().path().filename());

            // Get origin parent
            DbNode dbNode;
            bool found = false;
            if (!_syncDbCache.node(tmpNode->side(), tmpNode->moveOriginInfos().parentNodeId(), dbNode, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::node");
                return ExitCode::DbError;
            }
            if (!found) {
                LOG_SYNCPAL_WARN(_logger, "Failed to retrieve node for id="
                                                  << (node->id().has_value() ? *node->id() : std::string()).c_str());
                return ExitCode::DataError;
            }

            SyncPath localPath;
            SyncPath remotePath;
            if (!_syncDb->path(dbNode.nodeId(), localPath, remotePath, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::path");
                return ExitCode::DbError;
            }
            if (!found) {
                LOG_SYNCPAL_WARN(_logger, "Failed to retrieve node path for nodeId=" << dbNode.nodeId());
                return ExitCode::DataError;
            }

            path = _side == ReplicaSide::Local ? localPath : remotePath;
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

} // namespace KDC
