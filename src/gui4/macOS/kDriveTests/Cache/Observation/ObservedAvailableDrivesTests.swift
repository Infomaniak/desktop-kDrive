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

extension ObservedAvailableDrives {
    var receivedValues: AsyncStream<[AvailableDriveContext]> {
        AsyncStream { continuation in
            let cancellable = $wrappedValue
                .sink { value in continuation.yield(value) }

            continuation.onTermination = { _ in cancellable.cancel() }
        }
    }
}

/* FIXME: - Assert bug on CI server
 @MainActor
 struct ObservedAvailableDrivesTests {
     @Test(.timeLimit(.minutes(1)))
     func setObservedAvailableDrives() async throws {
         // GIVEN
         let cache = ServerCoherentCache()
         let initialUser = await cache.getUser(dbId: CacheData.expectedUserDbId)
         #expect(initialUser == nil, "Cache should initially be empty")

         @ObservedAvailableDrives(cacheObservation: cache) var observedDrives: [AvailableDriveContext]
         let receivedValues = $observedDrives.receivedValues // Start to save the received values

         #expect(observedDrives == [], "Available drives should initially be empty")
         await cache.addUser(CacheData.expectedUser)

         // WHEN
         let expectedDrive = CacheData.expectedAvailableDrive
         try await cache.updateAvailableDrives([expectedDrive], forUserDbId: CacheData.expectedUserDbId)

         // THEN
         _ = await receivedValues.dropFirst().first(where: { _ in true })

         let cachedDrive = await cache.getAvailableDrive(
             driveDb: CacheData.expectedAvailableDriveId,
             userDbId: CacheData.expectedUserDbId
         )
         #expect(cachedDrive == expectedDrive, "The cache should have been updated with an AvailableDrive")
         #expect(observedDrives.count == 1, "We should have one AvailableDrive in the array")

         guard let firstDrive = observedDrives.first else {
             Issue.record("Failed to unwrap the first AvailableDrive")
             return
         }

         #expect(firstDrive.availableDrive.driveId == expectedDrive.driveId, "The drive should match the one we just added")
         #expect(firstDrive.user.dbId == CacheData.expectedUserDbId, "The user should match the one we just added")
     }

     @Test(.timeLimit(.minutes(1)))
     func setMultipleObservedAvailableDrives() async throws {
         // GIVEN
         let cache = ServerCoherentCache()
         let initialUser = await cache.getUser(dbId: CacheData.expectedUserDbId)
         #expect(initialUser == nil, "Cache should initially be empty")

         @ObservedAvailableDrives(cacheObservation: cache) var observedDrives: [AvailableDriveContext]
         let receivedValues = $observedDrives.receivedValues // Start to save the received values

         #expect(observedDrives == [], "Observed available drives should initially be empty")
         await cache.addUser(CacheData.expectedUser)

         // WHEN
         let expectedDrive = CacheData.expectedAvailableDrive
         try await cache.updateAvailableDrives([expectedDrive], forUserDbId: CacheData.expectedUserDbId)

         let secondaryDrive = CacheData.secondAvailableDrive
         await cache.addUser(CacheData.secondUser)
         try await cache.updateAvailableDrives([secondaryDrive], forUserDbId: CacheData.secondUserDbId)

         // THEN
         _ = await receivedValues.dropFirst().first(where: { _ in true })

         let cachedDrive = await cache.getAvailableDrive(
             driveDb: CacheData.expectedAvailableDriveId,
             userDbId: CacheData.expectedUserDbId
         )
         #expect(cachedDrive == expectedDrive, "The cache should have been updated with an AvailableDrive")
         let secondaryCachedDrive = await cache.getAvailableDrive(
             driveDb: CacheData.secondAvailableDriveId,
             userDbId: CacheData.secondUserDbId
         )
         #expect(secondaryCachedDrive == secondaryDrive, "The cache should have been updated with another AvailableDrive")
         #expect(observedDrives.count == 2, "We should have two AvailableDrives in the array")

         guard let firstObservedDrive = observedDrives
             .first(where: { $0.availableDrive.driveId == CacheData.expectedAvailableDriveId }) else {
             Issue.record("Failed to unwrap the first AvailableDrive")
             return
         }

         #expect(firstObservedDrive.availableDrive.driveId == expectedDrive.driveId, "The drive should match")
         #expect(firstObservedDrive.user.dbId == CacheData.expectedUserDbId, "The user should match")

         guard let secondaryObservedDrive = observedDrives
             .first(where: { $0.availableDrive.driveId == CacheData.secondAvailableDriveId }) else {
             Issue.record("Failed to unwrap the secondary AvailableDrive")
             return
         }

         #expect(firstObservedDrive.availableDrive != secondaryObservedDrive.availableDrive, "The two drives should not match")
         #expect(secondaryObservedDrive.availableDrive.driveId == secondaryDrive.driveId, "The drive should match")
         #expect(secondaryObservedDrive.user.dbId == CacheData.secondUserDbId, "The user should match")
     }

     @Test(.timeLimit(.minutes(1)))
     func updateObservedAvailableDrive() async throws {
         // GIVEN
         let cache = ServerCoherentCache()
         let initialUser = await cache.getUser(dbId: CacheData.expectedUserDbId)
         #expect(initialUser == nil, "Cache should initially be empty")

         @ObservedAvailableDrives(cacheObservation: cache) var observedDrives: [AvailableDriveContext]
         let receivedValues = $observedDrives.receivedValues // Start to save the received values

         #expect(observedDrives == [], "Available drives should initially be empty")
         await cache.addUser(CacheData.expectedUser)

         let expectedDrive = CacheData.expectedAvailableDrive
         try await cache.updateAvailableDrives([expectedDrive], forUserDbId: CacheData.expectedUserDbId)

         let cachedDrive = await cache.getAvailableDrive(
             driveDb: CacheData.expectedAvailableDriveId,
             userDbId: CacheData.expectedUserDbId
         )
         #expect(cachedDrive == expectedDrive, "The cache should have been updated with an AvailableDrive")

         // WHEN
         let updatedDrive = try AvailableDrive(
             driveId: CacheData.expectedAvailableDriveId,
             accountId: CacheData.expectedAccountId,
             userDbId: CacheData.expectedUserDbId,
             userId: CacheData.expectedUserAPIId,
             name: "Updated Available Drive",
             color: #require(HexColor(hex: "00ff00"))
         )
         try await cache.updateAvailableDrives([updatedDrive], forUserDbId: CacheData.expectedUserDbId)

         // THEN
         _ = await receivedValues.dropFirst().first(where: { _ in true })

         let latestFetchedDrive = await cache.getAvailableDrive(
             driveDb: CacheData.expectedAvailableDriveId,
             userDbId: CacheData.expectedUserDbId
         )
         #expect(latestFetchedDrive?.name == "Updated Available Drive", "We should find the object in cache updated")
         #expect(observedDrives.count == 1, "We should have one object in the array")

         guard let firstDrive = observedDrives.first else {
             Issue.record("Failed to unwrap the first object")
             return
         }

         #expect(
             firstDrive.availableDrive.name == "Updated Available Drive",
             "The AvailableDrive in the array should match the updated one"
         )
         #expect(firstDrive.availableDrive.driveId == expectedDrive.driveId, "The drive should match the one we just added")
         #expect(firstDrive.user.dbId == CacheData.expectedUserDbId, "The user should match the one we just added")
     }

     @Test(.timeLimit(.minutes(1)))
     func deleteObservedAvailableDrive() async throws {
         // GIVEN
         let cache = ServerCoherentCache()
         let initialUser = await cache.getUser(dbId: CacheData.expectedUserDbId)
         #expect(initialUser == nil, "Cache should initially be empty")

         @ObservedAvailableDrives(cacheObservation: cache) var observedDrives: [AvailableDriveContext]
         let receivedValues = $observedDrives.receivedValues // Start to save the received values

         #expect(observedDrives == [], "Available drives should initially be empty")
         await cache.addUser(CacheData.expectedUser)

         let expectedDrive = CacheData.expectedAvailableDrive
         try await cache.updateAvailableDrives([expectedDrive], forUserDbId: CacheData.expectedUserDbId)

         let cachedDrive = await cache.getAvailableDrive(
             driveDb: CacheData.expectedAvailableDriveId,
             userDbId: CacheData.expectedUserDbId
         )
         #expect(cachedDrive == expectedDrive, "The cache should have been updated with an AvailableDrive")

         // WHEN - Update with empty array to remove the drive
         try await cache.updateAvailableDrives([], forUserDbId: CacheData.expectedUserDbId)

         // THEN
         _ = await receivedValues.dropFirst().first(where: { $0 == [] })

         let latestFetchedDrive = await cache.getAvailableDrive(
             driveDb: CacheData.expectedAvailableDriveId,
             userDbId: CacheData.expectedUserDbId
         )
         #expect(latestFetchedDrive == nil, "Object should no longer be available in cache")
         #expect(observedDrives.isEmpty, "Available drives should be empty once the object is deleted")
     }

     @Test(.timeLimit(.minutes(1)))
     func removeOneAvailableDrive() async throws {
         // GIVEN
         let cache = ServerCoherentCache()
         let initialUser = await cache.getUser(dbId: CacheData.expectedUserDbId)
         #expect(initialUser == nil, "Cache should initially be empty")

         @ObservedAvailableDrives(cacheObservation: cache) var observedDrives: [AvailableDriveContext]
         let receivedValues = $observedDrives.receivedValues // Start to save the received values

         #expect(observedDrives == [], "Available drives should initially be empty")
         await cache.addUser(CacheData.expectedUser)

         let expectedDrive = CacheData.expectedAvailableDrive
         try await cache.updateAvailableDrives([expectedDrive], forUserDbId: CacheData.expectedUserDbId)

         await cache.addUser(CacheData.secondUser)
         let secondaryDrive = CacheData.secondAvailableDrive
         try await cache.updateAvailableDrives([secondaryDrive], forUserDbId: CacheData.secondUserDbId)

         let cachedDrive = await cache.getAvailableDrive(
             driveDb: CacheData.expectedAvailableDriveId,
             userDbId: CacheData.expectedUserDbId
         )
         #expect(cachedDrive == expectedDrive, "The cache should have been updated with an AvailableDrive")
         let secondaryCachedDrive = await cache.getAvailableDrive(
             driveDb: CacheData.secondAvailableDriveId,
             userDbId: CacheData.secondUserDbId
         )
         #expect(secondaryCachedDrive == secondaryDrive, "The cache should have been updated with another AvailableDrive")

         // WHEN - Remove only the secondary drive
         try await cache.updateAvailableDrives([], forUserDbId: CacheData.secondUserDbId)

         // THEN
         _ = await receivedValues.dropFirst().dropFirst().first(where: { _ in true })

         #expect(observedDrives.count == 1, "We should have one AvailableDrive after the deletion of a drive")

         guard let firstObservedDrive = observedDrives
             .first(where: { $0.availableDrive.driveId == CacheData.expectedAvailableDriveId }) else {
             Issue.record("Failed to unwrap the first AvailableDrive")
             return
         }

         #expect(firstObservedDrive.availableDrive.driveId == expectedDrive.driveId, "The drive should match")
         #expect(firstObservedDrive.user.dbId == CacheData.expectedUserDbId, "The user should match")
     }
 }
 */
