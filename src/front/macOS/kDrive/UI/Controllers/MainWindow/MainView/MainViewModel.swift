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
    @Published private(set) var currentAccount: UIAccount?
    @Published private(set) var currentDrive: UIDrive?
    @Published private(set) var currentSynchro: UISynchro?
    @Published private(set) var currentSynchros = [UISynchroContext]()

    private var bindStore = Set<AnyCancellable>()

    init() {
        cacheObservable.usersPublisher
            .allSynchrosPublisher()
            .receive(on: DispatchQueue.main)
            .receiveOnMain(store: &bindStore) { [weak self] synchros in
                let uiSynchros = synchros.map { UISynchroContext(synchroContext: $0) }
                self?.handleUpdatedSynchros(uiSynchros)
            }
    }

    func refreshCache() {
        Task {
            try await coherentCache.refresh()
        }
    }

    func setCurrentSynchro(_ synchro: UISynchro) {
        Task {
            currentSynchro = synchro
            UserDefaults.standard.selectedSynchroDbId = synchro.dbId

            guard let selectedValues = getSelectedValuesFromSynchro(synchro) else {
                return
            }
            currentUser = selectedValues.user
            currentAccount = selectedValues.account
            currentDrive = selectedValues.drive
        }
    }

    private func handleUpdatedSynchros(_ synchros: [UISynchroContext]) {
        currentSynchros = synchros
        updateSelectedItems()
    }

    private func updateSelectedItems() {
        guard let currentSynchro = currentSynchro,
              let currentSyncContext = getSelectedValuesFromSynchro(currentSynchro) else {
            return
        }

        currentUser = currentSyncContext.user
        currentAccount = currentSyncContext.account
        currentDrive = currentSyncContext.drive
        self.currentSynchro = currentSyncContext.synchro
    }

    func restoreLastSelection() {
        Task {
            let synchroDbId = UserDefaults.standard.selectedSynchroDbId
            guard let syncho = await coherentCache.getSynchro(synchroDbId: Int32(synchroDbId)) else {
                return
            }

            setCurrentSynchro(UISynchro(synchro: syncho))
        }
    }

    private func getSelectedValuesFromSynchro(_ synchro: UISynchro) -> UISynchroContext? {
        guard let matchedSyncContext = currentSynchros.first(where: { syncContext in
            syncContext.synchro.dbId == Int32(synchro.dbId)
        }) else {
            return nil
        }
        return matchedSyncContext
    }
}
