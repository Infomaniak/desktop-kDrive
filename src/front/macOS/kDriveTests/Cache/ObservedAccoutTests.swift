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

final class ObservedAccoutTests: XCTestCase {
    static let expectedUserId: Int32 = 123
    static let expectedUserDbId: Int32 = 456
    static let expectedAccountDbId: Int32 = 3

    func testSetGetAccountFromPropertyWrapper() async throws {
        // GIVEN
        let cache = CoherentCache()
        let initialUser = await cache.getUser(dbId: Self.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedAccount(userDbId: Self.expectedUserDbId,
                         accountDbId: Self.expectedAccountDbId,
                         cacheObservation: cache) var observedAccount: Account?
        XCTAssertNil(observedAccount, "Account should initially be nil")

        let expectedAccount = Account(dbId: Self.expectedAccountDbId, name: "3", drives: [:])

        // WHEN
        let indexedAccounts: IndexedAccounts = [
            1: Account(dbId: 1, name: "1", drives: [:]),
            2: Account(dbId: 2, name: "2", drives: [:]),
            Self.expectedAccountDbId: expectedAccount,
            4: Account(dbId: 4, name: "4", drives: [:])
        ]
        let user = User(
            dbId: Self.expectedUserDbId,
            userId: Self.expectedUserId,
            name: "appleseed",
            email: "ja@apple.com",
            accounts: indexedAccounts,
            availableDrives: [:],
            avatar: Data(),
            isConnected: true,
            isStaff: true
        )

        await cache.addUser(user)

        // Give time for observation to propagate
        try await Task.sleep(nanoseconds: 5_000_000_000)

        // THEN
        let cachedUser = await cache.getUser(dbId: Self.expectedUserDbId)
        let cachedAccount = await cache.getAccount(Self.expectedAccountDbId, userDbId: Self.expectedUserDbId)
        XCTAssertEqual(cachedUser, user, "The cache user should have been updated")
        XCTAssertEqual(cachedAccount, cachedAccount, "The cache account should have been updated")
        XCTAssertEqual(observedAccount, expectedAccount, "The observed object should have been updated")
    }
}
