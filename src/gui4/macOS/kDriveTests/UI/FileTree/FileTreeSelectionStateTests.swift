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

@testable import kDriveCoreUI
import Testing

@Suite("FileTreeSelectionState Test")
struct FileTreeSelectionStateTests {
    // MARK: - Helpers

    /// Builds a folder node. The caller must keep a strong reference to the returned node for the
    /// weak `parent` link of its children to stay valid.
    private func makeFolder(id: String, path: String, parent: FileTreeNode? = nil, isEnabled: Bool = true) -> FileTreeNode {
        let item = FileTreeItem(id: id, name: id, path: path, size: nil, isFolder: true, isEnabled: isEnabled)
        return FileTreeNode(item: item, parent: parent)
    }

    // MARK: - Unloaded folders (children not loaded yet)

    @Test("Unloaded folder containing a blacklisted descendant is mixed")
    func unloadedFolderWithBlacklistedDescendantIsMixed() {
        let root = makeFolder(id: "root", path: "root")

        let state = FileTreeSelectionState(
            initialBlacklist: ["folder"],
            excludedNodePaths: ["folder": "root/sub/folder"]
        )

        #expect(state.displayState(of: root) == .mixed)
        #expect(state.headerState(for: [root]) == .mixed)
    }

    @Test("Unloaded folder is mixed when the blacklisted descendant is several levels below")
    func unloadedFolderWithDistantBlacklistedDescendantIsMixed() {
        let root = makeFolder(id: "root", path: "root")

        let state = FileTreeSelectionState(
            initialBlacklist: ["deep"],
            excludedNodePaths: ["deep": "root/a/b/deep"]
        )

        #expect(state.hasExcludedDescendant(of: root))
        #expect(state.displayState(of: root) == .mixed)
    }

    @Test("Unloaded folder without any blacklisted descendant is on")
    func unloadedFolderWithoutBlacklistedDescendantIsOn() {
        let root = makeFolder(id: "root", path: "root")

        let state = FileTreeSelectionState(
            initialBlacklist: ["folder"],
            excludedNodePaths: ["folder": "other/folder"]
        )

        #expect(state.displayState(of: root) == .on)
        #expect(state.headerState(for: [root]) == .on)
    }

    @Test("Unloaded folder is on when no excluded path is known")
    func unloadedFolderWithoutKnownExcludedPathIsOn() {
        let root = makeFolder(id: "root", path: "root")

        let state = FileTreeSelectionState(initialBlacklist: ["folder"])

        #expect(state.displayState(of: root) == .on)
    }

    @Test("Path that is not a strict descendant is ignored")
    func pathThatIsNotStrictDescendantIsIgnored() {
        let root = makeFolder(id: "root", path: "root")

        let state = FileTreeSelectionState(
            initialBlacklist: ["folder"],
            excludedNodePaths: ["folder": "rootage/folder"]
        )

        #expect(!state.hasExcludedDescendant(of: root))
        #expect(state.displayState(of: root) == .on)
    }

    @Test("Folder without path is never mixed")
    func folderWithoutPathIsNeverMixed() {
        let root = makeFolder(id: "root", path: "")

        let state = FileTreeSelectionState(
            initialBlacklist: ["folder"],
            excludedNodePaths: ["folder": "root/sub"]
        )

        #expect(!state.hasExcludedDescendant(of: root))
        #expect(state.displayState(of: root) == .on)
    }

    // MARK: - Blacklisted and disabled folders

    @Test("Blacklisted folder is off, even unloaded")
    func blacklistedFolderIsOff() {
        let root = makeFolder(id: "root", path: "root")

        let state = FileTreeSelectionState(initialBlacklist: ["root"])

        #expect(state.displayState(of: root) == .off)
        // The root folder can never be completely unchecked: the header shows mixed instead of off.
        #expect(state.headerState(for: [root]) == .mixed)
    }

    @Test("Access denied folder is off")
    func accessDeniedFolderIsOff() {
        let root = makeFolder(id: "root", path: "root", isEnabled: false)

        let state = FileTreeSelectionState(initialBlacklist: [], excludedNodePaths: ["folder": "root/sub"])

        #expect(state.displayState(of: root) == .off)
    }

    // MARK: - Loaded subtrees

    @Test("Parent of a blacklisted child is mixed once loaded")
    func parentOfBlacklistedChildIsMixedOnceLoaded() {
        let root = makeFolder(id: "root", path: "root")
        let blacklisted = makeFolder(id: "blacklisted", path: "root/blacklisted", parent: root)
        let selected = makeFolder(id: "selected", path: "root/selected", parent: root)
        root.children = [blacklisted, selected]

        let state = FileTreeSelectionState(initialBlacklist: ["blacklisted"])

        #expect(state.displayState(of: blacklisted) == .off)
        #expect(state.displayState(of: selected) == .on)
        #expect(state.displayState(of: root) == .mixed)
        #expect(state.headerState(for: [root]) == .mixed)
    }

    @Test("Mixed unloaded child propagates to its loaded parent")
    func mixedUnloadedChildPropagatesToLoadedParent() {
        let root = makeFolder(id: "root", path: "root")
        let middle = makeFolder(id: "middle", path: "root/middle", parent: root)
        root.children = [middle]

        let state = FileTreeSelectionState(
            initialBlacklist: ["deep"],
            excludedNodePaths: ["deep": "root/middle/deep"]
        )

        #expect(state.displayState(of: middle) == .mixed)
        #expect(state.displayState(of: root) == .mixed)
    }

    @Test("Deep blacklisted folder is reflected on ancestors before and after loading")
    func deepBlacklistedFolderIsReflectedBeforeAndAfterLoading() {
        let root = makeFolder(id: "root", path: "root")

        let state = FileTreeSelectionState(
            initialBlacklist: ["deep"],
            excludedNodePaths: ["deep": "root/sub/deep"]
        )

        // Before loading the children: previously displayed as on.
        #expect(state.displayState(of: root) == .mixed)

        // After loading the children.
        let sub = makeFolder(id: "sub", path: "root/sub", parent: root)
        root.children = [sub]

        #expect(state.displayState(of: sub) == .mixed)
        #expect(state.displayState(of: root) == .mixed)
    }

    @Test("Folder whose subfolders are all excluded is mixed")
    func folderWithAllSubfoldersExcludedIsMixed() {
        let root = makeFolder(id: "root", path: "root")
        let subA = makeFolder(id: "subA", path: "root/subA", parent: root)
        let subB = makeFolder(id: "subB", path: "root/subB", parent: root)
        root.children = [subA, subB]

        let state = FileTreeSelectionState(initialBlacklist: ["subA", "subB"])

        #expect(state.displayState(of: subA) == .off)
        #expect(state.displayState(of: root) == .mixed)
    }

    @Test("Excluded folder with loaded descendants stays off")
    func excludedFolderWithLoadedDescendantsStaysOff() {
        let root = makeFolder(id: "root", path: "root")
        let sub = makeFolder(id: "sub", path: "root/sub", parent: root)
        let deep = makeFolder(id: "deep", path: "root/sub/deep", parent: sub)
        sub.children = [deep]
        root.children = [sub]

        let state = FileTreeSelectionState(initialBlacklist: ["sub"])

        // The whole subtree is excluded: it must display off and not flip to mixed on loading.
        #expect(state.displayState(of: sub) == .off)
        #expect(state.displayState(of: deep) == .off)
        #expect(state.displayState(of: root) == .mixed)
    }

    @Test("Leaf folder is on")
    func leafFolderIsOn() {
        let root = makeFolder(id: "root", path: "root")
        root.children = []

        let state = FileTreeSelectionState(initialBlacklist: [])

        #expect(state.displayState(of: root) == .on)
    }

    // MARK: - Pending state (excluded paths being resolved)

    @Test("Unloaded folder is pending while excluded paths are being resolved")
    func unloadedFolderIsPendingWhileResolving() {
        let root = makeFolder(id: "root", path: "root")

        let state = FileTreeSelectionState(initialBlacklist: ["folder"], isResolvingExcludedPaths: true)

        #expect(state.isStatePending(of: root))
        #expect(state.isHeaderStatePending(for: [root]))
    }

    @Test("Header is pending when the tree has no root nodes while excluded paths are being resolved")
    func headerIsPendingWithEmptyTreeWhileResolving() {
        let state = FileTreeSelectionState(initialBlacklist: ["folder"], isResolvingExcludedPaths: true)

        #expect(state.isHeaderStatePending(for: []))
    }

    @Test("Pending folder falls back on on while excluded paths are being resolved")
    func pendingFolderFallsBackOnOnWhileResolving() {
        let root = makeFolder(id: "root", path: "root")

        let state = FileTreeSelectionState(initialBlacklist: ["folder"], isResolvingExcludedPaths: true)

        #expect(state.displayState(of: root) == .on)
    }

    @Test("Blacklisted folder is not pending while excluded paths are being resolved")
    func blacklistedFolderIsNotPendingWhileResolving() {
        let root = makeFolder(id: "root", path: "root")

        let state = FileTreeSelectionState(initialBlacklist: ["root"], isResolvingExcludedPaths: true)

        #expect(!state.isStatePending(of: root))
    }

    @Test("Disabled folder is not pending while excluded paths are being resolved")
    func disabledFolderIsNotPendingWhileResolving() {
        let root = makeFolder(id: "root", path: "root", isEnabled: false)

        let state = FileTreeSelectionState(initialBlacklist: ["folder"], isResolvingExcludedPaths: true)

        #expect(!state.isStatePending(of: root))
    }

    @Test("Loaded folder is not pending while excluded paths are being resolved")
    func loadedFolderIsNotPendingWhileResolving() {
        let root = makeFolder(id: "root", path: "root")
        root.children = []

        let state = FileTreeSelectionState(initialBlacklist: ["folder"], isResolvingExcludedPaths: true)

        #expect(!state.isStatePending(of: root))
    }

    @Test("Pending descendant does not make a loaded folder pending but makes the header pending")
    func pendingDescendantMakesHeaderPending() {
        let root = makeFolder(id: "root", path: "root")
        let child = makeFolder(id: "child", path: "root/child", parent: root)
        root.children = [child]

        let state = FileTreeSelectionState(initialBlacklist: ["folder"], isResolvingExcludedPaths: true)

        #expect(!state.isStatePending(of: root))
        #expect(state.isStatePending(of: child))
        #expect(state.isHeaderStatePending(for: [root]))
    }

    @Test("Folder is not pending once excluded paths resolution completed")
    func folderIsNotPendingOnceResolutionCompleted() {
        let root = makeFolder(id: "root", path: "root")

        var state = FileTreeSelectionState(initialBlacklist: ["folder"], isResolvingExcludedPaths: true)
        state.finishResolvingExcludedPaths(with: ["folder": "root/sub/folder"])

        #expect(!state.isStatePending(of: root))
        #expect(!state.isHeaderStatePending(for: [root]))
        #expect(state.displayState(of: root) == .mixed)
    }

    @Test("Paths cached by user toggles survive the excluded paths resolution")
    func userToggledPathsSurviveExcludedPathsResolution() {
        let root = makeFolder(id: "root", path: "root")
        let child = makeFolder(id: "child", path: "root/child", parent: root)
        root.children = [child]

        var state = FileTreeSelectionState(initialBlacklist: ["folder"], isResolvingExcludedPaths: true)
        state.setSelected(false, for: child)
        #expect(state.excludedNodePaths == ["child": "root/child"])

        // The resolution only returns the paths of the initially blacklisted folders.
        state.finishResolvingExcludedPaths(with: ["folder": "root/sub/folder"])

        #expect(state.excludedNodePaths == ["child": "root/child", "folder": "root/sub/folder"])
        #expect(state.blacklist == ["child", "folder"])
    }

    @Test("Deselected subfolder is still pruned when an ancestor is selected after the resolution")
    func deselectedSubfolderIsPrunedWhenAncestorSelectedAfterResolution() {
        let root = makeFolder(id: "root", path: "root")
        let middle = makeFolder(id: "middle", path: "root/middle", parent: root)
        middle.children = []
        root.children = [middle]

        var state = FileTreeSelectionState(initialBlacklist: [], isResolvingExcludedPaths: true)
        state.setSelected(false, for: middle)
        #expect(state.blacklist == ["middle"])

        // The resolution does not know the path of the toggled node; it must not be lost.
        state.finishResolvingExcludedPaths(with: [:])
        #expect(state.excludedNodePaths == ["middle": "root/middle"])

        // Re-selecting the parent must prune the deselected descendant from the blacklist.
        state.setSelected(true, for: root)
        #expect(state.blacklist.isEmpty)
        #expect(state.displayState(of: root) == .on)
    }

    // MARK: - Mutations

    @Test("Deselecting a node blacklists it and caches its path")
    func deselectingNodeBlacklistsItAndCachesItsPath() {
        let root = makeFolder(id: "root", path: "root")
        let child = makeFolder(id: "child", path: "root/child", parent: root)
        root.children = [child]

        var state = FileTreeSelectionState(initialBlacklist: [])
        state.setSelected(false, for: child)

        #expect(state.blacklist == ["child"])
        #expect(state.excludedNodePaths == ["child": "root/child"])
        #expect(state.displayState(of: child) == .off)
        #expect(state.displayState(of: root) == .mixed)
    }

    @Test("Deselecting a subfolder keeps every ancestor mixed up to the header")
    func deselectingSubfolderKeepsAncestorsMixed() {
        let root = makeFolder(id: "root", path: "root")
        let middle = makeFolder(id: "middle", path: "root/middle", parent: root)
        let leaf = makeFolder(id: "leaf", path: "root/middle/leaf", parent: middle)
        let sibling = makeFolder(id: "sibling", path: "root/middle/sibling", parent: middle)
        middle.children = [leaf, sibling]
        root.children = [middle]

        var state = FileTreeSelectionState(initialBlacklist: [])
        state.setSelected(false, for: leaf)

        #expect(state.blacklist == ["leaf"])
        #expect(state.displayState(of: leaf) == .off)
        #expect(state.displayState(of: sibling) == .on)
        #expect(state.displayState(of: middle) == .mixed)
        #expect(state.displayState(of: root) == .mixed)
        #expect(state.headerState(for: [root]) == .mixed)
    }

    @Test("Deselecting a subfolder in a single chain only deselects its own subtree")
    func deselectingSubfolderInSingleChainOnlyDeselectsItsSubtree() {
        // User scenario: A > B > C > D > E, all expanded and checked; unchecking C must
        // leave C, D and E unchecked while A and B stay mixed.
        let root = makeFolder(id: "root", path: "root")
        let a = makeFolder(id: "a", path: "root/a", parent: root)
        let b = makeFolder(id: "b", path: "root/a/b", parent: a)
        let c = makeFolder(id: "c", path: "root/a/b/c", parent: b)
        let d = makeFolder(id: "d", path: "root/a/b/c/d", parent: c)
        let e = makeFolder(id: "e", path: "root/a/b/c/d/e", parent: d)
        d.children = [e]
        c.children = [d]
        b.children = [c]
        a.children = [b]
        root.children = [a]

        var state = FileTreeSelectionState(initialBlacklist: [])
        state.setSelected(false, for: c)

        #expect(state.blacklist == ["c"])
        #expect(state.displayState(of: c) == .off)
        #expect(state.displayState(of: d) == .off)
        #expect(state.displayState(of: e) == .off)
        #expect(state.displayState(of: b) == .mixed)
        #expect(state.displayState(of: a) == .mixed)
        #expect(state.headerState(for: [root]) == .mixed)

        // Clicking the mixed ancestor re-selects the deselected subtree.
        state.setSelected(true, for: b)
        #expect(state.blacklist.isEmpty)
        #expect(state.displayState(of: c) == .on)
        #expect(state.displayState(of: a) == .on)
        #expect(state.headerState(for: [root]) == .on)
    }

    @Test("Selecting a folder prunes blacklisted descendants that are not loaded")
    func selectingFolderPrunesUnloadedBlacklistedDescendants() {
        let root = makeFolder(id: "root", path: "root")
        let child = makeFolder(id: "child", path: "root/child", parent: root)

        var state = FileTreeSelectionState(
            initialBlacklist: ["child", "deep"],
            excludedNodePaths: ["child": "root/child", "deep": "root/child/deep"]
        )
        state.setSelected(true, for: child)

        #expect(state.blacklist.isEmpty)
        #expect(state.excludedNodePaths.isEmpty)
        #expect(state.displayState(of: child) == .on)
    }

    @Test("Blacklisted descendant without known path stays blacklisted when parent is selected")
    func blacklistedDescendantWithoutKnownPathStaysBlacklisted() {
        let root = makeFolder(id: "root", path: "root")

        var state = FileTreeSelectionState(initialBlacklist: ["deep"])
        state.setSelected(true, for: root)

        #expect(state.blacklist == ["deep"])
        #expect(state.displayState(of: root) == .on)
    }

    @Test("Selecting a child of a blacklisted folder pushes the exclusion down to its siblings")
    func selectingChildOfBlacklistedFolderPushesExclusionDown() {
        let root = makeFolder(id: "root", path: "root")
        let folder = makeFolder(id: "folder", path: "root/folder", parent: root)
        root.children = [folder]
        let childA = makeFolder(id: "childA", path: "root/folder/childA", parent: folder)
        let childB = makeFolder(id: "childB", path: "root/folder/childB", parent: folder)
        folder.children = [childA, childB]

        var state = FileTreeSelectionState(
            initialBlacklist: ["folder"],
            excludedNodePaths: ["folder": "root/folder"]
        )
        state.setSelected(true, for: childA)

        #expect(state.blacklist == ["childB"])
        #expect(state.excludedNodePaths == ["childB": "root/folder/childB"])
        #expect(state.displayState(of: childA) == .on)
        #expect(state.displayState(of: childB) == .off)
        #expect(state.displayState(of: folder) == .mixed)
        #expect(state.displayState(of: root) == .mixed)
    }

    @Test("Deselect all blacklists root folders and caches their paths")
    func deselectAllBlacklistsRootFolders() {
        let rootA = makeFolder(id: "a", path: "a")
        let rootB = makeFolder(id: "b", path: "b")

        var state = FileTreeSelectionState(initialBlacklist: [])
        state.setAllSelected(false, rootNodes: [rootA, rootB])

        #expect(state.blacklist == ["a", "b"])
        #expect(state.excludedNodePaths == ["a": "a", "b": "b"])
        // The root folder can never be completely unchecked: the header shows mixed instead of off.
        #expect(state.headerState(for: [rootA, rootB]) == .mixed)
    }

    @Test("Select all clears the blacklist and the cached paths")
    func selectAllClearsBlacklistAndCachedPaths() {
        let rootA = makeFolder(id: "a", path: "a")

        var state = FileTreeSelectionState(
            initialBlacklist: ["a", "deep"],
            excludedNodePaths: ["a": "a", "deep": "a/deep"]
        )
        state.setAllSelected(true, rootNodes: [rootA])

        #expect(state.blacklist.isEmpty)
        #expect(state.excludedNodePaths.isEmpty)
        #expect(state.headerState(for: [rootA]) == .on)
    }
}
