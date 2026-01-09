/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2025 Infomaniak Network SA

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

struct SyncProgressInfoSignal: Codable, Sendable {
    let syncDbId: Int32
    let syncStatus: KDC.SyncStatus
    let syncStep: KDC.SyncStep
    let syncProgress: SyncProgressSignal
}

// TODO: Validate parsing
struct SyncProgressSignal: Codable, Sendable {
    let currentFile: Int64
    let totalFiles: Int64
    let completedSize: Int64
    let totalSize: Int64
    let estimatedRemainingTime: Int64
}

extension SyncProgressInfoSignal {
    var asSynchroProgressInfo: SynchroProgressInfo {
        let progress = SyncProgress(syncProgress: self.syncProgress)
        return SynchroProgressInfo(syncStatus: syncStatus,
                            syncStep: syncStep,
                            syncProgress: progress)
    }
}
