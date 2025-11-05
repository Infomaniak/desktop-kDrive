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
    @LazyInjectService var xpcConnectionProvider: XPCConnectionProvider
    @LazyInjectService var coherentCache: CoherentCacheProtocol

    public enum LoginJobError: Error {
        case userNotFound
        case userDbIdNotFound
    }

    public init() {}

    /// Login job
    /// - Parameters:
    ///   - code: auth code
    ///   - verifier: auth verifier
    /// - Returns: A user to the current state know by the client
    public func loginAndFetchUser(code: String, verifier: String) async -> User? {
        do {
            let userDbId = try await loginUserQuery(code: code, verifier: verifier)
            if let user = await coherentCache.getUser(dbId: userDbId) {
                return user
            }

            // TODO:  Fetch user info from the server and store in cache
            return nil
        } catch {
            // TODO: Specific error handling for comms layer
            IKLogger.data.error("login comm error \(error)")
            return nil
        }
    }

    /// Login query, wraps XPC queries and Cache calls
    /// - Parameters:
    ///   - code: auth code
    ///   - verifier: auth verifier
    /// - Returns: userDbId that can be used to fetch a user
    private func loginUserQuery(code: String, verifier: String) async throws -> Int32 {
        IKLogger.data.log("Query for login token")
        let guiConnection = try await xpcConnectionProvider.guiConnection
        let userQuery = LoginQuery(code: code, codeVerifier: verifier)
        let request = await RequestMessage<LoginQuery>(num: RequestNum.LOGIN_REQUESTTOKEN, body: userQuery)

        let requestData = try JSONEncoder().encode(request)
        guard let replyData = await guiConnection.sendQueryAsync(requestData) else {
            throw LoginJobError.userNotFound
        }

        let decodedMessage = try? JSONDecoder().decode(CallbackMessage<LoginResponse>.self, from: replyData)
        IKLogger.data.log("recv decodedMessage: \(String(describing: decodedMessage))")

        guard let userDbId = decodedMessage?.body.userDbId else {
            throw LoginJobError.userDbIdNotFound
        }

        return userDbId
    }
}
