/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2026 Infomaniak Network SA

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import Foundation
import kDriveCore

extension ErrorInfo {
    // periphery:ignore - The compiler struggles to see the level property
    var errorLevel: KDC.ErrorLevel {
        return level
    }
}

public struct UISynchroErrorCategorizer: Sendable {
    public init() {}

    public func categorize(error: ErrorInfo, kind: SynchroErrorKind) -> UISynchroErrorCategory {
        if isInSynchronizationDirectoriesList(kind: kind) {
            return .synchronizationDirectories
        }

        if isInFilesToCheckList(error: error, kind: kind) {
            return .filesToCheck
        }

        if isInConflictsList(error: error) {
            return .conflicts
        }

        if isInStorageList(kind: kind) {
            return .storage
        }

        return .systemAndPermissions
    }

    private func isInFilesToCheckList(error: ErrorInfo, kind: SynchroErrorKind) -> Bool {
        guard error.level == .Node else {
            return false
        }

        guard !isConflictResolvableByUser(error: error) else {
            return false
        }

        if isInStorageList(kind: kind) {
            return false
        }

        return true
    }

    private func isInConflictsList(error: ErrorInfo) -> Bool {
        guard error.level == .Node else {
            return false
        }

        return isConflictResolvableByUser(error: error)
    }

    private func isInSynchronizationDirectoriesList(kind: SynchroErrorKind) -> Bool {
        switch kind {
        case .systemSyncDirAccess,
             .systemSyncDirDiskMissing,
             .dataSyncDirChanged,
             .invalidSyncDirAccess,
             .invalidSyncDirNesting,
             .systemUnableToStartVFS:
            return true
        default:
            return false
        }
    }

    private func isInStorageList(kind: SynchroErrorKind) -> Bool {
        switch kind {
        case .systemNotEnoughDiskSpace,
             .quotaExceeded:
            return true
        default:
            return false
        }
    }

    private func isConflictResolvableByUser(error: ErrorInfo) -> Bool {
        return error.level == .Node && (error.conflictType == .CreateCreate || error.conflictType == .EditEdit)
    }
}
