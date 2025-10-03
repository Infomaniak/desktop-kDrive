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
import kDriveCore
import kDriveCoreUI

extension SidebarItem {
    static let home = SidebarItem(
        icon: NSImage(resource: .house),
        title: KDriveLocalizable.tabTitleHome
    )
    static let activity = SidebarItem(
        icon: NSImage(resource: .circularArrowsClockwise),
        title: KDriveLocalizable.tabTitleActivity
    )
    static let storage = SidebarItem(
        icon: NSImage(resource: .hardDiskDrive),
        title: KDriveLocalizable.tabTitleStorage
    )
    static let kDriveFolder = SidebarItem(
        icon: NSImage(resource: .kdriveFoldersStacked),
        title: KDriveLocalizable.sidebarItemKDriveTitle,
        type: .menu
    )
}

final class MainSidebarViewController: NSViewController {
    static let navigationCellIdentifier = NSUserInterfaceItemIdentifier(String(describing: SidebarTableCellView.self))

    weak var delegate: NavigableSidebarViewControllerDelegate?

    private var scrollView: NSScrollView!
    private var outlineView: ClickableOutlineView!
    private var popUpButton: ColoredPopUpButton!

    private let items: [SidebarItem] = [.home, .activity, .storage, .kDriveFolder]

    override func viewDidLoad() {
        super.viewDidLoad()
        setupView()
    }

    private func setupView() {
        let headerView = SidebarHeaderView()
        headerView.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(headerView)

        setupPopUpButton()
        setupScrollAndOutlineView()

        NSLayoutConstraint.activate([
            headerView.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            headerView.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: 16),
            headerView.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -16),

            popUpButton.topAnchor.constraint(equalTo: headerView.bottomAnchor, constant: 16),
            popUpButton.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: 16),
            popUpButton.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -16),

            scrollView.topAnchor.constraint(equalTo: popUpButton.bottomAnchor, constant: 16),
            scrollView.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor),
            scrollView.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor),
            scrollView.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor)
        ])
    }

    private func setupPopUpButton() {
        popUpButton = ColoredPopUpButton()
        popUpButton.translatesAutoresizingMaskIntoConstraints = false
        popUpButton.action = #selector(didSelectDrive)

        let drives = [
            UIDrive(id: 1, name: "Infomaniak", color: .systemGreen),
            UIDrive(id: 2, name: "Tim Cook et ses amis", color: .systemBlue)
        ]

        for drive in drives {
            popUpButton.addItem(withTitle: drive.name, image: NSImage(resource: .kdriveFoldersStacked), color: drive.color)
        }

        view.addSubview(popUpButton)
    }

    @objc func didSelectDrive() {
        // TODO: Handle updated drive
    }

    private func setupScrollAndOutlineView() {
        outlineView = ClickableOutlineView()
        outlineView.translatesAutoresizingMaskIntoConstraints = false
        outlineView.dataSource = self
        outlineView.delegate = self
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
}

// MARK: - NSOutlineViewDataSource

extension MainSidebarViewController: NSOutlineViewDataSource {
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

extension MainSidebarViewController: ClickableOutlineViewDelegate {
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

        cell?.setupForItem(item)
        return cell
    }

    func outlineViewSelectionDidChange(_ notification: Notification) {
        guard let selectedItem = outlineView.item(atRow: outlineView.selectedRow) as? SidebarItem else { return }
        delegate?.sidebarViewController(self, didSelectItem: selectedItem)
    }

    func outlineView(_ outlineView: NSOutlineView, didClick item: Any?) {
        guard let item = item as? SidebarItem,
              item.type == .menu,
              let menu = createMenu(forItem: item)
        else { return }

        (outlineView as? ClickableOutlineView)?.showMenu(menu, at: item)
    }
}

// MARK: - Menu actions

extension MainSidebarViewController {
    private func createMenu(forItem item: SidebarItem) -> NSMenu? {
        guard item == .kDriveFolder else { return nil }

        let menu = NSMenu()
        menu.autoenablesItems = false

        let openInFinder = NSMenuItem(
            title: KDriveLocalizable.buttonOpenInFinder,
            action: #selector(openInFinder),
            keyEquivalent: ""
        )
        if #available(macOS 26.0, *) {
            openInFinder.image = NSImage(systemSymbolName: "folder", accessibilityDescription: nil)
        }
        menu.addItem(openInFinder)

        let openInBrowser = NSMenuItem(
            title: KDriveLocalizable.buttonOpenInBrowser,
            action: #selector(openInBrowser),
            keyEquivalent: ""
        )
        if #available(macOS 26.0, *) {
            openInBrowser.image = NSImage(systemSymbolName: "network", accessibilityDescription: nil)
        }
        menu.addItem(openInBrowser)

        return menu
    }

    @objc private func openInFinder() {
        // TODO: Open Finder
    }

    @objc private func openInBrowser() {
        // TODO: Open kDrive web
    }
}
