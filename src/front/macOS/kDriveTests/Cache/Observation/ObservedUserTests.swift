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
    func testSetObservedUser() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedUser(dbId: ObservableData.expectedUserDbId, cacheObservation: cache) var observedUser: User?
        XCTAssertNil(observedUser, "User should initially be nil")

        // WHEN
        let expectedUser = ObservableData.expectedUserWithAccounts
        await cache.addUser(expectedUser)

        // Give time for observation to propagate
        try await Task.sleep(nanoseconds: 10_000_000_000)

        // THEN
        let cachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertEqual(cachedUser, expectedUser, "The cache should have been updated")
        XCTAssertEqual(observedUser, expectedUser, "The observed object should have been updated")
    }

    func testUpdateObservedUser() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedUser(dbId: ObservableData.expectedUserDbId, cacheObservation: cache) var observedUser: User?
        XCTAssertNil(observedUser, "User should initially be nil")

        let expectedUser = ObservableData.expectedUser
        await cache.addUser(expectedUser)

        let cachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertEqual(cachedUser, expectedUser, "The cache should have been updated")

        // WHEN
        let updatedUser = ObservableData.updatedUser
        await cache.addUser(updatedUser)

        // Give time for observation to propagate
        try await Task.sleep(nanoseconds: 10_000_000_000)

        // THEN
        let latestCachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertEqual(latestCachedUser, updatedUser, "The observed object should have been updated again")
    }

    func testUpdateObservedUserTwice() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedUser(dbId: ObservableData.expectedUserDbId, cacheObservation: cache) var observedUser: User?
        XCTAssertNil(observedUser, "User should initially be nil")

        let expectedUser = ObservableData.expectedUser
        await cache.addUser(expectedUser)

        let cachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertEqual(cachedUser, expectedUser, "The cache should have been updated")

        // WHEN
        let updatedUser = ObservableData.updatedUser
        await cache.addUser(updatedUser)
        await cache.addUser(updatedUser)

        // Give time for observation to propagate
        try await Task.sleep(nanoseconds: 10_000_000_000)

        // THEN
        let latestCachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertEqual(latestCachedUser, updatedUser, "The observed object should have been updated again")
    }

    func testRemoveObservedUser() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedUser(dbId: ObservableData.expectedUserDbId, cacheObservation: cache) var observedUser: User?
        XCTAssertNil(observedUser, "User should initially be nil")

        let expectedUser = ObservableData.expectedUser
        await cache.addUser(expectedUser)

        let cachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertEqual(cachedUser, expectedUser, "The cache should have been updated")

        // WHEN
        await cache.removeUser(dbId: ObservableData.expectedUserDbId)

        // Give time for observation to propagate
        try await Task.sleep(nanoseconds: 10_000_000_000)

        // THEN
        let latestCachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(latestCachedUser, "The observed object should be nil since the cache entry was deleted")
    }
}
