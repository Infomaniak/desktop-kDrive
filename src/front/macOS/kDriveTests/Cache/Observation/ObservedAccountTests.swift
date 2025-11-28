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

final class ObservedAccountTests: XCTestCase {
    func testSetGetAccountFromPropertyWrapper_userDbId_accountDbId() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedAccount(userDbId: ObservableData.expectedUserDbId,
                         accountDbId: ObservableData.expectedAccountDbId,
                         cacheObservation: cache) var observedAccount: Account?
        XCTAssertNil(observedAccount, "Account should initially be nil")

        let expectedAccount = Account(dbId: ObservableData.expectedAccountDbId, name: "3", drives: [:])

        // WHEN
        let user = ObservableData.expectedUser

        await cache.addUser(user)

        // Give time for observation to propagate
        try await Task.sleep(nanoseconds: 10_000_000_000)

        // THEN
        let cachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        let cachedAccount = await cache.getAccount(accountDbId: ObservableData.expectedAccountDbId, userDbId: ObservableData.expectedUserDbId)
        XCTAssertEqual(cachedUser, user, "The cache user should have been updated")
        XCTAssertEqual(cachedAccount, cachedAccount, "The cache account should have been updated")
        XCTAssertEqual(observedAccount, expectedAccount, "The observed object should have been updated")
    }

    func testSetGetAccountFromPropertyWrapper_accountDbId() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedAccount(accountDbId: ObservableData.expectedAccountDbId,
                         cacheObservation: cache) var observedAccount: Account?
        XCTAssertNil(observedAccount, "Account should initially be nil")

        let expectedAccount = Account(dbId: ObservableData.expectedAccountDbId, name: "3", drives: [:])

        // WHEN
        let user = ObservableData.expectedUser

        await cache.addUser(user)

        // Give time for observation to propagate
        try await Task.sleep(nanoseconds: 10_000_000_000)

        // THEN
        let cachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        let cachedAccount = await cache.getAccount(accountDbId: ObservableData.expectedAccountDbId, userDbId: ObservableData.expectedUserDbId)
        XCTAssertEqual(cachedUser, user, "The cache user should have been updated")
        XCTAssertEqual(cachedAccount, cachedAccount, "The cache account should have been updated")
        XCTAssertEqual(observedAccount, expectedAccount, "The observed object should have been updated")
    }
}
