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

struct CoherentCacheTests {
    static let expectedUserId: Int32 = 56789
    static let expectedUserDbId: Int32 = 12345

    @Test func testSetGetUserInCacheFromPrimaryKey() async throws {
        // GIVEN
        let user = User(
            dbId: Self.expectedUserDbId,
            userId: Self.expectedUserId,
            name: "appleseed",
            email: "ja@apple.com",
            accounts: [:],
            avatar: Data(),
            isConnected: true,
            isStaff: true
        )
        let cache = CoherentCache()
        #expect(await cache.getUser(apiId: Self.expectedUserId) == nil)

        // WHEN
        await cache.addUser(user)

        // THEN
        #expect(await cache.getUser(apiId: Self.expectedUserId) == user)
        #expect(await cache.getUser(dbId: Self.expectedUserDbId) == user)
    }

    @Test func testSetGetUserInCacheFromDbId() async throws {
        // GIVEN
        let user = User(
            dbId: Self.expectedUserDbId,
            userId: Self.expectedUserId,
            name: "appleseed",
            email: "ja@apple.com",
            accounts: [:],
            avatar: Data(),
            isConnected: true,
            isStaff: true
        )
        let cache = CoherentCache()
        #expect(await cache.getUser(dbId: Self.expectedUserDbId) == nil)

        // WHEN
        await cache.addUser(user)

        // THEN
        #expect(await cache.getUser(dbId: Self.expectedUserDbId) == user)
    }
}
