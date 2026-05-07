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

public struct ErrorInfoListJob: Sendable {
    @LazyInjectService private var coherentCache: CoherentCache
    @LazyInjectService private var queryFetcher: XPCQueryFetcherProtocol

    public init() {}

    @discardableResult
    public func errorInfoList(limit: Int32) async throws -> [ErrorInfo] {
        IKLogger.data.log("Query for errorInfo list")
        let query = ErrorInfoListQuery(limit: limit)
        let request = await RequestMessage<ErrorInfoListQuery>(num: RequestNum.ERROR_INFOLIST, body: query)

        let decodedMessage = try await queryFetcher.query(
            request,
            responseType: CallbackMessage<ErrorInfoListResponse>.self
        )

        let errorList = decodedMessage.body.errorInfoList.map { ErrorInfo(errorInfoMetadata: $0) }
        return errorList
    }
}
