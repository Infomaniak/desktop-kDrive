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
import Testing

enum CacheData {
    static let expectedUserAPIId: Int32 = 56789
    static let expectedUserDbId: Int32 = 12345
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

    static let updatedUserAPIId: Int32 = expectedUserAPIId + 1
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

    static let expectedAccountDbId: Int32 = 2468
    static let expectedAccountName = "myAccount"
    static var expectedAccount = Account(
        dbId: expectedAccountDbId, name: expectedAccountName, drives: [:]
    )

    static let updatedAccountName = "myUpdatedAccount"
    static var updatedAccount = Account(
        dbId: expectedAccountDbId, name: updatedAccountName, drives: [:]
    )

    static let expectedDriveDbId: Int32 = 424_242
    static let expectedDriveId: Int32 = 112_233
    static let expectedDriveName: String = "My Drive"
    static let expectedDriveColor: HexColor = .init(hex: "9de4ec")!
    static var expectedDrive = Drive(
        driveDbId: expectedDriveDbId,
        driveId: expectedDriveId,
        accountId: expectedAccountDbId,
        userDbId: expectedUserDbId,
        userId: expectedUserDbId,
        name: expectedDriveName,
        color: expectedDriveColor,
        synchros: [:]
    )

    static let updatedDriveId: Int32 = 112_244
    static let updatedDriveName: String = "My Drive Pro Max"
    static let updatedDriveColor: HexColor = .init(hex: "#aabbcc")!
    static var updatedDrive = Drive(
        driveDbId: expectedDriveDbId,
        driveId: updatedDriveId,
        accountId: expectedAccountDbId,
        userDbId: expectedUserDbId,
        userId: expectedUserDbId,
        name: updatedDriveName,
        color: updatedDriveColor,
        synchros: [:]
    )

    static let expectedSynchroDbId: Int32 = 244_112
    static let expectedSynchroLocalPath = "/dev/null"
    static let expectedSynchro = Synchro(
        dbId: expectedSynchroDbId, driveDbId: expectedDriveDbId, localPath: expectedSynchroLocalPath
    )

    static let updatedSynchroLocalPath = "C:/Windows/System32"
    static let updatedSynchro = Synchro(
        dbId: expectedSynchroDbId, driveDbId: expectedDriveDbId, localPath: updatedSynchroLocalPath
    )
}

struct CoherentCacheUserTests {
    @Test func getUserInCache() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        #expect(await cache.getUser(apiId: CacheData.expectedUserAPIId) == nil)

        // WHEN
        await cache.addUser(user)

        // THEN
        #expect(await cache.getUser(apiId: CacheData.expectedUserAPIId) == user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)
    }

    @Test func removeUserInCacheFromDbId() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == nil)
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)

        // WHEN
        await cache.removeUser(dbId: CacheData.expectedUserDbId)

        // THEN
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == nil)
    }

    @Test func updateUserInCache() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == nil)
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)

        // WHEN
        await cache.updateUser(CacheData.updatedUser)

        // THEN
        guard let userByDbId = await cache.getUser(dbId: CacheData.expectedUserDbId) else {
            Issue.record("We should be able to fetch a user from db id")
            return
        }

        #expect(userByDbId != CacheData.expectedUser)
        #expect(userByDbId == CacheData.updatedUser)

        #expect(userByDbId.userId == CacheData.updatedUserAPIId, "The API id should change")
        #expect(userByDbId.name == CacheData.updatedUserName, "The user name should change")

        #expect(await cache.getUser(apiId: CacheData.expectedUserAPIId) == nil,
                "Should not be able to fetch an object with the old API id")
        #expect(await cache.getUser(apiId: CacheData.updatedUserAPIId) == CacheData.updatedUser,
                "Should be able to fetch an object with the old API id")
    }
}

struct CoherentCacheAccountTests {
    @Test func getAccountInCache() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        #expect(await cache.getUser(apiId: CacheData.expectedUserAPIId) == nil)
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)

        // WHEN
        await cache.addAccount(CacheData.expectedAccount, userDbId: CacheData.expectedUserDbId)

        // THEN
        #expect(await cache.getAccount(accountDbId: CacheData.expectedAccountDbId, userDbId: CacheData.expectedUserDbId) == CacheData.expectedAccount)
        #expect(await cache.getAccount(accountDbId: CacheData.expectedAccountDbId) == CacheData.expectedAccount)
    }

    @Test func removeAccountInCacheFromDbId() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        #expect(await cache.getUser(apiId: CacheData.expectedUserAPIId) == nil)
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == CacheData.expectedUser)
        #expect(await cache.getAccount(accountDbId: CacheData.expectedAccountDbId) == nil)
        await cache.addAccount(CacheData.expectedAccount, userDbId: CacheData.expectedUserDbId)
        #expect(await cache.getAccount(accountDbId: CacheData.expectedAccountDbId) == CacheData.expectedAccount)

        // WHEN
        await cache.removeAccount(accountDbId: CacheData.expectedAccountDbId)

        // THEN
        #expect(await cache.getAccount(accountDbId: CacheData.expectedAccountDbId) == nil)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == CacheData.expectedUser)
    }

    @Test func updateAccountInCache() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        #expect(await cache.getUser(apiId: CacheData.expectedUserAPIId) == nil)
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == CacheData.expectedUser)
        #expect(await cache.getAccount(accountDbId: CacheData.expectedAccountDbId) == nil)
        await cache.addAccount(CacheData.expectedAccount, userDbId: CacheData.expectedUserDbId)
        #expect(await cache.getAccount(accountDbId: CacheData.expectedAccountDbId) == CacheData.expectedAccount)

        // WHEN
        do {
            try await cache.updateAccount(CacheData.updatedAccount)

            // THEN
            guard let accountByDbId = await cache.getAccount(accountDbId: CacheData.expectedAccountDbId) else {
                Issue.record("We should be able to fetch an account from db id")
                return
            }

            #expect(accountByDbId != CacheData.expectedAccount)
            #expect(accountByDbId == CacheData.updatedAccount)
            #expect(accountByDbId.name == CacheData.updatedAccountName)
        } catch {
            Issue.record("unexpected error: \(error)")
        }
    }
}

struct CoherentCacheDriveTests {
    @Test func getDriveInCache() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)
        await cache.addAccount(CacheData.expectedAccount, userDbId: CacheData.expectedUserDbId)
        #expect(await cache.getAccount(accountDbId: CacheData.expectedAccountDbId, userDbId: CacheData.expectedUserDbId) == CacheData.expectedAccount)

        // WHEN
        await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId, userDbId: CacheData.expectedUserDbId)

        // THEN
        #expect(await cache.getDrive(driveDbId: CacheData.expectedDriveDbId) == CacheData.expectedDrive)
    }

    @Test func removeDriveInCacheFromDbId() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)
        await cache.addAccount(CacheData.expectedAccount, userDbId: CacheData.expectedUserDbId)
        #expect(await cache.getAccount(accountDbId: CacheData.expectedAccountDbId, userDbId: CacheData.expectedUserDbId) == CacheData.expectedAccount)
        await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId, userDbId: CacheData.expectedUserDbId)
        #expect(await cache.getDrive(driveDbId: CacheData.expectedDriveDbId) == CacheData.expectedDrive)

        // WHEN
        await cache.removeDrive(driveDbId: CacheData.expectedDriveDbId,
                                accountDbId: CacheData.expectedAccountDbId,
                                userDbId: CacheData.expectedUserDbId)

        // THEN
        #expect(await cache.getDrive(driveDbId: CacheData.expectedDriveDbId) == nil)
    }

    @Test func updateDriveInCache() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)
        await cache.addAccount(CacheData.expectedAccount, userDbId: CacheData.expectedUserDbId)
        #expect(await cache.getAccount(accountDbId: CacheData.expectedAccountDbId, userDbId: CacheData.expectedUserDbId) == CacheData.expectedAccount)
        await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId, userDbId: CacheData.expectedUserDbId)
        #expect(await cache.getDrive(driveDbId: CacheData.expectedDriveDbId) == CacheData.expectedDrive)

        // WHEN
        do {
            try await cache.updateDrive(drive: CacheData.updatedDrive)

            // THEN
            #expect(await cache.getDrive(driveDbId: CacheData.expectedDriveDbId) != CacheData.expectedDrive)
            #expect(await cache.getDrive(driveDbId: CacheData.expectedDriveDbId) == CacheData.updatedDrive)
        } catch {
            Issue.record("unexpected error: \(error)")
        }
    }
}

struct CoherentCacheSynchroTests {
    @Test func getSynchroInCache() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)
        await cache.addAccount(CacheData.expectedAccount, userDbId: CacheData.expectedUserDbId)
        #expect(await cache.getAccount(accountDbId: CacheData.expectedAccountDbId, userDbId: CacheData.expectedUserDbId) == CacheData.expectedAccount)
        await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId, userDbId: CacheData.expectedUserDbId)
        #expect(await cache.getDrive(driveDbId: CacheData.expectedDriveDbId) == CacheData.expectedDrive)

        // WHEN
        await cache.addSynchro(CacheData.expectedSynchro,
                               toDrive: CacheData.expectedDriveDbId,
                               accountDbId: CacheData.expectedAccountDbId,
                               userDbId: CacheData.expectedUserDbId)

        // THEN
        #expect(await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId) == CacheData.expectedSynchro)
        #expect(await cache.getSynchro(
            synchroDbId: CacheData.expectedSynchroDbId,
            driveDbId: CacheData.expectedDriveDbId,
            accountDbId: CacheData.expectedAccountDbId,
            userDbId: CacheData.expectedUserDbId
        ) == CacheData.expectedSynchro)
    }

    @Test func removeSynchroInCacheFromDbId() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)
        await cache.addAccount(CacheData.expectedAccount, userDbId: CacheData.expectedUserDbId)
        #expect(await cache.getAccount(accountDbId: CacheData.expectedAccountDbId, userDbId: CacheData.expectedUserDbId) == CacheData.expectedAccount)
        await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId, userDbId: CacheData.expectedUserDbId)
        #expect(await cache.getDrive(driveDbId: CacheData.expectedDriveDbId) == CacheData.expectedDrive)
        await cache.addSynchro(CacheData.expectedSynchro,
                               toDrive: CacheData.expectedDriveDbId,
                               accountDbId: CacheData.expectedAccountDbId,
                               userDbId: CacheData.expectedUserDbId)
        #expect(await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId) == CacheData.expectedSynchro)

        // WHEN
        await cache.removeSynchro(
            synchroDbId: CacheData.expectedSynchroDbId,
            driveDbId: CacheData.expectedDriveDbId,
            accountDbId: CacheData.expectedAccountDbId,
            userDbId: CacheData.expectedUserDbId
        )

        // THEN
        #expect(await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId) == nil)
        #expect(await cache.getSynchro(
            synchroDbId: CacheData.expectedSynchroDbId,
            driveDbId: CacheData.expectedDriveDbId,
            accountDbId: CacheData.expectedAccountDbId,
            userDbId: CacheData.expectedUserDbId
        ) == nil)
    }

    @Test func updateSynchroInCache() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)
        await cache.addAccount(CacheData.expectedAccount, userDbId: CacheData.expectedUserDbId)
        #expect(await cache.getAccount(accountDbId: CacheData.expectedAccountDbId, userDbId: CacheData.expectedUserDbId) == CacheData.expectedAccount)
        await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId, userDbId: CacheData.expectedUserDbId)
        #expect(await cache.getDrive(driveDbId: CacheData.expectedDriveDbId) == CacheData.expectedDrive)
        await cache.addSynchro(CacheData.expectedSynchro,
                               toDrive: CacheData.expectedDriveDbId,
                               accountDbId: CacheData.expectedAccountDbId,
                               userDbId: CacheData.expectedUserDbId)
        #expect(await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId) == CacheData.expectedSynchro)

        // WHEN
        do {
            try await cache.updateSynchro(CacheData.updatedSynchro)

            // THEN
            #expect(await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId) != CacheData.expectedSynchro)
            #expect(await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId) == CacheData.updatedSynchro)

        } catch {
            Issue.record("unexpected error: \(error)")
        }
    }
}
