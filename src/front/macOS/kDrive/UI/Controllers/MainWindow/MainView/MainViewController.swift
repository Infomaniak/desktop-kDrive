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
    static let searchTextField = NSToolbarItem.Identifier("SearchTextField")
}

final class MainViewController: IKSplitViewController {
    @LazyInjectService private var router: MainViewRouter

    private let viewModel = MainViewModel()

    private var bindStore = Set<AnyCancellable>()

    override func viewDidLoad() {
        super.viewDidLoad()

        setupSplitView()
        bindViewModel()
        viewModel.refreshCache()
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
            contentViewController = ActivitiesViewController(mainViewModel: viewModel)
        case .storage:
            contentViewController = StorageViewController()
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
        initialItems.append(.searchTextField)

        return initialItems
    }

    override func toolbarDefaultItemIdentifiers(_ toolbar: NSToolbar) -> [NSToolbarItem.Identifier] {
        var initialItems = super.toolbarDefaultItemIdentifiers(toolbar)
        initialItems.append(.searchTextField)

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
        case .searchTextField:
            return NSSearchToolbarItem(itemIdentifier: .searchTextField)
        default:
            return nil
        }
    }
}
