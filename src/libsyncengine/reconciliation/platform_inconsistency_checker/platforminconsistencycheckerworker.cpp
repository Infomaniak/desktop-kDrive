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

#include "platforminconsistencycheckerworker.h"
#include "platforminconsistencycheckerutility.h"
#include "libcommon/log/sentry/ptraces.h"
#include "libcommonserver/utility/utility.h"
#include "utility/timerutility.h"

#include <log4cplus/loggingmacros.h>

#include <Poco/String.h>

namespace KDC {

PlatformInconsistencyCheckerWorker::PlatformInconsistencyCheckerWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name,
                                                                       const std::string &shortName) :
    OperationProcessor(syncPal, name, shortName) {}

void PlatformInconsistencyCheckerWorker::execute() {
    LOG_SYNCPAL_DEBUG(_logger, "Worker started: name=" << name());
    const TimerUtility timer;

    _idsToBeRemoved.clear();

    checkTree(ReplicaSide::Remote);
    checkTree(ReplicaSide::Local);

    for (const auto &[remoteId, localId]: _idsToBeRemoved) {
        if (!remoteId.empty() && !_syncPal->updateTree(ReplicaSide::Remote)->deleteNode(remoteId)) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTree::deleteNode: node id=" << CommonUtility::s2ws(remoteId));
        }
        if (!localId.empty() && !_syncPal->updateTree(ReplicaSide::Local)->deleteNode(localId)) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTree::deleteNode: node id=" << CommonUtility::s2ws(localId));
        }
    }

    LOG_SYNCPAL_DEBUG(_logger, "Platform Inconsistency checked tree in: " << timer.elapsed<DoubleSeconds>().count() << "s");

    LOG_SYNCPAL_DEBUG(_logger, "Worker stopped: name=" << name());
    setDone(ExitCode::Ok);
}

ExitCode PlatformInconsistencyCheckerWorker::checkTree(ReplicaSide side) {
    std::shared_ptr<Node> node = _syncPal->updateTree(side)->rootNode();
    const SyncPath &parentPath = node->name();
    assert(side == ReplicaSide::Remote ||
           side == ReplicaSide::Local && "Invalid side in PlatformInconsistencyCheckerWorker::checkTree");

    ExitCode exitCode = ExitCode::Unknown;
    sentry::PTraceUPtr perfmonitor;
    if (side == ReplicaSide::Remote) {
        perfmonitor = std::make_unique<sentry::pTraces::scoped::CheckLocalTree>(syncDbId());
        exitCode = checkRemoteTree(node, parentPath);
    } else if (side == ReplicaSide::Local) {
        perfmonitor = std::make_unique<sentry::pTraces::scoped::CheckRemoteTree>(syncDbId());
        exitCode = checkLocalTree(node, parentPath);
    }

    if (exitCode == ExitCode::Ok) {
        perfmonitor->stop();
    } else {
        LOG_SYNCPAL_WARN(_logger,
                         "PlatformInconsistencyCheckerWorker::check" << side << "Tree partially failed: code=" << exitCode);
    }

    return exitCode;
}

ExitCode PlatformInconsistencyCheckerWorker::checkRemoteTree(std::shared_ptr<Node> remoteNode, const SyncPath &parentPath) {
    if (remoteNode->hasChangeEvent(OperationType::Delete)) {
        return ExitCode::Ok;
    }

    if (pathChanged(remoteNode) && !checkPathAndName(remoteNode)) {
        // Item has been blacklisted
        return ExitCode::Ok;
    }

    bool checkAgainstSiblings = false;

    auto it = remoteNode->children().begin();
    for (; it != remoteNode->children().end(); it++) {
        if (stopAsked()) {
            return ExitCode::Ok;
        }

        std::shared_ptr<Node> currentChildNode = it->second;
        if (pathChanged(currentChildNode)) {
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
    if (pathChanged(localNode) && PlatformInconsistencyCheckerUtility::instance()->isNameTooLong(localNode->name())) {
        blacklistNode(localNode, InconsistencyType::NameLength);
        return ExitCode::Ok;
    }
    if (!localNode->isRoot() && PlatformInconsistencyCheckerUtility::isNameOnlySpaces(localNode->name())) {
        blacklistNode(localNode, InconsistencyType::ForbiddenCharOnlySpaces);
        return ExitCode::Ok;
    }
    for (auto it = localNode->children().begin(); it != localNode->children().end(); ++it) {
        if (stopAsked()) {
            return ExitCode::Ok;
        }

        if (const auto exitCode = checkLocalTree(it->second, parentPath / localNode->name()); exitCode != ExitCode::Ok) {
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

    if (localNode && localNode->id().has_value()) {
        _syncPal->blacklistTemporarily(localNode->id().value(), localNode->getPath(), localNode->side());
    }
    if (remoteNode && remoteNode->id().has_value()) {
        _syncPal->blacklistTemporarily(remoteNode->id().value(), remoteNode->getPath(), remoteNode->side());
    }

    Error error(_syncPal->syncDbId(), "", node->id().value(), node->type(), node->getPath(), ConflictType::None,
                inconsistencyType);
    _syncPal->addError(error);

    LOGW_SYNCPAL_INFO(_logger, L"Blacklisting " << node->side() << L" item with " << Utility::formatSyncPath(node->getPath())
                                                << L" because " << inconsistencyType << L".");

    auto safeNodeId = [](const std::shared_ptr<Node> &unsafeNodePtr) {
        return (unsafeNodePtr && unsafeNodePtr->id().has_value()) ? *unsafeNodePtr->id() : NodeId();
    };
    nodeIDs.remoteId = safeNodeId(remoteNode);
    nodeIDs.localId = safeNodeId(localNode);
    _idsToBeRemoved.emplace_back(nodeIDs);
}

bool PlatformInconsistencyCheckerWorker::checkPathAndName(std::shared_ptr<Node> remoteNode) {
    const SyncPath relativePath = remoteNode->getPath();
    if (PlatformInconsistencyCheckerUtility::instance()->nameHasForbiddenChars(remoteNode->name())) {
        blacklistNode(remoteNode, InconsistencyType::ForbiddenChar);
        return false;
    }

    if (PlatformInconsistencyCheckerUtility::nameEndWithForbiddenSpace(remoteNode->name())) {
        blacklistNode(remoteNode, InconsistencyType::ForbiddenCharEndWithSpace);
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
#if defined(KD_MACOS) || defined(KD_WINDOWS)
    std::unordered_map<SyncName, std::shared_ptr<Node>> processedNodesByName; // key: lowercase name
    auto childrenCopy = remoteParentNode->children();
    auto it = childrenCopy.begin();
    for (; it != childrenCopy.end(); it++) {
        if (stopAsked()) {
            return;
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
            const auto oneNodeIsDeleted = [this](const std::shared_ptr<Node> &node,
                                                 const std::shared_ptr<Node> &prevNode) -> bool {
                return pathChanged(node) && prevNode->hasChangeEvent(OperationType::Delete);
            };

            if (oneNodeIsDeleted(currentChildNode, prevChildNode) || oneNodeIsDeleted(prevChildNode, currentChildNode)) {
                continue;
            }

            if (currentChildNode->hasChangeEvent() && !isSpecialFolder) {
                // Blacklist the new one
                blacklistNode(currentChildNode, InconsistencyType::Case);
            } else {
                // Blacklist the previously discovered child
                blacklistNode(prevChildNode, InconsistencyType::Case);
            }
        }
    }
#else
    (void) remoteParentNode;
#endif
}

bool PlatformInconsistencyCheckerWorker::pathChanged(std::shared_ptr<Node> node) const {
    return node->hasChangeEvent(OperationType::Create) || node->hasChangeEvent(OperationType::Move);
}
} // namespace KDC
