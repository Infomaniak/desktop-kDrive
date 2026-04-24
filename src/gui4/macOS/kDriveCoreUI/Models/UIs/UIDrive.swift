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

import Cocoa
import Foundation
import kDriveCore
import SwiftUI

public struct UIHexColor: Sendable, Equatable, Hashable {
    public let red: Int
    public let green: Int
    public let blue: Int
}

extension UIHexColor {
    init(hexColor: HexColor) {
        red = Int(hexColor.red)
        green = Int(hexColor.green)
        blue = Int(hexColor.blue)
    }
}

public protocol UIDriveRepresentation: Sendable, Equatable, Hashable, Identifiable {
    var id: Int { get }
    var name: String { get }
    var hexColor: UIHexColor? { get }

    var nsColor: NSColor? { get }
    var color: Color? { get }
}

// MARK: - UIAvailableDrive

public struct UIAvailableDrive: UIDriveRepresentation, Hashable {
    public typealias ID = Int

    public var id: ID {
        return driveId
    }

    public let driveId: Int
    public let userDbId: Int
    public let name: String
    public let hexColor: UIHexColor?

    public var nsColor: NSColor? {
        guard let hexColor else {
            return nil
        }
        return NSColor(hexColor: hexColor)
    }

    public var color: Color? {
        guard let nsColor else {
            return nil
        }
        return Color(nsColor: nsColor)
    }

    public init(driveId: Int, userDbId: Int, name: String, hexColor: UIHexColor?) {
        self.driveId = driveId
        self.userDbId = userDbId
        self.name = name
        self.hexColor = hexColor
    }
}

public extension UIAvailableDrive {
    init(availableDrive: AvailableDrive) {
        var hexColor: UIHexColor?
        if let driveColor = availableDrive.color {
            hexColor = UIHexColor(hexColor: driveColor)
        }

        self.init(
            driveId: Int(availableDrive.driveId),
            userDbId: Int(availableDrive.userDbId),
            name: availableDrive.name,
            hexColor: hexColor
        )
    }
}

// MARK: - UIDrive

public struct UIDrive: UIDriveRepresentation {
    public typealias ID = Int

    public var id: ID {
        return driveId
    }

    public let dbId: Int
    public let driveId: Int
    public let name: String
    public let hexColor: UIHexColor?

    public var nsColor: NSColor? {
        guard let hexColor else {
            return nil
        }
        return NSColor(hexColor: hexColor)
    }

    public var color: Color? {
        guard let nsColor else {
            return nil
        }
        return Color(nsColor: nsColor)
    }

    public init(dbId: Int, driveId: Int, name: String, hexColor: UIHexColor?) {
        self.dbId = dbId
        self.driveId = driveId
        self.name = name
        self.hexColor = hexColor
    }
}

public extension UIDrive {
    init(drive: Drive) {
        var hexColor: UIHexColor?
        if let driveColor = drive.color {
            hexColor = UIHexColor(hexColor: driveColor)
        }

        self.init(
            dbId: Int(drive.driveDbId),
            driveId: Int(drive.driveId),
            name: drive.name,
            hexColor: hexColor
        )
    }
}

extension NSColor {
    convenience init(hexColor: UIHexColor) {
        self.init(
            red: CGFloat(hexColor.red) / 255,
            green: CGFloat(hexColor.green) / 255,
            blue: CGFloat(hexColor.blue) / 255,
            alpha: 1
        )
    }
}
