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
        #expect(state.headerState(for: [root]) == .off)
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

    @Test("Folder whose subfolders are all excluded is mixed, not off")
    func folderWithAllSubfoldersExcludedIsMixed() {
        let root = makeFolder(id: "root", path: "root")
        let subA = makeFolder(id: "subA", path: "root/subA", parent: root)
        let subB = makeFolder(id: "subB", path: "root/subB", parent: root)
        root.children = [subA, subB]

        let state = FileTreeSelectionState(initialBlacklist: ["subA", "subB"])

        #expect(state.displayState(of: subA) == .off)
        #expect(state.displayState(of: root) == .mixed)
    }

    @Test("Leaf folder is on")
    func leafFolderIsOn() {
        let root = makeFolder(id: "root", path: "root")
        root.children = []

        let state = FileTreeSelectionState(initialBlacklist: [])

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
        #expect(state.headerState(for: [rootA, rootB]) == .off)
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
