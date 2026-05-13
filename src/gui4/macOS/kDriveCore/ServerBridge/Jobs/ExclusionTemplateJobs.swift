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

public struct ExclusionTemplateJobs: Sendable {
    @LazyInjectService private var queryFetcher: XPCQueryFetcherProtocol

    public init() {}

    public func getNameExcluded(name: String) async throws -> Bool {
        IKLogger.data.log("Query for exclusionTemplateGetExcluded")
        let query = ExclusionTemplateGetExcludedQuery(name: name)
        let request = await RequestMessage<ExclusionTemplateGetExcludedQuery>(
            num: RequestNum.EXCLTEMPL_GETEXCLUDED,
            body: query
        )

        let decodedMessage = try await queryFetcher.query(
            request,
            responseType: CallbackMessage<ExclusionTemplateGetExcludedResponse>.self
        )

        return decodedMessage.body.isExcluded
    }

    public func getExclusionTemplateList(default isDefault: Bool) async throws -> [ExclusionTemplateInfo] {
        IKLogger.data.log("Query for exclusionTemplateGetList")
        let query = ExclusionTemplateGetListQuery(default: isDefault)
        let request = await RequestMessage<ExclusionTemplateGetListQuery>(
            num: RequestNum.EXCLTEMPL_GETLIST,
            body: query
        )

        let decodedMessage = try await queryFetcher.query(
            request,
            responseType: CallbackMessage<ExclusionTemplateGetListResponse>.self
        )

        return [ExclusionTemplateInfo](responses: decodedMessage.body.exclusionTemplateList)
    }
}
