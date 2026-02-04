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

import Cocoa
import Foundation
import kDriveCore
import SwiftUI

public protocol UIDriveRepresentation: Sendable, Equatable, Hashable {
    var id: Int { get }
    var name: String { get }
    var nsColor: NSColor? { get }
    var color: Color? { get }
}

// MARK: - UIAvailableDrive

public struct UIAvailableDrive: UIDriveRepresentation, Hashable {
    public var id: Int {
        return driveId
    }

    public let driveId: Int
    public let userDbId: Int
    public let name: String
    public let nsColor: NSColor?

    public var color: Color? {
        guard let nsColor else { return nil }
        return Color(nsColor: nsColor)
    }

    public init(driveId: Int, userDbId: Int, name: String, color: NSColor?) {
        self.driveId = driveId
        self.userDbId = userDbId
        self.name = name
        nsColor = color
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
            userDbId: Int(availableDrive.userDbId),
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
    public let userDbId: Int
    public let name: String
    public let nsColor: NSColor?
    public let synchros: [Int: UISynchro]

    public var color: Color? {
        guard let nsColor else { return nil }
        return Color(nsColor: nsColor)
    }

    public init(dbId: Int, driveId: Int, userDbId: Int, name: String, color: NSColor?, synchros: [Int: UISynchro]) {
        self.dbId = dbId
        self.driveId = driveId
        self.userDbId = userDbId
        self.name = name
        nsColor = color
        self.synchros = synchros
    }
}

public extension UIDrive {
    init(drive: Drive) {
        var color: NSColor?
        if let driveColor = drive.color {
            color = NSColor(hexColor: driveColor)
        }

        let synchros = Dictionary(uniqueKeysWithValues: drive.synchros.map { key, synchro in
            (Int(key), UISynchro(synchro: synchro))
        })

        self.init(
            dbId: Int(drive.driveDbId),
            driveId: Int(drive.driveId),
            userDbId: Int(drive.userDbId),
            name: drive.name,
            color: color,
            synchros: synchros
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
