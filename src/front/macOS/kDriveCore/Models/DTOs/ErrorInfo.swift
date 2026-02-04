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
import OrderedCollections

public typealias IndexedErrors = OrderedDictionary<Int32, ErrorInfo>

public struct ErrorInfo: Identifiable, Hashable, Sendable {
    public var id: Int32 {
        dbId
    }

    public let dbId: Int32
    public let synchroDbId: Int32
    public let time: TimeInterval
    public let level: KDC.ErrorLevel
    public let functionName: String
    public let workerName: String
    public let exitCode: KDC.ExitCode
    public let exitCause: KDC.ExitCause
    public let localNodeId: String
    public let remoteNodeId: String
    public let nodeType: KDC.NodeType
    public let path: String
    public let conflictType: KDC.ConflictType
    public let cancelType: KDC.CancelType
    public let inconsistencyType: KDC.InconsistencyType
    public let destinationPath: String
    public let autoResolved: Bool
}

extension ErrorInfo {
    init(errorInfoMetadata: ErrorInfoMetadata) {
        dbId = errorInfoMetadata.dbId
        synchroDbId = errorInfoMetadata.syncDbId
        time = errorInfoMetadata.time
        level = errorInfoMetadata.level
        functionName = errorInfoMetadata.functionName
        workerName = errorInfoMetadata.workerName
        exitCode = errorInfoMetadata.exitCode
        exitCause = errorInfoMetadata.exitCause
        localNodeId = errorInfoMetadata.localNodeId
        remoteNodeId = errorInfoMetadata.remoteNodeId
        nodeType = errorInfoMetadata.nodeType
        path = errorInfoMetadata.path
        conflictType = errorInfoMetadata.conflictType
        cancelType = errorInfoMetadata.cancelType
        inconsistencyType = errorInfoMetadata.inconsistencyType
        destinationPath = errorInfoMetadata.destinationPath
        autoResolved = errorInfoMetadata.autoResolved
    }
}
