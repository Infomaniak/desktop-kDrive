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
@testable import kDrive
@testable import kDriveCore
import kDriveCoreUI
import OrderedCollections
import Testing

struct PreferencesViewStateTests {
    private let userDbId = CacheData.expectedUserDbId
    private let userId = CacheData.expectedUserAPIId
    private let accountDbId = CacheData.expectedAccountDbId
    private let accountId = CacheData.expectedAccountId
    private let driveDbId = CacheData.expectedDriveDbId
    private let driveId = CacheData.expectedDriveId
    private let synchroDbId = CacheData.expectedSynchroDbId

    @Test("A synchronized drive hides its available representation")
    func synchronizedDriveWinsMerge() {
        let state = PreferencesViewState(indexedUsers: users(synchros: [synchroDbId: CacheData.expectedSynchro]))

        #expect(state.synchronizedDrives[Int(userDbId)]?.map(\.drive.driveId) == [Int(driveId)])
        #expect(state.availableDrives[Int(userDbId), default: []].isEmpty)
    }

    @Test("An empty stored drive is represented by its available drive")
    func emptyStoredDriveDoesNotAppearSynchronized() {
        let state = PreferencesViewState(indexedUsers: users(synchros: [:]))

        #expect(state.synchronizedDrives[Int(userDbId), default: []].isEmpty)
        #expect(state.availableDrives[Int(userDbId)]?.map(\.driveId) == [Int(driveId)])
    }

    @Test("Removing the stored drive keeps one available representation")
    func removedStoredDriveAppearsAvailable() {
        let state = PreferencesViewState(indexedUsers: users(includeStoredDrive: false))

        #expect(state.synchronizedDrives[Int(userDbId), default: []].isEmpty)
        #expect(state.availableDrives[Int(userDbId)]?.map(\.driveId) == [Int(driveId)])
    }

    private func users(
        synchros: IndexedSynchros = [:],
        includeStoredDrive: Bool = true
    ) -> IndexedUsers {
        var drive = CacheData.expectedDrive
        drive.synchros = synchros
        let drives: IndexedDrives = includeStoredDrive ? [driveDbId: drive] : [:]
        let account = Account(
            dbId: accountDbId,
            userDbId: userDbId,
            name: "Account",
            drives: drives,
            accountId: accountId
        )
        let availableDrive = AvailableDrive(
            driveId: driveId,
            accountId: accountId,
            accountName: "Account",
            userDbId: userDbId,
            userId: userId,
            name: "Drive",
            color: nil
        )
        let user = User(
            dbId: userDbId,
            userId: userId,
            name: "User",
            firstName: "Test",
            email: "test@example.com",
            accounts: [accountDbId: account],
            availableDrives: [driveId: availableDrive],
            avatar: nil,
            isConnected: true,
            isStaff: false
        )
        return [userDbId: user]
    }
}
