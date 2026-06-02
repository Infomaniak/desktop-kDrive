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

public enum NodeType: Sendable {
    case file
    case directory
}

extension NodeType {
    init?(nodeType: KDC.NodeType) {
        switch nodeType {
        case .File: self = .file
        case .Directory: self = .directory
        default: return nil
        }
    }
}

public struct ErrorNodeID: Sendable {
    public let local: String?
    public let remote: String?

    public init(local: String?, remote: String?) {
        self.local = local
        self.remote = remote
    }

    init(errorInfo: ErrorInfo) {
        self.local = !errorInfo.remoteNodeId.isEmpty ? errorInfo.localNodeId : nil
        self.remote = !errorInfo.remoteNodeId.isEmpty ? errorInfo.remoteNodeId : nil
    }
}

public struct SynchroErrorMetadata: Sendable {
    public let dbId: Int
    public let synchroDbId: Int
    public let date: Date

    public let path: String
    public let destinationPath: String
    public let nodeType: NodeType?

    public let nodeId: ErrorNodeID

    public let isAutoResolved: Bool
    public let level: KDC.ErrorLevel
    public let exitCode: KDC.ExitCode
    public let exitCause: KDC.ExitCause

    public init(
        dbId: Int,
        synchroDbId: Int,
        date: Date,
        path: String,
        destinationPath: String,
        nodeType: NodeType?,
        nodeId: ErrorNodeID,
        isAutoResolved: Bool,
        level: KDC.ErrorLevel,
        exitCode: KDC.ExitCode,
        exitCause: KDC.ExitCause
    ) {
        self.dbId = dbId
        self.synchroDbId = synchroDbId
        self.date = date
        self.path = path
        self.destinationPath = destinationPath
        self.nodeType = nodeType
        self.nodeId = nodeId
        self.isAutoResolved = isAutoResolved
        self.level = level
        self.exitCode = exitCode
        self.exitCause = exitCause
    }
}

public extension SynchroErrorMetadata {
    init(errorInfo: ErrorInfo) {
        dbId = Int(errorInfo.dbId)
        synchroDbId = Int(errorInfo.synchroDbId)
        date = Date(timeIntervalSince1970: errorInfo.time)
        path = errorInfo.path
        destinationPath = errorInfo.destinationPath
        nodeType = NodeType(nodeType: errorInfo.nodeType)
        nodeId = ErrorNodeID(errorInfo: errorInfo)
        isAutoResolved = errorInfo.autoResolved
        level = errorInfo.level
        exitCode = errorInfo.exitCode
        exitCause = errorInfo.exitCause
    }
}
