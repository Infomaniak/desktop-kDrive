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

public struct LoginJob: Sendable {
    @LazyInjectService private var queryFetcher: XPCQueryFetcherProtocol

    public init() {}

    public func login(code: String, verifier: String) async throws -> Int32 {
        IKLogger.data.log("Query for login token")
        let userQuery = LoginQuery(code: code, codeVerifier: verifier)
        let request = await RequestMessage<LoginQuery>(num: RequestNum.LOGIN_REQUESTTOKEN, body: userQuery)

        let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<LoginResponse>.self)

        return decodedMessage.body.userDbId
    }
}
