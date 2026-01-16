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

public typealias IndexedDrives = OrderedDictionary<Int32, Drive>

public enum DriveType: Equatable, Hashable, Sendable {
    case available(userId: Int32)
    case full(driveDbId: Int32, accountDbId: Int32, synchros: IndexedSynchros)
}

public protocol DriveRepresentation: Identifiable, Hashable, Sendable {
    var driveId: Int32 { get }
    var accountId: Int32 { get }
    var userDbId: Int32 { get }
    var name: String { get }
    var color: HexColor? { get }
    var type: DriveType { get }
}

public struct Drive: DriveRepresentation {
    public var id: Int32 {
        driveId
    }

    public let driveDbId: Int32
    public let driveId: Int32
    public let accountDbId: Int32
    public let accountId: Int32
    public let userDbId: Int32
    public let name: String
    public let color: HexColor?
    public let accessDenied: Bool
    public let admin: Bool
    public let locked: Bool
    public let maintenance: Bool
    public let notifications: Bool
    public var synchros: IndexedSynchros

    public var type: DriveType {
        .full(driveDbId: driveDbId, accountDbId: accountDbId, synchros: synchros)
    }
}

public typealias IndexedAvailableDrives = [Int32: AvailableDrive]

public struct AvailableDrive: DriveRepresentation {
    public var id: Int32 {
        driveId
    }

    public let driveId: Int32
    public let accountId: Int32
    public let userDbId: Int32
    public let userId: Int32
    public let name: String
    public let color: HexColor?

    public var type: DriveType {
        .available(userId: userId)
    }
}
