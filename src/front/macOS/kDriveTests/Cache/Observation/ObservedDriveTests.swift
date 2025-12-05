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
struct ObservedDriveTests_driveDbIdOnly {
    @Test(.timeLimit(.minutes(1)))
    func setObservedDrive() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedDrive(driveDbId: ObservableData.expectedDriveDbId, cacheObservation: cache) var observedDrive: Drive?
        let receivedValues = $observedDrive.receivedValues // Start to save the received values
        #expect(observedDrive == nil, "Drive should initially be nil")

        await cache.addUser(ObservableData.expectedUserWithAccounts)

        // WHEN
        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        // THEN
        let lastReceivedObject = await receivedValues.first(where: { $0 != nil })
        #expect(lastReceivedObject == expectedDrive)

        let cachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        #expect(cachedDrive == expectedDrive, "The cache should have been updated")
        #expect(observedDrive == expectedDrive, "The observed object should have been updated")
    }

    @Test(.timeLimit(.minutes(1)))
    func updateObservedDrive() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedDrive(driveDbId: ObservableData.expectedDriveDbId, cacheObservation: cache) var observedDrive: Drive?
        let receivedValues = $observedDrive.receivedValues
        #expect(observedDrive == nil, "Drive should initially be nil")

        await cache.addUser(ObservableData.expectedUserWithAccounts)

        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        let cachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        #expect(cachedDrive == expectedDrive, "The cache should have been updated")

        // WHEN
        let updatedDrive = ObservableData.updatedDrive
        try await cache.addDrive(updatedDrive, accountDbId: ObservableData.expectedAccountDbId)

        // THEN
        _ = await receivedValues.dropFirst().first(where: { $0 != nil })

        let latestCachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        #expect(latestCachedDrive == updatedDrive, "The cache should have been updated again")
        #expect(observedDrive == updatedDrive, "The observed object should have been updated again")
    }

    @Test(.timeLimit(.minutes(1)))
    func doubleUpdateObservedDrive() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedDrive(driveDbId: ObservableData.expectedDriveDbId, cacheObservation: cache) var observedDrive: Drive?
        let receivedValues = $observedDrive.receivedValues
        #expect(observedDrive == nil, "Drive should initially be nil")

        await cache.addUser(ObservableData.expectedUserWithAccounts)

        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        let cachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        #expect(cachedDrive == expectedDrive, "The cache should have been updated")

        // WHEN
        let updatedDrive = ObservableData.updatedDrive
        try await cache.addDrive(updatedDrive, accountDbId: ObservableData.expectedAccountDbId)
        try await cache.addDrive(updatedDrive, accountDbId: ObservableData.expectedAccountDbId)

        // THEN
        _ = await receivedValues.dropFirst().dropFirst().first(where: { $0 != nil })

        let latestCachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        #expect(latestCachedDrive == updatedDrive, "The cache should have been updated again")
        #expect(observedDrive == updatedDrive, "The observed object should have been updated again")
    }

    @Test(.timeLimit(.minutes(1)))
    func removeObservedDrive() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedDrive(driveDbId: ObservableData.expectedDriveDbId, cacheObservation: cache) var observedDrive: Drive?
        let receivedValues = $observedDrive.receivedValues
        #expect(observedDrive == nil, "Drive should initially be nil")

        await cache.addUser(ObservableData.expectedUserWithAccounts)

        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        let cachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        #expect(cachedDrive == expectedDrive, "The cache should have been updated")

        // WHEN
        await cache.removeDrive(driveDbId: ObservableData.expectedDrive.driveDbId,
                                accountDbId: ObservableData.expectedAccountDbId,
                                userDbId: ObservableData.expectedUserDbId)

        // THEN
        _ = await receivedValues.dropFirst().first(where: { $0 == nil })

        #expect(observedDrive == nil, "The observed object should have been updated to nil to reflect deletion")
    }
}

@MainActor
struct ObservedDriveTests_allIds {
    @Test(.timeLimit(.minutes(1)))
    func setObservedDrive() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedDrive(userDbId: ObservableData.expectedUserDbId,
                       accountDbId: ObservableData.expectedAccountDbId,
                       driveDbId: ObservableData.expectedDriveDbId,
                       cacheObservation: cache) var observedDrive: Drive?
        let receivedValues = $observedDrive.receivedValues
        #expect(observedDrive == nil, "Drive should initially be nil")

        await cache.addUser(ObservableData.expectedUserWithAccounts)

        // WHEN
        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        // THEN
        let lastReceivedObject = await receivedValues.first(where: { $0 != nil })
        #expect(lastReceivedObject == expectedDrive)

        let cachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        #expect(cachedDrive == expectedDrive, "The cache should have been updated")
        #expect(observedDrive == expectedDrive, "The observed object should have been updated")
    }

    @Test(.timeLimit(.minutes(1)))
    func updateObservedDrive() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedDrive(userDbId: ObservableData.expectedUserDbId,
                       accountDbId: ObservableData.expectedAccountDbId,
                       driveDbId: ObservableData.expectedDriveDbId,
                       cacheObservation: cache) var observedDrive: Drive?
        let receivedValues = $observedDrive.receivedValues
        #expect(observedDrive == nil, "Drive should initially be nil")

        await cache.addUser(ObservableData.expectedUserWithAccounts)

        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        let cachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        #expect(cachedDrive == expectedDrive, "The cache should have been updated")

        // WHEN
        let updatedDrive = ObservableData.updatedDrive
        try await cache.addDrive(updatedDrive, accountDbId: ObservableData.expectedAccountDbId)

        // THEN
        _ = await receivedValues.dropFirst().first(where: { $0 != nil })

        let latestCachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        #expect(latestCachedDrive == updatedDrive, "The cache should have been updated again")
        #expect(observedDrive == updatedDrive, "The observed object should have been updated again")
    }

    @Test(.timeLimit(.minutes(1)))
    func doubleUpdateObservedDrive() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedDrive(userDbId: ObservableData.expectedUserDbId,
                       accountDbId: ObservableData.expectedAccountDbId,
                       driveDbId: ObservableData.expectedDriveDbId,
                       cacheObservation: cache) var observedDrive: Drive?
        let receivedValues = $observedDrive.receivedValues
        #expect(observedDrive == nil, "Drive should initially be nil")

        await cache.addUser(ObservableData.expectedUserWithAccounts)

        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        let cachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        #expect(cachedDrive == expectedDrive, "The cache should have been updated")

        // WHEN
        let updatedDrive = ObservableData.updatedDrive
        try await cache.addDrive(updatedDrive, accountDbId: ObservableData.expectedAccountDbId)
        try await cache.addDrive(updatedDrive, accountDbId: ObservableData.expectedAccountDbId)

        // THEN
        _ = await receivedValues.dropFirst().dropFirst().first(where: { $0 != nil })

        let latestCachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        #expect(latestCachedDrive == updatedDrive, "The cache should have been updated again")
        #expect(observedDrive == updatedDrive, "The observed object should have been updated again")
    }

    @Test(.timeLimit(.minutes(1)))
    func removeObservedDrive() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedDrive(userDbId: ObservableData.expectedUserDbId,
                       accountDbId: ObservableData.expectedAccountDbId,
                       driveDbId: ObservableData.expectedDriveDbId,
                       cacheObservation: cache) var observedDrive: Drive?
        let receivedValues = $observedDrive.receivedValues
        #expect(observedDrive == nil, "Drive should initially be nil")

        await cache.addUser(ObservableData.expectedUserWithAccounts)

        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        let cachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        #expect(cachedDrive == expectedDrive, "The cache should have been updated")

        // WHEN
        await cache.removeDrive(driveDbId: ObservableData.expectedDrive.driveDbId,
                                accountDbId: ObservableData.expectedAccountDbId,
                                userDbId: ObservableData.expectedUserDbId)

        // THEN
        _ = await receivedValues.dropFirst().first(where: { $0 == nil })

        #expect(observedDrive == nil, "The observed object should have been updated to nil to reflect deletion")
    }
}
