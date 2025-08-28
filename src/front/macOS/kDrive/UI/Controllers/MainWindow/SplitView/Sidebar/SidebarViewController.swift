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

protocol SidebarViewControllerDelegate: AnyObject {
    func sidebarViewController(_ controller: SidebarViewController, didSelectItem item: SidebarItem)
}

final class SidebarViewController: NSViewController {
    static let navigationCellIdentifier = NSUserInterfaceItemIdentifier("NavigationSidebarCell")

    weak var delegate: SidebarViewControllerDelegate?

    private var scrollView: NSScrollView!
    private var outlineView: ClickableOutlineView!

    private let items: [SidebarItem] = [.home, .activity, .storage, .kDriveFolder]

    override func viewDidLoad() {
        super.viewDidLoad()
        setupOutlineView()
    }

    private func setupOutlineView() {
        scrollView = NSScrollView()
        scrollView.translatesAutoresizingMaskIntoConstraints = false
        scrollView.hasVerticalScroller = true
        scrollView.autohidesScrollers = true
        scrollView.drawsBackground = false
        view.addSubview(scrollView)

        outlineView = ClickableOutlineView()
        outlineView.translatesAutoresizingMaskIntoConstraints = false
        outlineView.dataSource = self
        outlineView.delegate = self
        outlineView.focusRingType = .none
        outlineView.rowSizeStyle = .medium
        scrollView.documentView = outlineView
        if #available(macOS 11.0, *) {
            outlineView.style = .sourceList
        } else {
            outlineView.selectionHighlightStyle = .sourceList
        }
        outlineView.headerView = nil

        let singleColumn = NSTableColumn(identifier: NSUserInterfaceItemIdentifier("SidebarColumn"))
        singleColumn.isEditable = false
        outlineView.addTableColumn(singleColumn)
        outlineView.outlineTableColumn = singleColumn

        NSLayoutConstraint.activate([
            scrollView.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            scrollView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            scrollView.bottomAnchor.constraint(equalTo: view.bottomAnchor),
            scrollView.leadingAnchor.constraint(equalTo: view.leadingAnchor)
        ])
    }

    override func viewWillAppear() {
        super.viewWillAppear()
        outlineView.selectRowIndexes(IndexSet(integer: 0), byExtendingSelection: false)
    }

    override func viewDidLayout() {
        super.viewDidLayout()
        updateScrollViewElasticity()
    }

    private func updateScrollViewElasticity() {
        guard let documentView = scrollView.documentView else { return }

        let isDocumentViewSmallerThanScrollView = documentView.bounds.height <= scrollView.documentVisibleRect.height
        scrollView.verticalScrollElasticity = isDocumentViewSmallerThanScrollView ? .none : .automatic
    }

    private func showMenu(for item: SidebarItem) {
        guard item == .kDriveFolder else { return }

        let menu = NSMenu()
        outlineView.showMenu(menu, at: item)
    }
}

// MARK: - NSOutlineViewDataSource

extension SidebarViewController: NSOutlineViewDataSource {
    func outlineView(_ outlineView: NSOutlineView, child index: Int, ofItem item: Any?) -> Any {
        return items[index]
    }

    func outlineView(_ outlineView: NSOutlineView, isItemExpandable item: Any) -> Bool {
        return false
    }

    func outlineView(_ outlineView: NSOutlineView, numberOfChildrenOfItem item: Any?) -> Int {
        guard item == nil else { return 0 }
        return items.count
    }
}

// MARK: - OutlineViewDelegate

extension SidebarViewController: ClickableOutlineViewDelegate {
    func outlineView(_ outlineView: NSOutlineView, shouldSelectItem item: Any) -> Bool {
        guard let item = item as? SidebarItem else { return false }
        return item.canBeSelected
    }

    func outlineView(_ outlineView: NSOutlineView, viewFor tableColumn: NSTableColumn?, item: Any) -> NSView? {
        guard let item = item as? SidebarItem else { return nil }

        var cell = outlineView.makeView(withIdentifier: Self.navigationCellIdentifier, owner: self) as? SidebarTableCellView
        if cell == nil {
            cell = SidebarTableCellView()
            cell?.identifier = Self.navigationCellIdentifier
        }

        cell?.imageView?.image = NSImage(resource: item.icon)
        cell?.textField?.stringValue = item.title

        return cell
    }

    func outlineViewSelectionDidChange(_ notification: Notification) {
        guard let selectedItem = outlineView.item(atRow: outlineView.selectedRow) as? SidebarItem else { return }
        delegate?.sidebarViewController(self, didSelectItem: selectedItem)
    }

    func outlineView(_ outlineView: NSOutlineView, didClick item: Any?) {
        guard let item = item as? SidebarItem, item.type == .menu else { return }

        if item == .kDriveFolder {
            showMenu(for: item)
        }
    }
}
