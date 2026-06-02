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
@testable import kDriveCore
import OrderedCollections
import Testing

extension ServerCoherentCache {
    var receivedUserValues: AsyncStream<IndexedUsers> {
        AsyncStream { continuation in
            let cancellable = usersPublisher
                .sink { value in continuation.yield(value) }

            continuation.onTermination = { _ in cancellable.cancel() }
        }
    }

    var receivedErrorValues: AsyncStream<IndexedErrors> {
        AsyncStream { continuation in
            let cancellable = serverErrorsPublisher
                .sink { value in continuation.yield(value) }

            continuation.onTermination = { _ in cancellable.cancel() }
        }
    }
}

@MainActor
struct ObservableCoherentCacheTests {
    @Test(.timeLimit(.minutes(1)))
    func observeUserChangesWithCombine() async throws {
        // GIVEN
        let user = ObservableData.expectedUser
        let cache = ServerCoherentCache()
        let receivedValues = await cache.receivedUserValues // Start to save the received values

        var receivedUsers: IndexedUsers?
        var cancellables = Set<AnyCancellable>()

        // WHEN
        let publisher = cache.usersPublisher
        let subscription = publisher
            .sink { indexedUsers in
                receivedUsers = indexedUsers
            }

        subscription.store(in: &cancellables)

        await cache.addUser(user)

        // THEN
        _ = await receivedValues.first(where: { _ in true })

        #expect(receivedUsers != nil, "Should have received users update")
        #expect(receivedUsers!.count == 1, "Should have received one user update")

        if let receivedUser = receivedUsers?.values.first {
            #expect(receivedUser.dbId == ObservableData.expectedUserDbId, "Received user ID should match expected")
            #expect(receivedUser.name == "appleseed", "Received user name should match expected")
        } else {
            Issue.record("Expected to find values")
        }
    }

    @Test(.timeLimit(.minutes(1)))
    func observeServerErrorsChangesWithCombine() async throws {
        // GIVEN
        let serverError = ObservableData.expectedServerError
        let cache = ServerCoherentCache()
        let receivedValues = await cache.receivedErrorValues // Start to save the received values

        var receivedServerErrors: IndexedErrors?
        var cancellables = Set<AnyCancellable>()

        // WHEN
        let publisher = cache.serverErrorsPublisher
        let subscription = publisher
            .sink { indexedErrors in
                receivedServerErrors = indexedErrors
            }

        subscription.store(in: &cancellables)

        try await cache.addOrUpdateError(serverError)

        // THEN
        _ = await receivedValues.first(where: { _ in true })

        #expect(receivedServerErrors != nil, "Should have received errors")
        #expect(receivedServerErrors!.count == 1, "Should have received one error")

        if let receivedServerError = receivedServerErrors?.values.first {
            #expect(receivedServerError.dbId == ObservableData.expectedServerErrorDbId, "Received error ID should match expected")
        } else {
            Issue.record("Expected to find values")
        }
    }
}
