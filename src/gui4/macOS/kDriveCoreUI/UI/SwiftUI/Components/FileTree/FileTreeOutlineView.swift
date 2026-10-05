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
import InfomaniakConcurrency
import InfomaniakDI
import kDriveResources

final class FileTreeNode {
    private(set) var item: FileTreeItem
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

    func updateSize(_ size: Int64?) {
        item = FileTreeItem(
            id: item.id,
            name: item.name,
            path: item.path,
            size: size,
            isFolder: item.isFolder,
            isEnabled: item.isEnabled
        )
    }
}

@MainActor
public final class FileTreeOutlineView: NSView {
    private static let maxNetworkingParallelism = 4

    public var childrenFetcher: FileTreeChildrenFetcher?
    public var onBlacklistChange: ((Set<String>) -> Void)?

    private let scrollView = NSScrollView()
    private let outlineView = NSOutlineView()
    private let tableHeaderView = FileTreeHeaderView()

    private var rootNodes: [FileTreeNode] = []
    private var selectionState = FileTreeSelectionState(initialBlacklist: [])

    private var loadTasks: [String: Task<Void, Never>] = [:]
    private var sizeTasks: [String: Task<Void, Never>] = [:]
    private var excludedPathsTask: Task<Void, Never>?

    // Cache of the paths resolved for a blacklist, so that reapplying the same blacklist (e.g. when
    // the root items are reloaded) does not trigger redundant network calls nor pending states again.
    private var resolvedExcludedNodePaths: [String: String] = [:]
    private var resolvedExcludedNodePathsFor: Set<String> = []

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

    /// - Parameter initialBlacklist: `nil` while the blacklist has not been provided by the caller
    ///   yet (e.g. still being fetched): every checkbox state is then unknown and displayed as
    ///   pending. An empty set means that nothing is excluded and states are definitive.
    public func setRootItems(_ items: [FileTreeItem], initialBlacklist: Set<String>?) {
        cancelLoadingTasks()

        rootNodes = items.map { FileTreeNode(item: $0, parent: nil) }

        if let initialBlacklist {
            if initialBlacklist == resolvedExcludedNodePathsFor {
                // The paths of this blacklist were already resolved: the checkbox states are known.
                selectionState = FileTreeSelectionState(
                    initialBlacklist: initialBlacklist,
                    excludedNodePaths: resolvedExcludedNodePaths
                )
            } else if !initialBlacklist.isEmpty {
                // Until the paths of the blacklisted folders are resolved, the checkbox state of an
                // unloaded folder is unknown: pending folders will display a loader instead of a
                // possibly wrong checkbox state.
                selectionState = FileTreeSelectionState(initialBlacklist: initialBlacklist, isResolvingExcludedPaths: true)
            } else {
                // Empty blacklist: every folder is selected.
                selectionState = FileTreeSelectionState(initialBlacklist: [])
            }
        } else {
            // The blacklist is not known yet: every checkbox state is unknown.
            selectionState = FileTreeSelectionState(initialBlacklist: [], isResolvingExcludedPaths: true)
        }

        outlineView.reloadData()
        updateHeaderCheckbox()

        guard let fetcher = childrenFetcher else { return }
        loadSizes(for: rootNodes.filter(\.isFolder), using: fetcher)

        if selectionState.isResolvingExcludedPaths, let initialBlacklist, !initialBlacklist.isEmpty {
            resolveExcludedNodePaths(for: initialBlacklist, using: fetcher)
        }
    }

    deinit {
        loadTasks.values.forEach { $0.cancel() }
        sizeTasks.values.forEach { $0.cancel() }
        excludedPathsTask?.cancel()
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
        guard let fetcher = childrenFetcher else {
            node.children = []
            outlineView.reloadItem(node, reloadChildren: true)
            return
        }

        node.isLoading = true

        let task = Task { [weak self, weak node] in
            guard let self, let node else { return }
            defer { self.loadTasks.removeValue(forKey: node.item.id) }

            let loadedItems = await fetcher.fetchChildren(for: node.item)

            guard !Task.isCancelled else { return }

            node.isLoading = false
            let loadedNodes = loadedItems.map { FileTreeNode(item: $0, parent: node) }
            node.children = loadedNodes

            guard !loadedNodes.isEmpty else {
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

            loadSizes(for: loadedNodes, using: fetcher)
        }
        loadTasks[node.item.id] = task
    }

    private func loadSizes(for nodes: [FileTreeNode], using fetcher: FileTreeChildrenFetcher) {
        let items = nodes.map(\.item)
        guard !items.isEmpty else { return }

        let taskIdentifier = UUID().uuidString
        let task = Task { [weak self] in
            guard let self else { return }
            defer { self.sizeTasks.removeValue(forKey: taskIdentifier) }

            let sizes = await items.concurrentMap(customConcurrency: Self.maxNetworkingParallelism) { item in
                await fetcher.fetchSize(for: item)
            }

            guard !Task.isCancelled else { return }
            for (node, size) in zip(nodes, sizes) {
                node.updateSize(size)

                let row = outlineView.row(forItem: node)
                guard row >= 0 else { continue }
                outlineView.reloadData(
                    forRowIndexes: IndexSet(integer: row),
                    columnIndexes: IndexSet(integer: outlineView.column(withIdentifier: Column.size))
                )
            }
        }
        sizeTasks[taskIdentifier] = task
    }

    private func cancelLoadingTasks() {
        loadTasks.values.forEach { $0.cancel() }
        sizeTasks.values.forEach { $0.cancel() }
        excludedPathsTask?.cancel()
        loadTasks.removeAll()
        sizeTasks.removeAll()
        excludedPathsTask = nil
    }

    // MARK: - Excluded paths resolution

    /// Resolves the remote path of each initially blacklisted folder. Until it completes, unloaded
    /// folders display a loader instead of a possibly wrong checkbox state.
    private func resolveExcludedNodePaths(for nodeIds: Set<String>, using fetcher: FileTreeChildrenFetcher) {
        let task = Task { [weak self] in
            let paths = await fetcher.fetchPaths(for: nodeIds)

            guard let self, !Task.isCancelled else { return }

            resolvedExcludedNodePaths = paths
            resolvedExcludedNodePathsFor = nodeIds
            selectionState.finishResolvingExcludedPaths(with: paths)
            refreshSelectionDisplay()
        }
        excludedPathsTask = task
    }

    // MARK: - Selection mutation

    @objc private func checkboxToggled(_ sender: NSButton) {
        let row = outlineView.row(for: sender)
        guard row >= 0, let node = outlineView.item(atRow: row) as? FileTreeNode else { return }
        toggleSelection(of: node)
        @InjectService var matomo: MatomoUtils
        if selectionState.displayState(of: node) == .on {
            matomo.track(eventWithCategory: .exclusionSelector, name: "selectDir")
        } else {
            matomo.track(eventWithCategory: .exclusionSelector, name: "unselectDir")
        }
    }

    private func toggleSelection(of node: FileTreeNode) {
        guard node.item.isEnabled else { return }

        let select = selectionState.displayState(of: node) != .on
        selectionState.setSelected(select, for: node)

        notifyBlacklistChange()
        refreshSelectionDisplay()
    }

    // MARK: - Header "select / deselect all"

    @objc private func headerCheckboxToggled() {
        let select = selectionState.headerState(for: rootNodes) != .on
        selectionState.setAllSelected(select, rootNodes: rootNodes)

        notifyBlacklistChange()
        refreshSelectionDisplay()
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
        if selectionState.isHeaderStatePending(for: rootNodes) {
            tableHeaderView.setCheckboxLoading(true)
            return
        }

        tableHeaderView.setCheckboxLoading(false)
        tableHeaderView.checkbox.allowsMixedState = true
        tableHeaderView.checkbox.state = selectionState.headerState(for: rootNodes).controlState
    }

    private func notifyBlacklistChange() {
        onBlacklistChange?(selectionState.blacklist)
    }

    // MARK: - Cell factories

    private func makeCheckboxCell(for node: FileTreeNode) -> NSView {
        let cell = outlineView
            .makeView(withIdentifier: Column.checkbox, owner: self) as? FileTreeCheckboxCell ?? FileTreeCheckboxCell()
        cell.identifier = Column.checkbox
        cell.configure(
            state: selectionState.displayState(of: node).controlState,
            isLoading: selectionState.isStatePending(of: node),
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
        guard let node = item as? FileTreeNode, !node.isPlaceholder, node.isFolder else {
            return false
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

// MARK: - FileTreeCheckboxState conversion

private extension FileTreeCheckboxState {
    var controlState: NSControl.StateValue {
        switch self {
        case .on:
            return .on
        case .off:
            return .off
        case .mixed:
            return .mixed
        }
    }
}
