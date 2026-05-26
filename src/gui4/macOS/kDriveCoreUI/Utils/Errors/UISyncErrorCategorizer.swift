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

struct UISyncErrorCategorizer {
    func categorize(errors: [UISyncError]) -> [UISyncErrorCategory: [UISyncError]] {
        var categorizedErrors: [UISyncErrorCategory: [UISyncError]] = [:]

        for error in errors {
            let category = categorize(error: error)
            categorizedErrors[category, default: []].append(error)
        }

        return categorizedErrors
    }

    func categorize(error: UISyncError) -> UISyncErrorCategory {
        if isInSynchronizationDirectoriesList(error: error) {
            return .synchronizationDirectories
        }

        if isInFilesToCheckList(error: error) {
            return .filesToCheck
        }

        if isInConflictsList(error: error) {
            return .conflicts
        }

        if isInStorageList(error: error) {
            return .storage
        }

        return .systemAndPermissions
    }

    private func isInFilesToCheckList(error: UISyncError) -> Bool {
        guard error.info.level == .Node else {
            return false
        }

        guard !isConflictResolvableByUser(error: error) else {
            return false
        }

        if isInStorageList(error: error) {
            return false
        }

        return true
    }

    private func isInConflictsList(error: UISyncError) -> Bool {
        guard error.info.level == .Node else {
            return false
        }

        return isConflictResolvableByUser(error: error)
    }

    private func isInSynchronizationDirectoriesList(error: UISyncError) -> Bool {
        switch error.kind {
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

    private func isInStorageList(error: UISyncError) -> Bool {
        switch error.kind {
        case .systemNotEnoughDiskSpace,
             .quotaExceeded:
            return true
        default:
            return false
        }
    }

    private func isConflictResolvableByUser(error: UISyncError) -> Bool {
        return error.info.level == .Node && (error.info.conflictType == .CreateCreate || error.info.conflictType == .EditEdit)
    }
}
