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
import kDriveCore
import Testing

enum CacheData {
    static let expectedUserAPIId: Int32 = 56789
    static let expectedUserDbId: Int32 = 12345
    static var expectedUser: User {
        User(
            dbId: expectedUserDbId,
            userId: expectedUserAPIId,
            name: "appleseed",
            email: "ja@apple.com",
            accounts: [:],
            availableDrives: [],
            avatar: Data(),
            isConnected: true,
            isStaff: true
        )
    }

    static let updatedUserAPIId: Int32 = expectedUserAPIId + 1
    static let updatedUserName = "appleseed2"
    static var updatedUser: User {
        User(
            dbId: expectedUserDbId,
            userId: updatedUserAPIId,
            name: updatedUserName,
            email: "ja@apple.com",
            accounts: [:],
            availableDrives: [],
            avatar: Data(),
            isConnected: true,
            isStaff: true
        )
    }

    static let expectedAccountDbId: Int32 = 2468
    static let expectedAccountName = "myAccount"
    static var expectedAccount: Account {
        Account(dbId: expectedAccountDbId, name: expectedAccountName, drives: [:])
    }
}

struct CoherentCacheUserTests {
    @Test func getUserInCache() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = CoherentCache()
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
        let cache = CoherentCache()
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
        let cache = CoherentCache()
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

        #expect(userByDbId.userId == CacheData.updatedUserAPIId, "The API id should change")
        #expect(userByDbId.name == CacheData.updatedUserName, "The user name should change")

        #expect(await cache.getUser(apiId: CacheData.expectedUserAPIId) == nil,
                "Should not be able to fetch an object with the old API id")
        #expect(await cache.getUser(apiId: CacheData.updatedUserAPIId) == CacheData.updatedUser,
                "Should be able to fetch an object with the old API id")
    }
}
