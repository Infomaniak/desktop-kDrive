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

public struct AccountJobs: Sendable {
    @LazyInjectService private var coherentCache: CoherentCache
    @LazyInjectService private var queryFetcher: XPCQueryFetcherProtocol

    public init() {}

    @discardableResult
    public func accountInfoList() async throws -> [AccountInfo] {
        IKLogger.data.log("Query for accountInfoList")
        let query = EmptyQuery()
        let request = await RequestMessage<EmptyQuery>(num: RequestNum.ACCOUNT_INFOLIST, body: query)

        let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<AccountListResponse>.self)

        let accountList = decodedMessage.body.accountInfoList
        await accountList.asyncForEach { try? await coherentCache.addOrUpdateAccount(Account(with: $0)) }

        return accountList
    }
}
