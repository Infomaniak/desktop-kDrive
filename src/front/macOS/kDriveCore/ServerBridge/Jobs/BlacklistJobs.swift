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
import InfomaniakDI

public struct BlacklistJobs: Sendable {
    @LazyInjectService private var queryFetcher: XPCQueryFetcherProtocol

    public init() {}

    public func getBlacklistedNodeList(syncDbId: Int32) async throws -> [String] {
        IKLogger.data.log("Query for blacklistedNodeList")
        let query = BlacklistedNodeListQuery(syncDbId: syncDbId)
        let request = await RequestMessage<BlacklistedNodeListQuery>(num: RequestNum.BLACKLISTED_NODE_LIST, body: query)

        let decodedMessage = try await queryFetcher.query(
            request,
            responseType: CallbackMessage<BlacklistedNodeListResponse>.self
        )

        return decodedMessage.body.nodeIdList
    }

    public func setBlacklistedNodeList(syncDbId: Int32, nodeIdList: [String]) async throws {
        IKLogger.data.log("Query to set blacklistedNodeList")
        let query = BlacklistedNodeSetListQuery(syncDbId: syncDbId, nodeIdList: nodeIdList)
        let request = await RequestMessage<BlacklistedNodeSetListQuery>(num: RequestNum.BLACKLISTED_NODE_SETLIST, body: query)

        _ = try await queryFetcher.query(request, responseType: CallbackMessage<EmptyResponse>.self)
    }
}
