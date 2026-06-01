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

import CppInterop
import Foundation
import InfomaniakConcurrency
import InfomaniakDI

public struct ErrorJobs: Sendable {
    @LazyInjectService private var coherentCache: CoherentCache
    @LazyInjectService private var queryFetcher: XPCQueryFetcherProtocol

    public init() {}

    /// The maximum limit matched what is done on windows
    static let maxErrorInfoListLimit: Int32 = 1000

    @discardableResult
    public func errorInfoList() async throws -> [ErrorInfo] {
        IKLogger.data.log("Query for errorInfo list")
        let query = ErrorInfoListQuery(limit: Self.maxErrorInfoListLimit)
        let request = await RequestMessage<ErrorInfoListQuery>(num: RequestNum.ERROR_INFOLIST, body: query)

        let decodedMessage = try await queryFetcher.query(
            request,
            responseType: CallbackMessage<ErrorInfoListResponse>.self
        )

        let errorList = decodedMessage.body.errorInfoList.map { ErrorInfo(errorInfoMetadata: $0) }
        try? await coherentCache.updateErrors(errorList)

        return errorList
    }

    public func refreshSyncErrors(syncDbId: Int32) async throws {
        IKLogger.data.log("Query to refresh sync errors")
        let query = SyncQuery(syncDbId: syncDbId)
        let request = await RequestMessage<SyncQuery>(num: RequestNum.ERROR_SYNC_REFRESH, body: query)

        try await queryFetcher.query(request, responseType: CallbackMessage<EmptyResponse>.self)
    }

    public func resolveConflicts(keepLocalErrorDbIds: [Int32], keepRemoteErrorDbIds: [Int32]) async throws {
        IKLogger.data.log("Query to resolve conflicts")
        let query = ErrorResolveConflictsQuery(
            keepLocalErrorDbIdList: keepLocalErrorDbIds,
            keepRemoteErrorDbIdList: keepRemoteErrorDbIds
        )
        let request = await RequestMessage<ErrorResolveConflictsQuery>(num: RequestNum.ERROR_RESOLVE_CONFLICTS, body: query)

        try await queryFetcher.query(request, responseType: CallbackMessage<EmptyResponse>.self)
    }
}
