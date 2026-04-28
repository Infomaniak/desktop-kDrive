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
import InfomaniakDI

public struct ParametersJobs: Sendable {
    @LazyInjectService private var queryFetcher: XPCQueryFetcherProtocol

    public init() {}

    public func parametersInfo() async throws -> ParametersInfo {
        IKLogger.data.log("Query for parametersInfo")
        let query = EmptyQuery()
        let request = await RequestMessage<EmptyQuery>(num: RequestNum.PARAMETERS_INFO, body: query)

        let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<ParametersInfoResponse>.self)

        return decodedMessage.body.parametersInfo
    }

    public func updateParameters(parametersInfo: ParametersInfo) async throws {
        IKLogger.data.log("Query for updating parameters")
        let query = ParametersUpdateQuery(parametersInfo: parametersInfo)
        let request = await RequestMessage<ParametersUpdateQuery>(
            num: RequestNum.PARAMETERS_UPDATE,
            body: query
        )

        try await queryFetcher.query(
            request,
            responseType: CallbackMessage<EmptyResponse>.self
        )
    }
}
