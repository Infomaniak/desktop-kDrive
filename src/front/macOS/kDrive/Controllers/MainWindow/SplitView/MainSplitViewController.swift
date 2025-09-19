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

extension NSToolbarItem.Identifier {
    static let searchTextField = NSToolbarItem.Identifier("SearchTextField")
}

final class MainSplitViewController: IKSplitViewController {
    override func viewDidLoad() {
        super.viewDidLoad()
        setupSplitView()
    }

    private func setupSplitView() {
        splitView.autosaveName = "SplitViewAutoSave"
        splitView.isVertical = true

        let sidebarViewController = MainSidebarViewController()
        sidebarViewController.delegate = self
        let sidebarItem = NSSplitViewItem(sidebarWithViewController: sidebarViewController)
        sidebarItem.minimumThickness = 150
        sidebarItem.maximumThickness = 300
        addSplitViewItem(sidebarItem)

        let homeViewController = HomeViewController(toolbarTitle: SidebarItem.home.title)
        let homeDetailItem = NSSplitViewItem(viewController: homeViewController)
        addSplitViewItem(homeDetailItem)
    }
}

// MARK: - SidebarViewControllerDelegate

extension MainSplitViewController: SidebarViewControllerDelegate {
    func sidebarViewController(_ controller: MainSidebarViewController, didSelectItem item: SidebarItem) {
        var contentViewController: NSViewController
        switch item {
        case .home:
            contentViewController = HomeViewController(toolbarTitle: item.title)
        case .activity:
            contentViewController = ActivityViewController(toolbarTitle: item.title)
        case .storage:
            contentViewController = StorageViewController(toolbarTitle: item.title)
        default:
            fatalError("Destination not handled")
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
