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

import AppKit
import Combine
import Foundation
import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import kDriveResources
import OrderedCollections

enum StatusItemState {
    case idle
    case running
    case paused
    case error

    var icon: NSImage {
        switch self {
        case .idle:
            return KDriveResources.kdriveNeutral.image
        case .running:
            return KDriveResources.kdriveCircleFilledArrowsClockwise.image
        case .paused:
            return KDriveResources.kdriveCircleFilledPause.image
        case .error:
            return KDriveResources.kdriveCircleFilledExclamationMark.image
        }
    }
}

@MainActor
final class StatusBarManager {
    private var statusItem: NSStatusItem

    private var cancellable: AnyCancellable?

    init() {
        let statusBar = NSStatusBar.system

        statusItem = statusBar.statusItem(withLength: NSStatusItem.squareLength)
        statusItem.button?.image = StatusItemState.idle.icon
        statusItem.button?.action = #selector(AppDelegate.openMainWindow)

        observeSynchros()
    }

    private func observeSynchros() {
        @InjectService var observableCache: CoherentCacheObservable
        cancellable = observableCache.usersPublisher
            .map { indexedUsers in
                var synchroStates = [UISynchroState]()

                for user in indexedUsers.values {
                    for account in user.accounts.values {
                        for drive in account.drives.values {
                            for synchro in drive.synchros.values {
                                let state = UISynchroState(fromSynchro: synchro)
                                synchroStates.append(state)
                            }
                        }
                    }
                }

                return synchroStates
            }
            .removeDuplicates()
            .receive(on: RunLoop.main)
            .sink(receiveValue: updateStatusItem)
    }

    private func updateStatusItem(states: [UISynchroState]) {
        let statusItemState = guessStatusItemState(from: states)
        statusItem.button?.image = statusItemState.icon
    }

    private func guessStatusItemState(from synchroStates: [UISynchroState]) -> StatusItemState {
        if synchroStates.contains(where: { $0.status == .running }) {
            return .running
        }
        if synchroStates.contains(where: { $0.errorCount != 0 }) {
            return .error
        }
        if synchroStates.allSatisfy({ $0.status == .paused || $0.status == .stopped }) {
            return .paused
        }
        return .idle
    }
}
