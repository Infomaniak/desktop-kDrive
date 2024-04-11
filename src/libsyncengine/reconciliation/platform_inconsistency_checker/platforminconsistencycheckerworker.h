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

#pragma once

#include "syncpal/operationprocessor.h"
#include "syncpal/syncpal.h"

namespace KDC {

class PlatformInconsistencyCheckerWorker : public OperationProcessor {
    public:
        PlatformInconsistencyCheckerWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name,
                                           const std::string &shortName);

        void execute() override;

    private:
        ExitCode checkTree(std::shared_ptr<Node> remoteNode, const SyncPath &parentPath);

        void blacklistNode(const std::shared_ptr<Node> remoteNode, const SyncPath &relativePath,
                           const InconsistencyType inconsistencyType);
        bool checkPathAndName(std::shared_ptr<Node> remoteNode);
        void checkNameClashAgainstSiblings(std::shared_ptr<Node> remoteParentNode);

        std::list<NodeId> _idsToBeRemoved;

        friend class TestPlatformInconsistencyCheckerWorker;
};

}  // namespace KDC
