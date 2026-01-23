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
@testable import kDriveCore
import OrderedCollections
import Testing

enum CacheData {
    static let expectedUserAPIId = Int32.random(in: 0 ... 10000)
    static let expectedUserDbId = Int32.random(in: 0 ... 10000)
    static var expectedUser = User(
        dbId: expectedUserDbId,
        userId: expectedUserAPIId,
        name: "appleseed",
        email: "ja@apple.com",
        accounts: [:],
        availableDrives: [:],
        avatar: Data(),
        isConnected: true,
        isStaff: true
    )

    static let updatedUserAPIId = Int32.random(in: 0 ... 10000)
    static let updatedUserName = "appleseed2"
    static var updatedUser = User(
        dbId: expectedUserDbId,
        userId: updatedUserAPIId,
        name: updatedUserName,
        email: "ja@apple.com",
        accounts: [:],
        availableDrives: [:],
        avatar: Data(),
        isConnected: true,
        isStaff: true
    )

    static let expectedAccountDbId = Int32.random(in: 0 ... 10000)
    static let expectedAccountId = Int32.random(in: 0 ... 10000)
    static let expectedAccountName = "myAccount"
    static var expectedAccount = Account(
        dbId: expectedAccountDbId, userDbId: expectedUserDbId, name: expectedAccountName, drives: [:]
    )

    static let updatedAccountName = "myUpdatedAccount"
    static var updatedAccount = Account(
        dbId: expectedAccountDbId, userDbId: expectedUserDbId, name: updatedAccountName, drives: [:]
    )

    static let expectedDriveDbId = Int32.random(in: 0 ... 10000)
    static let expectedDriveId = Int32.random(in: 0 ... 10000)
    static let expectedDriveName = "My Drive"
    static let expectedDriveColor: HexColor = .init(hex: "9de4ec")!
    static var expectedDrive = Drive.some(
        driveDbId: expectedDriveDbId,
        driveId: expectedDriveId,
        accountDbId: expectedAccountDbId,
        accountId: expectedAccountId,
        userDbId: expectedUserDbId,
        userId: expectedUserDbId,
        name: expectedDriveName,
        color: expectedDriveColor,
        synchros: [:]
    )

    static let updatedDriveId = Int32.random(in: 0 ... 10000)
    static let updatedDriveName = "My Drive Pro Max"
    static let updatedDriveColor: HexColor = .init(hex: "#aabbcc")!
    static var updatedDrive = Drive.some(
        driveDbId: expectedDriveDbId,
        driveId: updatedDriveId,
        accountDbId: expectedAccountDbId,
        accountId: expectedAccountId,
        userDbId: expectedUserDbId,
        userId: expectedUserDbId,
        name: updatedDriveName,
        color: updatedDriveColor,
        synchros: [:]
    )

    static let expectedSynchroDbId = Int32.random(in: 0 ... 10000)
    static let expectedSynchroLocalPath = "/dev/null"
    static let expectedSynchroPath = "/dev/null"
    static let expectedTargetPath = "dev/bin"
    static let expectedTargetNodeId = UUID().uuidString
    static let expectedSupportVFS = true
    static let expectedVirtualFileMode = KDC.VirtualFileMode.Mac
    static let expectedSynchro = Synchro(dbId: expectedSynchroDbId,
                                         driveDbId: expectedDriveDbId,
                                         localPath: expectedSynchroLocalPath,
                                         targetPath: expectedTargetPath,
                                         targetNodeId: expectedTargetNodeId,
                                         supportVfs: expectedSupportVFS,
                                         virtualFileMode: expectedVirtualFileMode)

    static let updatedSynchroLocalPath = "C:/Windows/System32"
    static let updatedSynchro = Synchro(dbId: expectedSynchroDbId,
                                        driveDbId: expectedDriveDbId,
                                        localPath: updatedSynchroLocalPath,
                                        targetPath: expectedTargetPath,
                                        targetNodeId: expectedTargetNodeId,
                                        supportVfs: expectedSupportVFS,
                                        virtualFileMode: expectedVirtualFileMode)
}
