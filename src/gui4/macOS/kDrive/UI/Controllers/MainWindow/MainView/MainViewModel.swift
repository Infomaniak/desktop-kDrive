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

    @LazyInjectService private var synchroStateObserver: UISynchroStateObserving
    @LazyInjectService private var synchroNodesObserver: UISynchroNodesObserving

    @Published private(set) var currentSynchroContext: UISynchroContext? {
        didSet {
            guard let currentSynchroContext else {
                return
            }

            synchroStateObserver.observeSynchro(currentSynchroContext.synchro.id)
            synchroNodesObserver.observeSynchro(currentSynchroContext.synchro.id)
        }
    }

    var currentUser: UIUser? {
        return currentSynchroContext?.user
    }

    var currentDrive: UIDrive? {
        return currentSynchroContext?.drive
    }

    var currentSynchro: UISynchro? {
        return currentSynchroContext?.synchro
    }

    var currentBlockingError: UIBlockingError? {
        return currentSynchroContext?.blockingError
    }

    private var bindStore = Set<AnyCancellable>()

    init() {
        Task {
            let synchroContexts = await coherentCache.getSynchroContexts()
            handleUpdatedSynchroContexts(UIIndexedSynchroContext(indexedSynchro: synchroContexts))
        }

        cacheObservable.usersPublisher
            .allSynchrosPublisher()
            .map { UIIndexedSynchroContext(indexedSynchro: $0) }
            .removeDuplicates()
            .receiveOnMain(store: &bindStore) { [weak self] synchroContext in
                self?.handleUpdatedSynchroContexts(synchroContext)
            }
    }

    func setCurrentSynchro(_ synchro: UISynchro) {
        Task {
            guard let synchroContext = await coherentCache.getSynchroContext(Int32(synchro.dbId)) else {
                return
            }

            currentSynchroContext = UISynchroContext(synchroContext: synchroContext)
            UserDefaults.standard.selectedSynchroDbId = synchro.dbId
        }
    }

    private func handleUpdatedSynchroContexts(_ context: UIIndexedSynchroContext) {
        updateCurrentSynchro(context)

        if currentBlockingError != nil {
            router.setCurrentTab(.blockingError)
        } else if router.currentPath.mainTab == .blockingError {
            router.setCurrentTab(.home)
        }
    }

    private func updateCurrentSynchro(_ context: UIIndexedSynchroContext) {
        guard currentSynchroContext != nil else {
            restoreLastSelection(context)
            return
        }

        guard let currentSynchro, let synchroContext = context[currentSynchro.id] else {
            selectFirstAvailableSynchro(context)
            return
        }

        currentSynchroContext = synchroContext
    }

    private func restoreLastSelection(_ context: UIIndexedSynchroContext) {
        let synchroDbId = UserDefaults.standard.selectedSynchroDbId
        guard let synchroContext = context[synchroDbId] else {
            selectFirstAvailableSynchro(context)
            return
        }

        setCurrentSynchro(synchroContext.synchro)
    }

    private func selectFirstAvailableSynchro(_ context: UIIndexedSynchroContext) {
        guard let firstAvailableSynchroContext = context.values.first else {
            return
        }

        setCurrentSynchro(firstAvailableSynchroContext.synchro)
    }
}
