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

public struct SynchroError: Error, Sendable {
    public let kind: SynchroErrorKind
    public let metadata: SynchroErrorMetadata

    public init(kind: SynchroErrorKind, metadata: SynchroErrorMetadata) {
        self.kind = kind
        self.metadata = metadata
    }
}

public extension SynchroError {
    init(errorInfo: ErrorInfo) {
        kind = SynchroErrorKind(errorInfo: errorInfo)
        metadata = SynchroErrorMetadata(errorInfo: errorInfo)
    }
}

public struct SynchroErrorMetadata: Sendable {
    public let dbId: Int
    public let synchroDbId: Int
    public let path: String

    public init(dbId: Int, synchroDbId: Int, path: String) {
        self.dbId = dbId
        self.synchroDbId = synchroDbId
        self.path = path
    }
}

public extension SynchroErrorMetadata {
    init(errorInfo: ErrorInfo) {
        dbId = Int(errorInfo.dbId)
        synchroDbId = Int(errorInfo.synchroDbId)
        path = errorInfo.path
    }
}
