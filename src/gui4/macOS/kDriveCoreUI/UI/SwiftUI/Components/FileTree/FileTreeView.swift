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

import SwiftUI

public struct FileTreeView: NSViewRepresentable {
    public let rootItems: [FileTreeItem]
    /// `nil` while the blacklist has not been fetched yet by the parent view.
    public let initialBlacklist: Set<String>?
    public let childrenFetcher: FileTreeChildrenFetcher
    public let onBlacklistChange: (Set<String>) -> Void

    public init(
        rootItems: [FileTreeItem],
        initialBlacklist: Set<String>? = nil,
        childrenFetcher: FileTreeChildrenFetcher,
        onBlacklistChange: @escaping (Set<String>) -> Void
    ) {
        self.rootItems = rootItems
        self.initialBlacklist = initialBlacklist
        self.childrenFetcher = childrenFetcher
        self.onBlacklistChange = onBlacklistChange
    }

    public func makeNSView(context: Context) -> FileTreeOutlineView {
        let view = FileTreeOutlineView()
        view.childrenFetcher = childrenFetcher
        view.onBlacklistChange = onBlacklistChange
        applyRootItems(to: view, coordinator: context.coordinator)
        return view
    }

    public func updateNSView(_ nsView: FileTreeOutlineView, context: Context) {
        nsView.childrenFetcher = childrenFetcher
        nsView.onBlacklistChange = onBlacklistChange
        applyRootItems(to: nsView, coordinator: context.coordinator)
    }

    public func makeCoordinator() -> Coordinator {
        Coordinator()
    }

    public final class Coordinator {
        var appliedRootIDs: [String] = []
        var appliedInitialBlacklist: Set<String>?
    }

    /// Reapplies the root items whenever one of the inputs they depend on changes. This is required
    /// because the initial blacklist can arrive after the root items (it is fetched asynchronously by
    /// the parent view).
    private func applyRootItems(to view: FileTreeOutlineView, coordinator: Coordinator) {
        let rootIDs = rootItems.map(\.id)
        guard coordinator.appliedRootIDs != rootIDs
            || coordinator.appliedInitialBlacklist != initialBlacklist else { return }

        coordinator.appliedRootIDs = rootIDs
        coordinator.appliedInitialBlacklist = initialBlacklist
        view.setRootItems(rootItems, initialBlacklist: initialBlacklist)
    }
}
