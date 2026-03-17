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

public typealias IndexedAccounts = [Int32: Account]
public typealias UserAccounts = (userDbId: Int32, indexedAccounts: [Int32: Account])

// TODO: Update to track userDbId in Account to match server type
public struct Account: Identifiable, Hashable, Sendable {
    public let dbId: Int32
    public let userDbId: Int32
    public var name: String
    public var drives: IndexedDrives

    public var id: Int32 {
        dbId
    }

    public init(dbId: Int32, userDbId: Int32, name: String, drives: [Int32: Drive]) {
        self.dbId = dbId
        self.userDbId = userDbId
        self.name = name
        self.drives = drives
    }
}
