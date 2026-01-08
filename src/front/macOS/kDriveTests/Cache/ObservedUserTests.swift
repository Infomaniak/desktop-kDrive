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
@testable import InfomaniakDI
import kDriveCore
import XCTest

final class ObservedUserTests: XCTestCase {
    static let expectedUserId: Int32 = 12345
    static let expectedUserDbId: Int32 = 5678

    func testSetGetUserFromPropertyWrapper() async throws {
        // GIVEN
        let cache = CoherentCache()
        let initialUser = await cache.getUser(id: Self.expectedUserId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedUser(id: ObservedUserTests.expectedUserId, cacheObservation: cache) var observedUser: User?
        XCTAssertNil(observedUser, "User should initially be nil")

        // WHEN
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

        await cache.addUser(user)

        // Give time for observation to propagate
        try await Task.sleep(nanoseconds: 1_000_000_000)

        // THEN
        let cachedUser = await cache.getUser(id: Self.expectedUserId)
        XCTAssertEqual(cachedUser, user, "The cache should have been updated")
        XCTAssertEqual(observedUser, user, "The observed object should have been updated")
    }
}
