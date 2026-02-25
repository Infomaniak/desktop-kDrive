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

struct CoherentCacheErrorTests {
    // MARK: Login Error

    @Test(.timeLimit(.minutes(1)))
    func setLoginErrorInSynchro() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        #expect(await cache.getAccount(accountDbId: CacheData.expectedAccountDbId,
                                       userDbId: CacheData.expectedUserDbId) == CacheData.expectedAccount)
        try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)
        #expect(await cache.getDrive(driveDbId: CacheData.expectedDriveDbId) == CacheData.expectedDrive)
        try await cache.addSynchro(CacheData.expectedSynchro)
        #expect(await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId) == CacheData.expectedSynchro)

        // WHEN
        try await cache.addOrUpdateError(CacheData.expectedLoginError)

        // THEN
        guard let fetchedSynchro = await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId) else {
            Issue.record("Synchro not found")
            return
        }

        #expect(fetchedSynchro.dbId == CacheData.expectedSynchroDbId)
        #expect(fetchedSynchro.latestError == .loggingError)
        #expect(fetchedSynchro.errors.values.first == CacheData.expectedLoginError)
        #expect(await cache.serverErrors.count == 0)
    }

    @Test(.timeLimit(.minutes(1)))
    func keepFirstErrorInSynchro() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        #expect(await cache.getAccount(accountDbId: CacheData.expectedAccountDbId,
                                       userDbId: CacheData.expectedUserDbId) == CacheData.expectedAccount)
        try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)
        #expect(await cache.getDrive(driveDbId: CacheData.expectedDriveDbId) == CacheData.expectedDrive)
        try await cache.addSynchro(CacheData.expectedSynchro)
        #expect(await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId) == CacheData.expectedSynchro)

        // WHEN
        try await cache.addOrUpdateError(CacheData.expectedAsleepError)
        try await cache.addOrUpdateError(CacheData.expectedLoginError)

        // THEN
        guard let fetchedSynchro = await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId) else {
            Issue.record("Synchro not found")
            return
        }

        #expect(fetchedSynchro.dbId == CacheData.expectedSynchroDbId)
        #expect(fetchedSynchro.latestError == .asleep, "Expecting the first error set to stick until resolution")
        #expect(fetchedSynchro.errors.values.first == CacheData.expectedAsleepError)
        #expect(fetchedSynchro.errors.count == 2)
        #expect(await cache.serverErrors.count == 0)
    }

    @Test(.timeLimit(.minutes(1)))
    func resolveOneOfManyErrorInSynchro() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        #expect(await cache.getAccount(accountDbId: CacheData.expectedAccountDbId,
                                       userDbId: CacheData.expectedUserDbId) == CacheData.expectedAccount)
        try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)
        #expect(await cache.getDrive(driveDbId: CacheData.expectedDriveDbId) == CacheData.expectedDrive)
        try await cache.addSynchro(CacheData.expectedSynchro)
        #expect(await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId) == CacheData.expectedSynchro)

        try await cache.addOrUpdateError(CacheData.expectedAsleepError)
        try await cache.addOrUpdateError(CacheData.expectedLoginError)
        #expect(await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId)?.errors.count == 2)
        #expect(await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId)?.latestError == .asleep)

        // WHEN
        try await cache.removeError(CacheData.expectedAsleepErrorDbId)

        // THEN
        guard let fetchedSynchro = await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId) else {
            Issue.record("Synchro not found")
            return
        }

        #expect(fetchedSynchro.dbId == CacheData.expectedSynchroDbId)
        #expect(fetchedSynchro.latestError == .loggingError,
                "Expecting the remaining error to be set as the latest on resolution")
        #expect(fetchedSynchro.errors.values.first == CacheData.expectedLoginError)
        #expect(fetchedSynchro.errors.count == 1)
        #expect(await cache.serverErrors.count == 0)
    }

    @Test(.timeLimit(.minutes(1)))
    func updateLoginErrorInSynchro() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        #expect(await cache.getAccount(accountDbId: CacheData.expectedAccountDbId,
                                       userDbId: CacheData.expectedUserDbId) == CacheData.expectedAccount)
        try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)
        #expect(await cache.getDrive(driveDbId: CacheData.expectedDriveDbId) == CacheData.expectedDrive)
        try await cache.addSynchro(CacheData.expectedSynchro)
        #expect(await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId) == CacheData.expectedSynchro)

        try await cache.addOrUpdateError(CacheData.expectedLoginError)
        #expect(await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId)?.latestError == .loggingError)

        // WHEN
        try await cache.addOrUpdateError(CacheData.updatedLoginError)

        // THEN
        guard let fetchedSynchro = await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId) else {
            Issue.record("Synchro not found")
            return
        }

        #expect(fetchedSynchro.dbId == CacheData.expectedSynchroDbId)
        #expect(fetchedSynchro.latestError == .loggingError)
        #expect(fetchedSynchro.errors.values.first != CacheData.expectedLoginError)
        #expect(fetchedSynchro.errors.values.first == CacheData.updatedLoginError)
        #expect(fetchedSynchro.errors.values.count == 1)
        #expect(await cache.serverErrors.count == 0)
    }

    @Test(.timeLimit(.minutes(1)))
    func removeErrorInSynchro() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        #expect(await cache.getAccount(accountDbId: CacheData.expectedAccountDbId,
                                       userDbId: CacheData.expectedUserDbId) == CacheData.expectedAccount)
        try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)
        #expect(await cache.getDrive(driveDbId: CacheData.expectedDriveDbId) == CacheData.expectedDrive)
        try await cache.addSynchro(CacheData.expectedSynchro)
        #expect(await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId) == CacheData.expectedSynchro)

        try await cache.addOrUpdateError(CacheData.expectedLoginError)
        #expect(await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId)?.latestError == .loggingError)

        // WHEN
        try await cache.removeError(CacheData.expectedLoginErrorDbId)

        // THEN
        guard let fetchedSynchro = await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId) else {
            Issue.record("Synchro not found")
            return
        }

        #expect(fetchedSynchro.dbId == CacheData.expectedSynchroDbId)
        #expect(fetchedSynchro.latestError == nil)
        #expect(fetchedSynchro.errors.values.isEmpty == true)
        #expect(await cache.serverErrors.count == 0)
    }

    // MARK: Server Error

    @Test(.timeLimit(.minutes(1)))
    func setServerErrorInSynchro() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        #expect(await cache.getAccount(accountDbId: CacheData.expectedAccountDbId,
                                       userDbId: CacheData.expectedUserDbId) == CacheData.expectedAccount)
        try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)
        #expect(await cache.getDrive(driveDbId: CacheData.expectedDriveDbId) == CacheData.expectedDrive)
        try await cache.addSynchro(CacheData.expectedSynchro)
        #expect(await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId) == CacheData.expectedSynchro)

        // WHEN
        try await cache.addOrUpdateError(CacheData.expectedServerError)

        // THEN
        #expect(await cache.serverErrors.count == 1)
        #expect(await cache.serverErrors.values.first == CacheData.expectedServerError)
        #expect(await cache.serverErrors[CacheData.expectedServerErrorDbId] == CacheData.expectedServerError)

        guard let fetchedSynchro = await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId) else {
            Issue.record("Synchro not found")
            return
        }

        #expect(fetchedSynchro.dbId == CacheData.expectedSynchroDbId)
        #expect(fetchedSynchro.latestError == nil)
        #expect(fetchedSynchro.errors.values.count == 0)
    }

    @Test(.timeLimit(.minutes(1)))
    func updateServerErrorInSynchro() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        #expect(await cache.getAccount(accountDbId: CacheData.expectedAccountDbId,
                                       userDbId: CacheData.expectedUserDbId) == CacheData.expectedAccount)
        try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)
        #expect(await cache.getDrive(driveDbId: CacheData.expectedDriveDbId) == CacheData.expectedDrive)
        try await cache.addSynchro(CacheData.expectedSynchro)
        #expect(await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId) == CacheData.expectedSynchro)

        try await cache.addOrUpdateError(CacheData.expectedServerError)
        #expect(await cache.serverErrors.count == 1)

        // WHEN
        try await cache.addOrUpdateError(CacheData.updatedServerError)

        // THEN
        #expect(await cache.serverErrors.count == 1)
        #expect(await cache.serverErrors.values.first == CacheData.updatedServerError)
        #expect(await cache.serverErrors.values.first != CacheData.expectedServerError)
        #expect(await cache.serverErrors[CacheData.expectedServerErrorDbId] == CacheData.updatedServerError)
        #expect(await cache.serverErrors[CacheData.expectedServerErrorDbId] != CacheData.expectedServerError)

        guard let fetchedSynchro = await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId) else {
            Issue.record("Synchro not found")
            return
        }

        #expect(fetchedSynchro.dbId == CacheData.expectedSynchroDbId)
        #expect(fetchedSynchro.latestError == nil)
        #expect(fetchedSynchro.errors.values.count == 0)
    }

    @Test(.timeLimit(.minutes(1)))
    func removeServerErrorInSynchro() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        #expect(await cache.getAccount(accountDbId: CacheData.expectedAccountDbId,
                                       userDbId: CacheData.expectedUserDbId) == CacheData.expectedAccount)
        try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)
        #expect(await cache.getDrive(driveDbId: CacheData.expectedDriveDbId) == CacheData.expectedDrive)
        try await cache.addSynchro(CacheData.expectedSynchro)
        #expect(await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId) == CacheData.expectedSynchro)

        try await cache.addOrUpdateError(CacheData.expectedServerError)
        #expect(await cache.serverErrors.count == 1)

        // WHEN
        try await cache.removeError(CacheData.expectedServerErrorDbId)

        // THEN
        #expect(await cache.serverErrors.count == 0)

        guard let fetchedSynchro = await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId) else {
            Issue.record("Synchro not found")
            return
        }

        #expect(fetchedSynchro.dbId == CacheData.expectedSynchroDbId)
        #expect(fetchedSynchro.latestError == nil)
        #expect(fetchedSynchro.errors.values.count == 0)
    }

    // MARK: Remove all Errors

    @Test(.timeLimit(.minutes(1)))
    func removeAllErrorsInSynchro() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        #expect(await cache.getAccount(accountDbId: CacheData.expectedAccountDbId,
                                       userDbId: CacheData.expectedUserDbId) == CacheData.expectedAccount)
        try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)
        #expect(await cache.getDrive(driveDbId: CacheData.expectedDriveDbId) == CacheData.expectedDrive)
        try await cache.addSynchro(CacheData.expectedSynchro)
        #expect(await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId) == CacheData.expectedSynchro)

        try await cache.addOrUpdateError(CacheData.expectedLoginError)
        try await cache.addOrUpdateError(CacheData.expectedServerError)
        #expect(await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId)?.latestError == .loggingError)
        #expect(await cache.serverErrors.count == 1)

        // WHEN
        await cache.clearErrors()

        // THEN
        #expect(await cache.serverErrors.count == 0)

        guard let fetchedSynchro = await cache.getSynchro(synchroDbId: CacheData.expectedSynchroDbId) else {
            Issue.record("Synchro not found")
            return
        }

        #expect(fetchedSynchro.dbId == CacheData.expectedSynchroDbId)
        #expect(fetchedSynchro.latestError == nil)
        #expect(fetchedSynchro.errors.count == 0)
    }
}
