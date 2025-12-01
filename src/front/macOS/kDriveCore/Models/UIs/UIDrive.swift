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

import AppKit
import Foundation

public protocol UIDriveRepresentation: Sendable {
    var id: Int { get }
    var name: String { get }
    var color: NSColor? { get }
}

// MARK: - UIAvailableDrive

public struct UIAvailableDrive: UIDriveRepresentation, Hashable {
    public var id: Int {
        return driveId
    }

    public let driveId: Int
    public let name: String
    public let color: NSColor?

    public init(driveId: Int, name: String, color: NSColor?) {
        self.driveId = driveId
        self.name = name
        self.color = color
    }
}

public extension UIAvailableDrive {
    init(availableDrive: AvailableDrive) {
        var color: NSColor?
        if let driveColor = availableDrive.color {
            color = NSColor(hexColor: driveColor)
        }

        self.init(
            driveId: Int(availableDrive.driveId),
            name: availableDrive.name,
            color: color
        )
    }
}

// MARK: - UIDrive

public struct UIDrive: UIDriveRepresentation {
    public var id: Int {
        return dbId
    }

    public let dbId: Int
    public let driveId: Int
    public let name: String
    public let color: NSColor?

    public init(dbId: Int, driveId: Int, name: String, color: NSColor?) {
        self.dbId = dbId
        self.driveId = driveId
        self.name = name
        self.color = color
    }
}

public extension UIDrive {
    init(drive: Drive) {
        var color: NSColor?
        if let driveColor = drive.color {
            color = NSColor(hexColor: driveColor)
        }

        self.init(
            dbId: Int(drive.driveDbId),
            driveId: Int(drive.driveId),
            name: drive.name,
            color: color
        )
    }
}

extension NSColor {
    convenience init(hexColor: HexColor) {
        self.init(
            red: CGFloat(hexColor.red) / 255,
            green: CGFloat(hexColor.green) / 255,
            blue: CGFloat(hexColor.blue) / 255,
            alpha: 1
        )
    }
}
