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

import CppInterop
import Foundation

struct SyncFileItemInfoSignal: Codable, Sendable {
    let syncDbId: Int32
    let itemInfo: SyncFileItemInfo
}

struct SyncFileItemInfo: Codable, Sendable {
    let operationId: Int32
    let type: KDC.NodeType
    /// Sync folder relative filesystem path
    @Base64CodedString var path: String
    @Base64CodedString var newPath: String
    @Base64CodedString var localNodeId: String
    @Base64CodedString var remoteNodeId: String
    let direction: KDC.SyncDirection
    let instruction: KDC.SyncFileInstruction
    let status: KDC.SyncFileStatus
    let conflict: KDC.ConflictType
    let inconsistency: KDC.InconsistencyType
    let cancelType: KDC.CancelType
    let date = Date()
    let size: Int64
    let progress: Int32
    @Base64CodedString var error: String

    enum CodingKeys: String, CodingKey {
        case operationId
        case type
        case path
        case newPath
        case localNodeId
        case remoteNodeId
        case direction
        case instruction
        case status
        case conflict
        case inconsistency
        case cancelType
        case size
        case progress
        case error
        // Note: date is intentionally excluded to preserve the runtime default value
    }
}

extension SyncFileItemInfo {
    var toSynchroFile: SynchroNode {
        SynchroNode(operationId: operationId,
                    type: type,
                    path: path,
                    newPath: newPath,
                    localNodeId: localNodeId,
                    remoteNodeId: remoteNodeId,
                    direction: direction,
                    instruction: instruction,
                    status: status,
                    conflict: conflict,
                    inconsistency: inconsistency,
                    cancelType: cancelType,
                    date: date,
                    size: size,
                    progress: progress,
                    error: error)
    }
}
