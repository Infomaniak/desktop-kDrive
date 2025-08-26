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
    static let trackingSplitView = NSToolbarItem.Identifier("TrackingSplitView")
}

class RootViewController: NSSplitViewController {
    override func viewDidLoad() {
        super.viewDidLoad()
        setupSplitView()
    }

    override func viewWillAppear() {
        super.viewWillAppear()
        setupToolbar()
    }

    private func setupSplitView() {
        splitView.autosaveName = "SplitViewAutoSave"
        splitView.isVertical = true

        let sidebarViewController = SidebarViewController()
        let sidebarItem = NSSplitViewItem(sidebarWithViewController: sidebarViewController)
        sidebarItem.minimumThickness = 150
        sidebarItem.maximumThickness = 300
        addSplitViewItem(sidebarItem)

        let homeViewController = HomeViewController()
        let homeDetailItem = NSSplitViewItem(viewController: homeViewController)
        addSplitViewItem(homeDetailItem)
    }

    private func setupToolbar() {
        let toolbar = NSToolbar()
        toolbar.delegate = self
        toolbar.allowsUserCustomization = false
        toolbar.displayMode = .iconOnly
        view.window?.toolbar = toolbar
    }
}

// MARK: - NSToolbarDelegate

extension RootViewController: NSToolbarDelegate {
    func toolbarAllowedItemIdentifiers(_ toolbar: NSToolbar) -> [NSToolbarItem.Identifier] {
        return [.trackingSplitView]
    }

    func toolbarDefaultItemIdentifiers(_ toolbar: NSToolbar) -> [NSToolbarItem.Identifier] {
        return [.trackingSplitView]
    }

    func toolbar(_ toolbar: NSToolbar, itemForItemIdentifier itemIdentifier: NSToolbarItem.Identifier, willBeInsertedIntoToolbar flag: Bool) -> NSToolbarItem? {
        guard itemIdentifier == .trackingSplitView else { return nil }
        return NSTrackingSeparatorToolbarItem(identifier: .trackingSplitView, splitView: splitView, dividerIndex: 0)
    }
}
