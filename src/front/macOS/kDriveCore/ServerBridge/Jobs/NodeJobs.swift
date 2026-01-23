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
    @LazyInjectService private var coherentCache: CoherentCache
    @LazyInjectService private var queryFetcher: XPCQueryFetcherProtocol

    public init() {}

    @discardableResult
    public func getNodePath(syncDbId: Int32, nodeId: String) async throws -> String {
        IKLogger.data.log("Query to get a node path")
        let query = NodePathQuery(syncDbId: syncDbId, nodeId: nodeId)
        let request = await RequestMessage<NodePathQuery>(num: RequestNum.NODE_PATH, body: query)

        let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<NodePathResponse>.self)

        try decodedMessage.validate()

        let path = decodedMessage.body.path
        return path
    }
}
