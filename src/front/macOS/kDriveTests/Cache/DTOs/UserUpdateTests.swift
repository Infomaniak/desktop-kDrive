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

struct UserUpdateTests {
    private static let dbId: Int32 = 1
    private static let userId: Int32 = 100
    private static let name = "Test User"
    private static let email = "test@example.com"
    private static let avatar = Data([0x01, 0x02, 0x03])
    private static let isConnected = true
    private static let isStaff = false

    func createUser(
        dbId: Int32 = dbId,
        userId: Int32 = userId,
        name: String = name,
        email: String = email,
        accounts: IndexedAccounts = [:],
        availableDrives: IndexedAvailableDrives = [:],
        avatar: Data? = avatar,
        isConnected: Bool = isConnected,
        isStaff: Bool = isStaff
    ) -> User {
        User(
            dbId: dbId,
            userId: userId,
            name: name,
            email: email,
            accounts: accounts,
            availableDrives: availableDrives,
            avatar: avatar,
            isConnected: isConnected,
            isStaff: isStaff
        )
    }

    @Test("Update with different dbId returns nil")
    func updateWithDifferentDbId() {
        // GIVEN
        let original = createUser()
        let other = createUser(dbId: 2, userId: 200, name: "Other User")

        // WHEN
        let result = original.updated(with: other, updateOptions: .all)

        // THEN
        #expect(result == nil)
    }

    @Test("Update userId with .userId option")
    func updateUserId() {
        // GIVEN
        let original = createUser()
        let other = createUser(userId: 999)

        // WHEN
        let result = original.updated(with: other, updateOptions: .userId)

        // THEN
        #expect(result != nil)
        #expect(result?.userId == 999)
        // Other fields should be unchanged
        #expect(result?.name == original.name)
        #expect(result?.email == original.email)
        #expect(result?.avatar == original.avatar)
        #expect(result?.isConnected == original.isConnected)
        #expect(result?.isStaff == original.isStaff)
        #expect(result?.accounts == original.accounts)
        #expect(result?.availableDrives == original.availableDrives)
    }

    @Test("Update name with .name option")
    func updateName() {
        // GIVEN
        let original = createUser()
        let other = createUser(name: "Updated Name")

        // WHEN
        let result = original.updated(with: other, updateOptions: .name)

        // THEN
        #expect(result != nil)
        #expect(result?.name == "Updated Name")
        // Other fields should be unchanged
        #expect(result?.dbId == original.dbId)
        #expect(result?.userId == original.userId)
        #expect(result?.email == original.email)
        #expect(result?.avatar == original.avatar)
        #expect(result?.isConnected == original.isConnected)
        #expect(result?.isStaff == original.isStaff)
        #expect(result?.accounts == original.accounts)
        #expect(result?.availableDrives == original.availableDrives)
    }

    @Test("Update email with .email option")
    func updateEmail() {
        // GIVEN
        let original = createUser()
        let other = createUser(email: "updated@example.com")

        // WHEN
        let result = original.updated(with: other, updateOptions: .email)

        // THEN
        #expect(result != nil)
        #expect(result?.email == "updated@example.com")
        // Other fields should be unchanged
        #expect(result?.dbId == original.dbId)
        #expect(result?.userId == original.userId)
        #expect(result?.name == original.name)
        #expect(result?.avatar == original.avatar)
        #expect(result?.isConnected == original.isConnected)
        #expect(result?.isStaff == original.isStaff)
        #expect(result?.accounts == original.accounts)
        #expect(result?.availableDrives == original.availableDrives)
    }

    @Test("Update avatar with .avatar option")
    func updateAvatar() {
        // GIVEN
        let original = createUser()
        let newAvatar = Data([0xAA, 0xBB, 0xCC])
        let other = createUser(avatar: newAvatar)

        // WHEN
        let result = original.updated(with: other, updateOptions: .avatar)

        // THEN
        #expect(result != nil)
        #expect(result?.avatar == newAvatar)
        // Other fields should be unchanged
        #expect(result?.dbId == original.dbId)
        #expect(result?.userId == original.userId)
        #expect(result?.name == original.name)
        #expect(result?.email == original.email)
        #expect(result?.isConnected == original.isConnected)
        #expect(result?.isStaff == original.isStaff)
        #expect(result?.accounts == original.accounts)
        #expect(result?.availableDrives == original.availableDrives)
    }

    @Test("Update isConnected with .isConnected option")
    func updateIsConnected() {
        // GIVEN
        let original = createUser(isConnected: true)
        let other = createUser(isConnected: false)

        // WHEN
        let result = original.updated(with: other, updateOptions: .isConnected)

        // THEN
        #expect(result != nil)
        #expect(result?.isConnected == false)
        // Other fields should be unchanged
        #expect(result?.dbId == original.dbId)
        #expect(result?.userId == original.userId)
        #expect(result?.name == original.name)
        #expect(result?.email == original.email)
        #expect(result?.avatar == original.avatar)
        #expect(result?.isStaff == original.isStaff)
        #expect(result?.accounts == original.accounts)
        #expect(result?.availableDrives == original.availableDrives)
    }

    @Test("Update isStaff with .isStaff option")
    func updateIsStaff() {
        // GIVEN
        let original = createUser(isStaff: false)
        let other = createUser(isStaff: true)

        // WHEN
        let result = original.updated(with: other, updateOptions: .isStaff)

        // THEN
        #expect(result != nil)
        #expect(result?.isStaff == true)
        // Other fields should be unchanged
        #expect(result?.dbId == original.dbId)
        #expect(result?.userId == original.userId)
        #expect(result?.name == original.name)
        #expect(result?.email == original.email)
        #expect(result?.avatar == original.avatar)
        #expect(result?.isConnected == original.isConnected)
        #expect(result?.accounts == original.accounts)
        #expect(result?.availableDrives == original.availableDrives)
    }

    @Test("Update accounts with .accounts option")
    func updateAccounts() {
        // GIVEN
        let original = createUser()
        var accounts: IndexedAccounts = [:]
        accounts[1] = Account(dbId: 1, userDbId: UserUpdateTests.dbId, name: "Test Account", drives: [:])
        let other = createUser(accounts: accounts)

        // WHEN
        let result = original.updated(with: other, updateOptions: .accounts)

        // THEN
        #expect(result != nil)
        #expect(result?.accounts.count == 1)
        #expect(result?.accounts[1]?.name == "Test Account")
        // Other fields should be unchanged
        #expect(result?.dbId == original.dbId)
        #expect(result?.userId == original.userId)
        #expect(result?.name == original.name)
        #expect(result?.email == original.email)
        #expect(result?.avatar == original.avatar)
        #expect(result?.isConnected == original.isConnected)
        #expect(result?.isStaff == original.isStaff)
        #expect(result?.availableDrives == original.availableDrives)
    }

    @Test("Update availableDrives with .availableDrives option")
    func updateAvailableDrives() throws {
        // GIVEN
        let original = createUser()
        var drives: IndexedAvailableDrives = [:]
        drives[1] = try AvailableDrive(
            driveId: 1,
            accountId: 1,
            userDbId: UserUpdateTests.dbId,
            userId: UserUpdateTests.userId,
            name: "Test Drive",
            color: #require(HexColor(hex: "FF0000"))
        )
        let other = createUser(availableDrives: drives)

        // WHEN
        let result = original.updated(with: other, updateOptions: .availableDrives)

        // THEN
        #expect(result != nil)
        #expect(result?.availableDrives.count == 1)
        #expect(result?.availableDrives[1]?.name == "Test Drive")
        // Other fields should be unchanged
        #expect(result?.dbId == original.dbId)
        #expect(result?.userId == original.userId)
        #expect(result?.name == original.name)
        #expect(result?.email == original.email)
        #expect(result?.avatar == original.avatar)
        #expect(result?.isConnected == original.isConnected)
        #expect(result?.isStaff == original.isStaff)
        #expect(result?.accounts == original.accounts)
    }

    @Test("Update all fields with .all option")
    func updateAllFields() {
        // GIVEN
        let original = createUser()
        let other = createUser(
            userId: 999,
            name: "Updated Name",
            email: "updated@example.com",
            avatar: Data([0xFF]),
            isConnected: false,
            isStaff: true
        )

        // WHEN
        let result = original.updated(with: other, updateOptions: .all)

        // THEN
        #expect(result != nil)
        #expect(result?.userId == 999)
        #expect(result?.name == "Updated Name")
        #expect(result?.email == "updated@example.com")
        #expect(result?.avatar == Data([0xFF]))
        #expect(result?.isConnected == false)
        #expect(result?.isStaff == true)
    }

    @Test("Update signal fields with .updateSignal option")
    func updateSignalFields() {
        // GIVEN
        let original = createUser()
        let other = createUser(
            userId: 999,
            name: "Updated Name",
            email: "updated@example.com",
            avatar: Data([0xFF]),
            isConnected: false,
            isStaff: true
        )

        // WHEN
        let result = original.updated(with: other, updateOptions: .updateSignal)

        // THEN
        #expect(result != nil)
        #expect(result?.userId == 999)
        #expect(result?.name == "Updated Name")
        #expect(result?.email == "updated@example.com")
        #expect(result?.avatar == Data([0xFF]))
        #expect(result?.isConnected == false)
        #expect(result?.isStaff == true)
        // These fields should NOT be updated by .updateSignal
        #expect(result?.dbId == original.dbId)
        #expect(result?.accounts == original.accounts)
        #expect(result?.availableDrives == original.availableDrives)
    }

    @Test("No update when option not specified")
    func noUpdateWhenOptionNotSpecified() {
        // GIVEN
        let original = createUser(name: "Original", email: "original@example.com")
        let other = createUser(name: "Updated", email: "updated@example.com")

        // WHEN - Only update name, not email
        let result = original.updated(with: other, updateOptions: .name)

        // THEN
        #expect(result != nil)
        #expect(result?.name == "Updated")
        // Email should not change
        #expect(result?.email == "original@example.com")
        // All other fields should also be unchanged
        #expect(result?.dbId == original.dbId)
        #expect(result?.userId == original.userId)
        #expect(result?.avatar == original.avatar)
        #expect(result?.isConnected == original.isConnected)
        #expect(result?.isStaff == original.isStaff)
        #expect(result?.accounts == original.accounts)
        #expect(result?.availableDrives == original.availableDrives)
    }

    @Test("Multiple options combined")
    func multipleOptionsCombined() {
        // GIVEN
        let original = createUser()
        let other = createUser(name: "Updated", email: "updated@example.com", isConnected: false)

        // WHEN
        let result = original.updated(with: other, updateOptions: [.name, .email])

        // THEN
        #expect(result != nil)
        #expect(result?.name == "Updated")
        #expect(result?.email == "updated@example.com")
        // These were not specified and should not change
        #expect(result?.isConnected == original.isConnected)
        // All other fields should also be unchanged
        #expect(result?.dbId == original.dbId)
        #expect(result?.userId == original.userId)
        #expect(result?.avatar == original.avatar)
        #expect(result?.isStaff == original.isStaff)
        #expect(result?.accounts == original.accounts)
        #expect(result?.availableDrives == original.availableDrives)
    }

    @Test("Update preserves dbId")
    func updatePreservesDbId() {
        // GIVEN
        let original = createUser(dbId: 42)
        let other = createUser(dbId: 42, name: "Updated")

        // WHEN
        let result = original.updated(with: other, updateOptions: .all)

        // THEN
        #expect(result != nil)
        #expect(result?.dbId == 42)
    }

    @Test("Original user is not mutated")
    func originalNotMutated() {
        // GIVEN
        let original = createUser(name: "Original", email: "original@example.com")
        let other = createUser(name: "Updated", email: "updated@example.com")

        // WHEN
        let result = original.updated(with: other, updateOptions: .all)

        // THEN
        #expect(result != nil)
        #expect(original.name == "Original")
        #expect(original.email == "original@example.com")
        #expect(result?.name == "Updated")
        #expect(result?.email == "updated@example.com")
    }
}
