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

    protected:
        void execute() override;

    private:
        /**
         * @brief Generate the required operations to solve the conflict(s). Some conflicts need to be dealt with alone, while
         * other might be handled by batched.
         * @param conflict The conflict to be resolved.
         * @param continueSolving A boolean value indicating if we can generate more conflict resolution operations.
         * @return ExitCode indicating if the operation was successful.
         */
        ExitCode generateOperations(const Conflict &conflict, bool &continueSolving);

        /**
         * @brief Since we should never try to upload a dehydrated placeholder and in order to simplify the algorithm, for
         * dehydrated placeholder involved in a conflict we will always propagate a DELETE operation on local replica. Therefore,
         * there will be no conflict anymore and, if the file is still present on remote replica, it will be discovered as new on
         * next sync, and restored in the adequate location.
         * @param conflict The conflict to be resolved.
         * @param continueSolving A boolean value indicating if we can generate more conflict resolution operations.
         * @return ExitCode indicating if the operation was successful.
         */
        ExitCode handleConflictOnDehydratedPlaceholder(const Conflict &conflict, bool &continueSolving);

        /**
         * @brief If we have a conflict between a local edit and a remote operation,
         * and if the local edit is omitted (i.e., only propagating a creation date to the DB),
         * the remote operation always wins.
         * @param conflict The conflict to be resolved.
         * @param continueSolving A boolean value indicating if we can generate more conflict resolution operations.
         * @return ExitCode indicating if the operation was successful.
         */
        void handleConflictOnOmittedEdit(const Conflict &conflict, bool &continueSolving);

        /**
         * @brief For Create-Create and Edit-Edit conflicts, the local file is renamed and excluded from the sync in order no to
         * lose any changes. The remote file will be pulled on next sync.
         * @param conflict The conflict to be resolved.
         * @param continueSolving A boolean value indicating if we can generate more conflict resolution operations.
         * @return ExitCode indicating if the operation was successful.
         */
        ExitCode generateLocalRenameOperation(const Conflict &conflict, bool &continueSolving);
        /**
         * @brief If the edited file is inside a deleted folder, move it to the rescue folder and propagate the Delete operation
         * next sync. Otherwise, remove item from DB so it will be restored on next sync with its updated content.
         * @param conflict The conflict to be resolved.
         * @param continueSolving A boolean value indicating if we can generate more conflict resolution operations.
         * @return ExitCode indicating if the operation was successful.
         */
        ExitCode generateEditDeleteConflictOperation(const Conflict &conflict, bool &continueSolving);
        /**
         * @brief If the moved item is local, revert the move operation. If the created item is local, rename it as a conflicted
         * file. Remote always wins.
         * @param conflict The conflict to be resolved.
         * @param continueSolving A boolean value indicating if we can generate more conflict resolution operations.
         * @return ExitCode indicating if the operation was successful.
         */
        ExitCode generateMoveCreateConflictOperation(const Conflict &conflict, bool &continueSolving);
        /**
         * @brief If the move operation happens within a directory that was deleted on the other replica, therefore, we ignore the
         * Move-Delete conflict. This conflict will be handled as a Move-ParentDelete conflict. Otherwise, rescue the eventual
         * edited files then propagate the delete operation.
         * @param conflict The conflict to be resolved.
         * @param continueSolving A boolean value indicating if we can generate more conflict resolution operations.
         * @return ExitCode indicating if the operation was successful.
         */
        ExitCode generateMoveDeleteConflictOperation(const Conflict &conflict, bool &continueSolving);
        /**
         * @brief Rescue the eventual edited files then propagate the delete operation.
         * @param conflict The conflict to be resolved.
         * @return ExitCode indicating if the operation was successful.
         */
        ExitCode generateParentDeleteConflictOperation(const Conflict &conflict);
        /**
         * @brief For Move-Move(Source), Move-Move(Dest), Move-Move(Cycle) and Move-Create conflicts, we revert one of the move
         * operation.
         * @param conflict The conflict to be resolved.
         * @param loserNode The node on which the operation will be applied.
         * @param conflict The conflict to be resolved.
         */
        ExitCode generateUndoMoveOperation(const Conflict &conflict, std::shared_ptr<Node> loserNode);

        /**
         * @brief Iterate through all children of `node` to check if there are items modified locally but not yet synchronized.
         * @param conflict The conflict to be resolved.
         * @param node The node that might need to be rescued.
         */
        void rescueModifiedLocalNodes(const Conflict &conflict, std::shared_ptr<Node> node);
        /**
         * @brief Generate the operation that will move the edited file into the rescue folder.
         * @param conflict The conflict to be resolved.
         * @param node The node that might need to be rescued.
         */
        void generateRescueOperation(const Conflict &conflict, std::shared_ptr<Node> node);

        /*
         * If return false, the file path is too long, the file needs to be moved to root directory
         */
        bool generateConflictedName(std::shared_ptr<Node> node, SyncName &newName, bool isOrphanNode = false) const;

        ExitCode undoMove(std::shared_ptr<Node> moveNode, SyncOpPtr moveOp);

        static std::wstring getLogString(SyncOpPtr op);

        friend class TestConflictResolverWorker;
};

} // namespace KDC
