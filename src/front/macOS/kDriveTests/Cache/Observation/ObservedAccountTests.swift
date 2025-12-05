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
@testable import kDriveCore
import Testing

@MainActor
struct ObservedAccountTests_dbIdOnly {
    @Test func setObservedAccount() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedAccount(accountDbId: ObservableData.expectedAccountDbId,
                         cacheObservation: cache) var observedAccount: Account?
        let receivedValues = $observedAccount.receivedValues // Start to save the received values
        #expect(observedAccount == nil, "Account should initially be nil")

        // WHEN
        let expectedUser = ObservableData.expectedUserWithAccounts
        await cache.addUser(expectedUser)

        // THEN
        let lastReceivedObject = await receivedValues.first(where: { $0 != nil })
        #expect(lastReceivedObject == ObservableData.expectedAccount)

        let cachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        let cachedAccount = await cache.getAccount(accountDbId: ObservableData.expectedAccountDbId, userDbId: ObservableData.expectedUserDbId)

        #expect(cachedUser == expectedUser, "The cache user should have been updated")
        #expect(cachedAccount == ObservableData.expectedAccount, "The cache account should have been updated")
        #expect(observedAccount == ObservableData.expectedAccount, "The observed object should have been updated")
    }

    @Test func updateObservedAccount() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedAccount(accountDbId: ObservableData.expectedAccountDbId,
                         cacheObservation: cache) var observedAccount: Account?
        let receivedValues = $observedAccount.receivedValues
        #expect(observedAccount == nil, "Account should initially be nil")

        let expectedUser = ObservableData.expectedUserWithAccounts
        await cache.addUser(expectedUser)

        let cachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        let cachedAccount = await cache.getAccount(accountDbId: ObservableData.expectedAccountDbId, userDbId: ObservableData.expectedUserDbId)

        #expect(cachedUser == expectedUser, "The cache user should have been updated")
        #expect(cachedAccount == ObservableData.expectedAccount, "The cache account should have been updated")

        // WHEN
        try await cache.updateAccount(ObservableData.updatedAccount)

        // THEN
        _ = await receivedValues.dropFirst().first(where: { $0 != nil })

        let latestAccount = await cache.getAccount(accountDbId: ObservableData.expectedAccountDbId, userDbId: ObservableData.expectedUserDbId)
        #expect(latestAccount == ObservableData.updatedAccount, "The cache account should have been updated again")
        #expect(observedAccount == ObservableData.updatedAccount, "The observed object should have been updated again")
    }

    @Test func doubleUpdateObservedAccount() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedAccount(accountDbId: ObservableData.expectedAccountDbId,
                         cacheObservation: cache) var observedAccount: Account?
        let receivedValues = $observedAccount.receivedValues
        #expect(observedAccount == nil, "Account should initially be nil")

        let expectedUser = ObservableData.expectedUserWithAccounts
        await cache.addUser(expectedUser)

        let cachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        let cachedAccount = await cache.getAccount(accountDbId: ObservableData.expectedAccountDbId, userDbId: ObservableData.expectedUserDbId)

        #expect(cachedUser == expectedUser, "The cache user should have been updated")
        #expect(cachedAccount == ObservableData.expectedAccount, "The cache account should have been updated")

        // WHEN
        try await cache.updateAccount(ObservableData.updatedAccount)
        try await cache.updateAccount(ObservableData.updatedAccount)

        // THEN
        _ = await receivedValues.dropFirst().dropFirst().first(where: { $0 != nil })

        let latestAccount = await cache.getAccount(accountDbId: ObservableData.expectedAccountDbId, userDbId: ObservableData.expectedUserDbId)
        #expect(latestAccount == ObservableData.updatedAccount, "The cache account should have been updated again")
        #expect(observedAccount == ObservableData.updatedAccount, "The observed object should have been updated again")
    }

    @Test func deleteObservedAccount() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedAccount(accountDbId: ObservableData.expectedAccountDbId,
                         cacheObservation: cache) var observedAccount: Account?
        let receivedValues = $observedAccount.receivedValues
        #expect(observedAccount == nil, "Account should initially be nil")

        let expectedUser = ObservableData.expectedUserWithAccounts
        await cache.addUser(expectedUser)

        let cachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        let cachedAccount = await cache.getAccount(accountDbId: ObservableData.expectedAccountDbId, userDbId: ObservableData.expectedUserDbId)

        #expect(cachedUser == expectedUser, "The cache user should have been updated")
        #expect(cachedAccount == ObservableData.expectedAccount, "The cache account should have been updated")

        // WHEN
        await cache.removeAccount(accountDbId: ObservableData.expectedAccountDbId)

        // THEN
        _ = await receivedValues.dropFirst().first(where: { $0 == nil })
        #expect(observedAccount == nil, "The observed object should be nil to reflect deletion")
    }
}

@MainActor
struct ObservedAccountTests_allIds {
    @Test func setObservedAccount() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedAccount(userDbId: ObservableData.expectedUserDbId,
                         accountDbId: ObservableData.expectedAccountDbId,
                         cacheObservation: cache) var observedAccount: Account?
        let receivedValues = $observedAccount.receivedValues
        #expect(observedAccount == nil, "Account should initially be nil")

        // WHEN
        let expectedUser = ObservableData.expectedUserWithAccounts
        await cache.addUser(expectedUser)

        // THEN
        let lastReceivedObject = await receivedValues.first(where: { $0 != nil })
        #expect(lastReceivedObject == ObservableData.expectedAccount)

        let cachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        let cachedAccount = await cache.getAccount(accountDbId: ObservableData.expectedAccountDbId, userDbId: ObservableData.expectedUserDbId)
        #expect(cachedUser == expectedUser, "The cache user should have been updated")
        #expect(cachedAccount == ObservableData.expectedAccount, "The cache account should have been updated")
        #expect(observedAccount == ObservableData.expectedAccount, "The observed object should have been updated")
    }

    @Test func updateObservedAccount() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedAccount(userDbId: ObservableData.expectedUserDbId,
                         accountDbId: ObservableData.expectedAccountDbId,
                         cacheObservation: cache) var observedAccount: Account?
        let receivedValues = $observedAccount.receivedValues
        #expect(observedAccount == nil, "Account should initially be nil")

        let expectedUser = ObservableData.expectedUserWithAccounts
        await cache.addUser(expectedUser)

        let cachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        let cachedAccount = await cache.getAccount(accountDbId: ObservableData.expectedAccountDbId, userDbId: ObservableData.expectedUserDbId)

        #expect(cachedUser == expectedUser, "The cache user should have been updated")
        #expect(cachedAccount == ObservableData.expectedAccount, "The cache account should have been updated")

        // WHEN
        try await cache.updateAccount(ObservableData.updatedAccount)

        // THEN
        _ = await receivedValues.dropFirst().first(where: { $0 != nil })

        let latestAccount = await cache.getAccount(accountDbId: ObservableData.expectedAccountDbId, userDbId: ObservableData.expectedUserDbId)
        #expect(latestAccount == ObservableData.updatedAccount, "The cache account should have been updated again")
        #expect(observedAccount == ObservableData.updatedAccount, "The observed object should have been updated again")
    }

    @Test func doubleUpdateObservedAccount() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedAccount(userDbId: ObservableData.expectedUserDbId,
                         accountDbId: ObservableData.expectedAccountDbId,
                         cacheObservation: cache) var observedAccount: Account?
        let receivedValues = $observedAccount.receivedValues
        #expect(observedAccount == nil, "Account should initially be nil")

        let expectedUser = ObservableData.expectedUserWithAccounts
        await cache.addUser(expectedUser)

        let cachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        let cachedAccount = await cache.getAccount(accountDbId: ObservableData.expectedAccountDbId, userDbId: ObservableData.expectedUserDbId)

        #expect(cachedUser == expectedUser, "The cache user should have been updated")
        #expect(cachedAccount == ObservableData.expectedAccount, "The cache account should have been updated")

        // WHEN
        try await cache.updateAccount(ObservableData.updatedAccount)
        try await cache.updateAccount(ObservableData.updatedAccount)

        // THEN
        _ = await receivedValues.dropFirst().dropFirst().first(where: { $0 != nil })

        let latestAccount = await cache.getAccount(accountDbId: ObservableData.expectedAccountDbId, userDbId: ObservableData.expectedUserDbId)
        #expect(latestAccount == ObservableData.updatedAccount, "The cache account should have been updated again")
        #expect(observedAccount == ObservableData.updatedAccount, "The observed object should have been updated again")
    }

    @Test func deleteObservedAccount() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedAccount(userDbId: ObservableData.expectedUserDbId,
                         accountDbId: ObservableData.expectedAccountDbId,
                         cacheObservation: cache) var observedAccount: Account?
        let receivedValues = $observedAccount.receivedValues
        #expect(observedAccount == nil, "Account should initially be nil")

        let expectedUser = ObservableData.expectedUserWithAccounts
        await cache.addUser(expectedUser)

        let cachedUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        let cachedAccount = await cache.getAccount(accountDbId: ObservableData.expectedAccountDbId, userDbId: ObservableData.expectedUserDbId)

        #expect(cachedUser == expectedUser, "The cache user should have been updated")
        #expect(cachedAccount == ObservableData.expectedAccount, "The cache account should have been updated")

        // WHEN
        await cache.removeAccount(accountDbId: ObservableData.expectedAccountDbId)

        // THEN
        _ = await receivedValues.dropFirst().first(where: { $0 == nil })

        #expect(observedAccount == nil, "The observed object should be nil to reflect deletion")
    }
}
