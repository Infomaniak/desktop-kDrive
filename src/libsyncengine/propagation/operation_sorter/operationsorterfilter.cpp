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

        // Move before Delete (just fill the list with deleted path and move origin paths on first loop)
        if (op->affectedNode()->hasChangeEvent(OperationType::Delete)) {
            if (op->affectedNode()->type() != NodeType::Directory) {
                continue;
            }

            (void) deletedDirectoryPaths.try_emplace(op->affectedNode()->getPath(), op);
        }
        if (op->affectedNode()->hasChangeEvent(OperationType::Move)) {
            (void) moveOriginPaths.try_emplace(op->affectedNode()->moveOriginInfos().path(), op);
        }

        filterFixEditBeforeMoveCandidates(op);
    }

    // Go through all operations once again to detect the move operations inside deleted path...
    for (const auto &[_, op]: _ops) {
        if (op->type() != OperationType::Move) {
            continue;
        }
    }
}

void OperationSorterFilter::filterFixDeleteBeforeMoveCandidates(
        const SyncOpPtr &op, std::unordered_map<SyncName, SyncOpPtr, StringHashFunction, std::equal_to<>> &deleteBeforeMove) {
    if (op->affectedNode()->hasChangeEvent(OperationType::Delete)) {
        if (const auto [_, ok] = deleteBeforeMove.try_emplace(op->affectedNode()->name(), op); !ok) {
            const auto &otherOp = deleteBeforeMove.at(op->affectedNode()->name());
            (void) _fixDeleteBeforeMoveCandidates.emplace_back(op, otherOp);
        }
    }
    if (op->affectedNode()->hasChangeEvent(OperationType::Move)) {
        if (const auto [_, ok] = deleteBeforeMove.try_emplace(op->affectedNode()->name(), op); !ok) {
            const auto &otherOp = deleteBeforeMove.at(op->affectedNode()->name());
            (void) _fixDeleteBeforeMoveCandidates.emplace_back(op, otherOp);
        }
    }
}

void OperationSorterFilter::filterFixMoveBeforeCreateCandidates(
        const SyncOpPtr &op, std::unordered_map<SyncName, SyncOpPtr, StringHashFunction, std::equal_to<>> &moveBeforeCreate) {
    if (op->affectedNode()->hasChangeEvent(OperationType::Create)) {
        if (const auto [_, ok] = moveBeforeCreate.try_emplace(op->affectedNode()->name(), op); !ok) {
            const auto &otherOp = moveBeforeCreate.at(op->affectedNode()->name());
            (void) _fixMoveBeforeCreateCandidates.emplace_back(op, otherOp);
        }
    }
    if (op->affectedNode()->hasChangeEvent(OperationType::Move)) {
        if (const auto [_, ok] =
                    moveBeforeCreate.try_emplace(Str2SyncName(op->affectedNode()->moveOriginInfos().path().filename()), op);
            !ok) {
            const auto &otherOp = moveBeforeCreate.at(op->affectedNode()->name());
            (void) _fixMoveBeforeCreateCandidates.emplace_back(op, otherOp);
        }
    }
}
void OperationSorterFilter::filterFixEditBeforeMoveCandidates(const SyncOpPtr &op) {
    if (op->affectedNode()->hasChangeEvent(OperationType::Edit) && op->affectedNode()->hasChangeEvent(OperationType::Move)) {
        (void) _fixEditBeforeMoveCandidates[op->affectedNode()->id().value()].emplace_back(op);
    }
}

} // namespace KDC
