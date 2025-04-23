// Infomaniak kDrive - Desktop
// Copyright (C) 2023-2025 Infomaniak Network SA
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "operationsorterfilter.h"

namespace KDC {

OperationSorterFilter::OperationSorterFilter(const std::unordered_map<UniqueId, SyncOpPtr> &ops) : _ops(ops) {}

void OperationSorterFilter::filterOperations() {
    std::unordered_map<SyncName, SyncOpPtr, StringHashFunction, std::equal_to<>> deleteBeforeMove;
    std::unordered_map<SyncName, SyncOpPtr, StringHashFunction, std::equal_to<>> moveBeforeCreate;
    std::unordered_map<SyncPath, SyncOpPtr> deletedDirectoryPaths;
    std::unordered_map<SyncPath, SyncOpPtr> moveOriginPaths;

    for (const auto &[_, op]: _ops) {
        filterFixDeleteBeforeMoveCandidates(op, deleteBeforeMove);
        filterFixMoveBeforeCreateCandidates(op, moveBeforeCreate);
        filterFixMoveBeforeDeleteCandidates(op, deletedDirectoryPaths, moveOriginPaths);

        filterFixEditBeforeMoveCandidates(op);
    }
}

void OperationSorterFilter::filterFixDeleteBeforeMoveCandidates(
        const SyncOpPtr &op, std::unordered_map<SyncName, SyncOpPtr, StringHashFunction, std::equal_to<>> &deleteBeforeMove) {
    if (op->affectedNode()->hasChangeEvent(OperationType::Delete)) {
        if (const auto [_, ok] = deleteBeforeMove.try_emplace(op->affectedNode()->name(), op); !ok) {
            const auto &otherOp = deleteBeforeMove.at(op->affectedNode()->name());
            if (op->targetSide() != otherOp->targetSide()) {
                return;
            }
            (void) _fixDeleteBeforeMoveCandidates.emplace_back(op, otherOp);
        }
    }
    if (op->affectedNode()->hasChangeEvent(OperationType::Move)) {
        if (const auto [_, ok] = deleteBeforeMove.try_emplace(op->affectedNode()->name(), op); !ok) {
            const auto &otherOp = deleteBeforeMove.at(op->affectedNode()->name());
            if (op->targetSide() != otherOp->targetSide()) {
                return;
            }
            (void) _fixDeleteBeforeMoveCandidates.emplace_back(op, otherOp);
        }
    }
}

void OperationSorterFilter::filterFixMoveBeforeCreateCandidates(
        const SyncOpPtr &op, std::unordered_map<SyncName, SyncOpPtr, StringHashFunction, std::equal_to<>> &moveBeforeCreate) {
    if (op->affectedNode()->hasChangeEvent(OperationType::Create)) {
        if (const auto [_, ok] = moveBeforeCreate.try_emplace(op->affectedNode()->name(), op); !ok) {
            const auto &otherOp = moveBeforeCreate.at(op->affectedNode()->name());
            if (op->targetSide() != otherOp->targetSide()) {
                return;
            }
            (void) _fixMoveBeforeCreateCandidates.emplace_back(op, otherOp);
        }
    }
    if (op->affectedNode()->hasChangeEvent(OperationType::Move)) {
        if (const auto [_, ok] =
                    moveBeforeCreate.try_emplace(Str2SyncName(op->affectedNode()->moveOriginInfos().path().filename()), op);
            !ok) {
            const auto &otherOp = moveBeforeCreate.at(op->affectedNode()->name());
            if (op->targetSide() != otherOp->targetSide()) {
                return;
            }
            (void) _fixMoveBeforeCreateCandidates.emplace_back(op, otherOp);
        }
    }
}

void OperationSorterFilter::filterFixMoveBeforeDeleteCandidates(const SyncOpPtr &op,
                                                                std::unordered_map<SyncPath, SyncOpPtr> &deletedDirectoryPaths,
                                                                std::unordered_map<SyncPath, SyncOpPtr> &moveOriginPaths) {
    if (op->affectedNode()->hasChangeEvent(OperationType::Delete)) {
        if (op->affectedNode()->type() != NodeType::Directory) {
            return;
        }
        const auto deletedPath = op->affectedNode()->getPath();
        (void) deletedDirectoryPaths.try_emplace(deletedPath, op);

        // Check if any of the moved item origin path is inside the deleted folder.
        for (const auto &[path, moveOp]: moveOriginPaths) {
            if (op->targetSide() != moveOp->targetSide()) {
                continue;
            }
            if (Utility::isDescendantOrEqual(path, deletedPath)) {
                (void) _fixMoveBeforeDeleteCandidates.emplace_back(op, moveOp);
            }
        }
        return;
    }

    if (op->affectedNode()->hasChangeEvent(OperationType::Move)) {
        (void) moveOriginPaths.try_emplace(op->affectedNode()->moveOriginInfos().path(), op);

        // Check if any of the deleted path contains the origin path.
        for (const auto &[path, deleteOp]: deletedDirectoryPaths) {
            if (op->targetSide() != deleteOp->targetSide()) {
                continue;
            }
            if (Utility::isDescendantOrEqual(op->affectedNode()->moveOriginInfos().path(), path)) {
                (void) _fixMoveBeforeDeleteCandidates.emplace_back(op, deleteOp);
            }
        }
    }
}

void OperationSorterFilter::filterFixEditBeforeMoveCandidates(const SyncOpPtr &op) {
    if (op->affectedNode()->hasChangeEvent(OperationType::Edit) && op->affectedNode()->hasChangeEvent(OperationType::Move)) {
        (void) _fixEditBeforeMoveCandidates[op->affectedNode()->id().value()].emplace_back(op);
    }
}

} // namespace KDC
