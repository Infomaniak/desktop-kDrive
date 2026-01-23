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

struct CoherentCacheDriveTests {
    @Test(.timeLimit(.minutes(1)))
    func getDriveInCache() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)
        await cache.addAccount(CacheData.expectedAccount, userDbId: CacheData.expectedUserDbId)
        #expect(await cache
            .getAccount(accountDbId: CacheData.expectedAccountDbId, userDbId: CacheData.expectedUserDbId) == CacheData
            .expectedAccount)

        // WHEN
        try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)

        // THEN
        #expect(await cache.getDrive(driveDbId: CacheData.expectedDriveDbId) == CacheData.expectedDrive)
    }

    @Test(.timeLimit(.minutes(1)))
    func removeDriveInCacheFromDbId() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)
        await cache.addAccount(CacheData.expectedAccount, userDbId: CacheData.expectedUserDbId)
        #expect(await cache
            .getAccount(accountDbId: CacheData.expectedAccountDbId, userDbId: CacheData.expectedUserDbId) == CacheData
            .expectedAccount)
        try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)
        #expect(await cache.getDrive(driveDbId: CacheData.expectedDriveDbId) == CacheData.expectedDrive)

        // WHEN
        await cache.removeDrive(driveDbId: CacheData.expectedDriveDbId,
                                accountDbId: CacheData.expectedAccountDbId,
                                userDbId: CacheData.expectedUserDbId)

        // THEN
        #expect(await cache.getDrive(driveDbId: CacheData.expectedDriveDbId) == nil)
    }

    @Test(.timeLimit(.minutes(1)))
    func updateDriveInCache() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)
        await cache.addAccount(CacheData.expectedAccount, userDbId: CacheData.expectedUserDbId)
        #expect(await cache
            .getAccount(accountDbId: CacheData.expectedAccountDbId, userDbId: CacheData.expectedUserDbId) == CacheData
            .expectedAccount)
        try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)
        #expect(await cache.getDrive(driveDbId: CacheData.expectedDriveDbId) == CacheData.expectedDrive)

        // WHEN
        do {
            try await cache.updateDrive(drive: CacheData.updatedDrive)

            // THEN
            #expect(await cache.getDrive(driveDbId: CacheData.expectedDriveDbId) != CacheData.expectedDrive)
            #expect(await cache.getDrive(driveDbId: CacheData.expectedDriveDbId) == CacheData.updatedDrive)
        } catch {
            Issue.record("unexpected error: \(error)")
        }
    }
}
