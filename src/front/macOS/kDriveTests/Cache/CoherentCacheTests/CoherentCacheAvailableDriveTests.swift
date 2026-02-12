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
@testable import kDriveCore
import OrderedCollections
import Testing

struct CoherentCacheAvailableDriveTests {
    @Test(.timeLimit(.minutes(1)))
    func getAvailableDriveWithUserDbId() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)
        
        // Add available drives to the user
        let availableDrives = [CacheData.expectedAvailableDrive]
        try await cache.updateAvailableDrives(availableDrives, forUserDbId: CacheData.expectedUserDbId)

        // WHEN
        let fetchedDrive = await cache.getAvailableDrive(
            driveDb: CacheData.expectedAvailableDriveId,
            userDbId: CacheData.expectedUserDbId
        )

        // THEN
        #expect(fetchedDrive == CacheData.expectedAvailableDrive)
    }

    @Test(.timeLimit(.minutes(1)))
    func getAvailableDriveWithUserDbIdNotFound() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)
        
        // Add available drives to the user
        let availableDrives = [CacheData.expectedAvailableDrive]
        try await cache.updateAvailableDrives(availableDrives, forUserDbId: CacheData.expectedUserDbId)

        // WHEN - Try to fetch with wrong driveDbId
        let fetchedDrive = await cache.getAvailableDrive(
            driveDb: Int32.random(in: 10001 ... 20000),
            userDbId: CacheData.expectedUserDbId
        )

        // THEN
        #expect(fetchedDrive == nil)
    }

    @Test(.timeLimit(.minutes(1)))
    func getAvailableDriveWithUserDbIdWrongUser() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)
        
        // Add available drives to the user
        let availableDrives = [CacheData.expectedAvailableDrive]
        try await cache.updateAvailableDrives(availableDrives, forUserDbId: CacheData.expectedUserDbId)

        // WHEN - Try to fetch with wrong userDbId
        let fetchedDrive = await cache.getAvailableDrive(
            driveDb: CacheData.expectedAvailableDriveId,
            userDbId: Int32.random(in: 10001 ... 20000)
        )

        // THEN
        #expect(fetchedDrive == nil)
    }

    @Test(.timeLimit(.minutes(1)))
    func getAvailableDriveWithoutUserDbId() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)
        
        // Add available drives to the user
        let availableDrives = [CacheData.expectedAvailableDrive]
        try await cache.updateAvailableDrives(availableDrives, forUserDbId: CacheData.expectedUserDbId)

        // WHEN - Fetch without specifying userDbId
        let fetchedDrive = await cache.getAvailableDrive(driveDb: CacheData.expectedAvailableDriveId)

        // THEN
        #expect(fetchedDrive == CacheData.expectedAvailableDrive)
    }

    @Test(.timeLimit(.minutes(1)))
    func getAvailableDriveWithoutUserDbIdNotFound() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)
        
        // Add available drives to the user
        let availableDrives = [CacheData.expectedAvailableDrive]
        try await cache.updateAvailableDrives(availableDrives, forUserDbId: CacheData.expectedUserDbId)

        // WHEN - Try to fetch with wrong driveDbId
        let fetchedDrive = await cache.getAvailableDrive(driveDb: Int32.random(in: 10001 ... 20000))

        // THEN
        #expect(fetchedDrive == nil)
    }

    @Test(.timeLimit(.minutes(1)))
    func getAvailableDriveWithoutUserDbIdAcrossMultipleUsers() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        
        // Add first user with available drive
        let firstUser = CacheData.expectedUser
        await cache.addUser(firstUser)
        let firstAvailableDrives = [CacheData.expectedAvailableDrive]
        try await cache.updateAvailableDrives(firstAvailableDrives, forUserDbId: CacheData.expectedUserDbId)
        
        // Add second user with available drive
        let secondUser = CacheData.secondUser
        await cache.addUser(secondUser)
        let secondAvailableDrives = [CacheData.secondAvailableDrive]
        try await cache.updateAvailableDrives(secondAvailableDrives, forUserDbId: CacheData.secondUserDbId)

        // WHEN - Fetch first drive without specifying userDbId
        let fetchedFirstDrive = await cache.getAvailableDrive(driveDb: CacheData.expectedAvailableDriveId)
        
        // WHEN - Fetch second drive without specifying userDbId
        let fetchedSecondDrive = await cache.getAvailableDrive(driveDb: CacheData.secondAvailableDriveId)

        // THEN
        #expect(fetchedFirstDrive == CacheData.expectedAvailableDrive)
        #expect(fetchedSecondDrive == CacheData.secondAvailableDrive)
    }

    @Test(.timeLimit(.minutes(1)))
    func getAvailableDriveWithUserDbIdIsolation() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        
        // Add first user with available drive
        let firstUser = CacheData.expectedUser
        await cache.addUser(firstUser)
        let firstAvailableDrives = [CacheData.expectedAvailableDrive]
        try await cache.updateAvailableDrives(firstAvailableDrives, forUserDbId: CacheData.expectedUserDbId)
        
        // Add second user with available drive
        let secondUser = CacheData.secondUser
        await cache.addUser(secondUser)
        let secondAvailableDrives = [CacheData.secondAvailableDrive]
        try await cache.updateAvailableDrives(secondAvailableDrives, forUserDbId: CacheData.secondUserDbId)

        // WHEN - Try to fetch second user's drive using first user's ID
        let fetchedDrive = await cache.getAvailableDrive(
            driveDb: CacheData.secondAvailableDriveId,
            userDbId: CacheData.expectedUserDbId
        )

        // THEN - Should be nil because the drive doesn't belong to the first user
        #expect(fetchedDrive == nil)
    }
}
