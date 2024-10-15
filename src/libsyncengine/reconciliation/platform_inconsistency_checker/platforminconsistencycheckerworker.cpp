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

#include "platforminconsistencycheckerworker.h"
#include "platforminconsistencycheckerutility.h"
#include "libcommonserver/utility/utility.h"

#include <log4cplus/loggingmacros.h>

#include <Poco/String.h>

namespace KDC {

PlatformInconsistencyCheckerWorker::PlatformInconsistencyCheckerWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name,
                                                                       const std::string &shortName) :
    OperationProcessor(syncPal, name, shortName) {}

void PlatformInconsistencyCheckerWorker::execute() {
    LOG_SYNCPAL_DEBUG(_logger, "Worker started: name=" << name().c_str());
    auto start = std::chrono::steady_clock::now();

    _idsToBeRemoved.clear();

   const ExitCode exitCodeRemote = checkRemoteTree(_syncPal->updateTree(ReplicaSide::Remote)->rootNode(),
                                              _syncPal->updateTree(ReplicaSide::Remote)->rootNode()->name());

    const ExitCode exitCodeLocal = checkLocalTree(_syncPal->updateTree(ReplicaSide::Local)->rootNode(),
                                            _syncPal->updateTree(ReplicaSide::Local)->rootNode()->name());

    ExitCode exitCode = ExitCode::Ok;
    if (exitCodeRemote != ExitCode::Ok) {
        LOG_SYNCPAL_WARN(_logger,
                         "PlatformInconsistencyCheckerWorker::checkRemoteTree partially failed, ExitCode=" << exitCodeRemote);
        exitCode = exitCodeRemote;
    }

    if (exitCodeLocal != ExitCode::Ok) {
        LOG_SYNCPAL_WARN(_logger,
                         "PlatformInconsistencyCheckerWorker::checkLocalTree partially failed, ExitCode=" << exitCodeLocal);
        exitCode = exitCodeLocal;
    }

    for (const auto &idItem: _idsToBeRemoved) {
        if (!_syncPal->updateTree(ReplicaSide::Remote)->deleteNode(idItem.remoteId)) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTree::deleteNode: node id=" << Utility::s2ws(idItem.remoteId.c_str()));
        }
        if (!_syncPal->updateTree(ReplicaSide::Local)->deleteNode(idItem.localId)) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTree::deleteNode: node id=" << Utility::s2ws(idItem.localId.c_str()));
        }
    }

    std::chrono::duration<double> elapsed_seconds = std::chrono::steady_clock::now() - start;
    LOG_SYNCPAL_DEBUG(_logger, "Platform Inconsistency checked tree in: " << elapsed_seconds.count() << "s");

    _syncPal->updateTree(ReplicaSide::Remote)->setInconsistencyCheckDone();

    setDone(exitCode);
    LOG_SYNCPAL_DEBUG(_logger, "Worker stopped: name=" << name().c_str());
}

ExitCode PlatformInconsistencyCheckerWorker::checkRemoteTree(std::shared_ptr<Node> remoteNode, const SyncPath &parentPath) {
    if (remoteNode->hasChangeEvent(OperationType::Delete)) {
        return ExitCode::Ok;
    }

    if (remoteNode->hasChangeEvent(OperationType::Create) || remoteNode->hasChangeEvent(OperationType::Move)) {
        if (!checkPathAndName(remoteNode)) {
            // Item has been blacklisted
            return ExitCode::Ok;
        }
    }

    bool checkAgainstSiblings = false;

    auto it = remoteNode->children().begin();
    for (; it != remoteNode->children().end(); it++) {
        if (stopAsked()) {
            return ExitCode::Ok;
        }

        while (pauseAsked() || isPaused()) {
            if (!isPaused()) {
                setPauseDone();
            }

            Utility::msleep(LOOP_PAUSE_SLEEP_PERIOD);
        }

        std::shared_ptr<Node> currentChildNode = it->second;

        if (currentChildNode->hasChangeEvent(OperationType::Create) || currentChildNode->hasChangeEvent(OperationType::Move)) {
            checkAgainstSiblings = true;
        }

        ExitCode exitCode = checkRemoteTree(currentChildNode, parentPath / remoteNode->name());
        if (exitCode != ExitCode::Ok) {
            return exitCode;
        }
    }

    if (checkAgainstSiblings) {
        checkNameClashAgainstSiblings(remoteNode);
    }

    return ExitCode::Ok;
}

ExitCode PlatformInconsistencyCheckerWorker::checkLocalTree(std::shared_ptr<Node> localNode, const SyncPath &parentPath) {
    if (localNode->hasChangeEvent(OperationType::Delete)) {
        return ExitCode::Ok;
    }

    if (localNode->hasChangeEvent(OperationType::Create) || localNode->hasChangeEvent(OperationType::Move)) {
        if (PlatformInconsistencyCheckerUtility::instance()->isNameTooLong(localNode->name())) {
            blacklistNode(localNode, InconsistencyType::NameLength);
            return ExitCode::Ok;
        }
    }

    auto childIt = localNode->children().begin();
    for (; childIt != localNode->children().end(); childIt++) {
        if (stopAsked()) {
            return ExitCode::Ok;
        }

        while (pauseAsked() || isPaused()) {
            if (!isPaused()) {
                setPauseDone();
            }

            Utility::msleep(LOOP_PAUSE_SLEEP_PERIOD);
        }

       const ExitCode exitCode = checkLocalTree(childIt->second, parentPath / localNode->name());
        if (exitCode != ExitCode::Ok) {
            return exitCode;
        }
    }
    return ExitCode::Ok;
}

void PlatformInconsistencyCheckerWorker::blacklistNode(const std::shared_ptr<Node> node,
                                                       const InconsistencyType inconsistencyType) {
    // Local node needs to be excluded before call to blacklistTemporarily because
    // we need the DB entry to retrieve the corresponding node
    NodeIdPair nodeIDs;
    const std::shared_ptr<Node> localNode = node->side() == ReplicaSide::Local ? node : correspondingNodeDirect(node);
    const std::shared_ptr<Node> remoteNode = node->side() == ReplicaSide::Remote ? node : correspondingNodeDirect(node);

    if (localNode) {
        const SyncPath absoluteLocalPath = _syncPal->localPath() / localNode->getPath();
        LOGW_SYNCPAL_INFO(_logger,
                          L"Excluding local item with " << Utility::formatSyncPath(absoluteLocalPath) << L".");
        PlatformInconsistencyCheckerUtility::renameLocalFile(
                absoluteLocalPath, node->side() == ReplicaSide::Remote
                                           ? PlatformInconsistencyCheckerUtility::SuffixType::Conflict
                                           : PlatformInconsistencyCheckerUtility::SuffixType::Blacklisted);

        if (node->side() == ReplicaSide::Local) {
            bool found = false;
            DbNodeId dbId = -1;
            if (!_syncPal->syncDb()->dbId(ReplicaSide::Local, *localNode->id(), dbId, found)) {
                LOGW_WARN(_logger,
                          L"Failed to retrieve dbId for local node: " << Utility::formatSyncPath(absoluteLocalPath));
            }
            if (found && !_syncPal->syncDb()->deleteNode(dbId, found)) {
                // Remove node (and childs by cascade) from DB if it exists (else ignore as it is already not in DB)
                LOGW_WARN(_logger, L"Failed to delete node from DB: " << Utility::formatSyncPath(absoluteLocalPath));
            }

            if (!_syncPal->vfsFileStatusChanged(absoluteLocalPath, node->side() == ReplicaSide::Remote ? SyncFileStatus::Conflict
                                                                                                       : SyncFileStatus::Error)) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in SyncPal::vfsFileStatusChanged: "
                                                   << Utility::formatSyncPath(absoluteLocalPath).c_str());
            }
        }
    }

    if (node->side() == ReplicaSide::Remote) {
        _syncPal->blacklistTemporarily(remoteNode->id().value(), remoteNode->getPath(), remoteNode->side());
    }

    Error error(_syncPal->syncDbId(), "", node->id().value(), node->type(), node->getPath(), ConflictType::None,
                inconsistencyType);
    _syncPal->addError(error);

    LOGW_SYNCPAL_INFO(_logger, L"Blacklisting " << node->side() << L" item with "
                                                << Utility::formatSyncPath(node->getPath()) << L" because "
                                                << inconsistencyType << L".");

    nodeIDs.remoteId = (remoteNode && remoteNode->id().has_value()) ? *remoteNode->id() : NodeId();
    nodeIDs.localId = (localNode && localNode->id().has_value()) ? *localNode->id() : NodeId();

    _idsToBeRemoved.emplace_back(nodeIDs);
}

bool PlatformInconsistencyCheckerWorker::checkPathAndName(std::shared_ptr<Node> remoteNode) {
    const SyncPath relativePath = remoteNode->getPath();
    if (PlatformInconsistencyCheckerUtility::instance()->nameHasForbiddenChars(remoteNode->name())) {
        blacklistNode(remoteNode, InconsistencyType::ForbiddenChar);
        return false;
    }

    if (PlatformInconsistencyCheckerUtility::instance()->checkReservedNames(remoteNode->name())) {
        blacklistNode(remoteNode, InconsistencyType::ReservedName);
        return false;
    }

    if (PlatformInconsistencyCheckerUtility::instance()->isNameTooLong(remoteNode->name())) {
        blacklistNode(remoteNode, InconsistencyType::NameLength);
        return false;
    }

    return true;
}

void PlatformInconsistencyCheckerWorker::checkNameClashAgainstSiblings(const std::shared_ptr<Node> &remoteParentNode) {
#if defined(__APPLE__) || defined(_WIN32)
    std::unordered_map<SyncName, std::shared_ptr<Node>> processedNodesByName; // key: lowercase name
    auto it = remoteParentNode->children().begin();
    for (; it != remoteParentNode->children().end(); it++) {
        if (stopAsked()) {
            return;
        }

        while (pauseAsked() || isPaused()) {
            if (!isPaused()) {
                setPauseDone();
            }

            Utility::msleep(LOOP_PAUSE_SLEEP_PERIOD);
        }

        std::shared_ptr<Node> currentChildNode = it->second;

        // Check case conflicts
        SyncName lowercaseName = Poco::toLower(it->second->name());
        auto insertInfo = processedNodesByName.insert({lowercaseName, currentChildNode});
        if (!insertInfo.second) {
            // Check if this node is a special folder
            bool isSpecialFolder = currentChildNode->isCommonDocumentsFolder() || currentChildNode->isSharedFolder();

            std::shared_ptr<Node> prevChildNode = insertInfo.first->second;

            // Some software save their files by deleting and re-creating them (Delete-Create), or by deleting the original file
            // and renaming a temporary file that contains the latest version (Delete-Move) In those cases, we should not check
            // for name clash, it is ok to have 2 children with the same name
            const auto oneNodeIsDeleted = [](const std::shared_ptr<Node> &node, const std::shared_ptr<Node> &prevNode) -> bool {
                return node->hasChangeEvent(OperationType::Move | OperationType::Create) &&
                       prevNode->hasChangeEvent(OperationType::Delete);
            };

            if (oneNodeIsDeleted(currentChildNode, prevChildNode) || oneNodeIsDeleted(prevChildNode, currentChildNode)) {
                continue;
            }

            if (currentChildNode->hasChangeEvent() && !isSpecialFolder) {
                // Blacklist the new one
                blacklistNode(currentChildNode, InconsistencyType::Case);
                continue;
            } else {
                // Blacklist the previously discovered child
                blacklistNode(prevChildNode, InconsistencyType::Case);
                continue;
            }
        }
    }
#else
    (void) remoteParentNode;
#endif
}

} // namespace KDC
