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

import Combine
import Foundation
import InfomaniakConcurrency
import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import OrderedCollections

struct PreferencesViewState: Equatable {
    var users = [UIUser]()
    var availableDrives = OrderedDictionary<UIUser.ID, [UIAvailableDrive]>()
    var synchronizedDrives = OrderedDictionary<UIUser.ID, [UIDriveContext]>()

    init() {}

    init(indexedUsers: IndexedUsers) {
        for user in indexedUsers.values {
            let uiUser = UIUser(user: user)
            users.append(uiUser)

            var synchronizedDriveIds = Set<Int32>()
            for account in user.accounts.values {
                for drive in account.drives.values where !drive.synchros.isEmpty {
                    synchronizedDrives[uiUser.id, default: []].append(UIDriveContext(drive: drive, account: account))
                    synchronizedDriveIds.insert(drive.driveId)
                }
            }
            synchronizedDrives[uiUser.id, default: []].sort { $0.drive.name < $1.drive.name }

            for availableDrive in user.availableDrives.values {
                guard !synchronizedDriveIds.contains(availableDrive.driveId) else { continue }
                availableDrives[uiUser.id, default: []].append(UIAvailableDrive(availableDrive: availableDrive))
            }
            availableDrives[uiUser.id, default: []].sort { $0.name < $1.name }
        }
    }
}

@MainActor
public class PreferencesViewModel: ObservableObject {
    @LazyInjectService private var cache: CoherentCache
    @LazyInjectService private var cacheObservable: CoherentCacheObservable

    @Published private(set) var state = PreferencesViewState()

    var users: [UIUser] {
        state.users
    }

    var availableDrive: OrderedDictionary<UIUser.ID, [UIAvailableDrive]> {
        state.availableDrives
    }

    var synchronizedDrive: OrderedDictionary<UIUser.ID, [UIDriveContext]> {
        state.synchronizedDrives
    }

    private var bindStore = Set<AnyCancellable>()

    init() {
        cacheObservable.usersPublisher
            .receiveOnMain(store: &bindStore) { [weak self] users in
                self?.updateUsers(users)
            }
    }

    func fetchInitialData() {
        Task {
            let users = await cache.getUsers()
            updateUsers(users)

            await users.values.asyncForEach { user in
                _ = try? await DriveJobs().availableDrives(userDbId: user.dbId)
            }
        }
    }

    func refreshData() async {
        await users.asyncForEach { user in
            _ = try? await DriveJobs().availableDrives(userDbId: Int32(user.dbId))
        }
    }

    private func updateUsers(_ users: IndexedUsers) {
        state = PreferencesViewState(indexedUsers: users)
    }
}
