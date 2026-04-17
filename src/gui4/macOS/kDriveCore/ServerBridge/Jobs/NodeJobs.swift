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
import InfomaniakConcurrency
import InfomaniakDI

public struct NodeJobs: Sendable {
    @LazyInjectService private var queryFetcher: XPCQueryFetcherProtocol

    public init() {}

    public func getNodePath(syncDbId: Int32, nodeId: String) async throws -> String {
        IKLogger.data.log("Query to get a node path")
        let query = NodePathQuery(syncDbId: syncDbId, nodeId: nodeId)
        let request = await RequestMessage<NodePathQuery>(num: RequestNum.NODE_PATH, body: query)

        let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<NodePathResponse>.self)

        return decodedMessage.body.path
    }

    public func getNodeInfo(userDbId: Int32,
                            driveId: Int32,
                            nodeId: String,
                            withPath: Bool = true) async throws -> NodeInfo {
        IKLogger.data.log("Query to get node info")
        let query = NodeQuery(userDbId: userDbId, driveId: driveId, nodeId: nodeId, withPath: withPath)
        let request = await RequestMessage<NodeQuery>(num: RequestNum.NODE_INFO, body: query)

        let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<NodeInfoResponse>.self)

        return NodeInfo(nodeInfoResponseMetadata: decodedMessage.body.nodeInfo)
    }

    public func getNodeSubfolders(userDbId: Int32,
                                  driveId: Int32,
                                  nodeId: String,
                                  withPath: Bool = true) async throws -> [NodeInfo] {
        IKLogger.data.log("Query to get node subfolder info")
        let query = NodeQuery(userDbId: userDbId, driveId: driveId, nodeId: nodeId, withPath: withPath)
        let request = await RequestMessage<NodeQuery>(num: RequestNum.NODE_SUBFOLDERS, body: query)

        let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<NodeSubfoldersResponse>.self)

        return decodedMessage.body.nodeSubFolderInfoList.map { NodeInfo(nodeInfoResponseMetadata: $0) }
    }

    public func getNodeSubfolders(driveDbId: Int32,
                                  nodeId: String,
                                  withPath: Bool = true) async throws -> [NodeInfo] {
        IKLogger.data.log("Query to get node subfolder info from driveDbId")
        let query = NodeAlternateQuery(driveDbId: driveDbId, nodeId: nodeId, withPath: withPath)
        let request = await RequestMessage<NodeAlternateQuery>(num: RequestNum.NODE_SUBFOLDERS2, body: query)

        let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<NodeSubfoldersResponse>.self)

        return decodedMessage.body.nodeSubFolderInfoList.map { NodeInfo(nodeInfoResponseMetadata: $0) }
    }

    public func getFolderSize(userDbId: Int32, driveId: Int32, nodeId: String) async throws -> Int64 {
        IKLogger.data.log("Query to get a node size")
        let query = NodeSizeQuery(userDbId: userDbId, driveId: driveId, nodeId: nodeId)
        let request = await RequestMessage<NodeSizeQuery>(num: RequestNum.NODE_FOLDER_SIZE, body: query)

        let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<NodeSizeResponse>.self)

        return decodedMessage.body.folderSize
    }

    public func createMissingFolders(userDbId: Int32,
                                     driveId: Int32,
                                     parentNodeId: String,
                                     relativePath: String) async throws -> String {
        IKLogger.data.log("Query to create missing folders")
        let query = CreateMissingFoldersQuery(userDbId: userDbId,
                                              driveId: driveId,
                                              parentNodeId: parentNodeId,
                                              relativePath: relativePath)
        let request = await RequestMessage<CreateMissingFoldersQuery>(num: RequestNum.NODE_CREATEMISSINGFOLDERS, body: query)

        let decodedMessage = try await queryFetcher.query(
            request,
            responseType: CallbackMessage<CreateMissingFoldersResponse>.self
        )

        return decodedMessage.body.nodeId
    }

    public func getNodeConflictInfo(syncDbId: Int32,
                                    relativePath: String,
                                    replicaSide: KDC.ReplicaSide) async throws -> NodeConflictInfo {
        IKLogger.data.log("Query to get node conflict info")
        let query = NodeConflictInfoQuery(syncDbId: syncDbId, relativePath: relativePath, replicaSide: replicaSide)
        let request = await RequestMessage<NodeConflictInfoQuery>(num: RequestNum.NODE_CONFLICT_INFO, body: query)

        let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<NodeConflictInfoResponse>.self)

        return NodeConflictInfo(nodeConflictInfoMetadata: decodedMessage.body.nodeConflictInfo)
    }
}
