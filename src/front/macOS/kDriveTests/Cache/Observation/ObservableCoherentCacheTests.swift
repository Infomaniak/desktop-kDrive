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
import kDriveCore
import OrderedCollections
import XCTest

final class ObservableCoherentCacheTests: XCTestCase {
    static let expectedUserId = Int32.random(in: 0 ... 1000)
    static let expectedUserDbId = Int32.random(in: 0 ... 1000)

    @MainActor func testObserveUserChangesWithCombine() throws {
        // GIVEN
        let user = User(
            dbId: Self.expectedUserDbId,
            userId: Self.expectedUserId,
            name: "appleseed",
            email: "ja@apple.com",
            accounts: [:],
            availableDrives: [:],
            avatar: Data(),
            isConnected: true,
            isStaff: true
        )

        let cache = ServerCoherentCache()
        let expectation = expectation(description: "User updates observed")
        var receivedUsers: IndexedUsers?
        var cancellables = Set<AnyCancellable>()

        // WHEN
        let publisher = cache.usersPublisher
        let subscription = publisher
            .sink { indexedUsers in
                receivedUsers = indexedUsers
                if !indexedUsers.isEmpty {
                    expectation.fulfill()
                } else {
                    XCTFail("Received an empty users list")
                }
            }

        subscription.store(in: &cancellables)

        Task {
            await cache.addUser(user)
        }

        // THEN
        waitForExpectations(timeout: 10.0) { error in
            if let error {
                XCTFail("Expectation failed with error: \(error)")
            }
        }

        XCTAssertEqual(receivedUsers!.count, 1, "Should have received one user update")

        if let receivedUser = receivedUsers?.values.first {
            XCTAssertEqual(receivedUser.dbId, Self.expectedUserDbId, "Received user ID should match expected")
            XCTAssertEqual(receivedUser.name, "appleseed", "Received user name should match expected")
        } else {
            XCTFail("Expected to find a user in the combine event")
        }
    }
}
