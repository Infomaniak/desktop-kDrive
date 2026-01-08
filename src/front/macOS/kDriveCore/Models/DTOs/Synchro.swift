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

import Collections
import Foundation

public typealias IndexedSynchros = [Int32: Synchro]

public struct Synchro: Identifiable, Hashable, Sendable {
    public var id: Int32 {
        dbId
    }

    public let dbId: Int32
    public let driveDbId: Int32
    public let localPath: String

    public let targetPath: String
    public let targetNodeId: String
    public let supportVfs: Bool
    public let virtualFileMode: KDC.VirtualFileMode
    public var progress: SynchroProgressInfo?
    public var synchNodes: OrderedDictionary<String, SynchroNode> = [:]

    private static let maxSynchNodesCount = 100

    public mutating func addOrUpdateSynchNode(_ node: SynchroNode) {
        synchNodes[node.localNodeId] = node

        guard synchNodes.count > Self.maxSynchNodesCount else {
            return
        }
        
        let itemsToRemove = max(synchNodes.count - Self.maxSynchNodesCount, 0)
        guard itemsToRemove > 0 else {
            return
        }
        
        synchNodes.removeFirst(itemsToRemove)
    }

    public func getSynchNode(by localNodeId: String) -> SynchroNode? {
        return synchNodes[localNodeId]
    }
}

public struct SynchroProgressInfo: Hashable, Sendable {
    public let syncStatus: Int32 // TODO: use SyncStatus enum
    public let syncStep: Int32 // TODO: use SyncStep enum
    public let syncProgress: Int32 // TODO: SyncProgress enum
}

public struct SynchroNode: Identifiable, Codable, Hashable, Sendable {
    public var id: String {
        localNodeId
    }

    public let type: KDC.NodeType
    public let path: String // Sync folder relative filesystem path
    public let newPath: String
    public let localNodeId: String
    public let remoteNodeId: String
    public let direction: KDC.SyncDirection
    public let instruction: KDC.SyncFileInstruction
    public let status: KDC.SyncFileStatus
    public let conflict: KDC.ConflictType
    public let inconsistency: KDC.InconsistencyType
    public let cancelType: KDC.CancelType
    public let error: String
}
