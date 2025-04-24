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
    NameToOpMap deleteBeforeMoveCandidates;
    NameToOpMap moveBeforeCreateCandidates;
    std::unordered_map<SyncPath, SyncOpPtr> deletedDirectoryPaths;
    std::unordered_map<SyncPath, SyncOpPtr> moveOriginPaths;
    std::unordered_map<SyncPath, SyncOpPtr> createdDirectoryPaths;
    std::unordered_map<SyncPath, SyncOpPtr> moveDestinationPaths;
    NameToOpMap deleteBeforeCreateCandidates;
    NameToOpMap moveOriginNames;
    NameToOpMap moveDestinationNames;
    std::list<std::pair<SyncOpPtr, SyncPath>> moveBeforeMoveHierarchyFlipCandidates;

    for (const auto &[_, op]: _ops) {
        filterDeleteBeforeMoveCandidates(op, deleteBeforeMoveCandidates);
        filterMoveBeforeCreateCandidates(op, moveBeforeCreateCandidates);
        filterMoveBeforeDeleteCandidates(op, deletedDirectoryPaths, moveOriginPaths);
        filterCreateBeforeMoveCandidates(op, createdDirectoryPaths, moveDestinationPaths);
        filterDeleteBeforeCreateCandidates(op, deleteBeforeCreateCandidates);
        filterMoveBeforeMoveOccupiedCandidates(op, moveOriginNames, moveDestinationNames);
        filterEditBeforeMoveCandidates(op);
        filterMoveBeforeMoveHierarchyFlipCandidates(op, moveBeforeMoveHierarchyFlipCandidates);
    }
}

void OperationSorterFilter::filterDeleteBeforeMoveCandidates(const SyncOpPtr &op, NameToOpMap &deleteBeforeMoveCandidates) {
    if (op->affectedNode()->hasChangeEvent(OperationType::Delete)) {
        if (const auto [_, ok] = deleteBeforeMoveCandidates.try_emplace(op->affectedNode()->name(), op); !ok) {
            const auto &otherOp = deleteBeforeMoveCandidates.at(op->affectedNode()->name());
            if (op->targetSide() != otherOp->targetSide()) {
                return;
            }
            (void) _fixDeleteBeforeMoveCandidates.emplace_back(op, otherOp);
        }
    }
    if (op->affectedNode()->hasChangeEvent(OperationType::Move)) {
        if (const auto [_, ok] = deleteBeforeMoveCandidates.try_emplace(op->affectedNode()->name(), op); !ok) {
            const auto &otherOp = deleteBeforeMoveCandidates.at(op->affectedNode()->name());
            if (op->targetSide() != otherOp->targetSide()) {
                return;
            }
            (void) _fixDeleteBeforeMoveCandidates.emplace_back(op, otherOp);
        }
    }
}

void OperationSorterFilter::filterMoveBeforeCreateCandidates(const SyncOpPtr &op, NameToOpMap &moveBeforeCreateCandidates) {
    if (op->affectedNode()->hasChangeEvent(OperationType::Create)) {
        if (const auto &[_, ok] = moveBeforeCreateCandidates.try_emplace(op->affectedNode()->name(), op); !ok) {
            const auto &otherOp = moveBeforeCreateCandidates.at(op->affectedNode()->name());
            if (op->targetSide() != otherOp->targetSide()) {
                return;
            }
            (void) _fixMoveBeforeCreateCandidates.emplace_back(op, otherOp);
        }
    }
    if (op->affectedNode()->hasChangeEvent(OperationType::Move)) {
        if (const auto &[_, ok] =
                    moveBeforeCreateCandidates.try_emplace(op->affectedNode()->moveOriginInfos().path().filename().native(), op);
            !ok) {
            const auto &otherOp = moveBeforeCreateCandidates.at(op->affectedNode()->name());
            if (op->targetSide() != otherOp->targetSide()) {
                return;
            }
            (void) _fixMoveBeforeCreateCandidates.emplace_back(op, otherOp);
        }
    }
}

void OperationSorterFilter::filterMoveBeforeDeleteCandidates(const SyncOpPtr &op,
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

void OperationSorterFilter::filterCreateBeforeMoveCandidates(const SyncOpPtr &op,
                                                             std::unordered_map<SyncPath, SyncOpPtr> &createdDirectoryPaths,
                                                             std::unordered_map<SyncPath, SyncOpPtr> &moveDestinationPaths) {
    if (op->affectedNode()->hasChangeEvent(OperationType::Create)) {
        if (op->affectedNode()->type() != NodeType::Directory) {
            return;
        }
        const auto createdPath = op->affectedNode()->getPath();
        (void) createdDirectoryPaths.try_emplace(createdPath, op);

        // Check if any of the moved item destination path is inside the created folder.
        for (const auto &[path, moveOp]: moveDestinationPaths) {
            if (op->targetSide() != moveOp->targetSide()) {
                continue;
            }
            if (Utility::isDescendantOrEqual(path, createdPath)) {
                (void) _fixCreateBeforeMoveCandidates.emplace_back(op, moveOp);
            }
        }
        return;
    }

    if (op->affectedNode()->hasChangeEvent(OperationType::Move)) {
        const auto destinationPath = op->affectedNode()->getPath();
        (void) moveDestinationPaths.try_emplace(destinationPath, op);

        // Check if any of the created path contains the destination path.
        for (const auto &[path, createOp]: createdDirectoryPaths) {
            if (op->targetSide() != createOp->targetSide()) {
                continue;
            }
            if (Utility::isDescendantOrEqual(destinationPath, path)) {
                (void) _fixCreateBeforeMoveCandidates.emplace_back(op, createOp);
            }
        }
    }
}
void OperationSorterFilter::filterDeleteBeforeCreateCandidates(const SyncOpPtr &op, NameToOpMap &deleteBeforeCreateCandidates) {
    if (op->affectedNode()->hasChangeEvent(OperationType::Delete)) {
        if (const auto [_, ok] = deleteBeforeCreateCandidates.try_emplace(op->affectedNode()->name(), op); !ok) {
            const auto &otherOp = deleteBeforeCreateCandidates.at(op->affectedNode()->name());
            if (op->targetSide() != otherOp->targetSide()) {
                return;
            }
            (void) _fixDeleteBeforeCreateCandidates.emplace_back(op, otherOp);
        }
    }
    if (op->affectedNode()->hasChangeEvent(OperationType::Create)) {
        if (const auto [_, ok] = deleteBeforeCreateCandidates.try_emplace(op->affectedNode()->name(), op); !ok) {
            const auto &otherOp = deleteBeforeCreateCandidates.at(op->affectedNode()->name());
            if (op->targetSide() != otherOp->targetSide()) {
                return;
            }
            (void) _fixDeleteBeforeCreateCandidates.emplace_back(op, otherOp);
        }
    }
}

void OperationSorterFilter::filterMoveBeforeMoveOccupiedCandidates(const SyncOpPtr &op, NameToOpMap &moveOriginNames,
                                                                   NameToOpMap &moveDestinationNames) {
    if (!op->affectedNode()->hasChangeEvent(OperationType::Move)) return;

    const SyncName originName = op->affectedNode()->moveOriginInfos().path().filename().native();
    const SyncName destinationName = op->affectedNode()->name();

    if (moveOriginNames.contains(destinationName)) {
        const auto &otherOp = moveOriginNames.at(destinationName);
        if (op->targetSide() != otherOp->targetSide()) {
            return;
        }
        (void) _fixMoveBeforeMoveOccupiedCandidates.emplace_back(op, otherOp);
    }
    if (moveDestinationNames.contains(originName)) {
        const auto &otherOp = moveDestinationNames.at(originName);
        if (op->targetSide() != otherOp->targetSide()) {
            return;
        }
        (void) _fixMoveBeforeMoveOccupiedCandidates.emplace_back(op, otherOp);
    }

    (void) moveOriginNames.try_emplace(originName, op);
    (void) moveDestinationNames.try_emplace(destinationName, op);
}

void OperationSorterFilter::filterEditBeforeMoveCandidates(const SyncOpPtr &op) {
    if (op->affectedNode()->hasChangeEvent(OperationType::Edit) && op->affectedNode()->hasChangeEvent(OperationType::Move)) {
        (void) _fixEditBeforeMoveCandidates[op->affectedNode()->id().value()].emplace_back(op);
    }
}

void OperationSorterFilter::filterMoveBeforeMoveHierarchyFlipCandidates(
        const SyncOpPtr &op, std::list<std::pair<SyncOpPtr, SyncPath>> &moveBeforeMoveHierarchyFlipCandidates) {
    if (!op->affectedNode()->hasChangeEvent(OperationType::Move) || op->nodeType() != NodeType::Directory) return;

    const auto &originPath = op->affectedNode()->moveOriginInfos().path();
    const auto &destinationPath = op->affectedNode()->getPath();

    // Check if any of the created path contains the destination path.
    for (const auto &[otherOp, otherDestinationPath]: moveBeforeMoveHierarchyFlipCandidates) {
        if (op->targetSide() != otherOp->targetSide()) {
            continue;
        }

        const auto &otherOriginPath = otherOp->affectedNode()->moveOriginInfos().path();

        if (Utility::isStrictDescendant(destinationPath, otherDestinationPath) &&
            Utility::isStrictDescendant(otherOriginPath, originPath)) {
            (void) _fixMoveBeforeMoveHierarchyFlipCandidates.emplace_back(op, otherOp);
        }
    }

    (void) moveBeforeMoveHierarchyFlipCandidates.emplace_back(op, destinationPath);
}

} // namespace KDC
