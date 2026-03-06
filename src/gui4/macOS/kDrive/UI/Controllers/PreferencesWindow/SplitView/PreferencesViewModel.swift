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
import InfomaniakConcurrency
import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import OrderedCollections

@MainActor
public class PreferencesViewModel: ObservableObject {
    @LazyInjectService private var cache: CoherentCache
    @LazyInjectService private var cacheObservable: CoherentCacheObservable

    @Published private(set) var users = [UIUser]()

    @Published private(set) var availableDrive = OrderedDictionary<UIUser.ID, [UIAvailableDrive]>()
    @Published private(set) var synchronizedDrive = OrderedDictionary<UIUser.ID, [UIDrive]>()

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
        var refreshedUsers = [UIUser]()
        var refreshedAvailableDrives = OrderedDictionary<UIUser.ID, [UIAvailableDrive]>()
        var refreshedSynchronizedDrives = OrderedDictionary<UIUser.ID, [UIDrive]>()

        for user in users.values {
            let uiUser = UIUser(user: user)
            refreshedUsers.append(uiUser)

            var synchronizedDrivesID = Set<Int32>()
            for account in user.accounts.values {
                for drive in account.drives.values {
                    refreshedSynchronizedDrives[uiUser.id, default: []].append(UIDrive(drive: drive))
                    synchronizedDrivesID.insert(drive.driveId)
                }
            }

            for availableDrive in user.availableDrives.values {
                guard !synchronizedDrivesID.contains(availableDrive.driveId) else { continue }
                refreshedAvailableDrives[uiUser.id, default: []].append(UIAvailableDrive(availableDrive: availableDrive))
            }
        }

        self.users = refreshedUsers
        availableDrive = refreshedAvailableDrives
        synchronizedDrive = refreshedSynchronizedDrives
    }
}
