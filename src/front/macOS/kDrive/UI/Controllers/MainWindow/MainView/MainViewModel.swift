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
    @LazyInjectService private var cacheObservable: CoherentCacheObservable

    @Published private(set) var currentSynchro: UISynchro?
    private var synchroSubscription: AnyCancellable?

    init() {}

    func selectNewSynchro(_ synchro: UISynchro) {
        synchroSubscription?.cancel()
        synchroSubscription = cacheObservable.usersPublisher.synchroPublisher(dbId: Int32(synchro.dbId))
            .receive(on: RunLoop.main)
            .sink { [weak self] newSynchro in
                guard let newSynchro else { return }
                self?.currentSynchro = UISynchro(synchro: newSynchro)
            }
    }

    func openCurrentSyncInFinder() {
        guard let currentSynchro else { return }

        let synchroURL = URL(fileURLWithPath: currentSynchro.localPath)
        NSWorkspace.shared.open(synchroURL)
    }
}
