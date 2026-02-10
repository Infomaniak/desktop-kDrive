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
import kDriveCoreUI

@MainActor
final class MainViewModel: ObservableObject {
    @LazyInjectService private var coherentCache: CoherentCache
    @LazyInjectService private var cacheObservable: CoherentCacheObservable
    @LazyInjectService private var router: MainViewRouter

    var currentUser: UIUser? { currentSynchroContext?.user }
    var currentDrive: UIDrive? { currentSynchroContext?.drive }
    var currentSynchro: UISynchro? { currentSynchroContext?.synchro }
    var currentBlockingError: UIBlockingError? { currentSynchroContext?.blockingError }

    @Published private(set) var currentSynchroContext: UISynchroContext?

    @Published private(set) var availableSynchros = [UISynchroContext]()

    private var bindStore = Set<AnyCancellable>()

    init() {
        cacheObservable.usersPublisher
            .allSynchrosPublisher()
            .map { synchros in
                synchros.map { UISynchroContext(synchroContext: $0) }
            }
            .receiveOnMain(store: &bindStore) { [weak self] uiSynchros in
                self?.handleUpdatedSynchros(uiSynchros)
            }
    }

    func refreshCache() {
        Task {
            try await coherentCache.refresh()
            await restoreLastSelection()
        }
    }

    func setCurrentSynchro(_ synchro: UISynchro) {
        guard let synchroContext = getSelectedValuesFromSynchro(synchro) else {
            return
        }

        currentSynchroContext = synchroContext
        UserDefaults.standard.selectedSynchroDbId = synchro.dbId
    }

    private func handleUpdatedSynchros(_ synchros: [UISynchroContext]) {
        availableSynchros = synchros
        updateSelectedItems()
        if currentBlockingError != nil {
            router.setCurrentTab(.blockingError)
        } else if router.currentPath.mainTab == .blockingError {
            router.setCurrentTab(.home)
        }
    }

    private func updateSelectedItems() {
        guard let currentSynchro,
              let updatedSynchroContext = getSelectedValuesFromSynchro(currentSynchro) else {
            return
        }

        currentSynchroContext = updatedSynchroContext
    }

    private func restoreLastSelection() async {
        let synchroDbId = UserDefaults.standard.selectedSynchroDbId
        guard let synchro = await coherentCache.getSynchro(synchroDbId: Int32(synchroDbId)) else {
            setDefaultSynchro()
            return
        }

        setCurrentSynchro(UISynchro(synchro: synchro))
    }

    private func setDefaultSynchro() {
        guard let firstAvailableSynchro = availableSynchros.first else {
            return
        }

        setCurrentSynchro(firstAvailableSynchro.synchro)
    }

    private func getSelectedValuesFromSynchro(_ synchro: UISynchro) -> UISynchroContext? {
        guard let matchedSyncContext = availableSynchros.first(where: { syncContext in
            syncContext.synchro.dbId == Int32(synchro.dbId)
        }) else {
            return nil
        }
        return matchedSyncContext
    }
}
