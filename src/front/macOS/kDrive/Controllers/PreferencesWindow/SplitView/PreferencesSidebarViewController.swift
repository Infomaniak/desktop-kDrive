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

extension SidebarItem {
    static let general = SidebarItem(icon: NSImage(resource: .settings), title: KDriveLocalizable.sidebarItemGeneral)
    static let accounts = SidebarItem(icon: NSImage(resource: .persons), title: KDriveLocalizable.sidebarItemAccounts)
    static let advanced = SidebarItem(icon: NSImage(resource: .hammerWrench), title: KDriveLocalizable.sidebarItemAdvanced)
}

class PreferencesSidebarViewController: NSViewController {
    private var scrollView: NSScrollView!
    private var outlineView: NSOutlineView!

    override func viewDidLoad() {
        super.viewDidLoad()
        setupView()
    }

    override func viewWillAppear() {
        super.viewWillAppear()
        outlineView.selectRowIndexes(IndexSet(integer: 0), byExtendingSelection: false)
    }

    private func setupView() {
        outlineView = NSOutlineView()
        outlineView.translatesAutoresizingMaskIntoConstraints = false
//        outlineView.dataSource = self
//        outlineView.delegate = self
        outlineView.focusRingType = .none
        outlineView.rowSizeStyle = .medium
        outlineView.headerView = nil
        outlineView.style = .sourceList

        let singleColumn = NSTableColumn(identifier: NSUserInterfaceItemIdentifier("SidebarColumn"))
        singleColumn.isEditable = false
        outlineView.addTableColumn(singleColumn)
        outlineView.outlineTableColumn = singleColumn

        scrollView = NSScrollView()
        scrollView.translatesAutoresizingMaskIntoConstraints = false
        scrollView.hasVerticalScroller = true
        scrollView.autohidesScrollers = true
        scrollView.drawsBackground = false
        scrollView.documentView = outlineView
        view.addSubview(scrollView)

        NSLayoutConstraint.activate([
            scrollView.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            scrollView.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor),
            scrollView.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor),
            scrollView.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor)
        ])
    }
}
