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

final class ObservedAccountTests_dbIdOnly: XCTestCase {
    func testSetObservedAccount() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedAccount(accountDbId: ObservableData.expectedAccountDbId,
                         cacheObservation: cache) var observedAccount: Account?
        XCTAssertNil(observedAccount, "Account should initially be nil")

        // WHEN
        let expectedUser = ObservableData.expectedUser
        await cache.addUser(expectedUser)

        // Give time for observation to propagate
        try await Task.sleep(nanoseconds: 10_000_000_000)

        // THEN
        let cachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        let cachedAccount = await cache.getAccount(accountDbId: ObservableData.expectedAccountDbId, userDbId: ObservableData.expectedUserDbId)

        XCTAssertEqual(cachedUser, expectedUser, "The cache user should have been updated")
        XCTAssertEqual(cachedAccount, ObservableData.expectedAccount, "The cache account should have been updated")
        XCTAssertEqual(observedAccount, ObservableData.expectedAccount, "The observed object should have been updated")
    }

    func testUpdateObservedAccount() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedAccount(accountDbId: ObservableData.expectedAccountDbId,
                         cacheObservation: cache) var observedAccount: Account?
        XCTAssertNil(observedAccount, "Account should initially be nil")

        let expectedUser = ObservableData.expectedUser
        await cache.addUser(expectedUser)

        let cachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        let cachedAccount = await cache.getAccount(accountDbId: ObservableData.expectedAccountDbId, userDbId: ObservableData.expectedUserDbId)

        XCTAssertEqual(cachedUser, expectedUser, "The cache user should have been updated")
        XCTAssertEqual(cachedAccount, ObservableData.expectedAccount, "The cache account should have been updated")

        // WHEN
        try await cache.updateAccount(ObservableData.updatedAccount)

        // Give time for observation to propagate
        try await Task.sleep(nanoseconds: 10_000_000_000)

        // THEN
        let latestAccount = await cache.getAccount(accountDbId: ObservableData.expectedAccountDbId, userDbId: ObservableData.expectedUserDbId)
        XCTAssertEqual(latestAccount, ObservableData.updatedAccount, "The cache account should have been updated again")
        XCTAssertEqual(observedAccount, ObservableData.updatedAccount, "The observed object should have been updated again")
    }
    
    func testDoubleUpdateObservedAccount() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedAccount(accountDbId: ObservableData.expectedAccountDbId,
                         cacheObservation: cache) var observedAccount: Account?
        XCTAssertNil(observedAccount, "Account should initially be nil")

        let expectedUser = ObservableData.expectedUser
        await cache.addUser(expectedUser)

        let cachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        let cachedAccount = await cache.getAccount(accountDbId: ObservableData.expectedAccountDbId, userDbId: ObservableData.expectedUserDbId)

        XCTAssertEqual(cachedUser, expectedUser, "The cache user should have been updated")
        XCTAssertEqual(cachedAccount, ObservableData.expectedAccount, "The cache account should have been updated")

        // WHEN
        try await cache.updateAccount(ObservableData.updatedAccount)
        try await cache.updateAccount(ObservableData.updatedAccount)

        // Give time for observation to propagate
        try await Task.sleep(nanoseconds: 10_000_000_000)

        // THEN
        let latestAccount = await cache.getAccount(accountDbId: ObservableData.expectedAccountDbId, userDbId: ObservableData.expectedUserDbId)
        XCTAssertEqual(latestAccount, ObservableData.updatedAccount, "The cache account should have been updated again")
        XCTAssertEqual(observedAccount, ObservableData.updatedAccount, "The observed object should have been updated again")
    }

    func testDeleteObservedAccount() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedAccount(accountDbId: ObservableData.expectedAccountDbId,
                         cacheObservation: cache) var observedAccount: Account?
        XCTAssertNil(observedAccount, "Account should initially be nil")

        let expectedUser = ObservableData.expectedUser
        await cache.addUser(expectedUser)

        let cachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        let cachedAccount = await cache.getAccount(accountDbId: ObservableData.expectedAccountDbId, userDbId: ObservableData.expectedUserDbId)

        XCTAssertEqual(cachedUser, expectedUser, "The cache user should have been updated")
        XCTAssertEqual(cachedAccount, ObservableData.expectedAccount, "The cache account should have been updated")

        // WHEN
        await cache.removeAccount(accountDbId: ObservableData.expectedAccountDbId)

        // Give time for observation to propagate
        try await Task.sleep(nanoseconds: 10_000_000_000)

        // THEN
        XCTAssertNil(observedAccount, "The observed object should be nil to reflect deletion")
    }
}

final class ObservedAccountTests_allIds: XCTestCase {
    func testSetObservedAccount() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedAccount(userDbId: ObservableData.expectedUserDbId,
                         accountDbId: ObservableData.expectedAccountDbId,
                         cacheObservation: cache) var observedAccount: Account?
        XCTAssertNil(observedAccount, "Account should initially be nil")

        // WHEN
        let expectedUser = ObservableData.expectedUser
        await cache.addUser(expectedUser)

        // Give time for observation to propagate
        try await Task.sleep(nanoseconds: 10_000_000_000)

        // THEN
        let cachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        let cachedAccount = await cache.getAccount(accountDbId: ObservableData.expectedAccountDbId, userDbId: ObservableData.expectedUserDbId)
        XCTAssertEqual(cachedUser, expectedUser, "The cache user should have been updated")
        XCTAssertEqual(cachedAccount, ObservableData.expectedAccount, "The cache account should have been updated")
        XCTAssertEqual(observedAccount, ObservableData.expectedAccount, "The observed object should have been updated")
    }

    func testUpdateObservedAccount() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedAccount(userDbId: ObservableData.expectedUserDbId,
                         accountDbId: ObservableData.expectedAccountDbId,
                         cacheObservation: cache) var observedAccount: Account?
        XCTAssertNil(observedAccount, "Account should initially be nil")

        let expectedUser = ObservableData.expectedUser
        await cache.addUser(expectedUser)

        let cachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        let cachedAccount = await cache.getAccount(accountDbId: ObservableData.expectedAccountDbId, userDbId: ObservableData.expectedUserDbId)

        XCTAssertEqual(cachedUser, expectedUser, "The cache user should have been updated")
        XCTAssertEqual(cachedAccount, ObservableData.expectedAccount, "The cache account should have been updated")

        // WHEN
        try await cache.updateAccount(ObservableData.updatedAccount)

        // Give time for observation to propagate
        try await Task.sleep(nanoseconds: 10_000_000_000)

        // THEN
        let latestAccount = await cache.getAccount(accountDbId: ObservableData.expectedAccountDbId, userDbId: ObservableData.expectedUserDbId)
        XCTAssertEqual(latestAccount, ObservableData.updatedAccount, "The cache account should have been updated again")
        XCTAssertEqual(observedAccount, ObservableData.updatedAccount, "The observed object should have been updated again")
    }
    
    func testDoubleUpdateObservedAccount() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedAccount(userDbId: ObservableData.expectedUserDbId,
                         accountDbId: ObservableData.expectedAccountDbId,
                         cacheObservation: cache) var observedAccount: Account?
        XCTAssertNil(observedAccount, "Account should initially be nil")

        let expectedUser = ObservableData.expectedUser
        await cache.addUser(expectedUser)

        let cachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        let cachedAccount = await cache.getAccount(accountDbId: ObservableData.expectedAccountDbId, userDbId: ObservableData.expectedUserDbId)

        XCTAssertEqual(cachedUser, expectedUser, "The cache user should have been updated")
        XCTAssertEqual(cachedAccount, ObservableData.expectedAccount, "The cache account should have been updated")

        // WHEN
        try await cache.updateAccount(ObservableData.updatedAccount)
        try await cache.updateAccount(ObservableData.updatedAccount)

        // Give time for observation to propagate
        try await Task.sleep(nanoseconds: 10_000_000_000)

        // THEN
        let latestAccount = await cache.getAccount(accountDbId: ObservableData.expectedAccountDbId, userDbId: ObservableData.expectedUserDbId)
        XCTAssertEqual(latestAccount, ObservableData.updatedAccount, "The cache account should have been updated again")
        XCTAssertEqual(observedAccount, ObservableData.updatedAccount, "The observed object should have been updated again")
    }

    func testDeleteObservedAccount() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedAccount(userDbId: ObservableData.expectedUserDbId,
                         accountDbId: ObservableData.expectedAccountDbId,
                         cacheObservation: cache) var observedAccount: Account?
        XCTAssertNil(observedAccount, "Account should initially be nil")

        let expectedUser = ObservableData.expectedUser
        await cache.addUser(expectedUser)

        let cachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        let cachedAccount = await cache.getAccount(accountDbId: ObservableData.expectedAccountDbId, userDbId: ObservableData.expectedUserDbId)

        XCTAssertEqual(cachedUser, expectedUser, "The cache user should have been updated")
        XCTAssertEqual(cachedAccount, ObservableData.expectedAccount, "The cache account should have been updated")

        // WHEN
        await cache.removeAccount(accountDbId: ObservableData.expectedAccountDbId)

        // Give time for observation to propagate
        try await Task.sleep(nanoseconds: 10_000_000_000)

        // THEN
        XCTAssertNil(observedAccount, "The observed object should be nil to reflect deletion")
    }
}
