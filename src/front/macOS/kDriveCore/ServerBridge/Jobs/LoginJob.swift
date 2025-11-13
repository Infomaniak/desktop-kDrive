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

public struct LoginJob: Sendable {
    @LazyInjectService private var queryFetcher: XPCQueryFetcherProtocol

    public enum LoginJobError: Error {
        case userNotFound
        case noReplyMessage
        case serverError(code: KDC.ExitCode, cause: KDC.ExitCause,)
    }

    public init() {}

    /// Login job
    /// - Parameters:
    ///   - code: auth code
    ///   - verifier: auth verifier
    @discardableResult
    public func login(code: String, verifier: String) async throws -> Int32 {
        try await loginUserQuery(code: code, verifier: verifier)
    }

    /// Login _query_ only
    /// - Parameters:
    ///   - code: auth code
    ///   - verifier: auth verifier
    /// - Returns: userDbId
    @discardableResult
    private func loginUserQuery(code: String, verifier: String) async throws -> Int32 {
        IKLogger.data.log("Query for login token")
        let userQuery = LoginQuery(code: code, codeVerifier: verifier)
        let request = await RequestMessage<LoginQuery>(num: RequestNum.LOGIN_REQUESTTOKEN, body: userQuery)

        do {
            let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<LoginResponse>.self)

            guard let decodedMessage else {
                throw LoginJobError.noReplyMessage
            }

            guard decodedMessage.code == .Ok, decodedMessage.cause == .Unknown else {
                throw LoginJobError.serverError(code: decodedMessage.code, cause: decodedMessage.cause)
            }

            return decodedMessage.body.userDbId
        } catch XPCQueryFetcher.QueryError.noReplyData {
            throw LoginJobError.userNotFound
        }
    }
}
