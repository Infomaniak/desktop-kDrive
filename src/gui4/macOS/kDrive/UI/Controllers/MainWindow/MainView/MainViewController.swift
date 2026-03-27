/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import Cocoa
import Combine
import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import kDriveResources
import SwiftUI
import UserNotifications

extension NSToolbarItem.Identifier {
    static let supportGroup = NSToolbarItem.Identifier("SupportGroup")
    static let syncControlsGroup = NSToolbarItem.Identifier("SyncControlsGroup")
    static let searchGroup = NSToolbarItem.Identifier("SearchGroup")
    static let pauseResumeButton = NSToolbarItem.Identifier("PauseResumeButton")
}

final class MainViewController: IKSplitViewController {
    @LazyInjectService private var router: MainViewRouter
    @LazyInjectService private var synchroStateObserver: UISynchroStateObserving

    private let viewModel = MainViewModel()

    private var bindStore = Set<AnyCancellable>()

    override func viewDidLoad() {
        super.viewDidLoad()

        setupSplitView()
        bindViewModel()
    }

    override func viewDidAppear() {
        super.viewDidAppear()

        configureWindowAppearance()
        splitView.setPosition(200, ofDividerAt: 0)

        Task {
            try? await UNUserNotificationCenter.current().requestAuthorization(options: [.alert, .sound])
        }
    }

    private func bindViewModel() {
        router.$currentPath
            .receiveOnMain(store: &bindStore) { [weak self] newPath in
                self?.onPathChange(newPath)
            }
        router.$currentModal
            .receiveOnMain(store: &bindStore) { [weak self] newPath in
                self?.onModalPathChange(newPath)
            }

        viewModel.$currentSynchroContext
            .receiveOnMain(store: &bindStore) { [weak self] _ in
                guard let self else { return }
                refreshPauseResumeToolbarItem(synchroStateObserver.synchroState)
            }

        synchroStateObserver.synchroStatePublisher
            .receiveOnMain(store: &bindStore) { [weak self] synchroState in
                self?.refreshPauseResumeToolbarItem(synchroState)
            }
    }

    private func configureWindowAppearance() {
        guard let window = view.window else { return }

        window.titlebarAppearsTransparent = false
        window.toolbarStyle = .unified
        window.isMovableByWindowBackground = false
    }

    private func setupSplitView() {
        splitView.autosaveName = "MainSplitViewAutoSave"
        splitView.isVertical = true

        let sidebarViewController = MainSidebarViewController(mainViewModel: viewModel)
        let sidebarItem = NSSplitViewItem(sidebarWithViewController: sidebarViewController)
        sidebarItem.minimumThickness = 150
        sidebarItem.maximumThickness = 250
        addSplitViewItem(sidebarItem)

        let homeViewController = HomeViewController(mainViewModel: viewModel)
        currentContentViewController = homeViewController
        let homeDetailItem = NSSplitViewItem(viewController: homeViewController)
        addSplitViewItem(homeDetailItem)
    }

    func onModalPathChange(_ modalPath: ModalPath?) {
        if let modalPath {
            // TODO: Present some modal view controller based on modalPath
        } else if let presentedViewController = presentedViewControllers?.first {
            dismiss(presentedViewController)
        }
    }

    func onPathChange(_ path: MainViewRouter.RouterPath) {
        let contentViewController: NSViewController
        switch path.details.last {
        case .home:
            contentViewController = HomeViewController(mainViewModel: viewModel)
        case .activities:
            contentViewController = ActivitiesViewController()
        case .storage:
            contentViewController = StorageViewController(mainViewModel: viewModel)
        case .blockingError:
            if let blockingError = viewModel.currentBlockingError {
                contentViewController = BlockingErrorViewController(blockingError: blockingError)
            } else {
                #if DEBUG
                fatalError("Attempting to navigate to blocking error view without an error")
                #else
                contentViewController = HomeViewController(mainViewModel: viewModel)
                #endif
            }
        case .activityError:
            contentViewController = TitledViewController(toolbarTitle: "activityError", contentView: Text("activityError - WIP"))
        case .versionConflict:
            contentViewController = TitledViewController(
                toolbarTitle: "versionConflict",
                contentView: Text("versionConflict - WIP")
            )
        default:
            contentViewController = HomeViewController(mainViewModel: viewModel)
        }

        switchContentViewController(destination: contentViewController)
    }
}

// MARK: - NSToolbarDelegate

extension MainViewController {
    override func toolbarAllowedItemIdentifiers(_ toolbar: NSToolbar) -> [NSToolbarItem.Identifier] {
        var initialItems = super.toolbarAllowedItemIdentifiers(toolbar)
        initialItems.append(contentsOf: [.supportGroup, .syncControlsGroup, .searchGroup])

        return initialItems
    }

    override func toolbarDefaultItemIdentifiers(_ toolbar: NSToolbar) -> [NSToolbarItem.Identifier] {
        var initialItems = super.toolbarDefaultItemIdentifiers(toolbar)
        initialItems.append(contentsOf: [
            .flexibleSpace,
            .supportGroup,
            .space,
            .syncControlsGroup,
            .space,
            .searchGroup
        ])

        return initialItems
    }

    override func toolbar(
        _ toolbar: NSToolbar,
        itemForItemIdentifier itemIdentifier: NSToolbarItem.Identifier,
        willBeInsertedIntoToolbar flag: Bool
    ) -> NSToolbarItem? {
        if let initialItem = super.toolbar(toolbar, itemForItemIdentifier: itemIdentifier, willBeInsertedIntoToolbar: flag) {
            return initialItem
        }

        switch itemIdentifier {
        case .supportGroup:
            return makeSupportGroup()
        case .syncControlsGroup:
            return makeSyncControlsGroup()
        case .searchGroup:
            return makeSearchGroup()
        default:
            return nil
        }
    }

    // MARK: - Toolbar Item Factories

    private func makeSupportGroup() -> NSToolbarItemGroup {
        let helpButton = NSToolbarItem(itemIdentifier: .init("HelpButton"))
        helpButton.image = KDriveResources.headphones.image
        helpButton.label = "Help"
        helpButton.target = self
        helpButton.action = #selector(openHelpURL)

        let group = NSToolbarItemGroup(itemIdentifier: .supportGroup)
        group.subitems = [helpButton]
        return group
    }

    private func makeSyncControlsGroup() -> NSToolbarItemGroup {
        let pauseResumeButton = NSToolbarItem(itemIdentifier: .pauseResumeButton)
        pauseResumeButton.target = self
        pauseResumeButton.action = #selector(togglePauseResume)
        updatePauseResumeButton(pauseResumeButton, state: synchroStateObserver.synchroState)

        let settingsButton = NSToolbarItem(itemIdentifier: .init("SettingsButton"))
        settingsButton.image = KDriveResources.cog.image
        settingsButton.label = KDriveLocalizable.buttonSettings
        settingsButton.target = nil
        settingsButton.action = #selector(AppDelegate.openPreferencesWindow)

        let group = NSToolbarItemGroup(itemIdentifier: .syncControlsGroup)
        group.subitems = [pauseResumeButton, settingsButton]
        return group
    }

    private func makeSearchGroup() -> NSToolbarItemGroup {
        let searchButton = NSToolbarItem(itemIdentifier: .init("SearchButton"))
        searchButton.image = KDriveResources.magnifyingGlass.image
        searchButton.label = KDriveLocalizable.buttonSearch
        searchButton.target = nil
        searchButton.action = nil

        let group = NSToolbarItemGroup(itemIdentifier: .searchGroup)
        group.subitems = [searchButton]
        return group
    }

    // MARK: - Toolbar Actions

    @objc private func openHelpURL() {
        NSWorkspace.shared.open(URLConstants.help)
    }

    @objc private func togglePauseResume() {
        guard let synchro = viewModel.currentSynchro else { return }
        let syncDbId = Int32(synchro.dbId)

        Task {
            do {
                switch synchroStateObserver.synchroState.status {
                case .starting, .running, .idle:
                    try await SyncJobs().stopSync(syncDbId: syncDbId)
                case .pauseAsked, .paused, .stopAsked, .stopped, .error:
                    try await SyncJobs().startSync(syncDbId: syncDbId)
                }
            } catch {
                IKLogger.general.error("Failed to toggle sync pause/resume: \(error)")
            }
        }
    }

    // MARK: - Toolbar State Updates

    private func refreshPauseResumeToolbarItem(_ state: UISynchroState) {
        guard let toolbar = view.window?.toolbar else { return }

        for item in toolbar.items {
            if item.itemIdentifier == .syncControlsGroup, let group = item as? NSToolbarItemGroup {
                if let pauseResumeItem = group.subitems.first(where: { $0.itemIdentifier == .pauseResumeButton }) {
                    updatePauseResumeButton(pauseResumeItem, state: state)
                }
            }
        }
    }

    private func updatePauseResumeButton(_ item: NSToolbarItem, state: UISynchroState) {
        let hasBlockingError = viewModel.currentBlockingError != nil

        guard !hasBlockingError else {
            setPauseResumeAppearance(item, showPause: true, enabled: false)
            return
        }

        switch state.status {
        case .starting, .running, .idle:
            setPauseResumeAppearance(item, showPause: true, enabled: true)
        case .pauseAsked, .paused, .stopAsked, .stopped:
            setPauseResumeAppearance(item, showPause: false, enabled: true)
        case .error:
            setPauseResumeAppearance(item, showPause: true, enabled: false)
        }
    }

    private func setPauseResumeAppearance(_ item: NSToolbarItem, showPause: Bool, enabled: Bool) {
        let image = showPause ? KDriveResources.pause.image : KDriveResources.play.image
        let label = showPause ? KDriveLocalizable.buttonPause : KDriveLocalizable.buttonStart

        item.image = image
        item.label = label
        item.isEnabled = enabled
    }
}
