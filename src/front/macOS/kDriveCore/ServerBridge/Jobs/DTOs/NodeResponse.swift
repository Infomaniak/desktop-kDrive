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

struct NodePathQuery: Codable, Sendable {
    let syncDbId: Int32
    @Base64CodedString var nodeId: String
}

struct NodePathResponse: Codable, Sendable {
    @Base64CodedString var path: String
}

struct NodeQuery: Codable, Sendable {
    let userDbId: Int32
    let driveId: Int32
    @Base64CodedString var nodeId: String
    let withPath: Bool
}

struct NodeAlternateQuery: Codable, Sendable {
    let driveDbId: Int32
    @Base64CodedString var nodeId: String
    let withPath: Bool
}

struct NodeInfoResponse: Codable, Sendable {
    let nodeInfo: NodeInfoResponseMetadata
}

struct NodeSubfoldersResponse: Codable, Sendable {
    let nodeSubFolderInfoList: [NodeInfoResponseMetadata]
}

public struct NodeInfoResponseMetadata: Codable, Sendable {
    let modtime: TimeInterval
    @Base64CodedString var name: String
    @Base64CodedString var nodeId: String
    @Base64CodedString var parentNodeId: String
    @Base64CodedString var path: String
    let size: Int64
    let accessDenied: Bool
}

struct NodeSizeQuery: Codable, Sendable {
    let userDbId: Int32
    let driveId: Int32
    @Base64CodedString var nodeId: String
}

struct NodeSizeResponse: Codable, Sendable {
    let folderSize: Int64
}

struct AddMissingFolderQuery: Codable, Sendable {
    let driveDbId: Int32
    let folderList: [MissingFolderQuery]
}

struct MissingFolderQuery: Codable, Sendable {
    @Base64CodedString var name: String
    @Base64CodedString var nodeId: String
}

public struct MissingFolder: Sendable {
    let name: String
    let nodeId: String

    public init(name: String, nodeId: String) {
        self.name = name
        self.nodeId = nodeId
    }
}

struct MissingFolderResponse: Codable, Sendable {
    @Base64CodedString var parentNodeId: String
}
