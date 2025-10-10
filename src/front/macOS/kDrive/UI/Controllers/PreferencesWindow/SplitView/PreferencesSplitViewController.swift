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
import kDriveCoreUI

final class PreferencesSplitViewController: IKSplitViewController {
    static let sidebarWidth: CGFloat = 200

    override func viewDidLoad() {
        super.viewDidLoad()
        setupSplitView()
    }

    private func setupSplitView() {
        splitView.autosaveName = "PreferencesSplitViewAutoSave"
        splitView.isVertical = true

        let sidebarViewController = PreferencesSidebarViewController()
        sidebarViewController.delegate = self
        let sidebarItem = NSSplitViewItem(sidebarWithViewController: sidebarViewController)
        sidebarItem.minimumThickness = Self.sidebarWidth
        sidebarItem.maximumThickness = Self.sidebarWidth
        addSplitViewItem(sidebarItem)

        let generalViewController = NSViewController()
        let contentItem = NSSplitViewItem(viewController: generalViewController)
        addSplitViewItem(contentItem)
    }
}

// MARK: - NavigableSidebarViewControllerDelegate

extension PreferencesSplitViewController: NavigableSidebarViewControllerDelegate {
    func sidebarViewController(_ controller: NSViewController, didSelectItem item: kDriveCoreUI.SidebarItem) {
        var contentViewController: NSViewController
        switch item {
        case .general:
            contentViewController = GeneralPreferencesViewController()
        case .accounts:
            contentViewController = AccountsPreferencesViewController()
        case .advanced:
            contentViewController = AdvancedPreferencesViewController()
        default:
            fatalError("Destination \(item) not handled")
        }

        switchContentViewController(destination: contentViewController)
    }
}
