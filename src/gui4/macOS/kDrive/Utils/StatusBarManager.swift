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

extension UISynchroStatus {
    var statusItemTooltip: String {
        switch self {
        case .starting, .running:
            return KDriveLocalizable.syncShortStatusRunning
        case .idle:
            return KDriveLocalizable.syncShortStatusIdle
        case .pauseAsked, .paused, .stopAsked, .stopped:
            return KDriveLocalizable.syncShortStatusPaused
        case .error:
            return KDriveLocalizable.syncShortStatusError
        }
    }
}

struct UISynchroStateContext: Sendable, Equatable {
    let synchro: UISynchro
    let state: UISynchroState

    init(synchro: Synchro) {
        self.synchro = UISynchro(synchro: synchro)
        state = UISynchroState(fromSynchro: synchro)
    }
}

@MainActor
final class StatusBarManager {
    static let autosaveName = "kdrive.statusbaritem"

    private var statusItem: NSStatusItem
    private var cancellable: AnyCancellable?

    init() {
        let statusBar = NSStatusBar.system

        statusItem = statusBar.statusItem(withLength: NSStatusItem.squareLength)
        statusItem.autosaveName = StatusBarManager.autosaveName
        statusItem.button?.image = StatusItemState.idle.icon

        statusItem.button?.target = self
        statusItem.button?.action = #selector(handleItemClick)
        statusItem.button?.sendAction(on: [.leftMouseUp, .rightMouseUp])

        observeSynchros()
        setupInitialState()
    }

    private func setupInitialState() {
        Task {
            @InjectService var cache: CoherentCache
            let synchros = await cache.getSynchroContexts()

            let synchroStates = synchros.map { UISynchroStateContext(synchro: $0.synchro) }
            updateStatusItem(context: synchroStates)
        }
    }

    private func observeSynchros() {
        @InjectService var observableCache: CoherentCacheObservable
        cancellable = observableCache.usersPublisher
            .allSynchrosPublisher()
            .map { contexts in
                contexts.map { UISynchroStateContext(synchro: $0.synchro) }
            }
            .removeDuplicates()
            .receive(on: RunLoop.main)
            .sink(receiveValue: updateStatusItem)
    }

    private func updateStatusItem(context: [UISynchroStateContext]) {
        let statusItemState = guessStatusItemState(from: context)
        statusItem.button?.image = statusItemState.icon

        let tooltip = generateTooltip(from: context)
        statusItem.button?.toolTip = tooltip
    }

    private func guessStatusItemState(from contexts: [UISynchroStateContext]) -> StatusItemState {
        if contexts.contains(where: { $0.state.status == .running }) {
            return .running
        }
        if contexts.contains(where: { $0.state.status == .error }) {
            return .error
        }
        if contexts.allSatisfy({ $0.state.status == .paused || $0.state.status == .stopped }) {
            return .paused
        }
        return .idle
    }

    private func generateTooltip(from contexts: [UISynchroStateContext]) -> String {
        if contexts.count == 1, let context = contexts.first {
            return context.state.status.statusItemTooltip
        }

        var tooltip = ""
        for context in contexts {
            let folder = context.synchro.localPath.lastPathComponent
            let status = context.state.status.statusItemTooltip

            tooltip += "\(folder) - \(status)"
            if context != contexts.last {
                tooltip += "\n"
            }
        }

        return tooltip
    }

    @objc private func handleItemClick() {
        guard let event = NSApp.currentEvent else { return }

        switch event.type {
        case .leftMouseUp:
            (NSApp.delegate as? AppDelegate)?.openMainWindow()
        case .rightMouseUp:
            presentMenu()
        default:
            break
        }
    }

    private func presentMenu() {
        let menu = NSMenu()
        menu.addItem(
            withTitle: KDriveLocalizable.statusBarOpenApp,
            action: #selector(AppDelegate.openMainWindow),
            keyEquivalent: ""
        )
        menu.addItem(
            withTitle: KDriveLocalizable.statusBarSettings,
            action: #selector(AppDelegate.openPreferencesWindow),
            keyEquivalent: ""
        )
        menu.addItem(
            withTitle: KDriveLocalizable.statusBarQuitApp,
            action: #selector(AppDelegate.quitApp),
            keyEquivalent: "q"
        )

        statusItem.menu = menu
        statusItem.button?.performClick(nil)
        statusItem.menu = nil
    }
}
