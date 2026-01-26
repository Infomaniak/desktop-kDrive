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

struct CoherentCacheAccountTests {
    @Test(.timeLimit(.minutes(1)))
    func getAccountInCache() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        #expect(await cache.getUser(apiId: CacheData.expectedUserAPIId) == nil)
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)

        // WHEN
        await cache.addAccount(CacheData.expectedAccount, userDbId: CacheData.expectedUserDbId)

        // THEN
        #expect(await cache
            .getAccount(accountDbId: CacheData.expectedAccountDbId, userDbId: CacheData.expectedUserDbId) == CacheData
            .expectedAccount)
        #expect(await cache.getAccount(accountDbId: CacheData.expectedAccountDbId) == CacheData.expectedAccount)
    }

    @Test(.timeLimit(.minutes(1)))
    func removeAccountInCacheFromDbId() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        #expect(await cache.getUser(apiId: CacheData.expectedUserAPIId) == nil)
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == CacheData.expectedUser)
        #expect(await cache.getAccount(accountDbId: CacheData.expectedAccountDbId) == nil)
        await cache.addAccount(CacheData.expectedAccount, userDbId: CacheData.expectedUserDbId)
        #expect(await cache.getAccount(accountDbId: CacheData.expectedAccountDbId) == CacheData.expectedAccount)

        // WHEN
        await cache.removeAccount(accountDbId: CacheData.expectedAccountDbId)

        // THEN
        #expect(await cache.getAccount(accountDbId: CacheData.expectedAccountDbId) == nil)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == CacheData.expectedUser)
    }

    @Test(.timeLimit(.minutes(1)))
    func updateAccountInCache() async throws {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        #expect(await cache.getUser(apiId: CacheData.expectedUserAPIId) == nil)
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == CacheData.expectedUser)
        #expect(await cache.getAccount(accountDbId: CacheData.expectedAccountDbId) == nil)
        await cache.addAccount(CacheData.expectedAccount, userDbId: CacheData.expectedUserDbId)
        #expect(await cache.getAccount(accountDbId: CacheData.expectedAccountDbId) == CacheData.expectedAccount)

        // WHEN
        do {
            try await cache.updateAccount(CacheData.updatedAccount)

            // THEN
            guard let accountByDbId = await cache.getAccount(accountDbId: CacheData.expectedAccountDbId) else {
                Issue.record("We should be able to fetch an account from db id")
                return
            }

            #expect(accountByDbId != CacheData.expectedAccount)
            #expect(accountByDbId == CacheData.updatedAccount)
            #expect(accountByDbId.name == CacheData.updatedAccountName)
        } catch {
            Issue.record("unexpected error: \(error)")
        }
    }
}
