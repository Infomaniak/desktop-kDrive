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

public extension NSToolbarItem.Identifier {
    static let trackingSplitView = NSToolbarItem.Identifier("TrackingSplitView")
}

open class IKSplitViewController: NSSplitViewController {
    public var toolbar = NSToolbar()

    override open func viewWillAppear() {
        super.viewWillAppear()
        setupToolbar()
    }

    private func setupToolbar() {
        toolbar.delegate = self
        toolbar.allowsUserCustomization = false
        toolbar.displayMode = .iconOnly
        view.window?.toolbar = toolbar
    }
}

// MARK: - Helpers

public extension IKSplitViewController {
    func switchContentViewController(destination viewController: NSViewController) {
        removeSplitViewItem(splitViewItems[1])
        addSplitViewItem(NSSplitViewItem(viewController: viewController))
    }
}

// MARK: - NSToolbarDelegate

extension IKSplitViewController: NSToolbarDelegate {
    open func toolbarAllowedItemIdentifiers(_ toolbar: NSToolbar) -> [NSToolbarItem.Identifier] {
        return [.trackingSplitView]
    }

    open func toolbarDefaultItemIdentifiers(_ toolbar: NSToolbar) -> [NSToolbarItem.Identifier] {
        return [.trackingSplitView]
    }

    open func toolbar(
        _ toolbar: NSToolbar,
        itemForItemIdentifier itemIdentifier: NSToolbarItem.Identifier,
        willBeInsertedIntoToolbar flag: Bool
    ) -> NSToolbarItem? {
        switch itemIdentifier {
        case .trackingSplitView:
            return NSTrackingSeparatorToolbarItem(identifier: .trackingSplitView, splitView: splitView, dividerIndex: 0)
        default:
            return nil
        }
    }
}
