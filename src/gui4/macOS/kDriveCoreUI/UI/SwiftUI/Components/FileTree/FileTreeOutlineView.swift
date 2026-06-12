/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2026 Infomaniak Network SA

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

import AppKit
import kDriveResources

final class FileTreeNode {
    let item: FileTreeItem
    weak var parent: FileTreeNode?

    var children: [FileTreeNode]?
    var isLoading = false

    let isPlaceholder: Bool

    private(set) lazy var loadingPlaceholder = FileTreeNode(placeholderParent: self)

    init(item: FileTreeItem, parent: FileTreeNode?) {
        self.item = item
        self.parent = parent
        isPlaceholder = false
    }

    private init(placeholderParent: FileTreeNode) {
        item = FileTreeItem(id: "", name: "", size: nil, isFolder: false, isEnabled: false)
        parent = placeholderParent
        isPlaceholder = true
    }

    var isFolder: Bool {
        return item.isFolder
    }
}

@MainActor
public final class FileTreeOutlineView: NSView {
    public var loadChildren: ((FileTreeItem) async -> [FileTreeItem])?
    public var onBlacklistChange: ((Set<String>) -> Void)?

    private let scrollView = NSScrollView()
    private let outlineView = NSOutlineView()
    private let tableHeaderView = FileTreeHeaderView()

    private var rootNodes: [FileTreeNode] = []
    private var blacklist: Set<String> = []

    private enum Column {
        static let checkbox = NSUserInterfaceItemIdentifier("FileTree.checkbox")
        static let name = NSUserInterfaceItemIdentifier("FileTree.name")
        static let size = NSUserInterfaceItemIdentifier("FileTree.size")
        static let shimmer = NSUserInterfaceItemIdentifier("FileTree.shimmer")
    }

    override public init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        setupOutlineView()
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    public func setRootItems(_ items: [FileTreeItem], initialBlacklist: Set<String>) {
        blacklist = initialBlacklist
        rootNodes = items.map { FileTreeNode(item: $0, parent: nil) }
        outlineView.reloadData()
        updateHeaderCheckbox()
    }

    private func setupOutlineView() {
        let checkboxColumn = NSTableColumn(identifier: Column.checkbox)
        checkboxColumn.title = ""
        checkboxColumn.width = 28
        checkboxColumn.minWidth = 28
        checkboxColumn.maxWidth = 28
        checkboxColumn.resizingMask = []

        let nameColumn = NSTableColumn(identifier: Column.name)
        nameColumn.title = KDriveLocalizable.labelName
        nameColumn.minWidth = 180
        nameColumn.resizingMask = .autoresizingMask

        let sizeColumn = NSTableColumn(identifier: Column.size)
        sizeColumn.title = KDriveLocalizable.labelSize
        sizeColumn.width = 100
        sizeColumn.minWidth = 70
        sizeColumn.resizingMask = .userResizingMask

        outlineView.addTableColumn(checkboxColumn)
        outlineView.addTableColumn(nameColumn)
        outlineView.addTableColumn(sizeColumn)

        outlineView.outlineTableColumn = nameColumn

        outlineView.dataSource = self
        outlineView.delegate = self

        outlineView.usesAlternatingRowBackgroundColors = true
        outlineView.indentationPerLevel = AppPadding.padding16
        outlineView.indentationMarkerFollowsCell = true
        outlineView.rowSizeStyle = .default
        outlineView.allowsColumnReordering = false
        outlineView.autoresizesOutlineColumn = false

        tableHeaderView.checkboxColumnIndex = outlineView.column(withIdentifier: Column.checkbox)
        tableHeaderView.checkbox.target = self
        tableHeaderView.checkbox.action = #selector(headerCheckboxToggled)
        outlineView.headerView = tableHeaderView

        if #available(macOS 11.0, *) {
            outlineView.style = .inset
        }

        scrollView.documentView = outlineView
        scrollView.hasVerticalScroller = true
        scrollView.autohidesScrollers = true
        scrollView.translatesAutoresizingMaskIntoConstraints = false
        addSubview(scrollView)

        NSLayoutConstraint.activate([
            scrollView.topAnchor.constraint(equalTo: topAnchor),
            scrollView.bottomAnchor.constraint(equalTo: bottomAnchor),
            scrollView.leadingAnchor.constraint(equalTo: leadingAnchor),
            scrollView.trailingAnchor.constraint(equalTo: trailingAnchor)
        ])
    }

    // MARK: - Lazy loading

    private func loadChildren(of node: FileTreeNode) {
        guard let loadChildren else {
            node.children = []
            outlineView.reloadItem(node, reloadChildren: true)
            return
        }

        node.isLoading = true
        let item = node.item

        Task {
            let loadedItems = await loadChildren(item)
            node.isLoading = false

            let loadedNodes = loadedItems.map { FileTreeNode(item: $0, parent: node) }
            node.children = loadedNodes

            guard !loadedNodes.isEmpty else {
                outlineView.collapseItem(node)
                outlineView.reloadItem(node, reloadChildren: true)
                refreshSelectionDisplay()
                return
            }

            let wasExpanded = outlineView.isItemExpanded(node)
            outlineView.reloadItem(node, reloadChildren: true)
            if wasExpanded {
                outlineView.expandItem(node)
            }
            refreshSelectionDisplay()
        }
    }

    // MARK: - Derived checkbox state

    private func displayState(of node: FileTreeNode) -> NSControl.StateValue {
        effectiveState(of: node, ancestorExcluded: isAncestorExcluded(node))
    }

    private func isAncestorExcluded(_ node: FileTreeNode) -> Bool {
        var parent = node.parent
        while let current = parent {
            if blacklist.contains(current.item.id) {
                return true
            }
            parent = current.parent
        }
        return false
    }

    private func effectiveState(of node: FileTreeNode, ancestorExcluded: Bool) -> NSControl.StateValue {
        guard node.item.isEnabled else { return .off }

        let selfExcluded = ancestorExcluded || blacklist.contains(node.item.id)

        guard let children = node.children, !children.isEmpty else {
            return selfExcluded ? .off : .on
        }

        var sawOn = false
        var sawOff = false
        for child in children {
            guard child.item.isEnabled else {
                continue
            }

            switch effectiveState(of: child, ancestorExcluded: selfExcluded) {
            case .on:
                sawOn = true
            case .off:
                sawOff = true
            default:
                sawOn = true
                sawOff = true
            }
            if sawOn, sawOff {
                return .mixed
            }
        }
        return sawOn ? .on : .off
    }

    // MARK: - Selection mutation

    @objc private func checkboxToggled(_ sender: NSButton) {
        let row = outlineView.row(for: sender)
        guard row >= 0, let node = outlineView.item(atRow: row) as? FileTreeNode else { return }
        toggleSelection(of: node)
    }

    private func toggleSelection(of node: FileTreeNode) {
        guard node.item.isEnabled else { return }

        let select = displayState(of: node) != .on
        setSelected(select, for: node)

        notifyBlacklistChange()
        refreshSelectionDisplay()
    }

    private func setSelected(_ select: Bool, for node: FileTreeNode) {
        pushDownAncestorExclusions(towards: node)

        if select {
            removeFromBlacklist(node)
        } else {
            removeDescendantsFromBlacklist(node)
            blacklist.insert(node.item.id)
        }
    }

    private func pushDownAncestorExclusions(towards node: FileTreeNode) {
        var path: [FileTreeNode] = []
        var parent = node.parent
        while let current = parent {
            path.append(current)
            parent = current.parent
        }

        for ancestor in path.reversed() where blacklist.contains(ancestor.item.id) {
            blacklist.remove(ancestor.item.id)
            for child in ancestor.children ?? [] {
                blacklist.insert(child.item.id)
            }
        }
    }

    private func removeFromBlacklist(_ node: FileTreeNode) {
        blacklist.remove(node.item.id)
        removeDescendantsFromBlacklist(node)
    }

    private func removeDescendantsFromBlacklist(_ node: FileTreeNode) {
        for child in node.children ?? [] {
            removeFromBlacklist(child)
        }
    }

    // MARK: - Header "select / deselect all"

    @objc private func headerCheckboxToggled() {
        if headerState() == .on {
            blacklist = Set(rootNodes.map(\.item.id))
        } else {
            blacklist.removeAll()
        }

        notifyBlacklistChange()
        refreshSelectionDisplay()
    }

    private func headerState() -> NSControl.StateValue {
        guard !rootNodes.isEmpty else { return .off }

        var sawOn = false
        var sawOff = false
        for node in rootNodes {
            switch displayState(of: node) {
            case .on:
                sawOn = true
            case .off:
                sawOff = true
            default:
                sawOn = true
                sawOff = true
            }
            if sawOn, sawOff {
                return .mixed
            }
        }
        return sawOn ? .on : .off
    }

    // MARK: - Refresh & notification

    private func refreshSelectionDisplay() {
        let checkboxColumnIndex = outlineView.column(withIdentifier: Column.checkbox)
        if checkboxColumnIndex >= 0, outlineView.numberOfRows > 0 {
            outlineView.reloadData(
                forRowIndexes: IndexSet(integersIn: 0 ..< outlineView.numberOfRows),
                columnIndexes: IndexSet(integer: checkboxColumnIndex)
            )
        }
        updateHeaderCheckbox()
    }

    private func updateHeaderCheckbox() {
        tableHeaderView.checkbox.allowsMixedState = true
        tableHeaderView.checkbox.state = headerState()
    }

    private func notifyBlacklistChange() {
        onBlacklistChange?(blacklist)
    }

    // MARK: - Cell factories

    private func makeCheckboxCell(for node: FileTreeNode) -> NSView {
        let cell = outlineView
            .makeView(withIdentifier: Column.checkbox, owner: self) as? FileTreeCheckboxCell ?? FileTreeCheckboxCell()
        cell.identifier = Column.checkbox
        cell.configure(
            state: displayState(of: node),
            isEnabled: node.item.isEnabled,
            target: self,
            action: #selector(checkboxToggled(_:))
        )

        return cell
    }

    private func makeNameCell(for node: FileTreeNode) -> NSView {
        let cell = outlineView.makeView(withIdentifier: Column.name, owner: self) as? FileTreeNameCell ?? FileTreeNameCell()
        cell.identifier = Column.name
        cell.configure(with: node.item)

        return cell
    }

    private func makeSizeCell(for node: FileTreeNode) -> NSView {
        let cell = outlineView.makeView(withIdentifier: Column.size, owner: self) as? FileTreeSizeCell ?? FileTreeSizeCell()
        cell.identifier = Column.size
        cell.configure(with: node.item)

        return cell
    }

    private func makeShimmerCell() -> NSView {
        let cell = outlineView
            .makeView(withIdentifier: Column.shimmer, owner: self) as? FileTreeShimmerCell ?? FileTreeShimmerCell()
        cell.identifier = Column.shimmer

        return cell
    }
}

// MARK: - NSOutlineViewDataSource

extension FileTreeOutlineView: NSOutlineViewDataSource {
    public func outlineView(_ outlineView: NSOutlineView, numberOfChildrenOfItem item: Any?) -> Int {
        guard let node = item as? FileTreeNode else { return rootNodes.count }

        if let children = node.children {
            return children.count
        }
        return node.isLoading ? 1 : 0
    }

    public func outlineView(_ outlineView: NSOutlineView, child index: Int, ofItem item: Any?) -> Any {
        guard let node = item as? FileTreeNode else { return rootNodes[index] }

        if let children = node.children {
            return children[index]
        }
        return node.loadingPlaceholder
    }

    public func outlineView(_ outlineView: NSOutlineView, isItemExpandable item: Any) -> Bool {
        guard let node = item as? FileTreeNode, !node.isPlaceholder, node.isFolder else { return false }

        if let children = node.children {
            return !children.isEmpty
        }
        return true
    }
}

// MARK: - NSOutlineViewDelegate

extension FileTreeOutlineView: NSOutlineViewDelegate {
    public func outlineView(_ outlineView: NSOutlineView, viewFor tableColumn: NSTableColumn?, item: Any) -> NSView? {
        guard let node = item as? FileTreeNode, let column = tableColumn else { return nil }

        if node.isPlaceholder {
            return column.identifier == Column.name ? makeShimmerCell() : nil
        }

        switch column.identifier {
        case Column.checkbox:
            return makeCheckboxCell(for: node)
        case Column.name:
            return makeNameCell(for: node)
        case Column.size:
            return makeSizeCell(for: node)
        default:
            return nil
        }
    }

    public func outlineView(_ outlineView: NSOutlineView, shouldExpandItem item: Any) -> Bool {
        guard let node = item as? FileTreeNode, node.isFolder, !node.isPlaceholder, node.item.isEnabled else { return false }

        if node.children == nil, !node.isLoading {
            loadChildren(of: node)
        }
        return true
    }

    public func outlineView(_ outlineView: NSOutlineView, shouldSelectItem item: Any) -> Bool {
        (item as? FileTreeNode)?.isPlaceholder == false
    }
}
