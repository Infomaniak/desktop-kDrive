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

import Cocoa
import Combine
import Foundation
import InfomaniakDI
import kDriveCore

@MainActor
final class MainViewModel {
    @LazyInjectService private var coherentCache: CoherentCache
    @LazyInjectService private var cacheObservable: CoherentCacheObservable

    @Published private(set) var currentUser: UIUser?
    @Published private(set) var currentSynchro: UISynchro?

    @Published private(set) var availableUsers = [Int: UIUser]()

    private var bindStore = Set<AnyCancellable>()

    init() {
        cacheObservable.usersPublisher
            .receiveOnMain(store: &bindStore) { [weak self] indexedUser in
                self?.handleUpdatedUsers(indexedUser)
            }
    }

    func fetchAvailableDrives() {
        Task {
            _ = try await UserJobs().userInfoList()
            _ = try await AccountJobs().accountInfoList()
            _ = try await DriveJobs().driveInfoList()
            _ = try await SyncJobs().availableSync()
        }
    }

    func selectNewSynchro(_ synchro: UISynchro) {
        Task {
            currentSynchro = synchro
        }
    }

    func openCurrentSyncInFinder() {
        guard let currentSynchro else { return }
        NSWorkspace.shared.open(currentSynchro.localPath)
    }

    private func handleUpdatedUsers(_ users: IndexedUsers) {
        availableUsers = Dictionary(uniqueKeysWithValues: users.map { userDbId, user in
            (Int(userDbId), UIUser(user: user))
        })
        print("Did Change")
    }
}
