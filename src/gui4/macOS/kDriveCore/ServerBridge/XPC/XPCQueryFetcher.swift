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

public protocol XPCQueryFetcherProtocol {
    @discardableResult
    func query<Response: Decodable>(_ request: Encodable, responseType: Response.Type) async throws -> Response
}

struct XPCQueryFetcher: XPCQueryFetcherProtocol {
    let xpcConnectionProvider: XPCConnectionProvider

    let encoder = JSONEncoder()
    let decoder = JSONDecoder()

    init(xpcConnectionProvider: XPCConnectionProvider? = nil) {
        if let xpcConnectionProvider {
            self.xpcConnectionProvider = xpcConnectionProvider
        } else {
            @InjectService var connectionProvider: XPCConnectionProvider
            self.xpcConnectionProvider = connectionProvider
        }
    }

    enum QueryError: Error {
        case noReplyData
        case unableToDecodeReply(parsingError: Error)
    }

    @discardableResult
    func query<Response: Decodable>(_ request: Encodable, responseType: Response.Type) async throws -> Response {
        let requestData = try encoder.encode(request)

        let guiConnection = try await xpcConnectionProvider.guiConnection
        guard let replyData = await guiConnection.sendQueryAsync(requestData) else {
            IKLogger.data.error("[KD] no replyData on sendQueryAsync woops")
            throw QueryError.noReplyData
        }

        // IKLogger.data.log("[KD] recv raw: \(String(data: replyData, encoding: .utf8))")
        let headerMessage = try decoder.decode(CallbackMessage<EmptyResponse>.self, from: replyData)
        try headerMessage.validate()

        do {
            let decodedMessage = try decoder.decode(Response.self, from: replyData)
            IKLogger.data.log("[KD] recv callback: \(String(describing: decodedMessage))")
            return decodedMessage
        } catch {
            IKLogger.data.error("[KD] recv decoding woops \(error)")
            throw QueryError.unableToDecodeReply(parsingError: error)
        }
    }
}
