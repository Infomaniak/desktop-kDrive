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

import Foundation
@testable import kDriveCore
import OrderedCollections
import Testing

struct CoherentCacheSynchroTests {
    @Test(.timeLimit(.minutes(1)))
    func getSynchroInCache() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        #expect(await cache
            .getAccount(accountDbId: CacheData.expectedAccountDbId) == CacheData
            .expectedAccount)
        try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)
        #expect(await cache.getDrive(driveDbId: CacheData.expectedDriveDbId) == CacheData.expectedDrive)

        // WHEN
        try await cache.addSynchro(CacheData.expectedSynchro)

        // THEN
        #expect(await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId) == CacheData.expectedSynchro)
        #expect(await cache.getSynchro(
            synchroDbId: CacheData.expectedSynchroDbId,
            driveDbId: CacheData.expectedDriveDbId,
            accountDbId: CacheData.expectedAccountDbId,
            userDbId: CacheData.expectedUserDbId
        ) == CacheData.expectedSynchro)
    }

    @Test(.timeLimit(.minutes(1)))
    func removeSynchroInCacheFromDbId() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        #expect(await cache
            .getAccount(accountDbId: CacheData.expectedAccountDbId) == CacheData
            .expectedAccount)
        try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)
        #expect(await cache.getDrive(driveDbId: CacheData.expectedDriveDbId) == CacheData.expectedDrive)
        try await cache.addSynchro(CacheData.expectedSynchro)
        #expect(await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId) == CacheData.expectedSynchro)

        // WHEN
        try await cache.removeSynchro(
            synchroDbId: CacheData.expectedSynchroDbId,
            driveDbId: CacheData.expectedDriveDbId
        )

        // THEN
        #expect(await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId) == nil)
        #expect(await cache.getSynchro(
            synchroDbId: CacheData.expectedSynchroDbId,
            driveDbId: CacheData.expectedDriveDbId,
            accountDbId: CacheData.expectedAccountDbId,
            userDbId: CacheData.expectedUserDbId
        ) == nil)
    }

    @Test(.timeLimit(.minutes(1)))
    func updateSynchroInCache() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        #expect(await cache
            .getAccount(accountDbId: CacheData.expectedAccountDbId) == CacheData
            .expectedAccount)
        try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)
        #expect(await cache.getDrive(driveDbId: CacheData.expectedDriveDbId) == CacheData.expectedDrive)
        try await cache.addSynchro(CacheData.expectedSynchro)
        #expect(await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId) == CacheData.expectedSynchro)

        // WHEN
        do {
            try await cache.updateSynchro(CacheData.updatedSynchro)

            // THEN
            #expect(await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId) != CacheData.expectedSynchro)
            #expect(await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId) == CacheData.updatedSynchro)

        } catch {
            Issue.record("unexpected error: \(error)")
        }
    }

    @Test(.timeLimit(.minutes(1)))
    func vfsConversionStartedSetsUpdatingFlag() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        await cache.addUser(CacheData.expectedUser)
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)
        try await cache.addSynchro(CacheData.expectedSynchro)
        #expect(await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId)?.isUpdatingVfsMode == false)

        // WHEN
        try await cache.vfsConversionStarted(synchroDbId: CacheData.expectedSynchroDbId)

        // THEN
        #expect(await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId)?.isUpdatingVfsMode == true)
    }

    @Test(.timeLimit(.minutes(1)))
    func vfsConversionStartedThrowsWhenSynchroMissing() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let unknownSyncDbId = Int32.random(in: 0 ... 10000)

        // WHEN / THEN
        await #expect(throws: ServerCoherentCache.CacheError.self) {
            try await cache.vfsConversionStarted(synchroDbId: unknownSyncDbId)
        }
    }

    @Test(.timeLimit(.minutes(1)))
    func vfsConversionCompletedClearsUpdatingFlag() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        await cache.addUser(CacheData.expectedUser)
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)

        var convertingSynchro = CacheData.expectedSynchro
        convertingSynchro.isUpdatingVfsMode = true
        try await cache.addSynchro(convertingSynchro)
        #expect(await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId)?.isUpdatingVfsMode == true)

        // WHEN
        try await cache.vfsConversionCompleted(synchroDbId: CacheData.expectedSynchroDbId)

        // THEN
        #expect(await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId)?.isUpdatingVfsMode == false)
    }

    @Test(.timeLimit(.minutes(1)))
    func vfsConversionCompletedThrowsWhenSynchroMissing() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let unknownSyncDbId = Int32.random(in: 0 ... 10000)

        // WHEN / THEN
        await #expect(throws: ServerCoherentCache.CacheError.self) {
            try await cache.vfsConversionCompleted(synchroDbId: unknownSyncDbId)
        }
    }
}
