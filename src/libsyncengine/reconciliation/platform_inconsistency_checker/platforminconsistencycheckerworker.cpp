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
                                                                       const std::string &shortName)
    : OperationProcessor(syncPal, name, shortName) {}

void PlatformInconsistencyCheckerWorker::execute() {
    LOG_SYNCPAL_DEBUG(_logger, "Worker started: name=" << name().c_str());
    auto start = std::chrono::steady_clock::now();

    _idsToBeRemoved.clear();

    ExitCode exitCode = checkTree(_syncPal->_remoteUpdateTree->rootNode(), _syncPal->_remoteUpdateTree->rootNode()->name());

    for (const auto &id : _idsToBeRemoved) {
        _syncPal->_remoteUpdateTree->deleteNode(id);
    }

    std::chrono::duration<double> elapsed_seconds = std::chrono::steady_clock::now() - start;
    LOG_SYNCPAL_DEBUG(_logger, "Platform Inconsistency checked tree in: " << elapsed_seconds.count() << "s");

    _syncPal->_remoteUpdateTree->setInconsistencyCheckDone();

    setDone(exitCode);
    LOG_SYNCPAL_DEBUG(_logger, "Worker stopped: name=" << name().c_str());
}

ExitCode PlatformInconsistencyCheckerWorker::checkTree(std::shared_ptr<Node> remoteNode, const SyncPath &parentPath) {
    if (remoteNode->hasChangeEvent(OperationTypeDelete)) {
        return ExitCodeOk;
    }

    if (remoteNode->hasChangeEvent(OperationTypeCreate) || remoteNode->hasChangeEvent(OperationTypeMove)) {
        if (!checkPathAndName(remoteNode)) {
            // Item has been blacklisted
            return ExitCodeOk;
        }
    }

    bool checkAgainstSiblings = false;

    auto it = remoteNode->children().begin();
    for (; it != remoteNode->children().end(); it++) {
        if (stopAsked()) {
            return ExitCodeOk;
        }

        while (pauseAsked() || isPaused()) {
            if (!isPaused()) {
                setPauseDone();
            }

            Utility::msleep(LOOP_PAUSE_SLEEP_PERIOD);
        }

        std::shared_ptr<Node> currentChildNode = it->second;

        if (currentChildNode->hasChangeEvent(OperationTypeCreate) || currentChildNode->hasChangeEvent(OperationTypeMove)) {
            checkAgainstSiblings = true;
        }

        ExitCode exitCode = checkTree(currentChildNode, parentPath / remoteNode->finalLocalName());
        if (exitCode != ExitCodeOk) {
            return exitCode;
        }
    }

    if (checkAgainstSiblings) {
        checkNameClashAgainstSiblings(remoteNode);
    }

    return ExitCodeOk;
}

bool PlatformInconsistencyCheckerWorker::checkPathAndName(std::shared_ptr<Node> remoteNode) {
    SyncPath path = remoteNode->getPath();
    if (PlatformInconsistencyCheckerUtility::instance()->checkNameForbiddenChars(remoteNode->name())) {
        _syncPal->blacklistTemporarily(remoteNode->id().value(), path, remoteNode->side());
        Error error(_syncPal->syncDbId(), "", remoteNode->id().value(), remoteNode->type(), path, ConflictTypeNone,
                    InconsistencyTypeForbiddenChar);
        _syncPal->addError(error);

        LOGW_SYNCPAL_INFO(_logger, L"Blacklisting item \"" << Path2WStr(path).c_str() << L"\" because of a forbidden character");

        _idsToBeRemoved.push_back(remoteNode->id().value());
        return false;
    }

    // Check reserved names and name size
    if (PlatformInconsistencyCheckerUtility::instance()->checkReservedNames(remoteNode->name())) {
        _syncPal->blacklistTemporarily(remoteNode->id().value(), path, remoteNode->side());
        Error error(_syncPal->syncDbId(), "", remoteNode->id().value(), remoteNode->type(), path, ConflictTypeNone,
                    InconsistencyTypeReservedName);
        _syncPal->addError(error);

        LOGW_SYNCPAL_INFO(_logger,
                          L"Blacklisting item \"" << Path2WStr(path).c_str() << L"\" because of the name is reserved on this OS");

        _idsToBeRemoved.push_back(remoteNode->id().value());
        return false;
    }

    if (PlatformInconsistencyCheckerUtility::instance()->checkNameSize(remoteNode->name())) {
        _syncPal->blacklistTemporarily(remoteNode->id().value(), path, remoteNode->side());
        Error error(_syncPal->syncDbId(), "", remoteNode->id().value(), remoteNode->type(), path, ConflictTypeNone,
                    InconsistencyTypeNameLength);
        _syncPal->addError(error);

        LOGW_SYNCPAL_INFO(_logger, L"Blacklisting item \"" << Path2WStr(path).c_str() << L"\" because of name size");

        _idsToBeRemoved.push_back(remoteNode->id().value());
        return false;
    }

    return true;
}

void PlatformInconsistencyCheckerWorker::checkNameClashAgainstSiblings(std::shared_ptr<Node> remoteParentNode) {
#if defined(__APPLE__) || defined(_WIN32)
    std::unordered_map<SyncName, std::shared_ptr<Node>> processedNodesByName;  // key: lowercase name
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
        SyncName lowercaseName = Poco::toLower(it->second->finalLocalName());
        auto insertInfo = processedNodesByName.insert({lowercaseName, currentChildNode});
        if (!insertInfo.second) {
            // Check if this node is a special folder
            bool isSpecialFolder = currentChildNode->isCommonDocumentsFolder() || currentChildNode->isSharedFolder();

            std::shared_ptr<Node> prevChildNode = insertInfo.first->second;

            // Some software save their files by deleting and re-creating them (Delete-Create), or by deleting the original file
            // and renaming a temporary file that contains the latest version (Delete-Move) In those cases, we should not check
            // for name clash, it is ok to have 2 children with the same name
            const auto oneNodeIsDeleted = [](const std::shared_ptr<Node> &node, const std::shared_ptr<Node> &prevNode) -> bool {
                return node->hasChangeEvent(OperationTypeMove | OperationTypeCreate) &&
                       prevNode->hasChangeEvent(OperationTypeDelete);
            };

            if (oneNodeIsDeleted(currentChildNode, prevChildNode) || oneNodeIsDeleted(prevChildNode, currentChildNode)) {
                continue;
            }

            if (currentChildNode->hasChangeEvent() && !isSpecialFolder) {
                // Blacklist the new one
                SyncPath path = currentChildNode->getPath();
                _syncPal->blacklistTemporarily(currentChildNode->id().value(), path, currentChildNode->side());
                Error error(_syncPal->syncDbId(), "", currentChildNode->id().value(), currentChildNode->type(), path,
                            ConflictTypeNone, InconsistencyTypeCase);
                _syncPal->addError(error);

                LOGW_SYNCPAL_INFO(_logger, L"Blacklisting item \"" << Path2WStr(path).c_str() << L"\" because of a name clash");

                _idsToBeRemoved.push_back(currentChildNode->id().value());
                continue;
            } else {
                // Blacklist the previously discovered child
                SyncPath path = prevChildNode->getPath();
                _syncPal->blacklistTemporarily(prevChildNode->id().value(), path, prevChildNode->side());
                Error error(_syncPal->syncDbId(), "", prevChildNode->id().value(), prevChildNode->type(), path, ConflictTypeNone,
                            InconsistencyTypeCase);
                _syncPal->addError(error);

                LOGW_SYNCPAL_INFO(_logger, L"Blacklisting item \"" << Path2WStr(path).c_str() << L"\" because of a name clash");

                _idsToBeRemoved.push_back(prevChildNode->id().value());
                continue;
            }
        }
    }
#else
    (void)remoteParentNode;
#endif
}

}  // namespace KDC
