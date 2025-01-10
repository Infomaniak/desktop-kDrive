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

#include "syncpal/operationprocessor.h"
#include "syncpal/syncpal.h"
#include "reconciliation/conflict_finder/conflict.h"
#include "reconciliation/syncoperation.h"

namespace KDC {

class ConflictResolverWorker : public OperationProcessor {
    public:
        ConflictResolverWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName);

        inline const std::unordered_map<DbNodeId, ReplicaSide> &registeredOrphans() const { return _registeredOrphans; }

    protected:
        virtual void execute() override;

    private:
        std::unordered_map<DbNodeId, ReplicaSide> _registeredOrphans; // key: DB node ID, value : winner side

        ExitCode generateOperations(const Conflict &conflict, bool &continueSolving);

        /*
         * If return false, the file path is too long, the file needs to be moved to root directory
         */
        bool generateConflictedName(const std::shared_ptr<Node> node, SyncName &newName, bool isOrphanNode = false) const;

        static void findAllChildNodes(const std::shared_ptr<Node> parentNode,
                                      std::unordered_set<std::shared_ptr<Node>> &children);
        ExitCode findAllChildNodeIdsFromDb(const std::shared_ptr<Node> parentNode, std::unordered_set<DbNodeId> &childrenDbIds);
        ExitCode undoMove(const std::shared_ptr<Node> moveNode, SyncOpPtr moveOp);

        friend class TestConflictResolverWorker;
};

} // namespace KDC
