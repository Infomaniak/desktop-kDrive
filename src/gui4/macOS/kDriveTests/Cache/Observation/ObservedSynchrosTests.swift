/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2026 Infomaniak Network SA

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

extension ObservedSynchros {
    var receivedValues: AsyncStream<[SynchroContext]> {
        AsyncStream { continuation in
            let cancellable = $wrappedValue
                .sink { value in continuation.yield(value) }

            continuation.onTermination = { _ in cancellable.cancel() }
        }
    }
}

@MainActor
struct ObservedSynchrosTests {
    @Test(.timeLimit(.minutes(1)))
    func setObservedSynchros() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedSynchros(cacheObservation: cache) var observedSynchros: [SynchroContext]
        let receivedValues = $observedSynchros.receivedValues // Start to save the received values

        #expect(observedSynchros == [], "Synchros should initially be empty")
        await cache.addUser(ObservableData.expectedUserWithAccounts)

        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        let cachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        #expect(cachedDrive == expectedDrive, "The cache should have been updated with a Drive")

        // WHEN
        let expectedSynchro = ObservableData.expectedSynchro
        try await cache.addSynchro(expectedSynchro)

        // THEN
        _ = await receivedValues.first(where: { _ in true })

        let fetchedSynchro = await cache.getSynchro(synchroDbId: ObservableData.expectedSynchroDbId)
        #expect(fetchedSynchro == ObservableData.expectedSynchro, "We should find the object in cache")
        #expect(observedSynchros.count == 1, "We should have one synchro in the array")

        guard let firstSynchro = observedSynchros.first else {
            Issue.record("Failed to unwrap the first synchro")
            return
        }

        #expect(firstSynchro.synchro == expectedSynchro, "The synchro in the array should match the one we just added")
        #expect(firstSynchro.drive.driveDbId == expectedDrive.driveDbId, "The drive should match the one we just added")
        #expect(
            firstSynchro.account.dbId == ObservableData.expectedAccount.dbId,
            "The account should match the one we just added"
        )
        #expect(firstSynchro.user.dbId == ObservableData.expectedUser.dbId, "The user should match the one we just added")
    }

    @Test(.timeLimit(.minutes(1)))
    func setMultipleObservedSynchros() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedSynchros(cacheObservation: cache) var observedSynchros: [SynchroContext]
        let receivedValues = $observedSynchros.receivedValues // Start to save the received values

        #expect(observedSynchros == [], "Synchros should initially be empty")
        await cache.addUser(ObservableData.expectedUserWithAccounts)

        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        let secondaryDrive = ObservableData.secondaryDrive
        try await cache.addDrive(secondaryDrive, accountDbId: ObservableData.expectedAccountDbId)

        let cachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        #expect(cachedDrive == expectedDrive, "The cache should have been updated with a Drive")
        let secondaryCachedDrive = await cache.getDrive(driveDbId: ObservableData.secondaryDriveDbId)
        #expect(secondaryCachedDrive == secondaryDrive, "The cache should have been updated with another Drive")

        // WHEN
        let expectedSynchro = ObservableData.expectedSynchro
        try await cache.addSynchro(expectedSynchro)
        let secondarySynchro = ObservableData.secondarySynchro
        try await cache.addSynchro(secondarySynchro)

        // THEN
        _ = await receivedValues.dropFirst().first(where: { _ in true })

        let fetchedSynchro = await cache.getSynchro(synchroDbId: ObservableData.expectedSynchroDbId)
        #expect(fetchedSynchro == ObservableData.expectedSynchro, "We should find the object in cache")
        #expect(observedSynchros.count == 2, "We should have two synchros in the array")

        guard let firstObservedSynchro = observedSynchros.first(where: { $0.synchro.dbId == ObservableData.expectedSynchroDbId })
        else {
            Issue.record("Failed to unwrap the first synchro")
            return
        }

        #expect(firstObservedSynchro.synchro == expectedSynchro, "The synchro should match")
        #expect(firstObservedSynchro.drive.driveDbId == expectedDrive.driveDbId, "The drive should match")
        #expect(firstObservedSynchro.account.dbId == ObservableData.expectedAccount.dbId, "The account should match")
        #expect(firstObservedSynchro.user.dbId == ObservableData.expectedUser.dbId, "The user should match")

        guard let secondaryObservedSynchro = observedSynchros
            .first(where: { $0.synchro.dbId == ObservableData.secondarySynchroDbId }) else {
            Issue.record("Failed to unwrap the secondary synchro")
            return
        }

        #expect(firstObservedSynchro.synchro != secondaryObservedSynchro.synchro, "The two synchros should not match")
        #expect(secondaryObservedSynchro.synchro == secondarySynchro, "The synchro should match")
        #expect(secondaryObservedSynchro.drive.driveDbId == secondaryDrive.driveDbId, "The drive should match")
        #expect(secondaryObservedSynchro.account.dbId == ObservableData.expectedAccount.dbId, "The account should match")
        #expect(secondaryObservedSynchro.user.dbId == ObservableData.expectedUser.dbId, "The user should match")
    }

    @Test(.timeLimit(.minutes(1)))
    func updateObservedSynchros() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedSynchros(cacheObservation: cache) var observedSynchros: [SynchroContext]
        let receivedValues = $observedSynchros.receivedValues

        #expect(observedSynchros == [], "Synchros should initially be empty")
        await cache.addUser(ObservableData.expectedUserWithAccounts)

        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        let cachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        #expect(cachedDrive == expectedDrive, "The cache should have been updated with a Drive")

        let expectedSynchro = ObservableData.expectedSynchro
        try await cache.addSynchro(expectedSynchro)

        let fetchedSynchro = await cache.getSynchro(synchroDbId: ObservableData.expectedSynchroDbId)
        #expect(fetchedSynchro == ObservableData.expectedSynchro, "We should find the object in cache")

        // WHEN
        let updatedSynchro = ObservableData.updatedSynchro
        try await cache.updateSynchro(updatedSynchro)

        // THEN
        _ = await receivedValues.dropFirst().first(where: { _ in true })

        let latestFetchedSynchro = await cache.getSynchro(synchroDbId: ObservableData.expectedSynchroDbId)
        #expect(latestFetchedSynchro == ObservableData.updatedSynchro, "We should find the object in cache updated")
        #expect(observedSynchros.count == 1, "We should have one synchro in the array")

        guard let firstSynchro = observedSynchros.first else {
            Issue.record("Failed to unwrap the first synchro")
            return
        }

        #expect(firstSynchro.synchro != expectedSynchro, "The synchro in the array should not match the old one")
        #expect(firstSynchro.synchro == updatedSynchro, "The synchro in the array should match the updated one")
        #expect(firstSynchro.drive.driveDbId == expectedDrive.driveDbId, "The drive should match the one we just added")
        #expect(
            firstSynchro.account.dbId == ObservableData.expectedAccount.dbId,
            "The account should match the one we just added"
        )
        #expect(firstSynchro.user.dbId == ObservableData.expectedUser.dbId, "The user should match the one we just added")
    }

    @Test(.timeLimit(.minutes(1)))
    func deleteObservedSynchros() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedSynchros(cacheObservation: cache) var observedSynchros: [SynchroContext]
        let receivedValues = $observedSynchros.receivedValues

        #expect(observedSynchros == [], "Synchros should initially be empty")
        await cache.addUser(ObservableData.expectedUserWithAccounts)

        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        let cachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        #expect(cachedDrive == expectedDrive, "The cache should have been updated with a Drive")

        let expectedSynchro = ObservableData.expectedSynchro
        try await cache.addSynchro(expectedSynchro)

        let fetchedSynchro = await cache.getSynchro(synchroDbId: ObservableData.expectedSynchroDbId)
        #expect(fetchedSynchro == ObservableData.expectedSynchro, "We should find the object in cache")

        // WHEN
        try await cache.removeSynchro(synchroDbId: expectedSynchro.dbId,
                                      driveDbId: expectedSynchro.driveDbId)

        // THEN
        _ = await receivedValues.dropFirst().first(where: { $0 == [] })

        let latestFetchedSynchro = await cache.getSynchro(synchroDbId: ObservableData.expectedSynchroDbId)
        #expect(latestFetchedSynchro == nil, "Object should no longer be available in cache")
        #expect(observedSynchros.isEmpty, "Synchros should be empty once the object is deleted")
    }

    @Test(.timeLimit(.minutes(1)))
    func testRemoveDrive() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedSynchros(cacheObservation: cache) var observedSynchros: [SynchroContext]
        let receivedValues = $observedSynchros.receivedValues // Start to save the received values

        #expect(observedSynchros == [], "Synchros should initially be empty")
        await cache.addUser(ObservableData.expectedUserWithAccounts)

        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        let secondaryDrive = ObservableData.secondaryDrive
        try await cache.addDrive(secondaryDrive, accountDbId: ObservableData.expectedAccountDbId)

        let cachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        #expect(cachedDrive == expectedDrive, "The cache should have been updated with a Drive")
        let secondaryCachedDrive = await cache.getDrive(driveDbId: ObservableData.secondaryDriveDbId)
        #expect(secondaryCachedDrive == secondaryDrive, "The cache should have been updated with another Drive")

        let expectedSynchro = ObservableData.expectedSynchro
        try await cache.addSynchro(expectedSynchro)
        let secondarySynchro = ObservableData.secondarySynchro
        try await cache.addSynchro(secondarySynchro)

        let fetchedSynchro = await cache.getSynchro(synchroDbId: ObservableData.expectedSynchroDbId)
        #expect(fetchedSynchro == ObservableData.expectedSynchro, "We should find the object in cache")
        let secondaryFetchedSynchro = await cache.getSynchro(synchroDbId: ObservableData.secondarySynchroDbId)
        #expect(secondaryFetchedSynchro == ObservableData.secondarySynchro, "We should find the object in cache")

        // WHEN
        try await cache.removeDrive(driveDbId: ObservableData.secondaryDriveDbId)

        // THEN
        _ = await receivedValues.dropFirst().dropFirst().first(where: { _ in true })

        #expect(observedSynchros.count == 1, "We should have one synchro in one drive after the deletion of a drive")

        guard let firstObservedSynchro = observedSynchros.first(where: { $0.synchro.dbId == ObservableData.expectedSynchroDbId })
        else {
            Issue.record("Failed to unwrap the first synchro")
            return
        }

        #expect(firstObservedSynchro.synchro == expectedSynchro, "The synchro should match")
        #expect(firstObservedSynchro.drive.driveDbId == expectedDrive.driveDbId, "The drive should match")
        #expect(firstObservedSynchro.account.dbId == ObservableData.expectedAccount.dbId, "The account should match")
        #expect(firstObservedSynchro.user.dbId == ObservableData.expectedUser.dbId, "The user should match")
    }
}
