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
    
    public let loadChildren: (FileTreeItem) async -> [FileTreeItem]
    public let onSelectionChange: (Set<String>) -> Void

    public init(
        rootItems: [FileTreeItem],
        loadChildren: @escaping (FileTreeItem) async -> [FileTreeItem],
        onSelectionChange: @escaping (Set<String>) -> Void
    ) {
        self.rootItems = rootItems
        self.loadChildren = loadChildren
        self.onSelectionChange = onSelectionChange
    }

    public func makeNSView(context: Context) -> FileTreeOutlineView {
        let view = FileTreeOutlineView()
        view.loadChildren = loadChildren
        view.onSelectionChange = onSelectionChange
        view.setRootItems(rootItems)
        context.coordinator.appliedRootIDs = rootItems.map(\.id)
        return view
    }

    public func updateNSView(_ nsView: FileTreeOutlineView, context: Context) {
        nsView.loadChildren = loadChildren
        nsView.onSelectionChange = onSelectionChange

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
