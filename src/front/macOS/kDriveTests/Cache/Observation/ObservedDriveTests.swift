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

final class ObservedDriveTests_driveDbIdOnly: XCTestCase {
    func testSetObservedDrive() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedDrive(driveDbId: ObservableData.expectedDriveDbId, cacheObservation: cache) var observedDrive: Drive?
        XCTAssertNil(observedDrive, "Drive should initially be nil")

        await cache.addUser(ObservableData.expectedUserWithAccounts)

        // WHEN
        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        // Give time for observation to propagate
        try await Task.sleep(nanoseconds: 10_000_000_000)

        // THEN
        let cachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        XCTAssertEqual(cachedDrive, expectedDrive, "The cache should have been updated")
        XCTAssertEqual(observedDrive, expectedDrive, "The observed object should have been updated")
    }

    func testUpdateObservedDrive() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedDrive(driveDbId: ObservableData.expectedDriveDbId, cacheObservation: cache) var observedDrive: Drive?
        XCTAssertNil(observedDrive, "Drive should initially be nil")

        await cache.addUser(ObservableData.expectedUserWithAccounts)

        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        let cachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        XCTAssertEqual(cachedDrive, expectedDrive, "The cache should have been updated")

        // WHEN
        let updatedDrive = ObservableData.updatedDrive
        try await cache.addDrive(updatedDrive, accountDbId: ObservableData.expectedAccountDbId)

        // Give time for observation to propagate
        try await Task.sleep(nanoseconds: 10_000_000_000)

        // THEN
        let latestCachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        XCTAssertEqual(latestCachedDrive, updatedDrive, "The cache should have been updated again")
        XCTAssertEqual(observedDrive, updatedDrive, "The observed object should have been updated again")
    }

    func testRemoveObservedDrive() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedDrive(driveDbId: ObservableData.expectedDriveDbId, cacheObservation: cache) var observedDrive: Drive?
        XCTAssertNil(observedDrive, "Drive should initially be nil")

        await cache.addUser(ObservableData.expectedUserWithAccounts)

        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        let cachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        XCTAssertEqual(cachedDrive, expectedDrive, "The cache should have been updated")

        // WHEN
        await cache.removeDrive(driveDbId: ObservableData.expectedDrive.driveDbId,
                                accountDbId: ObservableData.expectedAccountDbId,
                                userDbId: ObservableData.expectedUserDbId)

        // Give time for observation to propagate
        try await Task.sleep(nanoseconds: 10_000_000_000)

        // THEN
        XCTAssertNil(observedDrive, "The observed object should have been updated to nil to reflect deletion")
    }
}

final class ObservedDriveTests_allIds: XCTestCase {
    func testSetObservedDrive() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedDrive(userDbId: ObservableData.expectedUserDbId,
                       accountDbId: ObservableData.expectedAccountDbId,
                       driveDbId: ObservableData.expectedDriveDbId,
                       cacheObservation: cache) var observedDrive: Drive?
        XCTAssertNil(observedDrive, "Drive should initially be nil")

        await cache.addUser(ObservableData.expectedUserWithAccounts)

        // WHEN
        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        // Give time for observation to propagate
        try await Task.sleep(nanoseconds: 10_000_000_000)

        // THEN
        let cachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        XCTAssertEqual(cachedDrive, expectedDrive, "The cache should have been updated")
        XCTAssertEqual(observedDrive, expectedDrive, "The observed object should have been updated")
    }

    func testUpdateObservedDrive() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedDrive(userDbId: ObservableData.expectedUserDbId,
                       accountDbId: ObservableData.expectedAccountDbId,
                       driveDbId: ObservableData.expectedDriveDbId,
                       cacheObservation: cache) var observedDrive: Drive?
        XCTAssertNil(observedDrive, "Drive should initially be nil")

        await cache.addUser(ObservableData.expectedUserWithAccounts)

        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        let cachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        XCTAssertEqual(cachedDrive, expectedDrive, "The cache should have been updated")

        // WHEN
        let updatedDrive = ObservableData.updatedDrive
        try await cache.addDrive(updatedDrive, accountDbId: ObservableData.expectedAccountDbId)

        // Give time for observation to propagate
        try await Task.sleep(nanoseconds: 10_000_000_000)

        // THEN
        let latestCachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        XCTAssertEqual(latestCachedDrive, updatedDrive, "The cache should have been updated again")
        XCTAssertEqual(observedDrive, updatedDrive, "The observed object should have been updated again")
    }

    func testRemoveObservedDrive() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        XCTAssertNil(initialUser, "Cache should initially be empty")

        @ObservedDrive(userDbId: ObservableData.expectedUserDbId,
                       accountDbId: ObservableData.expectedAccountDbId,
                       driveDbId: ObservableData.expectedDriveDbId,
                       cacheObservation: cache) var observedDrive: Drive?
        XCTAssertNil(observedDrive, "Drive should initially be nil")

        await cache.addUser(ObservableData.expectedUserWithAccounts)

        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        let cachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        XCTAssertEqual(cachedDrive, expectedDrive, "The cache should have been updated")

        // WHEN
        await cache.removeDrive(driveDbId: ObservableData.expectedDrive.driveDbId,
                                accountDbId: ObservableData.expectedAccountDbId,
                                userDbId: ObservableData.expectedUserDbId)

        // Give time for observation to propagate
        try await Task.sleep(nanoseconds: 10_000_000_000)

        // THEN
        XCTAssertNil(observedDrive, "The observed object should have been updated to nil to reflect deletion")
    }
}
