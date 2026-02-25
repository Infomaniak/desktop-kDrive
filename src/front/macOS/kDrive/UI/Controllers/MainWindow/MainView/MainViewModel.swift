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
import OrderedCollections

@MainActor
final class MainViewModel: ObservableObject {
    @LazyInjectService private var coherentCache: CoherentCache
    @LazyInjectService private var cacheObservable: CoherentCacheObservable
    @LazyInjectService private var router: MainViewRouter

    @Published private(set) var currentSynchroContext: UISynchroContext?
    @Published private(set) var availableSynchros = UIIndexedSynchroContext()

    var currentUser: UIUser? {
        currentSynchroContext?.user
    }

    var currentDrive: UIDrive? {
        currentSynchroContext?.drive
    }

    var currentSynchro: UISynchro? {
        currentSynchroContext?.synchro
    }

    var currentBlockingError: UIBlockingError? {
        currentSynchroContext?.blockingError
    }

    private var bindStore = Set<AnyCancellable>()

    init() {
        cacheObservable.usersPublisher
            .allSynchrosPublisher()
            .map { UIIndexedSynchroContext(indexedSynchro: $0) }
            .removeDuplicates()
            .receiveOnMain(store: &bindStore) { [weak self] synchroContext in
                self?.handleUpdatedSynchroContexts(synchroContext)
            }

        Task {
            let synchroContexts = await coherentCache.getSynchroContexts()
            handleUpdatedSynchroContexts(UIIndexedSynchroContext(indexedSynchro: synchroContexts))
        }
    }

    func setCurrentSynchro(_ synchro: UISynchro) {
        guard let synchroContext = availableSynchros[synchro.dbId] else {
            return
        }

        currentSynchroContext = synchroContext
        UserDefaults.standard.selectedSynchroDbId = synchro.dbId
    }

    private func handleUpdatedSynchroContexts(_ context: UIIndexedSynchroContext) {
        availableSynchros = context

        updateCurrentSynchro()

        if currentBlockingError != nil {
            router.setCurrentTab(.blockingError)
        } else if router.currentPath.mainTab == .blockingError {
            router.setCurrentTab(.home)
        }
    }

    private func updateCurrentSynchro() {
        guard currentSynchroContext != nil else {
            restoreLastSelection()
            return
        }

        guard let currentSynchro, let synchroContext = availableSynchros[currentSynchro.id] else {
            selectFirstAvailableSynchro()
            return
        }

        currentSynchroContext = synchroContext
    }

    private func restoreLastSelection() {
        let synchroDbId = UserDefaults.standard.selectedSynchroDbId
        guard let synchroContext = availableSynchros[synchroDbId] else {
            selectFirstAvailableSynchro()
            return
        }

        setCurrentSynchro(synchroContext.synchro)
    }

    private func selectFirstAvailableSynchro() {
        guard let firstAvailableSynchroContext = availableSynchros.values.first else {
            return
        }

        setCurrentSynchro(firstAvailableSynchroContext.synchro)
    }
}
