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
import kDriveCore

@MainActor
public final class FileTreeChildrenFetcher {
    private let userDbId: Int32
    private let driveDbId: Int32
    private let rootNodeId: String?

    public init(userDbId: Int, driveDbId: Int, rootNodeId: String?) {
        self.userDbId = Int32(userDbId)
        self.driveDbId = Int32(driveDbId)
        self.rootNodeId = rootNodeId
    }

    public func fetchChildren(for item: FileTreeItem?) async -> [FileTreeItem] {
        let nodeId = item?.id ?? rootNodeId ?? ""

        do {
            let nodes = try await NodeJobs().getNodeSubfolders(
                userDbId: userDbId,
                driveId: driveDbId,
                nodeId: nodeId
            )

            return nodes.map { item in
                return FileTreeItem(
                    id: item.nodeId,
                    name: item.name,
                    size: nil,
                    isFolder: true,
                    isEnabled: !item.accessDenied
                )
            }
        } catch {
            return []
        }
    }

    public func fetchSize(for item: FileTreeItem) async -> Int64? {
        try? await NodeJobs().getFolderSize(
            userDbId: userDbId,
            driveId: driveDbId,
            nodeId: item.id
        )
    }
}
