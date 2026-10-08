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

import Foundation

/// Checkbox state of an item in the file tree.
enum FileTreeCheckboxState: Equatable {
    case on
    case off
    case mixed
}

/// Holds the selection state of the file tree (the blacklist) and derives the checkbox state of each
/// node from it.
///
/// The blacklist contains the node ids of the folders that must not be synchronized. Because the tree
/// is lazily loaded, a folder that has never been expanded carries no information about its
/// descendants: `excludedNodePaths` maps the id of each blacklisted folder to its remote path so that
/// unloaded folders containing excluded content can still be detected and displayed with a mixed
/// state.
struct FileTreeSelectionState {
    private(set) var blacklist: Set<String>
    private(set) var excludedNodePaths: [String: String]

    /// `true` while the remote paths of the initially blacklisted folders are still being resolved.
    /// Until then, the checkbox state of an unloaded folder is unknown and should be displayed as
    /// pending instead of a possibly wrong value (see `isStatePending(of:)`).
    private(set) var isResolvingExcludedPaths: Bool

    init(initialBlacklist: Set<String>, excludedNodePaths: [String: String] = [:], isResolvingExcludedPaths: Bool = false) {
        blacklist = initialBlacklist
        self.excludedNodePaths = excludedNodePaths
        self.isResolvingExcludedPaths = isResolvingExcludedPaths
    }

    /// Marks the excluded paths resolution as completed and adopts the resolved paths.
    mutating func finishResolvingExcludedPaths(with paths: [String: String]) {
        excludedNodePaths = paths
        isResolvingExcludedPaths = false
    }

    // MARK: - Derived checkbox state

    func displayState(of node: FileTreeNode) -> FileTreeCheckboxState {
        effectiveState(of: node, ancestorExcluded: isAncestorExcluded(node))
    }

    /// Returns `true` when the checkbox state of `node` cannot be determined yet: its children have
    /// not been loaded and the paths of the blacklisted folders are still being resolved. States of
    /// excluded, disabled or already loaded folders are always definitive.
    func isStatePending(of node: FileTreeNode) -> Bool {
        guard isResolvingExcludedPaths, node.item.isEnabled, node.isFolder, node.children == nil else { return false }

        return !isSelfOrAncestorExcluded(node)
    }

    /// Returns `true` when the header checkbox state is not reliable yet: either the tree has not
    /// been loaded yet, or at least one node of the tree is pending and its state may still flip
    /// once the excluded paths are resolved.
    func isHeaderStatePending(for rootNodes: [FileTreeNode]) -> Bool {
        guard isResolvingExcludedPaths else { return false }

        // Without root nodes the tree is not loaded yet and the header state is unknown as well.
        return rootNodes.isEmpty || rootNodes.contains { subtreeContainsPendingNode($0) }
    }

    private func subtreeContainsPendingNode(_ node: FileTreeNode) -> Bool {
        if isStatePending(of: node) {
            return true
        }
        return (node.children ?? []).contains { subtreeContainsPendingNode($0) }
    }

    func headerState(for rootNodes: [FileTreeNode]) -> FileTreeCheckboxState {
        guard !rootNodes.isEmpty else { return .mixed }

        var sawOn = false
        var sawOff = false
        for node in rootNodes {
            switch displayState(of: node) {
            case .on:
                sawOn = true
            case .off:
                sawOff = true
            case .mixed:
                sawOn = true
                sawOff = true
            }
            if sawOn, sawOff {
                return .mixed
            }
        }
        return sawOn ? .on : .mixed
    }

    /// Returns `true` if at least one blacklisted folder is a strict descendant of `node`. Only
    /// blacklisted folders whose path is known can be detected.
    func hasExcludedDescendant(of node: FileTreeNode) -> Bool {
        guard let path = node.item.path, !path.isEmpty else { return false }

        let prefix = path + "/"
        return excludedNodePaths.values.contains { $0.hasPrefix(prefix) }
    }

    private func isSelfOrAncestorExcluded(_ node: FileTreeNode) -> Bool {
        blacklist.contains(node.item.id) || isAncestorExcluded(node)
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

    private func effectiveState(of node: FileTreeNode, ancestorExcluded: Bool) -> FileTreeCheckboxState {
        guard node.item.isEnabled else { return .off }

        let selfExcluded = ancestorExcluded || blacklist.contains(node.item.id)

        guard let children = node.children else {
            // Children have not been loaded yet: fall back on the excluded paths to detect a
            // blacklisted descendant, otherwise the folder is considered fully selected.
            if selfExcluded {
                return .off
            }
            return hasExcludedDescendant(of: node) ? .mixed : .on
        }

        guard !children.isEmpty else {
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
            case .mixed:
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

    mutating func setSelected(_ select: Bool, for node: FileTreeNode) {
        pushDownAncestorExclusions(towards: node)

        if select {
            removeFromBlacklist(node)
        } else {
            removeDescendantsFromBlacklist(node)
            blacklist.insert(node.item.id)
            cachePath(of: node)
        }
    }

    mutating func setAllSelected(_ select: Bool, rootNodes: [FileTreeNode]) {
        blacklist.removeAll()
        excludedNodePaths.removeAll()

        guard !select else { return }

        for node in rootNodes {
            blacklist.insert(node.item.id)
            cachePath(of: node)
        }
    }

    private mutating func cachePath(of node: FileTreeNode) {
        if let path = node.item.path, !path.isEmpty {
            excludedNodePaths[node.item.id] = path
        }
    }

    private mutating func pushDownAncestorExclusions(towards node: FileTreeNode) {
        var path: [FileTreeNode] = []
        var parent = node.parent
        while let current = parent {
            path.append(current)
            parent = current.parent
        }

        for ancestor in path.reversed() where blacklist.contains(ancestor.item.id) {
            blacklist.remove(ancestor.item.id)
            excludedNodePaths.removeValue(forKey: ancestor.item.id)
            for child in ancestor.children ?? [] {
                blacklist.insert(child.item.id)
                cachePath(of: child)
            }
        }
    }

    private mutating func removeFromBlacklist(_ node: FileTreeNode) {
        blacklist.remove(node.item.id)
        excludedNodePaths.removeValue(forKey: node.item.id)
        removeDescendantsFromBlacklist(node)
    }

    private mutating func removeDescendantsFromBlacklist(_ node: FileTreeNode) {
        for child in node.children ?? [] {
            removeFromBlacklist(child)
        }

        // Prune blacklisted folders located below `node`: they are subsumed by `node` being selected
        // again. Only folders whose path is known (in `excludedNodePaths`) can be pruned, which also
        // covers folders that are not currently loaded in the tree.
        guard let path = node.item.path, !path.isEmpty else { return }

        let prefix = path + "/"
        let prunedNodeIds = excludedNodePaths.filter { $0.value.hasPrefix(prefix) }.map(\.key)
        for nodeId in prunedNodeIds {
            blacklist.remove(nodeId)
            excludedNodePaths.removeValue(forKey: nodeId)
        }
    }
}
