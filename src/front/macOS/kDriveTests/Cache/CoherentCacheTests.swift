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

enum CoherentCacheTestsData {
    static let expectedUserId: Int32 = 56789
    static let expectedUserDbId: Int32 = 12345
    static var expectedUser: User {
        User(
            dbId: expectedUserDbId,
            userId: expectedUserId,
            name: "appleseed",
            email: "ja@apple.com",
            accounts: [:],
            availableDrives: [:],
            avatar: Data(),
            isConnected: true,
            isStaff: true
        )
    }
}

struct CoherentCacheUserTests {
    @Test func setGetUserInCacheFromPrimaryKey() async throws {
        // GIVEN
        let user = CoherentCacheTestsData.expectedUser
        let cache = ServerCoherentCache()
        #expect(await cache.getUser(apiId: CoherentCacheTestsData.expectedUserId) == nil)

        // WHEN
        await cache.addUser(user)

        // THEN
        #expect(await cache.getUser(apiId: CoherentCacheTestsData.expectedUserId) == user)
        #expect(await cache.getUser(dbId: CoherentCacheTestsData.expectedUserDbId) == user)
    }

    @Test func setGetUserInCacheFromDbId() async throws {
        // GIVEN
        let user = CoherentCacheTestsData.expectedUser
        let cache = ServerCoherentCache()
        #expect(await cache.getUser(dbId: CoherentCacheTestsData.expectedUserDbId) == nil)

        // WHEN
        await cache.addUser(user)

        // THEN
        #expect(await cache.getUser(dbId: CoherentCacheTestsData.expectedUserDbId) == user)
    }
}
