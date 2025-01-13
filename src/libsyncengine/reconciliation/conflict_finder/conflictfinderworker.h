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

#pragma once

#include "update_detection/update_detector/updatetree.h"
#include "syncpal/operationprocessor.h"
#include "syncpal/syncpal.h"
#include "conflict.h"

#include <list>

namespace KDC {

class ConflictFinderWorker : public OperationProcessor {
    public:
        ConflictFinderWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName);

        void execute() override;
        void findConflicts();
        void findConflictsInTree(std::shared_ptr<UpdateTree> localTree, std::shared_ptr<UpdateTree> remoteTree,
                                 std::vector<std::shared_ptr<Node>> &localMoveDirNodes,
                                 std::vector<std::shared_ptr<Node>> &remoteMoveDirNodes);

    private:
        std::optional<Conflict> checkCreateCreateConflict(std::shared_ptr<Node> createNode);
        std::optional<Conflict> checkEditEditConflict(std::shared_ptr<Node> editNode);
        std::optional<Conflict> checkMoveCreateConflict(std::shared_ptr<Node> moveNode);
        std::optional<Conflict> checkEditDeleteConflict(std::shared_ptr<Node> deleteNode);
        std::optional<Conflict> checkMoveDeleteConflict(std::shared_ptr<Node> deleteNode);
        std::optional<std::vector<Conflict>> checkMoveParentDeleteConflicts(std::shared_ptr<Node> deleteNode);
        std::optional<std::vector<Conflict>> checkCreateParentDeleteConflicts(std::shared_ptr<Node> deleteNode);
        std::optional<Conflict> checkMoveMoveSourceConflict(std::shared_ptr<Node> moveNode);
        std::optional<Conflict> checkMoveMoveDestConflict(std::shared_ptr<Node> moveNode);
        std::optional<std::vector<Conflict>> determineMoveMoveCycleConflicts(
                std::vector<std::shared_ptr<Node>> localMoveDirNodes, std::vector<std::shared_ptr<Node>> remoteMoveDirNodes);
        std::optional<std::vector<std::shared_ptr<Node>>> findChangeEventInSubNodes(OperationType event,
                                                                                    std::shared_ptr<Node> parentNode);

        friend class TestConflictFinderWorker;
};

} // namespace KDC
