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

public struct UISynchro: Sendable, Equatable, Hashable, Identifiable {
    public typealias ID = Int

    public var id: ID {
        dbId
    }

    public let dbId: Int
    public let driveDbId: Int

    public let localPath: URL
    public let targetNodeId: String?

    public let supportsVirtualFileSystem: Bool
    public let useVirtualFileSystem: Bool

    public init(
        dbId: Int,
        driveDbId: Int,
        localPath: URL,
        targetNodeId: String?,
        supportsVirtualFileSystem: Bool,
        useVirtualFileSystem: Bool
    ) {
        self.dbId = dbId
        self.driveDbId = driveDbId

        self.localPath = localPath
        self.targetNodeId = targetNodeId

        self.supportsVirtualFileSystem = supportsVirtualFileSystem
        self.useVirtualFileSystem = useVirtualFileSystem
    }
}

public enum UISynchroStatus: Sendable, Equatable, Hashable {
    case starting
    case running
    case idle
    case pauseAsked
    case paused
    case stopAsked
    case stopped
    case error
}
