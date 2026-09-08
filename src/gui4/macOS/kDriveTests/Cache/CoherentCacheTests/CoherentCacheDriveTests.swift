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

struct CoherentCacheDriveTests {
    @Test(.timeLimit(.minutes(1)))
    func getDriveInCache() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        #expect(await cache
            .getAccount(accountDbId: CacheData.expectedAccountDbId) == CacheData
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
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        #expect(await cache
            .getAccount(accountDbId: CacheData.expectedAccountDbId) == CacheData
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
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        #expect(await cache
            .getAccount(accountDbId: CacheData.expectedAccountDbId) == CacheData
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

    @Test(.timeLimit(.minutes(1)))
    func updateDriveSignalMovesDriveToNewAccount() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let newAccountDbId = CacheData.expectedAccountDbId + 1
        let newAccount = Account(dbId: newAccountDbId,
                                 userDbId: CacheData.expectedUserDbId,
                                 name: "newAccount",
                                 drives: [:])
        var driveWithSynchro = CacheData.expectedDrive
        driveWithSynchro.synchros[CacheData.expectedSynchroDbId] = CacheData.expectedSynchro

        await cache.addUser(CacheData.expectedUser)
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        try await cache.addOrUpdateAccount(newAccount)
        try await cache.addDrive(driveWithSynchro, accountDbId: CacheData.expectedAccountDbId)

        let driveSignal = DriveInfoSignalMetadata(dbId: CacheData.expectedDriveDbId,
                                                  id: CacheData.updatedDriveId,
                                                  accountDbId: newAccountDbId,
                                                  name: CacheData.updatedDriveName,
                                                  color: CacheData.updatedDriveColor,
                                                  notifications: true,
                                                  admin: true,
                                                  maintenance: false,
                                                  locked: false,
                                                  accessDenied: false)

        // WHEN
        try await cache.addOrUpdateDriveSignal(driveSignal)

        // THEN
        let oldAccountDrive = await cache.getDrive(driveDbId: CacheData.expectedDriveDbId,
                                                   accountDbId: CacheData.expectedAccountDbId,
                                                   userDbId: CacheData.expectedUserDbId)
        let newAccountDrive = await cache.getDrive(driveDbId: CacheData.expectedDriveDbId,
                                                   accountDbId: newAccountDbId,
                                                   userDbId: CacheData.expectedUserDbId)

        #expect(oldAccountDrive == nil)
        #expect(newAccountDrive?.accountDbId == newAccountDbId)
        #expect(newAccountDrive?.name == CacheData.updatedDriveName)
        #expect(newAccountDrive?.synchros[CacheData.expectedSynchroDbId] == CacheData.expectedSynchro)
    }
}
