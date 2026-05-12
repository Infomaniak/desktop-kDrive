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

public struct ExclusionAppJobs: Sendable {
    @LazyInjectService private var queryFetcher: XPCQueryFetcherProtocol

    public init() {}

    public func getExclusionAppList(default: Bool) async throws -> [ExclusionAppInfo] {
        IKLogger.data.log("Query for exclusionAppList")
        let query = ExclusionAppGetListQuery(default: `default`)
        let request = await RequestMessage<ExclusionAppGetListQuery>(
            num: RequestNum.EXCLAPP_GETLIST,
            body: query
        )

        let decodedMessage = try await queryFetcher.query(
            request,
            responseType: CallbackMessage<ExclusionAppGetListResponse>.self
        )

        return [ExclusionAppInfo](responses: decodedMessage.body.applicationList)
    }

    public func setExclusionAppList(default: Bool, applicationList: [ExclusionAppInfo]) async throws {
        IKLogger.data.log("Set exclusionAppList")
        let responses = applicationList
            .map { ExclusionAppInfoExchange(appId: $0.appId, description: $0.description, def: $0.def) }
        let query = ExclusionAppSetListQuery(default: `default`, applicationList: responses)
        let request = await RequestMessage<ExclusionAppSetListQuery>(
            num: RequestNum.EXCLAPP_SETLIST,
            body: query
        )

        _ = try await queryFetcher.query(
            request,
            responseType: CallbackMessage<ExclusionAppSetListResponse>.self
        )
    }
}
