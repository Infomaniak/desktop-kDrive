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

        let logContext = RequestLogContext(request)
        logRequestSent(logContext)

        let startTime = DispatchTime.now()

        let guiConnection = try await xpcConnectionProvider.guiConnection
        guard let replyData = await guiConnection.sendQueryAsync(requestData) else {
            logNoReply(logContext)
            throw QueryError.noReplyData
        }

        let headerMessage = try decoder.decode(CallbackMessage<EmptyResponse>.self, from: replyData)
        logCallbackReceived(headerMessage, context: logContext, since: startTime)

        try headerMessage.validate()

        do {
            return try decoder.decode(Response.self, from: replyData)
        } catch {
            logDecodingFailure(error, header: headerMessage, context: logContext)
            throw QueryError.unableToDecodeReply(parsingError: error)
        }
    }
}

// MARK: - Logging

private extension XPCQueryFetcher {
    struct RequestLogContext {
        let num: String
        let id: String

        init(_ request: Encodable) {
            let loggable = request as? XPCLoggableRequest
            num = loggable.map { "\($0.requestNum)" } ?? "unknown"
            id = loggable.map { "\($0.requestId)" } ?? "?"
        }
    }

    func logRequestSent(_ context: RequestLogContext) {
        IKLogger.xpc.info("[KD] [XPC →] #\(context.id) \(context.num)")
    }

    func logNoReply(_ context: RequestLogContext) {
        IKLogger.xpc.error("[KD] [XPC ←] #\(context.id) \(context.num) no reply data")
    }

    func logCallbackReceived(_ header: CallbackMessage<EmptyResponse>, context: RequestLogContext, since start: DispatchTime) {
        let elapsed = String(format: "%.1f", Self.elapsedMilliseconds(since: start))
        let outcome = "[KD] [XPC ←] #\(header.id) \(context.num) \(header.code)/\(header.cause) (\(elapsed)ms)"
        if header.code != .Ok || header.cause != .Unknown {
            IKLogger.xpc.error(outcome)
        } else {
            IKLogger.xpc.info(outcome)
        }
    }

    func logDecodingFailure(_ error: Error, header: CallbackMessage<EmptyResponse>, context: RequestLogContext) {
        IKLogger.xpc.error("[KD] [XPC ←] #\(header.id) \(context.num) decode failed: \(error)")
    }

    static func elapsedMilliseconds(since start: DispatchTime) -> Double {
        let elapsedNanoseconds = DispatchTime.now().uptimeNanoseconds &- start.uptimeNanoseconds
        return Double(elapsedNanoseconds) / 1_000_000
    }
}
