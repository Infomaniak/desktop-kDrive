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

struct SidebarItem {
    enum ItemType {
        case navigation
        case menu
    }

    let icon: ImageResource
    let title: String
    let type: ItemType

    var canBeSelected: Bool {
        return type == .navigation
    }
}

class SidebarViewController: NSViewController {
    private let items = [
        SidebarItem(
            icon: .house,
            title: KDriveLocalizable.tabTitleHome,
            type: .navigation
        ),
        SidebarItem(
            icon: .circularArrowsClockwise,
            title: KDriveLocalizable.tabTitleActivity,
            type: .navigation
        ),
        SidebarItem(
            icon: .hardDiskDrive,
            title: KDriveLocalizable.tabTitleStorage,
            type: .navigation
        ),
        SidebarItem(
            icon: .kdriveFoldersStacked,
            title: KDriveLocalizable.sidebarSectionAdvancedSync,
            type: .menu
        )
    ]

    override func viewDidLoad() {
        super.viewDidLoad()

        let outlineView = NSOutlineView()
        outlineView.translatesAutoresizingMaskIntoConstraints = false
        outlineView.dataSource = self
        outlineView.delegate = self
        if #available(macOS 11.0, *) {
            outlineView.style = .sourceList
        } else {
            outlineView.selectionHighlightStyle = .sourceList
        }
    }
}

// MARK: - NSOutlineViewDataSource

extension SidebarViewController: NSOutlineViewDataSource {}

// MARK: - NSOutlineViewDelegate

extension SidebarViewController: NSOutlineViewDelegate {}
