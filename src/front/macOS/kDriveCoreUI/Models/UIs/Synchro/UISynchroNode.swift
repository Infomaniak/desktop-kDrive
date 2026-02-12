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

public enum UISynchroDirection: Sendable {
    case up
    case down
}

public enum UISynchroFileStatus: Sendable {
    case idle
    case syncing
    case done
    case error
}

public enum UISynchroFileInstruction: Sendable {
    case update
    case updateMetadata
    case remove
    case move
    case get
    case put
    case ignore
}

public struct UISynchroNode: Sendable, Identifiable, Equatable, Hashable {
    public typealias ID = String

    public let id: ID
    public let remoteID: String
    public let path: URL
    public let updatedPath: URL?
    public let type: UINodeType?
    public let direction: UISynchroDirection?
    public let status: UISynchroFileStatus?
    public let instruction: UISynchroFileInstruction?
    public let size: Int64
    public let syncDate: Date

    public var relevantPath: URL {
        return updatedPath ?? path
    }

    public var parentFolder: URL {
        return relevantPath.deletingLastPathComponent()
    }

    public var fileType: UTType? {
        return UTType(
            filenameExtension: relevantPath.pathExtension)
    }

    public var fileTypeRepresentation: FileTypeRepresentation {
        guard let fileType else {
            return .unknown
        }
        return FileTypeRepresentation(utType: fileType)
    }

    public init(
        id: ID,
        remoteID: String,
        type: UINodeType?,
        path: URL,
        updatedPath: URL?,
        direction: UISynchroDirection?,
        status: UISynchroFileStatus?,
        instruction: UISynchroFileInstruction?,
        size: Int64,
        synDate: Date
    ) {
        self.id = id
        self.remoteID = remoteID
        self.type = type
        self.path = path
        self.updatedPath = updatedPath
        self.direction = direction
        self.status = status
        self.instruction = instruction
        self.size = size
        self.syncDate = synDate
    }

    public static func == (lhs: UISynchroNode, rhs: UISynchroNode) -> Bool {
        return lhs.id == rhs.id
    }

    public func hash(into hasher: inout Hasher) {
        hasher.combine(id)
    }
}
