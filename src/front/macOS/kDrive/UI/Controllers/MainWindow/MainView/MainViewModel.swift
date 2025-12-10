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
    private var userSubscription: AnyCancellable?

    @Published private(set) var currentDrive: UIDrive?
    private var driveSubscription: AnyCancellable?

    @Published private(set) var currentSynchro: UISynchro?
    private var synchroSubscription: AnyCancellable?

    init() {}

    func selectNewSynchro(_ synchro: UISynchro) {
        subscribeToSynchro(synchro)

        if currentDrive == nil || currentDrive?.dbId != synchro.driveDbId {
            subscribeToDriveAndUser(driveDbId: synchro.driveDbId)
        }
    }

    func openCurrentSyncInFinder() {
        guard let currentSynchro else { return }

        let synchroURL = URL(fileURLWithPath: currentSynchro.localPath)
        NSWorkspace.shared.open(synchroURL)
    }

    private func subscribeToSynchro(_ synchro: UISynchro) {
        synchroSubscription?.cancel()
        synchroSubscription = cacheObservable.usersPublisher.synchroPublisher(dbId: Int32(synchro.dbId))
            .receive(on: RunLoop.main)
            .sink { [weak self] synchro in
                guard let synchro else { return }
                self?.currentSynchro = UISynchro(synchro: synchro)
            }
    }

    private func subscribeToDriveAndUser(driveDbId: Int) {
        Task {
            guard let drive = await coherentCache.getDrive(driveDbId: Int32(driveDbId)) else {
                return
            }

            driveSubscription?.cancel()
            driveSubscription = cacheObservable.usersPublisher.drivePublisher(driveDbId: drive.driveDbId)
                .receive(on: RunLoop.main)
                .sink { [weak self] drive in
                    guard let drive else { return }
                    self?.currentDrive = UIDrive(drive: drive)
                }

            userSubscription?.cancel()
            userSubscription = cacheObservable.usersPublisher.userPublisher(userDbId: drive.userDbId)
                .receive(on: RunLoop.main)
                .sink { [weak self] user in
                    guard let user else { return }
                    self?.currentUser = UIUser(user: user)
                }
        }
    }
}
