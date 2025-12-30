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

import Combine
import Foundation
@testable import InfomaniakDI
@testable import kDriveCore
import Testing

extension ObservedDrives {
    var receivedValues: AsyncStream<[DriveContext]> {
        AsyncStream { continuation in
            let cancellable = $wrappedValue
                .sink { value in continuation.yield(value) }

            continuation.onTermination = { _ in cancellable.cancel() }
        }
    }
}

@MainActor
struct ObservedDrivesTests {
    @Test(.timeLimit(.minutes(1)))
    func setObservedDrives() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedDrives(cacheObservation: cache) var observedDrives: [DriveContext]
        let receivedValues = $observedDrives.receivedValues // Start to save the received values

        #expect(observedDrives == [], "Drives should initially be empty")
        await cache.addUser(ObservableData.expectedUserWithAccounts)

        // WHEN
        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        // THEN
        _ = await receivedValues.first(where: { _ in true })

        let cachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        #expect(cachedDrive == expectedDrive, "The cache should have been updated with a Drive")
        #expect(observedDrives.count == 1, "We should have one Drive in the array")

        guard let firstDrive = observedDrives.first else {
            Issue.record("Failed to unwrap the first Drive")
            return
        }

        #expect(firstDrive.drive.driveDbId == expectedDrive.driveDbId, "The drive should match the one we just added")
        #expect(firstDrive.account.dbId == ObservableData.expectedAccount.dbId, "The account should match the one we just added")
        #expect(firstDrive.user.dbId == ObservableData.expectedUser.dbId, "The user should match the one we just added")
    }

    @Test(.timeLimit(.minutes(1)))
    func setMultipleObservedDrives() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedDrives(cacheObservation: cache) var observedDrives: [DriveContext]
        let receivedValues = $observedDrives.receivedValues // Start to save the received values

        #expect(observedDrives == [], "Observed Drives should initially be empty")
        await cache.addUser(ObservableData.expectedUserWithAccounts)

        // WHEN
        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        let secondaryDrive = ObservableData.secondaryDrive
        try await cache.addDrive(secondaryDrive, accountDbId: ObservableData.expectedAccountDbId)

        // THEN
        _ = await receivedValues.dropFirst().first(where: { _ in true })

        let cachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        #expect(cachedDrive == expectedDrive, "The cache should have been updated with a Drive")
        let secondaryCachedDrive = await cache.getDrive(driveDbId: ObservableData.secondaryDriveDbId)
        #expect(secondaryCachedDrive == secondaryDrive, "The cache should have been updated with another Drive")
        #expect(observedDrives.count == 2, "We should have two Drives in the array")

        guard let firstObservedDrive = observedDrives.first(where: { $0.drive.driveDbId == ObservableData.expectedDriveDbId }) else {
            Issue.record("Failed to unwrap the first Drive")
            return
        }

        #expect(firstObservedDrive.drive.driveDbId == expectedDrive.driveDbId, "The drive should match")
        #expect(firstObservedDrive.account.dbId == ObservableData.expectedAccount.dbId, "The account should match")
        #expect(firstObservedDrive.user.dbId == ObservableData.expectedUser.dbId, "The user should match")

        guard let secondaryObservedDrive = observedDrives.first(where: { $0.drive.driveDbId == ObservableData.secondaryDriveDbId }) else {
            Issue.record("Failed to unwrap the secondary Drive")
            return
        }

        #expect(firstObservedDrive.drive != secondaryObservedDrive.drive, "The two drives should not match")
        #expect(secondaryObservedDrive.drive.driveDbId == secondaryDrive.driveDbId, "The drive should match")
        #expect(secondaryObservedDrive.account.dbId == ObservableData.expectedAccount.dbId, "The account should match")
        #expect(secondaryObservedDrive.user.dbId == ObservableData.expectedUser.dbId, "The user should match")
    }
}
