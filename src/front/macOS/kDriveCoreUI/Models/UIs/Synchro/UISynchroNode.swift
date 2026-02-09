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
import UniformTypeIdentifiers

public enum UINodeType: Sendable {
    case directory
    case file
}

public enum UISyncDirection: Sendable {
    case up
    case down
}

public enum UISyncFileStatus: Sendable {
    case idle
    case syncing
    case done
    case error
}

public struct UISynchroNode: Sendable, Identifiable, Equatable, Hashable {
    public typealias ID = String

    public let id: ID
    public let type: UINodeType?
    public let direction: UISyncDirection?
    public let status: UISyncFileStatus?

    public let path: URL
    public let updatedPath: URL?

    // FIXME: Fake properties while waiting on server update
    public let size: Int64 = 7_000_000
    public let syncDate: Date = .now.addingTimeInterval(-60 * 5)
    public let fileType: UTType = .item

    public var relevantPath: URL {
        return updatedPath ?? path
    }

    public var parentFolder: URL {
        return relevantPath.deletingLastPathComponent()
    }

    public var fileTypeRepresentation: FileTypeRepresentation {
        FileTypeRepresentation(utType: fileType)
    }

    public init(id: ID, type: UINodeType?, path: URL, updatedPath: URL?, direction: UISyncDirection?, status: UISyncFileStatus?) {
        self.id = id
        self.type = type
        self.path = path
        self.updatedPath = updatedPath
        self.direction = direction
        self.status = status
    }

    public static func == (lhs: UISynchroNode, rhs: UISynchroNode) -> Bool {
        return lhs.id == rhs.id
    }
}
