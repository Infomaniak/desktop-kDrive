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

OperationSorterFilter::OperationSorterFilter(const std::unordered_map<UniqueId, SyncOpPtr> &ops) :
    _ops(ops) {}

void OperationSorterFilter::filterOperations() {
    NameToOpMap deleteBeforeMoveCandidates;
    NameToOpMap moveBeforeCreateCandidates;
    SyncPathToSyncOpMap deletedDirectoryPaths;
    SyncPathToSyncOpMap moveOriginPaths;
    SyncPathToSyncOpMap createdDirectoryPaths;
    SyncPathToSyncOpMap moveDestinationPaths;
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
// delete before move, e.g. user deletes an object at path "x" and moves another object "a" to "x".
void OperationSorterFilter::filterDeleteBeforeMoveCandidates(const SyncOpPtr &op, NameToOpMap &deleteBeforeMoveCandidates) {
    if (op->type() == OperationType::Delete || op->type() == OperationType::Move) {
        if (const auto [_, ok] = deleteBeforeMoveCandidates.try_emplace(op->affectedNode()->normalizedName(), op); !ok) {
            const auto &otherOp = deleteBeforeMoveCandidates.at(op->affectedNode()->normalizedName());
            if (op->targetSide() != otherOp->targetSide() || op->type() == otherOp->type()) {
                return;
            }
            (void) _fixDeleteBeforeMoveCandidates.emplace_back(op, otherOp);
        }
    }
}

void OperationSorterFilter::filterMoveBeforeCreateCandidates(const SyncOpPtr &op, NameToOpMap &moveBeforeCreateCandidates) {
    if (op->type() == OperationType::Create) {
        const SyncName name = op->affectedNode()->normalizedName();
        if (const auto &[_, ok] = moveBeforeCreateCandidates.try_emplace(name, op); !ok) {
            const auto &otherOp = moveBeforeCreateCandidates.at(name);
            if (op->targetSide() != otherOp->targetSide() || otherOp->type() != OperationType::Move) {
                return;
            }
            (void) _fixMoveBeforeCreateCandidates.emplace_back(op, otherOp);
        }
    }
    if (op->type() == OperationType::Move) {
        const SyncName name = op->affectedNode()->moveOriginInfos().normalizedPath().filename().native();
        if (const auto &[_, ok] = moveBeforeCreateCandidates.try_emplace(name, op); !ok) {
            // move before create, e.g. user moves an object "a" to "b" and creates another object at "a".
            const auto &otherOp = moveBeforeCreateCandidates.at(name);
            if (op->targetSide() != otherOp->targetSide() || otherOp->type() != OperationType::Create) {
                return;
            }
            (void) _fixMoveBeforeCreateCandidates.emplace_back(op, otherOp);
        }
    }
}

void OperationSorterFilter::filterMoveBeforeDeleteCandidates(const SyncOpPtr &op, SyncPathToSyncOpMap &deletedDirectoryPaths,
                                                             SyncPathToSyncOpMap &moveOriginPaths) {
    if (op->type() == OperationType::Delete) {
        if (op->affectedNode()->type() != NodeType::Directory) {
            return;
        }

        SyncPath normalizedDeletedPath;
        if (const auto deletedPath = op->affectedNode()->getPath(); !Utility::normalizedSyncPath(deletedPath, normalizedDeletedPath)) {
            LOGW_WARN(Log::instance()->getLogger(), L"Failed to normalize: " << Utility::formatSyncPath(deletedPath));
            normalizedDeletedPath = deletedPath;
        }
        (void) deletedDirectoryPaths.try_emplace(normalizedDeletedPath, op);

        // Check if any of the moved item origin path is inside the deleted folder.
        for (const auto &[path, moveOp]: moveOriginPaths) {
            if (op->targetSide() != moveOp->targetSide()) {
                continue;
            }
            if (Utility::isDescendantOrEqual(path, normalizedDeletedPath)) {
                (void) _fixMoveBeforeDeleteCandidates.emplace_back(op, moveOp);
            }
        }
        return;
    }

    if (op->type() == OperationType::Move) {
        (void) moveOriginPaths.try_emplace(op->affectedNode()->moveOriginInfos().normalizedPath(), op);

        // Check if any of the deleted path contains the origin path.
        for (const auto &[path, deleteOp]: deletedDirectoryPaths) {
            if (op->targetSide() != deleteOp->targetSide()) {
                continue;
            }
            if (Utility::isDescendantOrEqual(op->affectedNode()->moveOriginInfos().normalizedPath(), path)) {
                (void) _fixMoveBeforeDeleteCandidates.emplace_back(op, deleteOp);
            }
        }
    }
}

void OperationSorterFilter::filterCreateBeforeMoveCandidates(const SyncOpPtr &op, SyncPathToSyncOpMap &createdDirectoryPaths,
                                                             SyncPathToSyncOpMap &moveDestinationPaths) {
    if (op->type() == OperationType::Create) {
        if (op->affectedNode()->type() != NodeType::Directory) {
            return;
        }

        SyncPath normalizedCreatedPath;
        if (const auto createdPath = op->affectedNode()->getPath(); !Utility::normalizedSyncPath(createdPath, normalizedCreatedPath)) {
            LOGW_WARN(Log::instance()->getLogger(), L"Failed to normalize: " << Utility::formatSyncPath(createdPath));
            normalizedCreatedPath = createdPath;
        }
        (void) createdDirectoryPaths.try_emplace(normalizedCreatedPath, op);

        // Check if any of the moved item destination path is inside the created folder.
        for (const auto &[path, moveOp]: moveDestinationPaths) {
            if (op->targetSide() != moveOp->targetSide()) {
                continue;
            }
            if (Utility::isDescendantOrEqual(path, normalizedCreatedPath)) {
                (void) _fixCreateBeforeMoveCandidates.emplace_back(op, moveOp);
            }
        }
        return;
    }

    if (op->type() == OperationType::Move) {
        SyncPath normalizedDestinationPath;
        if (const auto destinationPath = op->affectedNode()->getPath(); !Utility::normalizedSyncPath(destinationPath, normalizedDestinationPath)) {
            LOGW_WARN(Log::instance()->getLogger(), L"Failed to normalize: " << Utility::formatSyncPath(destinationPath));
            normalizedDestinationPath = destinationPath;
        }
        (void) moveDestinationPaths.try_emplace(normalizedDestinationPath, op);

        // Check if any of the created path contains the destination path.
        for (const auto &[path, createOp]: createdDirectoryPaths) {
            if (op->targetSide() != createOp->targetSide()) {
                continue;
            }
            if (Utility::isDescendantOrEqual(normalizedDestinationPath, path)) {
                (void) _fixCreateBeforeMoveCandidates.emplace_back(op, createOp);
            }
        }
    }
}
void OperationSorterFilter::filterDeleteBeforeCreateCandidates(const SyncOpPtr &op, NameToOpMap &deleteBeforeCreateCandidates) {
    if (op->type() == OperationType::Delete) {
        if (const auto [_, ok] = deleteBeforeCreateCandidates.try_emplace(op->affectedNode()->normalizedName(), op); !ok) {
            const auto &otherOp = deleteBeforeCreateCandidates.at(op->affectedNode()->normalizedName());
            if (op->targetSide() != otherOp->targetSide() || otherOp->type() != OperationType::Create) {
                return;
            }
            (void) _fixDeleteBeforeCreateCandidates.emplace_back(op, otherOp);
        }
    }
    if (op->type() == OperationType::Create) {
        if (const auto [_, ok] = deleteBeforeCreateCandidates.try_emplace(op->affectedNode()->normalizedName(), op); !ok) {
            const auto &otherOp = deleteBeforeCreateCandidates.at(op->affectedNode()->normalizedName());
            if (op->targetSide() != otherOp->targetSide() || otherOp->type() != OperationType::Delete) {
                return;
            }
            (void) _fixDeleteBeforeCreateCandidates.emplace_back(op, otherOp);
        }
    }
}

void OperationSorterFilter::filterMoveBeforeMoveOccupiedCandidates(const SyncOpPtr &op, NameToOpMap &moveOriginNames,
                                                                   NameToOpMap &moveDestinationNames) {
    if (op->type() != OperationType::Move) return;

    const SyncName originName = op->affectedNode()->moveOriginInfos().normalizedPath().filename().native();
    const SyncName destinationName = op->affectedNode()->normalizedName();

    if (moveOriginNames.contains(destinationName)) {
        const auto &otherOp = moveOriginNames.at(destinationName);
        if (op == otherOp || op->targetSide() != otherOp->targetSide()) {
            return;
        }
        // op must be executed after otherOp
        (void) _fixMoveBeforeMoveOccupiedCandidates.emplace_back(op, otherOp);
    }
    if (moveDestinationNames.contains(originName)) {
        const auto &otherOp = moveDestinationNames.at(originName);
        if (op == otherOp || op->targetSide() != otherOp->targetSide()) {
            return;
        }
        // otherOp must be executed after op
        (void) _fixMoveBeforeMoveOccupiedCandidates.emplace_back(otherOp, op);
    }

    (void) moveOriginNames.try_emplace(originName, op);
    (void) moveDestinationNames.try_emplace(destinationName, op);
}

void OperationSorterFilter::filterEditBeforeMoveCandidates(const SyncOpPtr &op) {
    // We keep only operations on nodes that have both EDIT and MOVE operations.
    if (op->affectedNode()->hasChangeEvent(OperationType::Edit) && op->affectedNode()->hasChangeEvent(OperationType::Move)) {
        (void) _fixEditBeforeMoveCandidates[op->affectedNode()->id().value()].emplace_back(op);
    }
}

void OperationSorterFilter::filterMoveBeforeMoveHierarchyFlipCandidates(
        const SyncOpPtr &op, std::list<std::pair<SyncOpPtr, SyncPath>> &moveBeforeMoveHierarchyFlipCandidates) {
    if (op->type() != OperationType::Move || op->nodeType() != NodeType::Directory) return;

    const auto &originPath = op->affectedNode()->moveOriginInfos().normalizedPath();
    SyncPath normalizedDestinationPath;
    if (const auto destinationPath = op->affectedNode()->getPath(); !Utility::normalizedSyncPath(destinationPath, normalizedDestinationPath)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Failed to normalize: " << Utility::formatSyncPath(destinationPath));
        normalizedDestinationPath = destinationPath;
    }

    // Check if any of the moved path contains the destination path.
    for (const auto &[otherOp, otherDestinationPath]: moveBeforeMoveHierarchyFlipCandidates) {
        if (op->targetSide() != otherOp->targetSide()) {
            continue;
        }

        const auto &otherOriginPath = otherOp->affectedNode()->moveOriginInfos().normalizedPath();

        if (Utility::isStrictDescendant(normalizedDestinationPath, otherDestinationPath) &&
            Utility::isStrictDescendant(otherOriginPath, originPath)) {
            // op must be executed after otherOp
            (void) _fixMoveBeforeMoveHierarchyFlipCandidates.emplace_back(op, otherOp);
            continue;
        }
        if (Utility::isStrictDescendant(otherDestinationPath, normalizedDestinationPath) &&
            Utility::isStrictDescendant(originPath, otherOriginPath)) {
            // otherOp must be executed after op
            (void) _fixMoveBeforeMoveHierarchyFlipCandidates.emplace_back(otherOp, op);
        }
    }

    (void) moveBeforeMoveHierarchyFlipCandidates.emplace_back(op, normalizedDestinationPath);
}

} // namespace KDC
