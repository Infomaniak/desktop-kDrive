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
@testable import kDriveCore
import Testing

@MainActor
struct ObservedUserTests {
    @Test(.timeLimit(.minutes(1)))
    func setObservedUser() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedUser(userDbId: ObservableData.expectedUserDbId, cacheObservation: cache) var observedUser: User?
        let receivedValues = $observedUser.receivedValues // Start to save the received values
        #expect(observedUser == nil, "User should initially be nil")

        // WHEN
        let expectedUser = ObservableData.expectedUserWithAccounts
        await cache.addUser(expectedUser)

        // THEN
        let lastReceivedObject = await receivedValues.first(where: { $0 != nil })
        #expect(lastReceivedObject == expectedUser, "The object should be a received event")

        let cachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(cachedUser == expectedUser, "The cache should have been updated")
        #expect(observedUser == expectedUser, "The observed object should have been updated")
    }

    @Test(.timeLimit(.minutes(1)))
    func updateObservedUser() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedUser(userDbId: ObservableData.expectedUserDbId, cacheObservation: cache) var observedUser: User?
        let receivedValues = $observedUser.receivedValues
        #expect(observedUser == nil, "User should initially be nil")

        let expectedUser = ObservableData.expectedUser
        await cache.addUser(expectedUser)

        let cachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(cachedUser == expectedUser, "The cache should have been updated")

        // WHEN
        let updatedUser = ObservableData.updatedUser
        await cache.addUser(updatedUser)

        // THEN
        _ = await receivedValues.dropFirst().first(where: { $0 != nil })

        let latestCachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(latestCachedUser == updatedUser, "The object should be in cache")
        #expect(observedUser == updatedUser, "The observed object should be up to date")
    }

    @Test(.timeLimit(.minutes(1)))
    func doubleUpdateObservedUser() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedUser(userDbId: ObservableData.expectedUserDbId, cacheObservation: cache) var observedUser: User?
        let receivedValues = $observedUser.receivedValues
        #expect(observedUser == nil, "User should initially be nil")

        let expectedUser = ObservableData.expectedUser
        await cache.addUser(expectedUser)

        let cachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(cachedUser == expectedUser, "The cache should have been updated")

        // WHEN
        let updatedUser = ObservableData.updatedUser
        await cache.addUser(updatedUser)
        await cache.addUser(updatedUser)

        // THEN
        _ = await receivedValues.dropFirst().dropFirst().first(where: { $0 != nil })

        let latestCachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(latestCachedUser == updatedUser, "The object should no longer be in cache")
        #expect(observedUser == updatedUser, "The observed object should be up to date")
    }

    @Test(.timeLimit(.minutes(1)))
    func removeObservedUser() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedUser(userDbId: ObservableData.expectedUserDbId, cacheObservation: cache) var observedUser: User?
        let receivedValues = $observedUser.receivedValues
        #expect(observedUser == nil, "User should initially be nil")

        let expectedUser = ObservableData.expectedUser
        await cache.addUser(expectedUser)

        let cachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(cachedUser == expectedUser, "The cache should have been updated")

        // WHEN
        await cache.removeUser(dbId: ObservableData.expectedUserDbId)

        // THEN
        _ = await receivedValues.dropFirst().first(where: { $0 == nil })

        let latestCachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(latestCachedUser == nil, "The object should no longer be in cache")
        #expect(observedUser == nil, "The observed object should be nil since the cache entry was deleted")
    }
}
