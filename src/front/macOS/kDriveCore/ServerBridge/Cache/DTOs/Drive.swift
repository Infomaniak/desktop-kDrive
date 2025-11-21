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

public typealias IndexedDrives = [Int32: Drive]

public struct Drive: Identifiable, Hashable, Sendable {
    public var id: Int32 {
        driveDbId
    }

    public let driveDbId: Int32
    public let driveId: Int32
    public let accountId: Int32
    public let userDbId: Int32
    public let userId: Int32
    public let name: String
    public let color: HexColor?
    public var synchros: [Int32: Synchro]
}


public typealias IndexedAvailableDrives = [Int32: AvailableDrive]

public struct AvailableDrive: Identifiable, Hashable, Sendable {
    public var id: Int32 {
        driveId
    }

    public let driveId: Int32
    public let accountId: Int32
    public let userDbId: Int32
    public let userId: Int32
    public let name: String
    public let color: HexColor?
    public var synchros: [Int32: Synchro]
}
