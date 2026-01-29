/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2025 Infomaniak Network SA

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

public struct NodeInfo: Sendable {
    public let modtime: TimeInterval
    public let name: String
    public let nodeId: String
    public let parentNodeId: String
    public let path: String
    public let size: Int64
    public let accessDenied: Bool

    init(nodeInfoResponseMetadata: NodeInfoResponseMetadata) {
        modtime = nodeInfoResponseMetadata.modtime
        name = nodeInfoResponseMetadata.name
        nodeId = nodeInfoResponseMetadata.nodeId
        parentNodeId = nodeInfoResponseMetadata.parentNodeId
        path = nodeInfoResponseMetadata.path
        size = nodeInfoResponseMetadata.size
        accessDenied = nodeInfoResponseMetadata.accessDenied
    }

    init(
        modtime: TimeInterval,
        name: String,
        nodeId: String,
        parentNodeId: String,
        path: String,
        size: Int64,
        accessDenied: Bool
    ) {
        self.modtime = modtime
        self.name = name
        self.nodeId = nodeId
        self.parentNodeId = parentNodeId
        self.path = path
        self.size = size
        self.accessDenied = accessDenied
    }
}
