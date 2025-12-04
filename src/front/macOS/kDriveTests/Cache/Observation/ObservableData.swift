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

@testable import kDriveCore

public enum ObservableData {
    static let expectedUserId = Int32.random(in: 0 ... 1000)
    static let expectedUserDbId = Int32.random(in: 0 ... 1000)
    static let expectedAccountDbId = Int32.random(in: 0 ... 1000)
    static let expectedAccountId = Int32.random(in: 0 ... 1000)
    static let expectedDriveDbId = Int32.random(in: 0 ... 1000)
    static let expectedDriveId = Int32.random(in: 0 ... 1000)

    static let expectedDrive = Drive(driveDbId: expectedDriveDbId,
                                     driveId: expectedDriveId,
                                     accountId: expectedAccountId,
                                     userDbId: expectedUserDbId,
                                     userId: expectedUserId,
                                     name: "The amazing test Drive",
                                     color: HexColor(hex: "#ffffff"), synchros: [:])

    static let updatedDrive = Drive(driveDbId: expectedDriveDbId,
                                    driveId: expectedDriveId,
                                    accountId: expectedAccountId,
                                    userDbId: expectedUserDbId,
                                    userId: expectedUserId,
                                    name: "The amazing UPDATED test Drive",
                                    color: HexColor(hex: "#0a0a0a"), synchros: [:])

    static let expectedAccount = Account(dbId: expectedAccountDbId, name: "Some account", drives: [expectedDriveDbId: expectedDrive])

    static let updatedAccount = Account(dbId: expectedAccountDbId, name: "Some UPDATED account", drives: [expectedDriveDbId: updatedDrive])

    static let indexedAccounts: IndexedAccounts = [
        1: Account(dbId: 1, name: "1", drives: [:]),
        2: Account(dbId: 2, name: "2", drives: [:]),
        expectedAccountDbId: expectedAccount,
        4: Account(dbId: 4, name: "4", drives: [:])
    ]
    static let expectedUser = User(
        dbId: expectedUserDbId,
        userId: expectedUserId,
        name: "appleseed",
        email: "ja@apple.com",
        accounts: indexedAccounts,
        availableDrives: [:],
        avatar: Data(),
        isConnected: true,
        isStaff: true
    )
}
