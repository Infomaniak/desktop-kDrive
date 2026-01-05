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

extension Drive {
    static func some(driveDbId: Int32,
                     driveId: Int32,
                     accountDbId: Int32,
                     accountId: Int32,
                     userDbId: Int32,
                     userId: Int32,
                     name: String,
                     color: HexColor?,
                     synchros: IndexedSynchros) -> Drive {
        Drive(driveDbId: driveDbId,
              driveId: driveId,
              accountDbId: accountDbId,
              accountId: accountId,
              userDbId: userDbId,
              name: name,
              color: color,
              accessDenied: false,
              admin: false,
              locked: false,
              maintenance: false,
              notifications: false,
              synchros: synchros)
    }
}

public enum ObservableData {
    static let expectedUserId = Int32.random(in: 0 ... 1000)
    static let expectedUserDbId = Int32(1)
    static let expectedAccountDbId = Int32(2)
    static let expectedAccountId = Int32.random(in: 0 ... 1000)
    static let expectedDriveDbId = Int32(3)
    static let expectedDriveId = Int32.random(in: 0 ... 1000)

    static let expectedDrive = Drive.some(driveDbId: expectedDriveDbId,
                                          driveId: expectedDriveId,
                                          accountDbId: expectedAccountDbId,
                                          accountId: expectedAccountId,
                                          userDbId: expectedUserDbId,
                                          userId: expectedUserId,
                                          name: "The amazing test Drive",
                                          color: HexColor(hex: "#ffffff"),
                                          synchros: [:])

    static let secondaryDriveDbId = Int32(4)
    static let secondaryDriveId = Int32.random(in: 0 ... 1000)
    static let secondaryDrive = Drive.some(driveDbId: secondaryDriveDbId,
                                           driveId: secondaryDriveId,
                                           accountDbId: expectedAccountDbId,
                                           accountId: expectedAccountId,
                                           userDbId: expectedUserDbId,
                                           userId: expectedUserId,
                                           name: "Secondary  amazing test Drive",
                                           color: HexColor(hex: "#aaaaaa"),
                                           synchros: [:])

    static let expectedDriveResponse = DriveResponse(driveDbId: expectedDriveDbId,
                                                     driveId: expectedDriveId,
                                                     accountDbId: expectedAccountDbId,
                                                     color: HexColor(hex: "#ffffff")!,
                                                     name: "The amazing test Drive",
                                                     accessDenied: false,
                                                     admin: true,
                                                     locked: false,
                                                     maintenance: false,
                                                     notifications: true)

    static let updatedDrive = Drive.some(driveDbId: expectedDriveDbId,
                                         driveId: expectedDriveId,
                                         accountDbId: expectedAccountDbId,
                                         accountId: expectedAccountId,
                                         userDbId: expectedUserDbId,
                                         userId: expectedUserId,
                                         name: "The amazing UPDATED test Drive",
                                         color: HexColor(hex: "#0a0a0a"),
                                         synchros: [:])

    static let expectedAccount = Account(dbId: expectedAccountDbId,
                                         userDbId: expectedUserDbId,
                                         name: "Some account",
                                         drives: [expectedDriveDbId: expectedDrive])

    static let updatedAccount = Account(dbId: expectedAccountDbId,
                                        userDbId: expectedUserDbId,
                                        name: "Some UPDATED account",
                                        drives: [expectedDriveDbId: updatedDrive])

    static let indexedAccounts: IndexedAccounts = [
        10: Account(dbId: 10, userDbId: expectedUserDbId, name: "10", drives: [:]),
        20: Account(dbId: 20, userDbId: expectedUserDbId, name: "20", drives: [:]),
        expectedAccountDbId: expectedAccount,
        40: Account(dbId: 40, userDbId: expectedUserDbId, name: "40", drives: [:])
    ]

    static let expectedUser = User(
        dbId: expectedUserDbId,
        userId: expectedUserId,
        name: "appleseed",
        email: "ja@apple.com",
        accounts: [:],
        availableDrives: [:],
        avatar: Data(),
        isConnected: true,
        isStaff: false
    )

    static let expectedUserWithAccounts = User(
        dbId: expectedUserDbId,
        userId: expectedUserId,
        name: "appleseed",
        email: "ja@apple.com",
        accounts: indexedAccounts,
        availableDrives: [:],
        avatar: Data(),
        isConnected: true,
        isStaff: false
    )

    static let updatedUser = User(
        dbId: expectedUserDbId,
        userId: expectedUserId,
        name: "appleseed  UPDATE",
        email: "ja@apple.com",
        accounts: indexedAccounts,
        availableDrives: [:],
        avatar: Data(),
        isConnected: true,
        isStaff: true
    )

    static let expectedSynchroDbId = Int32(5)
    static let expectedSynchroPath = "/dev/null"
    static let expectedTargetPath = "dev/bin"
    static let expectedTargetNodeId = UUID().uuidString
    static let expectedSupportVFS = true
    static let expectedVirtualFileMode = KDC.VirtualFileMode.Mac
    static let expectedSynchro = Synchro(dbId: expectedSynchroDbId,
                                         driveDbId: expectedDriveDbId,
                                         localPath: expectedSynchroPath,
                                         targetPath: expectedTargetPath,
                                         targetNodeId: expectedTargetNodeId,
                                         supportVfs: expectedSupportVFS,
                                         virtualFileMode: expectedVirtualFileMode)

    static let updatedSynchroPath = "C:\\Windows\\System32"
    static let updatedSynchro = Synchro(dbId: expectedSynchroDbId,
                                        driveDbId: expectedDriveDbId,
                                        localPath: updatedSynchroPath,
                                        targetPath: expectedTargetPath,
                                        targetNodeId: expectedTargetNodeId,
                                        supportVfs: expectedSupportVFS,
                                        virtualFileMode: expectedVirtualFileMode)

    static let secondarySynchroDbId = Int32(6)
    static let secondarySynchroPath = "/root/you-shall-not-pass"
    static let secondaryTargetPath = "/etc/shadow.d/definitely-readable"
    static let secondaryTargetNodeId = UUID().uuidString
    static let secondarySupportVFS = true
    static let secondaryVirtualFileMode = KDC.VirtualFileMode.Mac
    static let secondarySynchro = Synchro(dbId: secondarySynchroDbId,
                                          driveDbId: secondaryDriveDbId,
                                          localPath: secondarySynchroPath,
                                          targetPath: secondaryTargetPath,
                                          targetNodeId: secondaryTargetNodeId,
                                          supportVfs: secondarySupportVFS,
                                          virtualFileMode: secondaryVirtualFileMode)
}
