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
import kDriveCoreUI

extension NSToolbarItem.Identifier {
    static let searchTextField = NSToolbarItem.Identifier("SearchTextField")
}

final class MainSplitViewController: IKSplitViewController {
    override func viewDidLoad() {
        super.viewDidLoad()
        setupSplitView()
    }

    override func viewDidAppear() {
        super.viewDidAppear()

        configureWindowAppearance()
        splitView.setPosition(200, ofDividerAt: 0)
    }

    private func configureWindowAppearance() {
        guard let window = view.window else { return }

        window.titlebarAppearsTransparent = false
        window.toolbarStyle = .unified
        window.styleMask.insert(.fullSizeContentView)
    }

    private func setupSplitView() {
        splitView.autosaveName = "MainSplitViewAutoSave"
        splitView.isVertical = true

        let sidebarViewController = MainSidebarViewController()
        sidebarViewController.delegate = self
        let sidebarItem = NSSplitViewItem(sidebarWithViewController: sidebarViewController)
        sidebarItem.minimumThickness = 150
        sidebarItem.maximumThickness = 250
        addSplitViewItem(sidebarItem)

        let homeViewController = HomeViewController(toolbarTitle: SidebarItem.home.title)
        let homeDetailItem = NSSplitViewItem(viewController: homeViewController)
        addSplitViewItem(homeDetailItem)
    }
}

// MARK: - NavigableSidebarViewControllerDelegate

extension MainSplitViewController: NavigableSidebarViewControllerDelegate {
    func sidebarViewController(_ controller: NSViewController, didSelectItem item: SidebarItem) {
        var contentViewController: NSViewController
        switch item {
        case .home:
            contentViewController = HomeViewController()
        case .activity:
            contentViewController = ActivityViewController()
        case .storage:
            contentViewController = StorageViewController()
        default:
            fatalError("Destination \(item) not handled")
        }

        switchContentViewController(destination: contentViewController)
    }
}

// MARK: - NSToolbarDelegate

extension MainSplitViewController {
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
