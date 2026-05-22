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
    func categorize(errors: [SyncError]) -> [UISyncErrorCategory: [SyncError]] {
        return [:]
    }

    func categorize(error: SyncError) -> UISyncErrorCategory {
        if isInSynchronizationDirectories(error: error) {
            return .synchronizationDirectories
        }

        return .conflicts
    }

    private func isInSynchronizationDirectories(error: SyncError) -> Bool {
        switch error {
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
}
