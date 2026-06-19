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
import SwiftUI

public struct RemoteFolderTreeView: NSViewRepresentable {
    public let rootItems: [FileTreeItem]

    public let loadChildren: @MainActor (FileTreeItem) async -> [FileTreeItem]
    public let onSelectionChange: (FileTreeItem?) -> Void
    public let createFolder: @MainActor (_ parent: FileTreeItem, _ name: String) async -> FileTreeItem?

    public init(
        rootItems: [FileTreeItem],
        loadChildren: @escaping @MainActor (FileTreeItem) async -> [FileTreeItem],
        onSelectionChange: @escaping (FileTreeItem?) -> Void,
        createFolder: @escaping @MainActor (_ parent: FileTreeItem, _ name: String) async -> FileTreeItem?
    ) {
        self.rootItems = rootItems
        self.loadChildren = loadChildren
        self.onSelectionChange = onSelectionChange
        self.createFolder = createFolder
    }

    public func makeNSView(context: Context) -> RemoteFolderOutlineView {
        let view = RemoteFolderOutlineView()
        view.loadChildren = loadChildren
        view.onSelectionChange = onSelectionChange
        view.createFolder = createFolder
        view.setRootItems(rootItems)
        context.coordinator.appliedRootIDs = rootItems.map(\.id)
        return view
    }

    public func updateNSView(_ nsView: RemoteFolderOutlineView, context: Context) {
        nsView.loadChildren = loadChildren
        nsView.onSelectionChange = onSelectionChange
        nsView.createFolder = createFolder

        let ids = rootItems.map(\.id)
        if context.coordinator.appliedRootIDs != ids {
            context.coordinator.appliedRootIDs = ids
            nsView.setRootItems(rootItems)
        }
    }

    public func makeCoordinator() -> Coordinator {
        Coordinator()
    }

    public final class Coordinator {
        var appliedRootIDs: [String] = []
    }
}

final class RemoteFolderNode {
    let item: FileTreeItem
    weak var parent: RemoteFolderNode?

    var children: [RemoteFolderNode]?
    var isLoading = false

    let isPlaceholder: Bool
    let isEditing: Bool

    private(set) lazy var loadingPlaceholder = RemoteFolderNode(placeholderParent: self)

    init(item: FileTreeItem, parent: RemoteFolderNode?) {
        self.item = item
        self.parent = parent
        isPlaceholder = false
        isEditing = false
    }

    private init(placeholderParent: RemoteFolderNode) {
        item = FileTreeItem(id: "", name: "", size: nil, isFolder: false, isEnabled: false)
        parent = placeholderParent
        isPlaceholder = true
        isEditing = false
    }

    private init(editingParent: RemoteFolderNode) {
        item = FileTreeItem(id: "", name: "", size: nil, isFolder: true, isEnabled: false)
        parent = editingParent
        isPlaceholder = false
        isEditing = true
    }

    static func editingNode(parent: RemoteFolderNode) -> RemoteFolderNode {
        RemoteFolderNode(editingParent: parent)
    }

    var isFolder: Bool {
        return item.isFolder
    }
}

@MainActor
public final class RemoteFolderOutlineView: NSView {
    var loadChildren: (@MainActor (FileTreeItem) async -> [FileTreeItem])?
    var onSelectionChange: ((FileTreeItem?) -> Void)?
    var createFolder: (@MainActor (_ parent: FileTreeItem, _ name: String) async -> FileTreeItem?)?

    private let scrollView = NSScrollView()
    private let outlineView = NSOutlineView()

    private var rootNodes: [RemoteFolderNode] = []
    private weak var editingNode: RemoteFolderNode?

    private enum Column {
        static let name = NSUserInterfaceItemIdentifier("RemoteFolderTree.name")
        static let edit = NSUserInterfaceItemIdentifier("RemoteFolderTree.edit")
        static let shimmer = NSUserInterfaceItemIdentifier("RemoteFolderTree.shimmer")
    }

    override public init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        setupOutlineView()
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    func setRootItems(_ items: [FileTreeItem]) {
        editingNode = nil
        rootNodes = items.map { RemoteFolderNode(item: $0, parent: nil) }
        outlineView.reloadData()

        for node in rootNodes {
            outlineView.expandItem(node)
        }
    }

    private func setupOutlineView() {
        let nameColumn = NSTableColumn(identifier: Column.name)
        nameColumn.title = KDriveLocalizable.labelName
        nameColumn.minWidth = 180
        nameColumn.resizingMask = .autoresizingMask

        outlineView.addTableColumn(nameColumn)
        outlineView.outlineTableColumn = nameColumn

        outlineView.dataSource = self
        outlineView.delegate = self

        outlineView.usesAlternatingRowBackgroundColors = true
        outlineView.indentationPerLevel = AppPadding.padding16
        outlineView.indentationMarkerFollowsCell = true
        outlineView.rowSizeStyle = .default
        outlineView.allowsColumnReordering = false
        outlineView.allowsEmptySelection = true
        outlineView.allowsMultipleSelection = false
        outlineView.autoresizesOutlineColumn = false

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

    private func loadChildren(of node: RemoteFolderNode, completion: (() -> Void)? = nil) {
        guard let loadChildren else {
            node.children = []
            outlineView.reloadItem(node, reloadChildren: true)
            completion?()
            return
        }

        node.isLoading = true
        let item = node.item

        Task {
            let loadedItems = await loadChildren(item)
            node.isLoading = false

            let loadedNodes = loadedItems.map { RemoteFolderNode(item: $0, parent: node) }
            node.children = loadedNodes

            let wasExpanded = outlineView.isItemExpanded(node)
            outlineView.reloadItem(node, reloadChildren: true)
            if wasExpanded || !loadedNodes.isEmpty {
                outlineView.expandItem(node)
            }
            completion?()
        }
    }

    // MARK: - Folder creation

    private func beginFolderCreation(in node: RemoteFolderNode) {
        removeEditingNode()

        if node.children == nil, !node.isLoading {
            loadChildren(of: node) { [weak self] in
                self?.insertEditingNode(in: node)
            }
        } else {
            insertEditingNode(in: node)
        }
    }

    private func insertEditingNode(in node: RemoteFolderNode) {
        let editNode = RemoteFolderNode.editingNode(parent: node)
        if node.children == nil {
            node.children = []
        }
        node.children?.insert(editNode, at: 0)
        editingNode = editNode

        outlineView.reloadItem(node, reloadChildren: true)
        outlineView.expandItem(node)

        DispatchQueue.main.async { [weak self] in
            guard let self else { return }
            let row = outlineView.row(forItem: editNode)
            guard row >= 0 else { return }
            outlineView.scrollRowToVisible(row)
            let cell = outlineView.view(atColumn: 0, row: row, makeIfNecessary: true) as? RemoteFolderEditCell
            cell?.beginEditing()
        }
    }

    private func removeEditingNode() {
        guard let editingNode, let parent = editingNode.parent else { return }
        parent.children?.removeAll { $0 === editingNode }
        self.editingNode = nil
        outlineView.reloadItem(parent, reloadChildren: true)
    }

    private func commitFolderCreation(named name: String) {
        let trimmedName = name.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmedName.isEmpty else {
            removeEditingNode()
            return
        }
        guard let editingNode, let parent = editingNode.parent, let createFolder else {
            removeEditingNode()
            return
        }

        let parentItem = parent.item

        Task {
            let createdItem = await createFolder(parentItem, trimmedName)
            removeEditingNode()

            guard let createdItem else { return }

            let createdNode = RemoteFolderNode(item: createdItem, parent: parent)
            if parent.children == nil {
                parent.children = []
            }
            parent.children?.insert(createdNode, at: 0)
            outlineView.reloadItem(parent, reloadChildren: true)
            outlineView.expandItem(parent)

            let row = outlineView.row(forItem: createdNode)
            if row >= 0 {
                outlineView.selectRowIndexes(IndexSet(integer: row), byExtendingSelection: false)
                outlineView.scrollRowToVisible(row)
            }
        }
    }

    // MARK: - Cell factories

    private func makeNameCell(for node: RemoteFolderNode) -> NSView {
        let cell = outlineView.makeView(withIdentifier: Column.name, owner: self) as? RemoteFolderNameCell
            ?? RemoteFolderNameCell()
        cell.identifier = Column.name
        cell.configure(with: node.item) { [weak self, weak node] in
            guard let self, let node else { return }
            beginFolderCreation(in: node)
        }

        return cell
    }

    private func makeEditCell() -> NSView {
        let cell = outlineView.makeView(withIdentifier: Column.edit, owner: self) as? RemoteFolderEditCell
            ?? RemoteFolderEditCell()
        cell.identifier = Column.edit
        cell.configure { [weak self] name in
            self?.commitFolderCreation(named: name)
        } onCancel: { [weak self] in
            self?.removeEditingNode()
        }

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

extension RemoteFolderOutlineView: NSOutlineViewDataSource {
    public func outlineView(_ outlineView: NSOutlineView, numberOfChildrenOfItem item: Any?) -> Int {
        guard let node = item as? RemoteFolderNode else { return rootNodes.count }

        if let children = node.children {
            return children.count
        }
        return node.isLoading ? 1 : 0
    }

    public func outlineView(_ outlineView: NSOutlineView, child index: Int, ofItem item: Any?) -> Any {
        guard let node = item as? RemoteFolderNode else { return rootNodes[index] }

        if let children = node.children {
            return children[index]
        }
        return node.loadingPlaceholder
    }

    public func outlineView(_ outlineView: NSOutlineView, isItemExpandable item: Any) -> Bool {
        guard let node = item as? RemoteFolderNode, !node.isPlaceholder, !node.isEditing, node.isFolder else { return false }

        if let children = node.children {
            return !children.isEmpty
        }
        return true
    }
}

// MARK: - NSOutlineViewDelegate

extension RemoteFolderOutlineView: NSOutlineViewDelegate {
    public func outlineView(_ outlineView: NSOutlineView, viewFor tableColumn: NSTableColumn?, item: Any) -> NSView? {
        guard let node = item as? RemoteFolderNode else { return nil }

        if node.isPlaceholder {
            return makeShimmerCell()
        }
        if node.isEditing {
            return makeEditCell()
        }
        return makeNameCell(for: node)
    }

    public func outlineView(_ outlineView: NSOutlineView, shouldExpandItem item: Any) -> Bool {
        guard let node = item as? RemoteFolderNode, node.isFolder, !node.isPlaceholder, !node.isEditing else { return false }

        if node.children == nil, !node.isLoading {
            loadChildren(of: node)
        }
        return true
    }

    public func outlineView(_ outlineView: NSOutlineView, shouldSelectItem item: Any) -> Bool {
        guard let node = item as? RemoteFolderNode else { return false }
        return !node.isPlaceholder && !node.isEditing && node.item.isEnabled
    }

    public func outlineViewSelectionDidChange(_ notification: Notification) {
        let row = outlineView.selectedRow
        guard row >= 0, let node = outlineView.item(atRow: row) as? RemoteFolderNode else {
            onSelectionChange?(nil)
            return
        }
        onSelectionChange?(node.item)
    }
}
