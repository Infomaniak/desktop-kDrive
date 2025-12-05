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
import XCTest

final class ObservedSynchroTests_dbIdOnly: XCTestCase {
    func testSetObservedSynchro() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedSynchro(synchroDbId: ObservableData.expectedSynchroDbId, cacheObservation: cache) var observedSynchro: Synchro?
        let receivedValues = $observedSynchro.receivedValues // Start to save the received values

        XCTAssertNil(observedSynchro, "Synchro should initially be nil")
        await cache.addUser(ObservableData.expectedUserWithAccounts)

        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        let cachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        XCTAssertEqual(cachedDrive, expectedDrive, "The cache should have been updated with a Drive")

        // WHEN
        let expectedSynchro = ObservableData.expectedSynchro
        try await cache.addSynchro(expectedSynchro)

        // THEN
        _ = await receivedValues.first(where: { $0 != nil })

        let fetchedSynchro = await cache.getSynchro(synchroDbId: ObservableData.expectedSynchroDbId)
        XCTAssertEqual(fetchedSynchro, ObservableData.expectedSynchro, "We should find the object in cache")
        XCTAssertEqual(observedSynchro, ObservableData.expectedSynchro, "The observed object should have been updated")
    }

    func testSetObservedSynchro_driveDoesNotExists() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedSynchro(synchroDbId: ObservableData.expectedSynchroDbId, cacheObservation: cache) var observedSynchro: Synchro?

        XCTAssertNil(observedSynchro, "Synchro should initially be nil")

        // WHEN
        let expectedSynchro = ObservableData.expectedSynchro
        do {
            try await cache.addSynchro(expectedSynchro)
            XCTFail("This should have thrown")
        } catch ServerCoherentCache.CacheError.driveNotFound {
            // cool story bro
        } catch {
            XCTFail("Unexpected error \(error)")
        }
    }

    func testUpdateObservedSynchro() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedSynchro(synchroDbId: ObservableData.expectedSynchroDbId, cacheObservation: cache) var observedSynchro: Synchro?
        let receivedValues = $observedSynchro.receivedValues

        XCTAssertNil(observedSynchro, "Synchro should initially be nil")
        await cache.addUser(ObservableData.expectedUserWithAccounts)

        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        let cachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        XCTAssertEqual(cachedDrive, expectedDrive, "The cache should have been updated with a Drive")

        let expectedSynchro = ObservableData.expectedSynchro
        try await cache.addSynchro(expectedSynchro)

        let fetchedSynchro = await cache.getSynchro(synchroDbId: ObservableData.expectedSynchroDbId)
        XCTAssertEqual(fetchedSynchro, ObservableData.expectedSynchro, "We should find the object in cache")

        // WHEN
        let updatedSynchro = ObservableData.updatedSynchro
        try await cache.updateSynchro(updatedSynchro)

        // THEN
        _ = await receivedValues.dropFirst().first(where: { $0 != nil })

        let latestFetchedSynchro = await cache.getSynchro(synchroDbId: ObservableData.expectedSynchroDbId)
        XCTAssertEqual(latestFetchedSynchro, ObservableData.updatedSynchro, "We should find the object in cache updated")
        XCTAssertEqual(observedSynchro, ObservableData.updatedSynchro, "The observed object should have been updated again")
    }

    func testDoubleUpdateObservedSynchro() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedSynchro(synchroDbId: ObservableData.expectedSynchroDbId, cacheObservation: cache) var observedSynchro: Synchro?
        let receivedValues = $observedSynchro.receivedValues

        XCTAssertNil(observedSynchro, "Synchro should initially be nil")
        await cache.addUser(ObservableData.expectedUserWithAccounts)

        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        let cachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        XCTAssertEqual(cachedDrive, expectedDrive, "The cache should have been updated with a Drive")

        let expectedSynchro = ObservableData.expectedSynchro
        try await cache.addSynchro(expectedSynchro)

        let fetchedSynchro = await cache.getSynchro(synchroDbId: ObservableData.expectedSynchroDbId)
        XCTAssertEqual(fetchedSynchro, ObservableData.expectedSynchro, "We should find the object in cache")

        // WHEN
        let updatedSynchro = ObservableData.updatedSynchro
        try await cache.updateSynchro(updatedSynchro)
        try await cache.updateSynchro(updatedSynchro)

        // THEN
        _ = await receivedValues.dropFirst().dropFirst().dropFirst().first(where: { $0 != nil })

        let latestFetchedSynchro = await cache.getSynchro(synchroDbId: ObservableData.expectedSynchroDbId)
        XCTAssertEqual(latestFetchedSynchro, ObservableData.updatedSynchro, "We should find the object in cache updated")
        XCTAssertEqual(observedSynchro, ObservableData.updatedSynchro, "The observed object should have been updated again")
    }

    func testDeleteObservedSynchro() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedSynchro(synchroDbId: ObservableData.expectedSynchroDbId, cacheObservation: cache) var observedSynchro: Synchro?
        let receivedValues = $observedSynchro.receivedValues

        XCTAssertNil(observedSynchro, "Synchro should initially be nil")
        await cache.addUser(ObservableData.expectedUserWithAccounts)

        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        let cachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        XCTAssertEqual(cachedDrive, expectedDrive, "The cache should have been updated with a Drive")

        let expectedSynchro = ObservableData.expectedSynchro
        try await cache.addSynchro(expectedSynchro)

        let fetchedSynchro = await cache.getSynchro(synchroDbId: ObservableData.expectedSynchroDbId)
        XCTAssertEqual(fetchedSynchro, ObservableData.expectedSynchro, "We should find the object in cache")

        // WHEN
        try await cache.removeSynchro(synchroDbId: expectedSynchro.dbId,
                                      driveDbId: expectedSynchro.driveDbId)

        // THEN
        _ = await receivedValues.dropFirst().first(where: { $0 == nil })

        let latestFetchedSynchro = await cache.getSynchro(synchroDbId: ObservableData.expectedSynchroDbId)
        XCTAssertNil(latestFetchedSynchro, "Object should no longer be available in cache")
        XCTAssertNil(observedSynchro, "Object should be nilified once it's removed from the cache")
    }
}

final class ObservedSynchroTests_allIds: XCTestCase {
    func testSetObservedSynchro() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedSynchro(userDbId: ObservableData.expectedUserDbId,
                         accountDbId: ObservableData.expectedAccountDbId,
                         driveDbId: ObservableData.expectedDriveDbId,
                         synchroDbId: ObservableData.expectedSynchroDbId,
                         cacheObservation: cache) var observedSynchro: Synchro?
        let receivedValues = $observedSynchro.receivedValues

        XCTAssertNil(observedSynchro, "Synchro should initially be nil")
        await cache.addUser(ObservableData.expectedUserWithAccounts)

        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        let cachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        XCTAssertEqual(cachedDrive, expectedDrive, "The cache should have been updated with a Drive")

        // WHEN
        let expectedSynchro = ObservableData.expectedSynchro
        try await cache.addSynchro(expectedSynchro)

        // THEN
        _ = await receivedValues.first(where: { $0 != nil })

        let fetchedSynchro = await cache.getSynchro(synchroDbId: ObservableData.expectedSynchroDbId)
        XCTAssertEqual(fetchedSynchro, ObservableData.expectedSynchro, "We should find the object in cache")
        XCTAssertEqual(observedSynchro, ObservableData.expectedSynchro, "The observed object should have been updated")
    }

    func testSetObservedSynchro_driveDoesNotExists() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedSynchro(userDbId: ObservableData.expectedUserDbId,
                         accountDbId: ObservableData.expectedAccountDbId,
                         driveDbId: ObservableData.expectedDriveDbId,
                         synchroDbId: ObservableData.expectedSynchroDbId,
                         cacheObservation: cache) var observedSynchro: Synchro?

        XCTAssertNil(observedSynchro, "Synchro should initially be nil")

        // WHEN
        let expectedSynchro = ObservableData.expectedSynchro
        do {
            try await cache.addSynchro(expectedSynchro)
            XCTFail("This should have thrown")
        } catch ServerCoherentCache.CacheError.driveNotFound {
            // cool story bro
        } catch {
            XCTFail("Unexpected error \(error)")
        }
    }

    func testUpdateObservedSynchro() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedSynchro(userDbId: ObservableData.expectedUserDbId,
                         accountDbId: ObservableData.expectedAccountDbId,
                         driveDbId: ObservableData.expectedDriveDbId,
                         synchroDbId: ObservableData.expectedSynchroDbId,
                         cacheObservation: cache) var observedSynchro: Synchro?
        let receivedValues = $observedSynchro.receivedValues

        XCTAssertNil(observedSynchro, "Synchro should initially be nil")
        await cache.addUser(ObservableData.expectedUserWithAccounts)

        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        let cachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        XCTAssertEqual(cachedDrive, expectedDrive, "The cache should have been updated with a Drive")

        let expectedSynchro = ObservableData.expectedSynchro
        try await cache.addSynchro(expectedSynchro)

        let fetchedSynchro = await cache.getSynchro(synchroDbId: ObservableData.expectedSynchroDbId)
        XCTAssertEqual(fetchedSynchro, ObservableData.expectedSynchro, "We should find the object in cache")

        // WHEN
        let updatedSynchro = ObservableData.updatedSynchro
        try await cache.updateSynchro(updatedSynchro)

        // THEN
        _ = await receivedValues.dropFirst().first(where: { $0 != nil })

        let latestFetchedSynchro = await cache.getSynchro(synchroDbId: ObservableData.expectedSynchroDbId)
        XCTAssertEqual(latestFetchedSynchro, ObservableData.updatedSynchro, "We should find the object in cache updated")
        XCTAssertEqual(observedSynchro, ObservableData.updatedSynchro, "The observed object should have been updated again")
    }

    func testDoubleUpdateObservedSynchro() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedSynchro(userDbId: ObservableData.expectedUserDbId,
                         accountDbId: ObservableData.expectedAccountDbId,
                         driveDbId: ObservableData.expectedDriveDbId,
                         synchroDbId: ObservableData.expectedSynchroDbId,
                         cacheObservation: cache) var observedSynchro: Synchro?
        let receivedValues = $observedSynchro.receivedValues

        XCTAssertNil(observedSynchro, "Synchro should initially be nil")
        await cache.addUser(ObservableData.expectedUserWithAccounts)

        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        let cachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        XCTAssertEqual(cachedDrive, expectedDrive, "The cache should have been updated with a Drive")

        let expectedSynchro = ObservableData.expectedSynchro
        try await cache.addSynchro(expectedSynchro)

        let fetchedSynchro = await cache.getSynchro(synchroDbId: ObservableData.expectedSynchroDbId)
        XCTAssertEqual(fetchedSynchro, ObservableData.expectedSynchro, "We should find the object in cache")

        // WHEN
        let updatedSynchro = ObservableData.updatedSynchro
        try await cache.updateSynchro(updatedSynchro)
        try await cache.updateSynchro(updatedSynchro)

        // THEN
        _ = await receivedValues.dropFirst().dropFirst().first(where: { $0 != nil })

        let latestFetchedSynchro = await cache.getSynchro(synchroDbId: ObservableData.expectedSynchroDbId)
        XCTAssertEqual(latestFetchedSynchro, ObservableData.updatedSynchro, "We should find the object in cache updated")
        XCTAssertEqual(observedSynchro, ObservableData.updatedSynchro, "The observed object should have been updated again")
    }

    func testDeleteObservedSynchro() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedSynchro(userDbId: ObservableData.expectedUserDbId,
                         accountDbId: ObservableData.expectedAccountDbId,
                         driveDbId: ObservableData.expectedDriveDbId,
                         synchroDbId: ObservableData.expectedSynchroDbId,
                         cacheObservation: cache) var observedSynchro: Synchro?
        let receivedValues = $observedSynchro.receivedValues

        XCTAssertNil(observedSynchro, "Synchro should initially be nil")
        await cache.addUser(ObservableData.expectedUserWithAccounts)

        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        let cachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        XCTAssertEqual(cachedDrive, expectedDrive, "The cache should have been updated with a Drive")

        let expectedSynchro = ObservableData.expectedSynchro
        try await cache.addSynchro(expectedSynchro)

        let fetchedSynchro = await cache.getSynchro(synchroDbId: ObservableData.expectedSynchroDbId)
        XCTAssertEqual(fetchedSynchro, ObservableData.expectedSynchro, "We should find the object in cache")

        // WHEN
        try await cache.removeSynchro(synchroDbId: expectedSynchro.dbId,
                                      driveDbId: expectedSynchro.driveDbId)

        // THEN
        _ = await receivedValues.dropFirst().first(where: { $0 == nil })

        let latestFetchedSynchro = await cache.getSynchro(synchroDbId: ObservableData.expectedSynchroDbId)
        XCTAssertNil(latestFetchedSynchro, "Object should no longer be available in cache")
        XCTAssertNil(observedSynchro, "Object should be nilified once it's removed from the cache")
    }
}
