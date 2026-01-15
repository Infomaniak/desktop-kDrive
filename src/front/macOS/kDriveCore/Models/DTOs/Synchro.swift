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
import OrderedCollections

public typealias IndexedSynchros = OrderedDictionary<Int32, Synchro>

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
    public var errors: IndexedErrors = [:]
    public var latestError: SynchroError?

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
    public let syncStatus: KDC.SyncStatus
    public let syncStep: KDC.SyncStep
    public let syncProgress: SyncProgress
}

public struct SyncProgress: Hashable, Sendable {
    public let currentFile: Int64
    public let totalFiles: Int64
    public let completedSize: Int64
    public let totalSize: Int64
    public let estimatedRemainingTime: Int64

    init(syncProgress: SyncProgressSignal) {
        currentFile = syncProgress.currentFile
        totalFiles = syncProgress.totalFiles
        completedSize = syncProgress.completedSize
        totalSize = syncProgress.totalSize
        estimatedRemainingTime = syncProgress.estimatedRemainingTime
    }
}

public struct SynchroNode: Identifiable, Codable, Hashable, Sendable {
    public var id: String {
        localNodeId
    }

    public let type: KDC.NodeType
    /// Sync folder relative filesystem path
    public let path: String
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
    public let size: Int64
}

public enum SynchroError: Error, Hashable, Sendable, CaseIterable {
    case asleep
    case wakingUp
    case notRenew // "drive locked"
    case maintenance
    case accessDenied
    case loggingError

    init?(errorInfo: ErrorInfo) {
        switch errorInfo.exitCause {
        case .DriveAsleep:
            self = .asleep
        case .DriveWakingUp:
            self = .wakingUp
        case .DriveNotRenew:
            self = .notRenew
        case .DriveMaintenance:
            self = .maintenance
        case .DriveAccessError:
            self = .accessDenied
        case .LoginError:
            self = .loggingError
        default:
            return nil
        }
    }
}
