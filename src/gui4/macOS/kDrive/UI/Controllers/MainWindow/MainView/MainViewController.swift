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
import SwiftUI

extension NSToolbarItem.Identifier {
    static let supportGroup = NSToolbarItem.Identifier("SupportGroup")
    static let syncControlsGroup = NSToolbarItem.Identifier("SyncControlsGroup")
    static let searchGroup = NSToolbarItem.Identifier("SearchGroup")
    static let pauseResumeButton = NSToolbarItem.Identifier("PauseResumeButton")
}

final class MainViewController: IKSplitViewController {
    @LazyInjectService private var router: MainViewRouter

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

        bindToolbarToViewModel()
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

    func onPathChange(_ path: Path) {
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
        helpButton.image = NSImage(systemSymbolName: "questionmark.circle", accessibilityDescription: "Help")
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
        updatePauseResumeButton(pauseResumeButton)

        let settingsButton = NSToolbarItem(itemIdentifier: .init("SettingsButton"))
        settingsButton.image = NSImage(systemSymbolName: "gearshape", accessibilityDescription: "Settings")
        settingsButton.label = "Settings"
        settingsButton.target = nil
        settingsButton.action = #selector(AppDelegate.openPreferencesWindow(_:))

        let group = NSToolbarItemGroup(itemIdentifier: .syncControlsGroup)
        group.subitems = [pauseResumeButton, settingsButton]
        return group
    }

    private func makeSearchGroup() -> NSToolbarItemGroup {
        let searchButton = NSToolbarItem(itemIdentifier: .init("SearchButton"))
        searchButton.image = NSImage(systemSymbolName: "magnifyingglass", accessibilityDescription: "Search")
        searchButton.label = "Search"
        searchButton.target = nil
        searchButton.action = nil

        let group = NSToolbarItemGroup(itemIdentifier: .searchGroup)
        group.subitems = [searchButton]
        return group
    }

    // MARK: - Toolbar Actions

    @objc private func openHelpURL() {
        NSWorkspace.shared.open(URLConstants.helpURL)
    }

    @objc private func togglePauseResume() {
        guard let synchro = viewModel.currentSynchro else { return }
        let syncDbId = Int32(synchro.dbId)

        Task {
            do {
                let status = synchro.progressInfo?.status
                if status == .running || status == .starting {
                    try await SyncJobs().stopSync(syncDbId: syncDbId)
                } else {
                    try await SyncJobs().startSync(syncDbId: syncDbId)
                }
            } catch {
                IKLogger.general.error("Failed to toggle sync pause/resume: \(error)")
            }
        }
    }

    // MARK: - Toolbar State Updates

    private func bindToolbarToViewModel() {
        viewModel.$currentSynchroContext
            .receiveOnMain(store: &bindStore) { [weak self] _ in
                self?.refreshPauseResumeToolbarItem()
            }
    }

    private func refreshPauseResumeToolbarItem() {
        guard let toolbar = view.window?.toolbar else { return }
        for item in toolbar.items {
            if item.itemIdentifier == .syncControlsGroup, let group = item as? NSToolbarItemGroup {
                if let pauseResumeItem = group.subitems.first(where: { $0.itemIdentifier == .pauseResumeButton }) {
                    updatePauseResumeButton(pauseResumeItem)
                }
            }
        }
    }

    private func updatePauseResumeButton(_ item: NSToolbarItem) {
        guard let synchro = viewModel.currentSynchro else {
            setPauseResumeAppearance(item, showPause: true, enabled: false)
            return
        }

        let status = synchro.progressInfo?.status
        let hasBlockingError = viewModel.currentBlockingError != nil

        if hasBlockingError {
            setPauseResumeAppearance(item, showPause: true, enabled: false)
        } else if status == .running || status == .starting {
            setPauseResumeAppearance(item, showPause: true, enabled: true)
        } else {
            setPauseResumeAppearance(item, showPause: false, enabled: true)
        }
    }

    private func setPauseResumeAppearance(_ item: NSToolbarItem, showPause: Bool, enabled: Bool) {
        let symbolName = showPause ? "pause.circle" : "play.circle"
        let label = showPause ? "Pause" : "Resume"
        item.image = NSImage(systemSymbolName: symbolName, accessibilityDescription: label)
        item.label = label
        item.isEnabled = enabled
    }
}
